#include "MOSpawnManagerSubsystem.h"
#include "MOFramework.h"
#include "MOGameSettings.h"
#include "MOSpawnPoint.h"
#include "MOSpawnSettings.h"
#include "MOSpawnSettingsActor.h"
#include "MOBuildableActor.h"
#include "MOCraftingStationActor.h"
#include "MOSkillsComponent.h"
#include "MORecruitmentComponent.h"
#include "MOIdentityComponent.h"
#include "VoxelWorld.h"

#include "EngineUtils.h"
#include "AIController.h"
#include "BrainComponent.h"

namespace
{
	// Common first and last names for random survivor generation.
	// Exposed via UMOSpawnManagerSubsystem::GenerateRandomSurvivorName() so the
	// new-game flow can pre-generate a name for the initial survivor (used in
	// the default save slot name).
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

	void AssignSurvivorNameIfNeeded(APawn* Pawn, EMOSpawnCategory Category)
	{
		if (!Pawn || Category != EMOSpawnCategory::Survivor)
		{
			return;
		}

		UMOIdentityComponent* IdentityComp = Pawn->FindComponentByClass<UMOIdentityComponent>();
		if (!IdentityComp || !IdentityComp->DisplayName.IsEmpty())
		{
			return; // No identity, or already named — nothing to do.
		}

		// One-shot pending name: if the new-game panel pre-generated a name for
		// this pawn (so the save slot could match it), consume it here and
		// clear so a second survivor spawn doesn't inherit the same name.
		FString NameToAssign;
		if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
		{
			if (!Settings->PendingSurvivorName.IsEmpty())
			{
				NameToAssign = Settings->PendingSurvivorName;
				Settings->PendingSurvivorName.Empty();
				UE_LOG(LogMOFramework, Log,
					TEXT("[SpawnManager] Consumed pending survivor name '%s' from new-game flow"),
					*NameToAssign);
			}
		}

		if (NameToAssign.IsEmpty())
		{
			NameToAssign = UMOSpawnManagerSubsystem::GenerateRandomSurvivorName();
		}

		IdentityComp->SetDisplayName(FText::FromString(NameToAssign));
		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Assigned name '%s' to survivor"), *NameToAssign);
	}
}

FString UMOSpawnManagerSubsystem::GenerateRandomSurvivorName()
{
	const FString& FirstName = SurvivorFirstNames[FMath::RandRange(0, SurvivorFirstNames.Num() - 1)];
	const FString& LastName = SurvivorLastNames[FMath::RandRange(0, SurvivorLastNames.Num() - 1)];
	return FString::Printf(TEXT("%s %s"), *FirstName, *LastName);
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

	// Only do expensive lookups when the spawn check interval elapses
	// Use cached interval, refresh it only during spawn checks
	if (TimeSinceLastCheck < CachedSpawnCheckInterval)
	{
		return;
	}

	TimeSinceLastCheck = 0.0f;

	// Now do the expensive lookups (only once per spawn check, not every frame)
	AMOSpawnSettingsActor* WorldActor = AMOSpawnSettingsActor::GetSpawnSettingsActor(this);

	// Early out if no settings actor - don't do any spawn work
	if (!WorldActor)
	{
		// No settings actor in this level - skip all spawn processing
		// Check again in 5 seconds
		CachedSpawnCheckInterval = 5.0f;
		return;
	}

	FMOSpawnGlobalSettings GlobalSettings = AMOSpawnSettingsActor::GetGlobalSettings(this);
	FMOSpawnDebugSettings DebugSettings = AMOSpawnSettingsActor::GetDebugSettings(this);

	// Update cached interval for next tick
	CachedSpawnCheckInterval = GlobalSettings.SpawnCheckInterval;

	CleanupDeadEntities();
	UpdatePersistence();
	ProcessAllCategories();
	UpdateFrozenPawnWakeCheck();
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
	CachedSpawnCheckInterval = GlobalSettings.SpawnCheckInterval;

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

		// Survivor config — closer than wildlife (you're meant to find them).
		// Restrict to shoreline band: tree canopies sit above 300, water at
		// or below ~0–5, so [5, 300] keeps survivors visible on open beach /
		// low ground where the player can actually find them.
		{
			FMOSpawnCategoryConfig Config;
			Config.Category = EMOSpawnCategory::Survivor;
			Config.MinCooldownSeconds = 3600.0f;
			Config.MaxCooldownSeconds = 7200.0f;
			Config.MaxSpawnedCount = 3;
			Config.MinSpawnDistance = 10.0f;    // 10m — close enough to discover but not spawned on top of you
			Config.MaxSpawnDistance = 30.0f;
			Config.MinDistanceFromStructure = 5.0f;
			Config.MinGroupSize = 1;
			Config.MaxGroupSize = 1;
			Config.bFirstSpawnFaster = true;
			Config.FirstSpawnMaxCooldown = 1800.0f;
			Config.bEnabled = true;
			Config.bRestrictSpawnZ = true;
			Config.MinSpawnZ = 5.0f;
			Config.MaxSpawnZ = 300.0f;
			CategoryConfigs.Add(Config);
		}

		// Prey config — far enough that the player doesn't see them pop in
		{
			FMOSpawnCategoryConfig Config;
			Config.Category = EMOSpawnCategory::Prey;
			Config.MinCooldownSeconds = 2700.0f;
			Config.MaxCooldownSeconds = 4500.0f;
			Config.MaxSpawnedCount = 10;
			Config.MinSpawnDistance = 100.0f;   // 100m — outside ordinary view
			Config.MaxSpawnDistance = 300.0f;
			Config.MinDistanceFromStructure = 50.0f;
			Config.MinGroupSize = 2;
			Config.MaxGroupSize = 3;
			Config.bFirstSpawnFaster = false;
			Config.bEnabled = true;
			CategoryConfigs.Add(Config);
		}

		// Predator config — farther still, they ambush from a distance
		{
			FMOSpawnCategoryConfig Config;
			Config.Category = EMOSpawnCategory::Predator;
			Config.MinCooldownSeconds = 5400.0f;
			Config.MaxCooldownSeconds = 9000.0f;
			Config.MaxSpawnedCount = 3;
			Config.MinSpawnDistance = 100.0f;   // 100m
			Config.MaxSpawnDistance = 200.0f;
			Config.MinDistanceFromStructure = 50.0f;
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

	// Hard cap: don't spawn past MaxSpawnedCount. The previous behavior — DespawnOldest
	// to make room and spawn a new one — created a rolling rotation that felt like
	// "mobs keep spawning past the limit" because new entities kept appearing and old
	// ones kept disappearing. With a hard cap, the count stays put until something
	// dies / is GC'd, then the next cooldown can spawn a replacement.
	const int32 CurrentCount = GetSpawnedCount(Category);
	if (CurrentCount >= Config.MaxSpawnedCount)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[SpawnManager] At cap (%d) for category %d — skipping spawn"),
			Config.MaxSpawnedCount, (int32)Category);
		return nullptr;
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
		MaybeSpawnSurvivorBeacon(SpawnedEntities.Num() - 1);
		OnEntitySpawned.Broadcast(SpawnedPawn, Category);
		NotifyPlayerOfSpawn(SpawnedPawn);

		if (ShouldFreezeCategory(Category))
		{
			FreezeSpawnedPawn(SpawnedPawn);
		}

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

	DestroyBeaconForRecord(SpawnedEntities[OldestIndex]);
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
			DestroyBeaconForRecord(SpawnedEntities[i]);
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
			DestroyBeaconForRecord(SpawnedEntities[i]);
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
		// First spawn - return 0 to allow spawn, but ProcessCategory will set initial staggered cooldown
		return 0.0f;
	}

	const FTimespan TimeSinceSpawn = FDateTime::Now() - *LastSpawn;
	const float Remaining = *Cooldown - TimeSinceSpawn.GetTotalSeconds();
	return FMath::Max(0.0f, Remaining);
}

bool UMOSpawnManagerSubsystem::IsLocationValidForSpawnNearEntities(FVector Location, float MinEntityDistance) const
{
	if (MinEntityDistance <= 0.0f)
	{
		return true;
	}

	// Check distance from all recently spawned entities
	for (const FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		if (!Record.IsValid())
		{
			continue;
		}

		const float Distance = FVector::Dist(Location, Record.SpawnedPawn->GetActorLocation());
		if (Distance < MinEntityDistance)
		{
			return false;
		}
	}

	return true;
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
	CachedSpawnCheckInterval = GlobalSettings.SpawnCheckInterval;

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

	// Check if this is a first spawn (no cooldown set yet)
	const bool bIsFirstSpawn = !LastSpawnTimes.Contains(Category);

	if (bIsFirstSpawn)
	{
		// First spawn - set an initial staggered cooldown instead of spawning all categories at once
		// This prevents everything from spawning on top of each other at game start
		// Use bFirstSpawnFaster config to determine if we use a shorter initial cooldown
		FMOSpawnDebugSettings DebugSettings = AMOSpawnSettingsActor::GetDebugSettings(this);
		float TimeMultiplier = FMath::Max(0.001f, DebugSettings.CooldownTimeMultiplier);

		// Stagger based on category enum value to spread out initial spawns
		float BaseStagger = 5.0f + (static_cast<int32>(Category) * 10.0f);  // 5s, 15s, 25s, 35s per category

		float InitialCooldown;
		if (Config.bFirstSpawnFaster)
		{
			// Use the faster first spawn cooldown, but still stagger
			InitialCooldown = FMath::FRandRange(BaseStagger, BaseStagger + Config.FirstSpawnMaxCooldown * 0.1f);
		}
		else
		{
			// Use a portion of the normal cooldown for initial spawn
			InitialCooldown = FMath::FRandRange(BaseStagger, BaseStagger + Config.MinCooldownSeconds * 0.25f);
		}

		InitialCooldown *= TimeMultiplier;

		LastSpawnTimes.Add(Category, FDateTime::Now());
		CurrentCooldowns.Add(Category, InitialCooldown);

		UE_LOG(LogMOFramework, Log, TEXT("[SpawnManager] Category %d first spawn - setting initial staggered cooldown: %.1fs"),
			(int32)Category, InitialCooldown);
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
			// Pawn is gone but the beacon might still be standing — destroy
			// it so we don't leak a "find this survivor" landmark with no
			// survivor at the other end.
			DestroyBeaconForRecord(SpawnedEntities[i]);
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

	// Optional Z-range gate. Same band-restrict logic as the fallback path,
	// applied to designed spawn points too. Doesn't re-trace ground here —
	// trusts the spawn point's authored Z. If a designer drops a Survivor
	// spawn point on a cliff, this gates it out.
	if (Config.bRestrictSpawnZ && (SpawnLocation.Z < Config.MinSpawnZ || SpawnLocation.Z > Config.MaxSpawnZ))
	{
		UE_LOG(LogMOFramework, Verbose,
			TEXT("[SpawnManager] SpawnAtPoint: rejected Z=%.1f for %s (allowed: %.1f..%.1f)"),
			SpawnLocation.Z, *UEnum::GetValueAsString(Config.Category), Config.MinSpawnZ, Config.MaxSpawnZ);
		return nullptr;
	}

	// Verify location is still valid (not too close to structures)
	if (!IsLocationValidForSpawn(SpawnLocation, Config.MinDistanceFromStructure))
	{
		return nullptr;
	}

	// Verify location is not too close to other spawned entities
	const float MinEntitySeparation = FMath::Max(200.0f, Config.MinSpawnDistance * 0.5f);
	if (!IsLocationValidForSpawnNearEntities(SpawnLocation, MinEntitySeparation))
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
		MaybeSpawnSurvivorBeacon(SpawnedEntities.Num() - 1);
		OnEntitySpawned.Broadcast(SpawnedPawn, Config.Category);
		NotifyPlayerOfSpawn(SpawnedPawn);

		if (ShouldFreezeCategory(Config.Category))
		{
			FreezeSpawnedPawn(SpawnedPawn);
		}

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
		// Random distance within config range. MinSpawnDistance/MaxSpawnDistance are
		// authored in METERS (per their UPROPERTY comments). Convert to UE world units
		// (centimeters) for actual placement. Without this, MinSpawnDistance=50 produced
		// spawns at 50cm (~half a meter) instead of 50m.
		const float DistanceMeters = FMath::FRandRange(Config.MinSpawnDistance, Config.MaxSpawnDistance);
		const float Distance = DistanceMeters * 100.0f;

		// Random angle
		const float Angle = FMath::FRandRange(0.0f, 360.0f);
		const FVector Offset = FVector(
			FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance,
			FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance,
			0.0f
		);

		FVector SpawnLocation = PlayerLocation + Offset;

		// Trace down to find ground — VOXEL ONLY. Single-trace would happily
		// stop at the first hit, which is often a PCG-spawned tree/rock ISM,
		// so the mob ends up perched in a tree. Use multi-trace and pick the
		// first AVoxelWorld hit so we land on actual terrain.
		TArray<FHitResult> GroundHits;
		const FVector TraceStart = SpawnLocation + FVector(0, 0, 5000.0f);
		const FVector TraceEnd = SpawnLocation - FVector(0, 0, 10000.0f);
		float GroundZ = SpawnLocation.Z; // fallback if no voxel hit found
		bool bHitVoxel = false;
		if (World->LineTraceMultiByChannel(GroundHits, TraceStart, TraceEnd, ECC_Visibility))
		{
			for (const FHitResult& Hit : GroundHits)
			{
				if (Hit.GetActor() && Hit.GetActor()->IsA<AVoxelWorld>())
				{
					GroundZ = Hit.ImpactPoint.Z;
					SpawnLocation = Hit.ImpactPoint + FVector(0, 0, 100.0f);
					bHitVoxel = true;
					break;
				}
			}
		}

		// If we didn't actually land on voxel terrain, this attempt is
		// inherently unsafe (we'd be in air or perched on whatever the trace
		// hit). Skip and try a different angle.
		if (!bHitVoxel)
		{
			continue;
		}

		// Optional Z-range gate. Categories like Survivor restrict spawns to
		// a terrain band (shoreline) where trees don't grow, eliminating
		// "found a survivor in the canopy" reports. The voxel-only trace
		// above guarantees GroundZ is real terrain, not foliage.
		if (Config.bRestrictSpawnZ && (GroundZ < Config.MinSpawnZ || GroundZ > Config.MaxSpawnZ))
		{
			UE_LOG(LogMOFramework, Verbose,
				TEXT("[SpawnManager] Fallback: rejected ground Z=%.1f for %s (allowed: %.1f..%.1f)"),
				GroundZ, *UEnum::GetValueAsString(Category), Config.MinSpawnZ, Config.MaxSpawnZ);
			continue;
		}

		// Check structure avoidance (MinDistanceFromStructure is also in METERS — convert to cm).
		const float MinStructureDistanceCm = Config.MinDistanceFromStructure * 100.0f;
		if (MinStructureDistanceCm > 0.0f && !IsLocationValidForSpawn(SpawnLocation, MinStructureDistanceCm))
		{
			continue;
		}

		// Check distance from other spawned entities (prevent spawning on top of each other).
		// Use half of MinSpawnDistance as the minimum separation, floor of 2 meters.
		const float MinEntitySeparationCm = FMath::Max(200.0f, (Config.MinSpawnDistance * 100.0f) * 0.5f);
		if (!IsLocationValidForSpawnNearEntities(SpawnLocation, MinEntitySeparationCm))
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
			MaybeSpawnSurvivorBeacon(SpawnedEntities.Num() - 1);

			// Notify
			OnEntitySpawned.Broadcast(SpawnedPawn, Category);
			NotifyPlayerOfSpawn(SpawnedPawn);

			if (ShouldFreezeCategory(Category))
			{
				FreezeSpawnedPawn(SpawnedPawn);
			}

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

// ============================================================================
// AI FREEZE
// ============================================================================

bool UMOSpawnManagerSubsystem::ShouldFreezeCategory(EMOSpawnCategory Category) const
{
	// Survivors are gameplay-critical for discovery — never freeze them.
	// Everything else (Prey, Predator, Ambient) freezes until the player is within
	// WakeDistanceCm. This is what stops the CPU running idle behavior trees on
	// 50+ deer scattered across the world.
	return Category != EMOSpawnCategory::Survivor;
}

void UMOSpawnManagerSubsystem::FreezeSpawnedPawn(APawn* Pawn)
{
	if (!Pawn) return;

	AController* Controller = Pawn->GetController();
	AAIController* AIController = Cast<AAIController>(Controller);
	if (!AIController) return;

	if (UBrainComponent* Brain = AIController->GetBrainComponent())
	{
		Brain->StopLogic(TEXT("SpawnedFarFromPlayer"));
	}
}

void UMOSpawnManagerSubsystem::WakeSpawnedPawn(APawn* Pawn)
{
	if (!Pawn) return;

	AController* Controller = Pawn->GetController();
	AAIController* AIController = Cast<AAIController>(Controller);
	if (!AIController) return;

	if (UBrainComponent* Brain = AIController->GetBrainComponent())
	{
		Brain->RestartLogic();
	}
}

// ============================================================================
// DIAGNOSTIC ACCESSORS (MO.AI.* cheat commands)
// ============================================================================
//
// These exist to verify the freeze pipeline actually halts BT ticks. The
// production code path already calls StopLogic/RestartLogic, but "the
// API call returned without error" is not the same as "the BT actually
// stopped ticking" — see Engineering Principle #8 in CLAUDE.md.
//
// DumpFreezeState gives a per-pawn snapshot. ForceFreezeAll/ForceWakeAll
// let you toggle the global state for profiler comparison passes.

void UMOSpawnManagerSubsystem::DumpFreezeState() const
{
	APawn* PlayerPawn = GetPlayerPawn();
	const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	const float WakeDistanceSq = WakeDistanceCm * WakeDistanceCm;

	const UEnum* CategoryEnum = StaticEnum<EMOSpawnCategory>();

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MO.AI.DumpFreezeState] === %d tracked entities, WakeDistance=%.0fcm (%.1fm), Player=%s ==="),
		SpawnedEntities.Num(), WakeDistanceCm, WakeDistanceCm / 100.0f,
		PlayerPawn ? *PlayerPawn->GetName() : TEXT("NULL"));

	int32 RunningCount = 0;
	int32 FrozenCount = 0;
	int32 AnomalyCount = 0;

	for (const FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		APawn* Pawn = Record.SpawnedPawn.Get();
		if (!Pawn)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MO.AI]   <stale> category=%s"),
				CategoryEnum ? *CategoryEnum->GetNameStringByValue(static_cast<int64>(Record.Category)) : TEXT("?"));
			continue;
		}

		AAIController* AIController = Cast<AAIController>(Pawn->GetController());
		UBrainComponent* Brain = AIController ? AIController->GetBrainComponent() : nullptr;

		const bool bShouldFreeze = ShouldFreezeCategory(Record.Category);
		const float DistanceSq = PlayerPawn ? FVector::DistSquared(PlayerLoc, Pawn->GetActorLocation()) : FLT_MAX;
		const float DistanceM = FMath::Sqrt(DistanceSq) / 100.0f;
		const bool bRunning = Brain && Brain->IsRunning();
		const bool bInWakeRange = bShouldFreeze && DistanceSq <= WakeDistanceSq;

		if (bRunning) ++RunningCount; else ++FrozenCount;

		// Anomalies: the freeze pipeline says we SHOULD be frozen but Brain
		// is running (the case the audit was suspicious about), OR we're
		// well past WakeDistance and still running — both indicate the
		// freeze isn't catching this entity.
		const bool bShouldBeFrozen = bShouldFreeze && !bInWakeRange;
		const bool bAnomaly = bShouldBeFrozen && bRunning;
		if (bAnomaly) ++AnomalyCount;

		UE_LOG(LogMOFramework, Warning,
			TEXT("[MO.AI]   %s%s  category=%s  dist=%.1fm  Brain=%s%s  inWakeRange=%s"),
			bAnomaly ? TEXT("[!] ") : TEXT("    "),
			*Pawn->GetName(),
			CategoryEnum ? *CategoryEnum->GetNameStringByValue(static_cast<int64>(Record.Category)) : TEXT("?"),
			DistanceM,
			Brain ? TEXT("present") : TEXT("MISSING"),
			Brain ? (bRunning ? TEXT(":running") : TEXT(":stopped")) : TEXT(""),
			bInWakeRange ? TEXT("YES") : TEXT("no"));
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MO.AI.DumpFreezeState] === Summary: %d running, %d stopped, %d anomalies "
		     "(anomaly = should be frozen by distance/category but Brain still running) ==="),
		RunningCount, FrozenCount, AnomalyCount);
}

int32 UMOSpawnManagerSubsystem::ForceFreezeAll()
{
	int32 Affected = 0;
	for (const FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		if (!ShouldFreezeCategory(Record.Category)) continue;
		APawn* Pawn = Record.SpawnedPawn.Get();
		if (!Pawn) continue;

		AAIController* AIController = Cast<AAIController>(Pawn->GetController());
		UBrainComponent* Brain = AIController ? AIController->GetBrainComponent() : nullptr;
		if (!Brain || !Brain->IsRunning()) continue;

		Brain->StopLogic(TEXT("MO.AI.ForceFreezeAll"));
		++Affected;
	}
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MO.AI.ForceFreezeAll] Stopped %d brains (out of %d tracked entities)"),
		Affected, SpawnedEntities.Num());
	return Affected;
}

int32 UMOSpawnManagerSubsystem::ForceWakeAll()
{
	int32 Affected = 0;
	for (const FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		APawn* Pawn = Record.SpawnedPawn.Get();
		if (!Pawn) continue;

		AAIController* AIController = Cast<AAIController>(Pawn->GetController());
		UBrainComponent* Brain = AIController ? AIController->GetBrainComponent() : nullptr;
		if (!Brain || Brain->IsRunning()) continue;

		Brain->RestartLogic();
		++Affected;
	}
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MO.AI.ForceWakeAll] Restarted %d brains (out of %d tracked entities)"),
		Affected, SpawnedEntities.Num());
	return Affected;
}

void UMOSpawnManagerSubsystem::UpdateFrozenPawnWakeCheck()
{
	APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn) return;

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const float WakeDistanceSq = WakeDistanceCm * WakeDistanceCm;

	for (const FMOSpawnedEntityRecord& Record : SpawnedEntities)
	{
		if (!Record.IsValid()) continue;
		if (!ShouldFreezeCategory(Record.Category)) continue;

		APawn* Pawn = Record.SpawnedPawn.Get();
		if (!Pawn) continue;

		// Skip pawns whose AI brain is already running (not frozen).
		AAIController* AIController = Cast<AAIController>(Pawn->GetController());
		if (!AIController) continue;
		UBrainComponent* Brain = AIController->GetBrainComponent();
		if (!Brain || Brain->IsRunning()) continue;

		// In range? Wake it up.
		const float DistanceSq = FVector::DistSquared(PlayerLocation, Pawn->GetActorLocation());
		if (DistanceSq <= WakeDistanceSq)
		{
			Brain->RestartLogic();
			UE_LOG(LogMOFramework, Verbose, TEXT("[SpawnManager] Woke %s (player %.1fm away)"),
				*Pawn->GetName(), FMath::Sqrt(DistanceSq) / 100.0f);
		}
	}
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

// ============================================================================
// SURVIVOR DISCOVERY BEACON
// ============================================================================

void UMOSpawnManagerSubsystem::MaybeSpawnSurvivorBeacon(int32 RecordIndex)
{
	if (!SpawnedEntities.IsValidIndex(RecordIndex))
	{
		return;
	}
	FMOSpawnedEntityRecord& Record = SpawnedEntities[RecordIndex];

	// Only survivors get beacons — the whole point is letting the player
	// find them at distance. Prey/predator/ambient don't need waypoints.
	if (Record.Category != EMOSpawnCategory::Survivor)
	{
		return;
	}

	// Pull beacon config from the Project Settings (UMOSpawnSettings). Single
	// source of truth — editable under MOFramework → Spawn Manager → Survivor
	// Beacon. No fallback duplicated on this subsystem.
	const UMOSpawnSettings* SpawnSettings = UMOSpawnSettings::GetSpawnSettings();
	if (!SpawnSettings)
	{
		return;
	}

	// Resolve the soft class reference. Loads on first survivor spawn, then
	// stays loaded for the session. If unset by the designer, the feature is
	// off — silently skip (no warning; "not configured" is a valid state).
	UClass* BeaconClass = SpawnSettings->SurvivorBeaconActorClass.LoadSynchronous();
	if (!BeaconClass)
	{
		return;
	}

	APawn* SurvivorPawn = Record.SpawnedPawn.Get();
	if (!IsValid(SurvivorPawn))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Offset is interpreted in the survivor's local frame (X=forward, Y=right).
	// Rotate by yaw only so the beacon ends up flat next to her regardless of
	// any roll/pitch from animation or terrain. Roll/pitch baked into the
	// offset would look erratic across spawn locations.
	const FRotator SurvivorRotation = SurvivorPawn->GetActorRotation();
	const FRotator YawOnly(0.0f, SurvivorRotation.Yaw, 0.0f);
	FVector BeaconLocation = SurvivorPawn->GetActorLocation() + YawOnly.RotateVector(SpawnSettings->SurvivorBeaconOffset);

	if (SpawnSettings->bSnapBeaconToGround)
	{
		// Trace from well above the offset down to well below it. Survivor
		// might be on a slope; her actor Z isn't a reliable ground anchor.
		// VOXEL ONLY — multi-trace and pick the first AVoxelWorld hit so the
		// beacon doesn't end up perched on a tree branch.
		TArray<FHitResult> BeaconHits;
		const FVector TraceStart = BeaconLocation + FVector(0.0f, 0.0f, 500.0f);
		const FVector TraceEnd = BeaconLocation - FVector(0.0f, 0.0f, 2000.0f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MOSurvivorBeaconGroundSnap), /*bTraceComplex=*/false);
		Params.AddIgnoredActor(SurvivorPawn);
		if (World->LineTraceMultiByChannel(BeaconHits, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			for (const FHitResult& BHit : BeaconHits)
			{
				if (BHit.GetActor() && BHit.GetActor()->IsA<AVoxelWorld>())
				{
					BeaconLocation = BHit.ImpactPoint;
					break;
				}
			}
		}
		// If the trace failed (e.g. void below), fall back to the unsnapped
		// position — bad placement is preferable to no beacon at all.
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient; // not persisted by save system
	AActor* Beacon = World->SpawnActor<AActor>(BeaconClass, BeaconLocation, FRotator::ZeroRotator, SpawnParams);
	if (!IsValid(Beacon))
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[SpawnManager] Survivor beacon spawn failed (class=%s, loc=%s)"),
			*GetNameSafe(BeaconClass), *BeaconLocation.ToString());
		return;
	}

	// Tag for runtime introspection / debug overlays / late-find cleanup.
	Beacon->Tags.AddUnique(TEXT("MO_SurvivorBeacon"));

	Record.BeaconActor = Beacon;

	// If the beacon is a crafting station (e.g. the campfire BP, which is
	// AMOCraftingStationActor), light it automatically. The BP defaults
	// already render correctly out-of-the-box (no ghost / no construction
	// phase) — what's missing is the active state that drives the fire
	// particles, the point light, and the audio loop. Drive that here via
	// the same SetStationActive(true) entry point the player's right-click
	// "Light" menu hits, so there's exactly one code path that "turns the
	// fire on".
	//
	// Beacons are landmarks, not crafting workstations the player tends —
	// disable the fuel requirement so the fire never goes out (and so
	// SetStationActive doesn't refuse to activate at CurrentFuel=0). The
	// CurrentFuel=MaxFuel set is cosmetic (any future UI reading fuel
	// shows it as "full").
	if (AMOCraftingStationActor* Station = Cast<AMOCraftingStationActor>(Beacon))
	{
		Station->bRequiresFuel = false;
		Station->CurrentFuel = Station->MaxFuel;
		Station->SetStationActive(true);
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[SpawnManager] Spawned survivor beacon %s at %s for %s"),
		*Beacon->GetName(), *BeaconLocation.ToString(), *SurvivorPawn->GetName());
}

void UMOSpawnManagerSubsystem::DestroyBeaconForRecord(FMOSpawnedEntityRecord& Record)
{
	AActor* Beacon = Record.BeaconActor.Get();
	if (IsValid(Beacon))
	{
		Beacon->Destroy();
	}
	Record.BeaconActor.Reset();
}
