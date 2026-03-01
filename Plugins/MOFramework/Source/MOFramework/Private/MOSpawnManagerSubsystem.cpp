#include "MOSpawnManagerSubsystem.h"
#include "MOFramework.h"
#include "MOSpawnPoint.h"
#include "MOSpawnSettings.h"
#include "MOSpawnSettingsActor.h"
#include "MOBuildableActor.h"
#include "MOSkillsComponent.h"
#include "MORecruitmentComponent.h"
#include "MOIdentityComponent.h"

#include "EngineUtils.h"

namespace
{
	// Common first and last names for random survivor generation
	const TArray<FString> SurvivorFirstNames = {
		TEXT("Alex"), TEXT("Sam"), TEXT("Jordan"), TEXT("Taylor"), TEXT("Morgan"),
		TEXT("Casey"), TEXT("Riley"), TEXT("Quinn"), TEXT("Avery"), TEXT("Parker"),
		TEXT("Emma"), TEXT("Liam"), TEXT("Olivia"), TEXT("Noah"), TEXT("Ava"),
		TEXT("Sophia"), TEXT("Jackson"), TEXT("Isabella"), TEXT("Lucas"), TEXT("Mia"),
		TEXT("James"), TEXT("Charlotte"), TEXT("Benjamin"), TEXT("Amelia"), TEXT("Elijah"),
		TEXT("Harper"), TEXT("William"), TEXT("Evelyn"), TEXT("Henry"), TEXT("Abigail")
	};

	const TArray<FString> SurvivorLastNames = {
		TEXT("Smith"), TEXT("Johnson"), TEXT("Williams"), TEXT("Brown"), TEXT("Jones"),
		TEXT("Garcia"), TEXT("Miller"), TEXT("Davis"), TEXT("Rodriguez"), TEXT("Martinez"),
		TEXT("Anderson"), TEXT("Taylor"), TEXT("Thomas"), TEXT("Moore"), TEXT("Jackson"),
		TEXT("Martin"), TEXT("Lee"), TEXT("Walker"), TEXT("Hall"), TEXT("Allen"),
		TEXT("Young"), TEXT("King"), TEXT("Wright"), TEXT("Scott"), TEXT("Green")
	};

	FString GenerateRandomSurvivorName()
	{
		const FString& FirstName = SurvivorFirstNames[FMath::RandRange(0, SurvivorFirstNames.Num() - 1)];
		const FString& LastName = SurvivorLastNames[FMath::RandRange(0, SurvivorLastNames.Num() - 1)];
		return FString::Printf(TEXT("%s %s"), *FirstName, *LastName);
	}

	void AssignSurvivorNameIfNeeded(APawn* Pawn, EMOSpawnCategory Category)
	{
		if (!Pawn || Category != EMOSpawnCategory::Survivor)
		{
			return;
		}

		UMOIdentityComponent* IdentityComp = Pawn->FindComponentByClass<UMOIdentityComponent>();
		if (IdentityComp && IdentityComp->DisplayName.IsEmpty())
		{
			FString GeneratedName = GenerateRandomSurvivorName();
			IdentityComp->SetDisplayName(FText::FromString(GeneratedName));
			UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Assigned name '%s' to survivor"), *GeneratedName);
		}
	}
}
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// Console command to reload spawn settings during PIE
static FAutoConsoleCommandWithWorld GReloadSpawnSettingsCmd(
	TEXT("MO.SpawnManager.Reload"),
	TEXT("Reload spawn manager settings from World Actor (or Project Settings fallback) without restarting PIE"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World)
		{
			if (UMOSpawnManagerSubsystem* SpawnManager = World->GetSubsystem<UMOSpawnManagerSubsystem>())
			{
				SpawnManager->ReloadSettings();
			}
		}
	})
);

UMOSpawnManagerSubsystem::UMOSpawnManagerSubsystem()
{
}

void UMOSpawnManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InitializeDefaultConfigs();

	UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Initialized with %d category configs"), CategoryConfigs.Num());
}

void UMOSpawnManagerSubsystem::Deinitialize()
{
	SpawnedEntities.Empty();
	LastSpawnTimes.Empty();
	CurrentCooldowns.Empty();
	HasSpawnedOnce.Empty();

	Super::Deinitialize();
}

bool UMOSpawnManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create in game worlds, not editor
	UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}

	return World->IsGameWorld();
}

void UMOSpawnManagerSubsystem::Tick(float DeltaTime)
{
	TimeSinceLastCheck += DeltaTime;

	// Get spawn check interval from world actor (dynamic lookup)
	AMOSpawnSettingsActor* WorldActor = AMOSpawnSettingsActor::GetSpawnSettingsActor(this);
	FMOSpawnGlobalSettings GlobalSettings = AMOSpawnSettingsActor::GetGlobalSettings(this);
	FMOSpawnDebugSettings DebugSettings = AMOSpawnSettingsActor::GetDebugSettings(this);
	float CurrentSpawnCheckInterval = GlobalSettings.SpawnCheckInterval;

	// Debug logging every 10 seconds
	static float DebugLogTimer = 0.f;
	DebugLogTimer += DeltaTime;
	if (DebugLogTimer >= 10.f)
	{
		DebugLogTimer = 0.f;
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] WorldActor: %s, CheckInterval: %.1f, TimeMultiplier: %.2f"),
			WorldActor ? *WorldActor->GetName() : TEXT("NONE"),
			CurrentSpawnCheckInterval,
			DebugSettings.CooldownTimeMultiplier);
	}

	if (TimeSinceLastCheck >= CurrentSpawnCheckInterval)
	{
		TimeSinceLastCheck = 0.0f;

		CleanupDeadEntities();
		UpdatePersistence();
		ProcessAllCategories();
	}
}

TStatId UMOSpawnManagerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOSpawnManagerSubsystem, STATGROUP_Tickables);
}

void UMOSpawnManagerSubsystem::InitializeDefaultConfigs()
{
	// Only initialize if empty
	if (CategoryConfigs.Num() > 0)
	{
		return;
	}

	// Try to load from world actor first (will be found once BeginPlay runs on the settings actor)
	// For now, load from project settings - ReloadSettings() can be called later to pick up world actor
	CategoryConfigs = AMOSpawnSettingsActor::GetAllCategoryConfigs(this);
	FMOSpawnGlobalSettings GlobalSettings = AMOSpawnSettingsActor::GetGlobalSettings(this);
	FMOSpawnDebugSettings DebugSettings = AMOSpawnSettingsActor::GetDebugSettings(this);

	SurvivorPersistenceHours = GlobalSettings.SurvivorPersistenceHours;
	SpawnCheckInterval = GlobalSettings.SpawnCheckInterval;
	MaxSpawnPointQueryDistance = GlobalSettings.MaxSpawnPointQueryDistance;

	// Check if we got configs (either from world actor or Project Settings fallback)
	if (CategoryConfigs.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Loaded configs (TimeMultiplier=%.2f, CheckInterval=%.1fs)"),
			DebugSettings.CooldownTimeMultiplier, SpawnCheckInterval);
	}
	else
	{
		// Fallback to hardcoded defaults if settings not available
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Could not load settings, using hardcoded defaults"));

		// Survivor config
		{
			FMOSpawnCategoryConfig Config;
			Config.Category = EMOSpawnCategory::Survivor;
			Config.MinCooldownSeconds = 3600.0f;
			Config.MaxCooldownSeconds = 7200.0f;
			Config.MaxSpawnedCount = 3;
			Config.MinSpawnDistance = 200.0f;
			Config.MaxSpawnDistance = 500.0f;
			Config.MinDistanceFromStructure = 100.0f;
			Config.MinGroupSize = 1;
			Config.MaxGroupSize = 1;
			Config.bFirstSpawnFaster = true;
			Config.FirstSpawnMaxCooldown = 1800.0f;
			Config.bEnabled = true;
			CategoryConfigs.Add(Config);
		}

		// Prey config
		{
			FMOSpawnCategoryConfig Config;
			Config.Category = EMOSpawnCategory::Prey;
			Config.MinCooldownSeconds = 2700.0f;
			Config.MaxCooldownSeconds = 4500.0f;
			Config.MaxSpawnedCount = 10;
			Config.MinSpawnDistance = 100.0f;
			Config.MaxSpawnDistance = 400.0f;
			Config.MinDistanceFromStructure = 200.0f;
			Config.MinGroupSize = 2;
			Config.MaxGroupSize = 3;
			Config.bFirstSpawnFaster = false;
			Config.bEnabled = true;
			CategoryConfigs.Add(Config);
		}

		// Predator config
		{
			FMOSpawnCategoryConfig Config;
			Config.Category = EMOSpawnCategory::Predator;
			Config.MinCooldownSeconds = 5400.0f;
			Config.MaxCooldownSeconds = 9000.0f;
			Config.MaxSpawnedCount = 3;
			Config.MinSpawnDistance = 300.0f;
			Config.MaxSpawnDistance = 600.0f;
			Config.MinDistanceFromStructure = 300.0f;
			Config.MinGroupSize = 1;
			Config.MaxGroupSize = 2;
			Config.bFirstSpawnFaster = false;
			Config.bEnabled = true;
			CategoryConfigs.Add(Config);
		}

		// Ambient config
		{
			FMOSpawnCategoryConfig Config;
			Config.Category = EMOSpawnCategory::Ambient;
			Config.MinCooldownSeconds = 300.0f;
			Config.MaxCooldownSeconds = 600.0f;
			Config.MaxSpawnedCount = 20;
			Config.MinSpawnDistance = 5.0f;
			Config.MaxSpawnDistance = 50.0f;
			Config.MinDistanceFromStructure = 0.0f;
			Config.MinGroupSize = 1;
			Config.MaxGroupSize = 4;
			Config.bFirstSpawnFaster = false;
			Config.bEnabled = true;
			CategoryConfigs.Add(Config);
		}
	}
}

// ============================================================================
// CORE API
// ============================================================================

APawn* UMOSpawnManagerSubsystem::TrySpawnForCategory(EMOSpawnCategory Category)
{
	const FMOSpawnCategoryConfig Config = GetCategoryConfig(Category);
	if (!Config.bEnabled)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Category %d is disabled"), (int32)Category);
		return nullptr;
	}

	if (Config.SpawnableClasses.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] No spawnable classes for category %d"), (int32)Category);
		return nullptr;
	}

	// Check max count
	const int32 CurrentCount = GetSpawnedCount(Category);
	if (CurrentCount >= Config.MaxSpawnedCount)
	{
		// Try to despawn oldest
		if (!DespawnOldest(Category))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Max count (%d) reached for category %d, can't despawn oldest"),
				Config.MaxSpawnedCount, (int32)Category);
			return nullptr;
		}
	}

	// Get valid spawn points
	TArray<AMOSpawnPoint*> ValidPoints = GetValidSpawnPoints(Category);

	// If no spawn points, try fallback spawning near player
	if (ValidPoints.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] No spawn points for category %d, using fallback spawn near player"), (int32)Category);
		return TryFallbackSpawn(Category, Config);
	}

	// Select random spawn point weighted by SelectionWeight
	float TotalWeight = 0.0f;
	for (const AMOSpawnPoint* Point : ValidPoints)
	{
		TotalWeight += Point->SelectionWeight;
	}

	float RandomValue = FMath::FRandRange(0.0f, TotalWeight);
	AMOSpawnPoint* SelectedPoint = nullptr;
	float AccumulatedWeight = 0.0f;

	for (AMOSpawnPoint* Point : ValidPoints)
	{
		AccumulatedWeight += Point->SelectionWeight;
		if (RandomValue <= AccumulatedWeight)
		{
			SelectedPoint = Point;
			break;
		}
	}

	if (!SelectedPoint)
	{
		SelectedPoint = ValidPoints[0];
	}

	// Spawn at the selected point
	return SpawnAtPoint(SelectedPoint, Config);
}

APawn* UMOSpawnManagerSubsystem::ForceSpawnAtLocation(EMOSpawnCategory Category, FVector Location, FRotator Rotation)
{
	const FMOSpawnCategoryConfig Config = GetCategoryConfig(Category);

	if (Config.SpawnableClasses.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] No spawnable classes for category %d"), (int32)Category);
		return nullptr;
	}

	// Select random class
	const int32 ClassIndex = FMath::RandRange(0, Config.SpawnableClasses.Num() - 1);
	TSubclassOf<APawn> PawnClass = Config.SpawnableClasses[ClassIndex];

	APawn* SpawnedPawn = SpawnPawnAtLocation(PawnClass, Location, Rotation);
	if (SpawnedPawn)
	{
		// Assign name for survivors
		AssignSurvivorNameIfNeeded(SpawnedPawn, Category);

		// Add to tracking
		SpawnedEntities.Add(FMOSpawnedEntityRecord(SpawnedPawn, Category));
		OnEntitySpawned.Broadcast(SpawnedPawn, Category);
		NotifyPlayerOfSpawn(SpawnedPawn);

		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Force spawned %s at %s"),
			*SpawnedPawn->GetName(), *Location.ToString());
	}

	return SpawnedPawn;
}

bool UMOSpawnManagerSubsystem::DespawnOldest(EMOSpawnCategory Category)
{
	APawn* PlayerPawn = GetPlayerPawn();
	FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	// Get minimum despawn distance from settings
	FMOSpawnGlobalSettings GlobalSettings = AMOSpawnSettingsActor::GetGlobalSettings(this);
	const float MinDespawnDistance = GlobalSettings.MinDespawnDistance;

	// Find oldest non-persistent entity of this category that's far enough from player
	int32 OldestIndex = INDEX_NONE;
	FDateTime OldestTime = FDateTime::MaxValue();

	for (int32 i = 0; i < SpawnedEntities.Num(); ++i)
	{
		const FMOSpawnedEntityRecord& Record = SpawnedEntities[i];

		if (Record.Category != Category)
		{
			continue;
		}

		if (!Record.IsValid())
		{
			continue;
		}

		// Skip persistent entities that haven't expired
		if (Record.bPersistent && !Record.IsPersistenceExpired())
		{
			continue;
		}

		// Skip entities too close to player - don't despawn in front of them
		if (PlayerPawn && Record.SpawnedPawn.IsValid())
		{
			float DistanceToPlayer = FVector::Dist(Record.SpawnedPawn->GetActorLocation(), PlayerLocation);
			if (DistanceToPlayer < MinDespawnDistance)
			{
				continue;
			}
		}

		if (Record.SpawnTime < OldestTime)
		{
			OldestTime = Record.SpawnTime;
			OldestIndex = i;
		}
	}

	if (OldestIndex == INDEX_NONE)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[SpawnManager] No entities far enough to despawn for category %d (min distance: %.0fcm)"),
			(int32)Category, MinDespawnDistance);
		return false;
	}

	// Despawn
	FMOSpawnedEntityRecord& Record = SpawnedEntities[OldestIndex];
	APawn* Pawn = Record.SpawnedPawn.Get();

	if (Pawn)
	{
		float DistanceToPlayer = PlayerPawn ? FVector::Dist(Pawn->GetActorLocation(), PlayerLocation) : 0.f;
		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Despawning oldest: %s (%.0fm from player)"),
			*Pawn->GetName(), DistanceToPlayer / 100.f);
		OnEntityDespawned.Broadcast(Pawn, Category);
		Pawn->Destroy();
	}

	SpawnedEntities.RemoveAt(OldestIndex);
	return true;
}

void UMOSpawnManagerSubsystem::DespawnEntity(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	for (int32 i = SpawnedEntities.Num() - 1; i >= 0; --i)
	{
		if (SpawnedEntities[i].SpawnedPawn.Get() == Pawn)
		{
			OnEntityDespawned.Broadcast(Pawn, SpawnedEntities[i].Category);
			SpawnedEntities.RemoveAt(i);
			break;
		}
	}

	Pawn->Destroy();
}

void UMOSpawnManagerSubsystem::MarkEntityPersistent(APawn* Pawn, float PersistenceHours)
{
	for (FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		if (Record.SpawnedPawn.Get() == Pawn)
		{
			Record.bPersistent = true;
			Record.PersistenceExpiry = FDateTime::Now() + FTimespan::FromHours(PersistenceHours);
			UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Marked %s persistent for %.1f hours"),
				*Pawn->GetName(), PersistenceHours);
			return;
		}
	}
}

void UMOSpawnManagerSubsystem::ClearEntityPersistence(APawn* Pawn)
{
	for (FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		if (Record.SpawnedPawn.Get() == Pawn)
		{
			Record.bPersistent = false;
			Record.PersistenceExpiry = FDateTime::MinValue();
			UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Cleared persistence for %s"), *Pawn->GetName());
			return;
		}
	}
}

void UMOSpawnManagerSubsystem::RemoveFromTracking(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	for (int32 i = SpawnedEntities.Num() - 1; i >= 0; --i)
	{
		if (SpawnedEntities[i].SpawnedPawn.Get() == Pawn)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Removed %s from tracking (recruited - will never despawn)"), *Pawn->GetName());
			SpawnedEntities.RemoveAt(i);
			return;
		}
	}
}

void UMOSpawnManagerSubsystem::ConvertToCorpse(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	// TODO: Convert pawn to corpse mesh/actor
	// For now, just despawn
	UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Converting %s to corpse (placeholder)"), *Pawn->GetName());
	DespawnEntity(Pawn);
}

// ============================================================================
// QUERIES
// ============================================================================

FMOSpawnCategoryConfig UMOSpawnManagerSubsystem::GetCategoryConfig(EMOSpawnCategory Category) const
{
	// Check if world actor exists
	AMOSpawnSettingsActor* WorldActor = AMOSpawnSettingsActor::GetSpawnSettingsActor(this);

	// Get config from world actor if it exists
	FMOSpawnCategoryConfig Result;
	bool bHasWorldActor = (WorldActor != nullptr);

	if (bHasWorldActor)
	{
		// Get timing/behavior settings from world actor
		Result = AMOSpawnSettingsActor::GetCategoryConfig(this, Category);
	}
	else
	{
		// Fall back to cached configs (from project settings or defaults)
		for (const FMOSpawnCategoryConfig& Config : CategoryConfigs)
		{
			if (Config.Category == Category)
			{
				Result = Config;
				break;
			}
		}
	}

	// If world actor config has no SpawnableClasses, get them from cached configs (Project Settings)
	if (Result.SpawnableClasses.Num() == 0)
	{
		for (const FMOSpawnCategoryConfig& Config : CategoryConfigs)
		{
			if (Config.Category == Category && Config.SpawnableClasses.Num() > 0)
			{
				Result.SpawnableClasses = Config.SpawnableClasses;
				break;
			}
		}
	}

	// Ensure category is set correctly
	Result.Category = Category;

	return Result;
}

int32 UMOSpawnManagerSubsystem::GetSpawnedCount(EMOSpawnCategory Category) const
{
	int32 Count = 0;
	for (const FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		if (Record.Category == Category && Record.IsValid())
		{
			Count++;
		}
	}
	return Count;
}

float UMOSpawnManagerSubsystem::GetCategoryCooldownRemaining(EMOSpawnCategory Category) const
{
	const FDateTime* LastSpawn = LastSpawnTimes.Find(Category);
	const float* Cooldown = CurrentCooldowns.Find(Category);

	if (!LastSpawn || !Cooldown)
	{
		return 0.0f;
	}

	const FTimespan TimeSinceSpawn = FDateTime::Now() - *LastSpawn;
	const float Remaining = *Cooldown - TimeSinceSpawn.GetTotalSeconds();
	return FMath::Max(0.0f, Remaining);
}

bool UMOSpawnManagerSubsystem::IsLocationValidForSpawn(FVector Location, float MinStructureDistance) const
{
	if (MinStructureDistance <= 0.0f)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Query for buildable actors (campfires, baskets, structures, etc.)
	for (TActorIterator<AMOBuildableActor> It(World); It; ++It)
	{
		AMOBuildableActor* BuildableActor = *It;

		// Only count completed buildings (not ghosts)
		if (!BuildableActor->IsComplete())
		{
			continue;
		}

		const float Distance = FVector::Dist(Location, BuildableActor->GetActorLocation());
		if (Distance < MinStructureDistance)
		{
			return false;
		}
	}

	return true;
}

TArray<AMOSpawnPoint*> UMOSpawnManagerSubsystem::GetValidSpawnPoints(EMOSpawnCategory Category) const
{
	TArray<AMOSpawnPoint*> Result;

	APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn)
	{
		return Result;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FMOSpawnCategoryConfig Config = GetCategoryConfig(Category);

	UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}

	// Find all spawn points
	for (TActorIterator<AMOSpawnPoint> It(World); It; ++It)
	{
		AMOSpawnPoint* Point = *It;

		// Check if point can spawn this category
		if (!Point->CanSpawnCategory(Category))
		{
			continue;
		}

		// Check if point is available
		if (!Point->IsAvailable())
		{
			continue;
		}

		// Check distance from player
		const float DistanceToPlayer = FVector::Dist(Point->GetActorLocation(), PlayerLocation);
		if (DistanceToPlayer < Config.MinSpawnDistance || DistanceToPlayer > Config.MaxSpawnDistance)
		{
			continue;
		}

		// Check distance from structures
		if (!IsLocationValidForSpawn(Point->GetActorLocation(), Config.MinDistanceFromStructure))
		{
			continue;
		}

		Result.Add(Point);
	}

	return Result;
}

bool UMOSpawnManagerSubsystem::CanSpawn() const
{
	// Must have a player
	APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn)
	{
		return false;
	}

	// Must be in a game world
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	return true;
}

void UMOSpawnManagerSubsystem::ReloadSettings()
{
	// Load from world actor (falls back to Project Settings automatically)
	CategoryConfigs = AMOSpawnSettingsActor::GetAllCategoryConfigs(this);
	FMOSpawnGlobalSettings GlobalSettings = AMOSpawnSettingsActor::GetGlobalSettings(this);
	FMOSpawnDebugSettings DebugSettings = AMOSpawnSettingsActor::GetDebugSettings(this);

	SurvivorPersistenceHours = GlobalSettings.SurvivorPersistenceHours;
	SpawnCheckInterval = GlobalSettings.SpawnCheckInterval;
	MaxSpawnPointQueryDistance = GlobalSettings.MaxSpawnPointQueryDistance;

	// Check if we have a world actor
	AMOSpawnSettingsActor* WorldActor = AMOSpawnSettingsActor::GetSpawnSettingsActor(this);
	if (WorldActor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Reloaded settings from World Actor '%s' (TimeMultiplier=%.2f, CheckInterval=%.1fs)"),
			*WorldActor->GetName(), DebugSettings.CooldownTimeMultiplier, SpawnCheckInterval);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Reloaded settings from Project Settings (TimeMultiplier=%.2f, CheckInterval=%.1fs)"),
			DebugSettings.CooldownTimeMultiplier, SpawnCheckInterval);
	}
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOSpawnManagerSubsystem::ProcessAllCategories()
{
	if (!CanSpawn())
	{
		return;
	}

	// Get fresh configs from world actor (dynamic lookup each tick)
	// This ensures we always use the latest settings from the placed actor
	TArray<FMOSpawnCategoryConfig> CurrentConfigs = AMOSpawnSettingsActor::GetAllCategoryConfigs(this);

	// If no world actor, use cached configs from project settings
	if (CurrentConfigs.Num() == 0)
	{
		CurrentConfigs = CategoryConfigs;
	}

	for (const FMOSpawnCategoryConfig& Config : CurrentConfigs)
	{
		if (Config.bEnabled)
		{
			// Get the full config with SpawnableClasses merged from project settings
			FMOSpawnCategoryConfig FullConfig = GetCategoryConfig(Config.Category);
			ProcessCategory(Config.Category, FullConfig);
		}
	}
}

void UMOSpawnManagerSubsystem::ProcessCategory(EMOSpawnCategory Category, const FMOSpawnCategoryConfig& Config)
{
	// Check if we have spawnable classes
	if (Config.SpawnableClasses.Num() == 0)
	{
		// Only log once per category to avoid spam
		static TSet<EMOSpawnCategory> WarnedCategories;
		if (!WarnedCategories.Contains(Category))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Category %d has no SpawnableClasses configured - skipping"),
				(int32)Category);
			WarnedCategories.Add(Category);
		}
		return;
	}

	// Check if cooldown has elapsed
	const float RemainingCooldown = GetCategoryCooldownRemaining(Category);
	if (RemainingCooldown > 0.0f)
	{
		return;
	}

	// Log that we're attempting to spawn
	UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Cooldown elapsed for category %d, attempting spawn..."), (int32)Category);

	// Try to spawn
	const int32 GroupSize = FMath::RandRange(Config.MinGroupSize, Config.MaxGroupSize);
	int32 SpawnedCount = 0;

	for (int32 i = 0; i < GroupSize; ++i)
	{
		APawn* Spawned = TrySpawnForCategory(Category);
		if (Spawned)
		{
			SpawnedCount++;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Failed to spawn entity %d/%d for category %d"),
				i + 1, GroupSize, (int32)Category);
			break;  // No valid spawn points available
		}
	}

	if (SpawnedCount > 0)
	{
		// Update spawn time and roll new cooldown
		LastSpawnTimes.Add(Category, FDateTime::Now());

		const bool bIsFirstSpawn = !HasSpawnedOnce.Contains(Category) || !HasSpawnedOnce[Category];
		CurrentCooldowns.Add(Category, RollNewCooldown(Config));
		HasSpawnedOnce.Add(Category, true);

		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] SUCCESS: Spawned %d entities for category %d, next cooldown: %.1fs"),
			SpawnedCount, (int32)Category, CurrentCooldowns[Category]);
	}
	else
	{
		// Still set a cooldown to avoid spamming spawn attempts
		LastSpawnTimes.Add(Category, FDateTime::Now());
		CurrentCooldowns.Add(Category, 30.0f);  // Try again in 30 seconds
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] FAILED: No spawns for category %d, retrying in 30s"), (int32)Category);
	}
}

void UMOSpawnManagerSubsystem::CleanupDeadEntities()
{
	for (int32 i = SpawnedEntities.Num() - 1; i >= 0; --i)
	{
		if (!SpawnedEntities[i].IsValid())
		{
			SpawnedEntities.RemoveAt(i);
		}
	}
}

void UMOSpawnManagerSubsystem::UpdatePersistence()
{
	const FDateTime Now = FDateTime::Now();

	for (FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		if (Record.bPersistent && Record.PersistenceExpiry != FDateTime::MinValue())
		{
			if (Now > Record.PersistenceExpiry)
			{
				// Persistence expired
				Record.bPersistent = false;

				// Check if this is a survivor with an active quest
				if (Record.Category == EMOSpawnCategory::Survivor && Record.SpawnedPawn.IsValid())
				{
					APawn* Pawn = Record.SpawnedPawn.Get();
					UMORecruitmentComponent* RecruitComp = Pawn->FindComponentByClass<UMORecruitmentComponent>();
					if (RecruitComp && RecruitComp->HasActiveQuest())
					{
						// Quest timed out - convert to corpse
						UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Survivor %s quest timed out"),
							*Pawn->GetName());
						ConvertToCorpse(Pawn);
					}
				}
			}
		}
	}
}

APawn* UMOSpawnManagerSubsystem::SpawnAtPoint(AMOSpawnPoint* Point, const FMOSpawnCategoryConfig& Config)
{
	if (!Point || Config.SpawnableClasses.Num() == 0)
	{
		return nullptr;
	}

	// Get spawn location
	FVector SpawnLocation = Point->GetRandomSpawnLocation();

	// Verify location is still valid
	if (!IsLocationValidForSpawn(SpawnLocation, Config.MinDistanceFromStructure))
	{
		return nullptr;
	}

	// Select random class
	const int32 ClassIndex = FMath::RandRange(0, Config.SpawnableClasses.Num() - 1);
	TSubclassOf<APawn> PawnClass = Config.SpawnableClasses[ClassIndex];

	// Random rotation
	FRotator SpawnRotation = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

	APawn* SpawnedPawn = SpawnPawnAtLocation(PawnClass, SpawnLocation, SpawnRotation);
	if (SpawnedPawn)
	{
		Point->MarkUsed();

		// Assign name for survivors
		AssignSurvivorNameIfNeeded(SpawnedPawn, Config.Category);

		// Add to tracking
		SpawnedEntities.Add(FMOSpawnedEntityRecord(SpawnedPawn, Config.Category));
		OnEntitySpawned.Broadcast(SpawnedPawn, Config.Category);
		NotifyPlayerOfSpawn(SpawnedPawn);

		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Spawned %s at spawn point %s"),
			*SpawnedPawn->GetName(), *Point->GetName());
	}

	return SpawnedPawn;
}

APawn* UMOSpawnManagerSubsystem::SpawnPawnAtLocation(TSubclassOf<APawn> PawnClass, FVector Location, FRotator Rotation)
{
	if (!PawnClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedPawn = World->SpawnActor<APawn>(PawnClass, Location, Rotation, SpawnParams);
	return SpawnedPawn;
}

APawn* UMOSpawnManagerSubsystem::TryFallbackSpawn(EMOSpawnCategory Category, const FMOSpawnCategoryConfig& Config)
{
	// Fallback spawning when no spawn points exist - spawn in random direction from player
	APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Fallback spawn failed - no player pawn"));
		return nullptr;
	}

	if (Config.SpawnableClasses.Num() == 0)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();

	// Try multiple times to find a valid spawn location
	const int32 MaxAttempts = 10;
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Random distance within config range
		const float Distance = FMath::FRandRange(Config.MinSpawnDistance, Config.MaxSpawnDistance);

		// Random angle
		const float Angle = FMath::FRandRange(0.0f, 360.0f);
		const FVector Offset = FVector(
			FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance,
			FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance,
			0.0f
		);

		FVector SpawnLocation = PlayerLocation + Offset;

		// Trace down to find ground
		FHitResult HitResult;
		FVector TraceStart = SpawnLocation + FVector(0, 0, 5000.0f);
		FVector TraceEnd = SpawnLocation - FVector(0, 0, 10000.0f);

		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility))
		{
			SpawnLocation = HitResult.ImpactPoint + FVector(0, 0, 100.0f);  // Spawn slightly above ground
		}

		// Check structure avoidance
		if (Config.MinDistanceFromStructure > 0.0f && !IsLocationValidForSpawn(SpawnLocation, Config.MinDistanceFromStructure))
		{
			continue;
		}

		// Pick random class
		const int32 ClassIndex = FMath::RandRange(0, Config.SpawnableClasses.Num() - 1);
		TSubclassOf<APawn> PawnClass = Config.SpawnableClasses[ClassIndex];

		if (!PawnClass)
		{
			continue;
		}

		// Random rotation
		FRotator SpawnRotation = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

		APawn* SpawnedPawn = SpawnPawnAtLocation(PawnClass, SpawnLocation, SpawnRotation);
		if (SpawnedPawn)
		{
			// Assign name for survivors
			AssignSurvivorNameIfNeeded(SpawnedPawn, Category);

			// Add to tracking
			FMOSpawnedEntityRecord Record(SpawnedPawn, Category);
			SpawnedEntities.Add(Record);

			// Notify
			OnEntitySpawned.Broadcast(SpawnedPawn, Category);
			NotifyPlayerOfSpawn(SpawnedPawn);

			UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Fallback spawned %s at %s (%.0fm from player)"),
				*SpawnedPawn->GetClass()->GetName(),
				*SpawnLocation.ToString(),
				Distance / 100.0f);

			return SpawnedPawn;
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[SpawnManager] Fallback spawn failed after %d attempts"), MaxAttempts);
	return nullptr;
}

float UMOSpawnManagerSubsystem::RollNewCooldown(const FMOSpawnCategoryConfig& Config) const
{
	// Get time multiplier from world actor or project settings (for testing - lower = faster spawns)
	FMOSpawnDebugSettings DebugSettings = AMOSpawnSettingsActor::GetDebugSettings(this);
	float TimeMultiplier = FMath::Max(0.001f, DebugSettings.CooldownTimeMultiplier);

	// Check for first spawn faster
	const bool bIsFirstSpawn = !HasSpawnedOnce.Contains(Config.Category) || !HasSpawnedOnce[Config.Category];

	float Cooldown;
	if (bIsFirstSpawn && Config.bFirstSpawnFaster)
	{
		Cooldown = FMath::FRandRange(Config.MinCooldownSeconds, Config.FirstSpawnMaxCooldown);
	}
	else
	{
		Cooldown = FMath::FRandRange(Config.MinCooldownSeconds, Config.MaxCooldownSeconds);
	}

	// Apply time multiplier
	Cooldown *= TimeMultiplier;

	if (TimeMultiplier != 1.0f)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Cooldown for %d: %.1f sec (multiplier=%.2f)"),
			(int32)Config.Category, Cooldown, TimeMultiplier);
	}

	return Cooldown;
}

APawn* UMOSpawnManagerSubsystem::GetPlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	return PC->GetPawn();
}

void UMOSpawnManagerSubsystem::NotifyPlayerOfSpawn(APawn* SpawnedPawn)
{
	if (!SpawnedPawn)
	{
		return;
	}

	APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn)
	{
		return;
	}

	// Check player's perception skill
	UMOSkillsComponent* Skills = PlayerPawn->FindComponentByClass<UMOSkillsComponent>();
	if (!Skills)
	{
		return;
	}

	const int32 PerceptionLevel = Skills->GetSkillLevel(FName("Perception"));
	const float Distance = FVector::Dist(SpawnedPawn->GetActorLocation(), PlayerPawn->GetActorLocation());

	// Base chance modified by skill and distance
	const float BaseChance = 0.1f;  // 10% base
	const float SkillBonus = PerceptionLevel * 0.05f;  // +5% per level
	const float DistancePenalty = FMath::Clamp(Distance / 1000.0f, 0.0f, 0.5f);  // Further = harder

	const float FinalChance = FMath::Clamp(BaseChance + SkillBonus - DistancePenalty, 0.0f, 1.0f);

	if (FMath::FRand() < FinalChance)
	{
		// TODO: Send notification to player UI
		// TODO: Add marker to compass/map
		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Player noticed spawn of %s (chance: %.1f%%)"),
			*SpawnedPawn->GetName(), FinalChance * 100.0f);
	}
}
