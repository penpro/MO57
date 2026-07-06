#include "MOColonyManagerSubsystem.h"
#include "MOFramework.h"
#include "MOCharacter.h"
#include "MOContainerActor.h"
#include "MOCraftingStationActor.h"
#include "MOBuildableActor.h"
#include "MOIdentityComponent.h"
#include "MORecruitmentComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOVitalsComponent.h"
#include "MOPersonalityComponent.h"
#include "MOCharacterHistoryComponent.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MORecipeDatabaseSettings.h"
#include "MORecipeDefinitionRow.h"
#include "MOGameClockSubsystem.h"
#include "MOSurvivorController.h"
#include "MOSurvivorJobQueueComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

UMOColonyManagerSubsystem* UMOColonyManagerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOColonyManagerSubsystem>() : nullptr;
}

void UMOColonyManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Server-only loop; clients get colony state via queries/replicated comps.
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(UpkeepTimer, this,
		&UMOColonyManagerSubsystem::RunUpkeepTick, UpkeepIntervalSeconds, /*bLoop=*/true);
}

void UMOColonyManagerSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpkeepTimer);
	}
	Super::Deinitialize();
}

// ============================================================================
// SETTLEMENT
// ============================================================================

const FMOSettlementRecord& UMOColonyManagerSubsystem::FoundSettlement(const FString& Name, const FVector& Center, float Radius)
{
	if (!Settlement.IsValid())
	{
		Settlement.SettlementId = FGuid::NewGuid();
	}
	Settlement.DisplayName = FText::FromString(Name);
	Settlement.Center = Center;
	Settlement.Radius = Radius;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Settlement '%s' founded at %s (r=%.0f)"),
		*Name, *Center.ToCompactString(), Radius);
	return Settlement;
}

bool UMOColonyManagerSubsystem::IsInSettlement(const FVector& Location) const
{
	return Settlement.IsValid()
		&& FVector::DistSquared2D(Location, Settlement.Center) <= FMath::Square(Settlement.Radius);
}

TArray<APawn*> UMOColonyManagerSubsystem::GetColonyRoster() const
{
	TArray<APawn*> Roster;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Roster;
	}
	for (TActorIterator<APawn> It(const_cast<UWorld*>(World)); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn))
		{
			continue;
		}
		const UMORecruitmentComponent* Recruit = Pawn->FindComponentByClass<UMORecruitmentComponent>();
		if (Recruit && Recruit->IsPossessable() && GetPawnGuid(Pawn).IsValid())
		{
			Roster.Add(Pawn);
		}
	}
	return Roster;
}

// ============================================================================
// HOUSING
// ============================================================================

bool UMOColonyManagerSubsystem::AssignResidence(APawn* Villager, AActor* House)
{
	const FGuid PawnGuid = GetPawnGuid(Villager);
	const AMOBuildableActor* Buildable = Cast<AMOBuildableActor>(House);
	const UMOIdentityComponent* HouseId = House ? House->FindComponentByClass<UMOIdentityComponent>() : nullptr;
	if (!PawnGuid.IsValid() || !Buildable || !HouseId || !HouseId->GetGuid().IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] AssignResidence rejected: missing pawn guid / buildable / house identity"));
		return false;
	}

	// Capacity is DATA: the building recipe's HousingCapacity.
	int32 Capacity = 0;
	if (const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Buildable->GetRecipeId()))
	{
		Capacity = Recipe->HousingCapacity;
	}
	if (Capacity <= 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] AssignResidence rejected: recipe '%s' has no HousingCapacity"),
			*Buildable->GetRecipeId().ToString());
		return false;
	}
	const FGuid HouseGuid = HouseId->GetGuid();
	if (GetHouseOccupancy(HouseGuid) >= Capacity && Residency.FindRef(PawnGuid) != HouseGuid)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] AssignResidence rejected: house full (%d/%d)"),
			GetHouseOccupancy(HouseGuid), Capacity);
		return false;
	}

	Residency.Add(PawnGuid, HouseGuid);
	VillagerUnhousedHours.Remove(PawnGuid);
	if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
	{
		History->AddEntry(EHistoryEntryType::Activity,
			FString::Printf(TEXT("Moved into %s"), *Buildable->GetName()));
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] %s assigned to house %s (%d/%d)"),
		*Villager->GetName(), *House->GetName(), GetHouseOccupancy(HouseGuid), Capacity);
	return true;
}

void UMOColonyManagerSubsystem::ClearResidence(const FGuid& PawnGuid)
{
	Residency.Remove(PawnGuid);
}

int32 UMOColonyManagerSubsystem::GetHouseOccupancy(const FGuid& HouseGuid) const
{
	int32 N = 0;
	for (const auto& Pair : Residency)
	{
		if (Pair.Value == HouseGuid)
		{
			++N;
		}
	}
	return N;
}

// ============================================================================
// MOOD — pure math over real-sim inputs
// ============================================================================

float UMOColonyManagerSubsystem::ComputeVillagerMood(const FMOVillagerMoodInputs& In)
{
	// Content-neutral baseline. Positive comforts are few in V1 (a home);
	// most pressure is downward, exactly like early survival.
	float Mood = 0.65f;

	float Pressure = 0.0f;
	if (In.bStarving)    { Pressure += 0.30f; }
	if (In.bDehydrated)  { Pressure += 0.20f; }
	Pressure += 0.10f * FMath::Clamp(In.Wetness, 0.0f, 1.0f);
	Pressure += 0.15f * FMath::Clamp(In.Shock / 100.0f, 0.0f, 1.0f);
	Pressure += 0.15f * FMath::Clamp(In.TraumaticStress / 100.0f, 0.0f, 1.0f);
	Pressure += 0.10f * FMath::Clamp(In.MoraleFatigue / 100.0f, 0.0f, 1.0f);

	if (In.bHasHome)
	{
		Mood += 0.10f;
	}
	else
	{
		// Homelessness bites harder the longer it lasts: ~0.02/h up to 0.25.
		Pressure += FMath::Min(In.UnhousedHours * 0.02f, 0.25f);
	}

	// Volatile personalities (modifier > 1) crash harder under the same
	// pressure; stable ones ride it out.
	Mood -= Pressure * FMath::Clamp(In.MoodVarianceModifier, 0.25f, 2.0f);

	return FMath::Clamp(Mood, 0.0f, 1.0f);
}

float UMOColonyManagerSubsystem::GetVillagerMood(const FGuid& PawnGuid) const
{
	const float* Found = VillagerMood.Find(PawnGuid);
	return Found ? *Found : 0.65f;
}

// ============================================================================
// QUOTAS / STANDING ORDERS (V2.1)
// ============================================================================

void UMOColonyManagerSubsystem::SetQuota(FName OutputItemId, FName RecipeId, int32 TargetCount, int32 Priority)
{
	Quotas.RemoveAll([&](const FMOColonyQuota& Q) { return Q.OutputItemId == OutputItemId; });
	if (TargetCount > 0)
	{
		FMOColonyQuota Q;
		Q.OutputItemId = OutputItemId;
		Q.RecipeId = RecipeId;
		Q.TargetCount = TargetCount;
		Q.Priority = Priority;
		Quotas.Add(Q);
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Quota %s: keep %d via %s (prio %d)"),
		*OutputItemId.ToString(), TargetCount, *RecipeId.ToString(), Priority);
}

TArray<FName> UMOColonyManagerSubsystem::DecideQuotaWork(const TArray<FMOColonyQuota>& InQuotas,
	const TMap<FName, int32>& Stock, const TSet<FName>& RecipesInFlight, int32 IdleVillagers)
{
	TArray<FMOColonyQuota> Sorted = InQuotas;
	Sorted.Sort([](const FMOColonyQuota& A, const FMOColonyQuota& B) { return A.Priority > B.Priority; });

	TArray<FName> Assignments;
	for (const FMOColonyQuota& Q : Sorted)
	{
		if (Assignments.Num() >= IdleVillagers)
		{
			break;
		}
		if (Q.RecipeId.IsNone() || Q.TargetCount <= 0)
		{
			continue;
		}
		if (Stock.FindRef(Q.OutputItemId) >= Q.TargetCount)
		{
			continue;   // quota met — no busywork
		}
		if (RecipesInFlight.Contains(Q.RecipeId))
		{
			continue;   // someone is already on it — one villager per order per pass
		}
		Assignments.Add(Q.RecipeId);
	}
	return Assignments;
}

void UMOColonyManagerSubsystem::RunQuotaPass(const TArray<APawn*>& Roster)
{
	if (Quotas.Num() == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Stock across settlement communal storage, for quota'd items only.
	TMap<FName, int32> Stock;
	TArray<AMOContainerActor*> Storages;
	for (TActorIterator<AMOContainerActor> It(World); It; ++It)
	{
		if (IsValid(*It) && IsInSettlement(It->GetActorLocation()))
		{
			Storages.Add(*It);
			if (const UMOInventoryComponent* Inv = It->FindComponentByClass<UMOInventoryComponent>())
			{
				for (const FMOColonyQuota& Q : Quotas)
				{
					Stock.FindOrAdd(Q.OutputItemId) += Inv->GetItemCountByDefinitionId(Q.OutputItemId);
				}
			}
		}
	}
	if (Storages.Num() == 0)
	{
		return;
	}

	// Idle AI villagers + recipes already in flight.
	TArray<APawn*> Idle;
	TSet<FName> InFlight;
	for (APawn* Villager : Roster)
	{
		const bool bAIControlled = Villager->GetController()
			&& Villager->GetController()->IsA<AMOSurvivorController>();
		if (!bAIControlled)
		{
			continue;
		}
		const UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>();
		if (!JobQueue)
		{
			continue;
		}
		const FMOSurvivorJobEntry Current = JobQueue->GetCurrentJob();
		if (Current.IsValid())
		{
			if (Current.JobType == EMOSurvivorJobType::CraftAtStation)
			{
				InFlight.Add(Current.CraftRecipeId);
			}
		}
		else
		{
			Idle.Add(Villager);
		}
	}
	if (Idle.Num() == 0)
	{
		return;
	}

	const TArray<FName> Assignments = DecideQuotaWork(Quotas, Stock, InFlight, Idle.Num());
	int32 IdleIndex = 0;
	for (const FName& RecipeId : Assignments)
	{
		APawn* Villager = Idle[IdleIndex++];
		UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>();

		// Nearest station/storage to the worker — same resolution the UI uses.
		AActor* Station = nullptr;
		float BestD = TNumericLimits<float>::Max();
		for (TActorIterator<AMOCraftingStationActor> It(World); It; ++It)
		{
			const float D = FVector::DistSquared(It->GetActorLocation(), Villager->GetActorLocation());
			if (IsValid(*It) && D < BestD) { Station = *It; BestD = D; }
		}
		AActor* Storage = Storages[0];
		BestD = TNumericLimits<float>::Max();
		for (AMOContainerActor* S : Storages)
		{
			const float D = FVector::DistSquared(S->GetActorLocation(), Villager->GetActorLocation());
			if (D < BestD) { Storage = S; BestD = D; }
		}
		if (!Station || !JobQueue)
		{
			continue;
		}
		const FGuid JobId = JobQueue->EnqueueCraftJob(RecipeId, Station, Storage, 1);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Quota assigned: %s -> %s (ok=%d)"),
			*Villager->GetName(), *RecipeId.ToString(), JobId.IsValid() ? 1 : 0);
		if (JobId.IsValid())
		{
			if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				History->AddEntry(EHistoryEntryType::Activity,
					FString::Printf(TEXT("Took standing order: %s"), *RecipeId.ToString()));
			}
		}
	}
}

// ============================================================================
// UPKEEP
// ============================================================================

void UMOColonyManagerSubsystem::RunUpkeepTick()
{
	UWorld* World = GetWorld();
	if (!World || !Settlement.IsValid())
	{
		return;
	}

	// Game-hours since the last pass (time-scale aware) for duration effects.
	double NowGameSeconds = 0.0;
	if (UMOGameClockSubsystem* Clock = World->GetSubsystem<UMOGameClockSubsystem>())
	{
		NowGameSeconds = Clock->GetGameTimeSeconds();
	}
	const float GameHoursElapsed = (LastUpkeepGameSeconds >= 0.0)
		? static_cast<float>((NowGameSeconds - LastUpkeepGameSeconds) / 3600.0)
		: 0.0f;
	LastUpkeepGameSeconds = NowGameSeconds;

	const TArray<APawn*> Roster = GetColonyRoster();

	// Standing orders first: idle hands pick up quota work this same pass.
	RunQuotaPass(Roster);

	for (APawn* Villager : Roster)
	{
		const FGuid PawnGuid = GetPawnGuid(Villager);

		// First-touch personality: villagers become PEOPLE when they join.
		if (UMOPersonalityComponent* Personality = Villager->FindComponentByClass<UMOPersonalityComponent>())
		{
			const FMOPersonalityTraits& T = Personality->GetTraits();
			if (T.Conscientiousness == 0.0f && T.Sociability == 0.0f && T.Stability == 0.0f)
			{
				Personality->RandomizeTraits();
			}
		}

		// EAT — only AI-controlled villagers self-feed; a possessed pawn is
		// the player's own stomach to manage.
		const bool bAIControlled = Villager->GetController()
			&& Villager->GetController()->IsA<AMOSurvivorController>();
		if (bAIControlled)
		{
			TryFeedVillager(Villager, PawnGuid);
		}

		// HOUSING clock.
		const bool bHasHome = Residency.Contains(PawnGuid);
		if (!bHasHome)
		{
			VillagerUnhousedHours.FindOrAdd(PawnGuid) += GameHoursElapsed;
		}
		else
		{
			VillagerUnhousedHours.Remove(PawnGuid);
		}

		// MOOD from the real sims.
		FMOVillagerMoodInputs In;
		if (const UMOMetabolismComponent* Metab = Villager->FindComponentByClass<UMOMetabolismComponent>())
		{
			In.bStarving = Metab->IsStarving();
			In.bDehydrated = Metab->IsDehydrated();
		}
		if (const UMOVitalsComponent* Vitals = Villager->FindComponentByClass<UMOVitalsComponent>())
		{
			In.Wetness = Vitals->GetWetnessLevel();
		}
		if (const UMOMentalStateComponent* Mental = Villager->FindComponentByClass<UMOMentalStateComponent>())
		{
			const FMOMentalState& MS = Mental->GetMentalState();
			In.Shock = MS.ShockAccumulation;
			In.TraumaticStress = MS.TraumaticStress;
			In.MoraleFatigue = MS.MoraleFatigue;
		}
		if (const UMOPersonalityComponent* Personality = Villager->FindComponentByClass<UMOPersonalityComponent>())
		{
			In.MoodVarianceModifier = Personality->GetMoodVarianceModifier();
		}
		In.bHasHome = bHasHome;
		In.UnhousedHours = VillagerUnhousedHours.FindRef(PawnGuid);

		const float NewMood = ComputeVillagerMood(In);
		const float OldMood = VillagerMood.FindRef(PawnGuid);
		VillagerMood.Add(PawnGuid, NewMood);

		// History + broadcast on quarter-bucket transitions only (no spam).
		if (FMath::FloorToInt(OldMood * 4.0f) != FMath::FloorToInt(NewMood * 4.0f))
		{
			OnVillagerMoodChanged.Broadcast(PawnGuid, NewMood);
			if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				History->AddEntry(EHistoryEntryType::MoodChange,
					FString::Printf(TEXT("Mood %s (%.2f)"),
						NewMood > OldMood ? TEXT("improved") : TEXT("worsened"), NewMood));
			}
		}
	}
}

void UMOColonyManagerSubsystem::TryFeedVillager(APawn* Villager, const FGuid& PawnGuid)
{
	UMOMetabolismComponent* Metab = Villager->FindComponentByClass<UMOMetabolismComponent>();
	if (!Metab)
	{
		return;
	}
	if (!Metab->IsStarving() && Metab->GetCalorieBalance() > EatCalorieDeficitThreshold)
	{
		return;   // not hungry — real appetite, not a schedule
	}

	FName FoodItemId;
	AMOContainerActor* Storage = FindCommunalFood(FoodItemId);
	if (!Storage || FoodItemId.IsNone())
	{
		return;   // empty larder: the metabolism sim keeps running down — that IS the failure state
	}

	FMOItemDefinitionRow FoodDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(FoodItemId, FoodDef))
	{
		return;
	}
	UMOInventoryComponent* StorageInv = Storage->FindComponentByClass<UMOInventoryComponent>();
	if (!StorageInv || !StorageInv->RemoveItemByDefinitionId(FoodItemId, 1))
	{
		return;
	}

	// The REAL eating path — same digestion the player gets.
	Metab->ConsumeFood(FoodDef.Nutrition, FoodItemId);

	if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
	{
		History->AddEntry(EHistoryEntryType::Activity,
			FString::Printf(TEXT("Ate %s from communal storage"), *FoodItemId.ToString()));
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] %s ate 1x %s from %s"),
		*Villager->GetName(), *FoodItemId.ToString(), *Storage->GetName());
}

AMOContainerActor* UMOColonyManagerSubsystem::FindCommunalFood(FName& OutItemId) const
{
	OutItemId = NAME_None;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AMOContainerActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AMOContainerActor* Container = *It;
		if (!IsValid(Container) || !IsInSettlement(Container->GetActorLocation()))
		{
			continue;
		}
		const UMOInventoryComponent* Inv = Container->FindComponentByClass<UMOInventoryComponent>();
		if (!Inv)
		{
			continue;
		}
		// First item with real calories wins (menu planning is a V2 nicety).
		TArray<FMOInventoryEntry> Items;
		Inv->GetInventoryEntries(Items);
		for (const FMOInventoryEntry& Item : Items)
		{
			FMOItemDefinitionRow Def;
			if (UMOItemDatabaseSettings::GetItemDefinition(Item.ItemDefinitionId, Def)
				&& Def.Nutrition.Calories > 0.0f)
			{
				OutItemId = Item.ItemDefinitionId;
				return Container;
			}
		}
	}
	return nullptr;
}

FGuid UMOColonyManagerSubsystem::GetPawnGuid(const APawn* Pawn)
{
	const UMOIdentityComponent* Id = Pawn ? Pawn->FindComponentByClass<UMOIdentityComponent>() : nullptr;
	return Id ? Id->GetGuid() : FGuid();
}

// ============================================================================
// SAVE / LOAD
// ============================================================================

FMOColonySaveData UMOColonyManagerSubsystem::BuildSaveData() const
{
	FMOColonySaveData Data;
	Data.Settlement = Settlement;
	Data.Quotas = Quotas;
	Data.Residency = Residency;
	Data.VillagerMood = VillagerMood;
	Data.VillagerUnhousedHours = VillagerUnhousedHours;
	for (APawn* Villager : GetColonyRoster())
	{
		const FGuid PawnGuid = GetPawnGuid(Villager);
		if (const UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
		{
			FMOCharacterHistorySaveData HistoryData;
			History->BuildSaveData(HistoryData);
			Data.VillagerHistory.Add(PawnGuid, HistoryData);
		}
	}
	Data.bHasValidData = Settlement.IsValid() || Residency.Num() > 0;
	return Data;
}

bool UMOColonyManagerSubsystem::ApplySaveDataAuthority(const FMOColonySaveData& InData)
{
	if (!InData.bHasValidData)
	{
		return false;
	}
	Settlement = InData.Settlement;
	Quotas = InData.Quotas;
	Residency = InData.Residency;
	VillagerMood = InData.VillagerMood;
	VillagerUnhousedHours = InData.VillagerUnhousedHours;
	// History replays onto the components once pawns are respawned; the roster
	// walk finds them by GUID.
	for (APawn* Villager : GetColonyRoster())
	{
		const FGuid PawnGuid = GetPawnGuid(Villager);
		if (const FMOCharacterHistorySaveData* HistoryData = InData.VillagerHistory.Find(PawnGuid))
		{
			if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				History->ApplySaveDataAuthority(*HistoryData);
			}
		}
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Restored settlement '%s' (%d residents housed)"),
		*Settlement.DisplayName.ToString(), Residency.Num());
	return true;
}
