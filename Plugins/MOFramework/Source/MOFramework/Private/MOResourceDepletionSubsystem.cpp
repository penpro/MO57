#include "MOResourceDepletionSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "MOFramework.h"
#include "MOHarvestDebugSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMOResourceDepletionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Hardcoded defaults if nothing's configured. These match the actual ItemIds in
	// DT_Resources (Stick01, Bark01, RiverCobble01, Stone01, FlintNodule01) — verified
	// from the harvest debug log + Resources_JSON.json. Anything in this map gets
	// counted; anything not in it is treated as unlimited.
	//
	// Add more ItemIds either by extending this default block or via DefaultGame.ini
	// under [/Script/MOFramework.MOResourceDepletionSubsystem] (Config = Game).
	if (InitialCountByItem.Num() == 0)
	{
		FMOResourceYieldRange SmallPickup;
		SmallPickup.MinCount = 0;
		SmallPickup.MaxCount = 5;

		FMOResourceYieldRange Bark;
		Bark.MinCount = 10;
		Bark.MaxCount = 20;

		InitialCountByItem.Add(FName(TEXT("Stick01")), SmallPickup);
		InitialCountByItem.Add(FName(TEXT("Bark01")), Bark);
		InitialCountByItem.Add(FName(TEXT("RiverCobble01")), SmallPickup);
		InitialCountByItem.Add(FName(TEXT("Stone01")), SmallPickup);
		InitialCountByItem.Add(FName(TEXT("FlintNodule01")), SmallPickup);
	}

	// Tick every 60s — respawn granularity is hours, no need to check more often.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RespawnCheckTimerHandle,
			this,
			&UMOResourceDepletionSubsystem::CheckRespawns,
			60.0f,
			/*loop=*/true);
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[ResourceDepletion] Initialized with %d configured yield ranges, respawn=%.1fh"),
		InitialCountByItem.Num(), RespawnHoursReal);
}

void UMOResourceDepletionSubsystem::Deinitialize()
{
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persist->UnregisterSaveDomain(this);
			}
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnCheckTimerHandle);
	}
	DepletionMap.Empty();
	Super::Deinitialize();
}

bool UMOResourceDepletionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Game worlds only — no editor preview worlds.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

UMOResourceDepletionSubsystem* UMOResourceDepletionSubsystem::Get(const UObject* WorldCtx)
{
	if (!WorldCtx) return nullptr;
	const UWorld* World = WorldCtx->GetWorld();
	if (!World) return nullptr;
	return World->GetSubsystem<UMOResourceDepletionSubsystem>();
}

bool UMOResourceDepletionSubsystem::CanYield(const FString& NodeKey, FName ItemId) const
{
	const FMOResourceNodeDepletion* State = DepletionMap.Find(NodeKey);
	if (!State)
	{
		// Not yet tracked — treat as fresh. CanYield is true unless we have a config
		// entry rolled to 0; but we only roll on Consume, so always true here.
		return true;
	}

	const int32* Remaining = State->RemainingByItem.Find(ItemId);
	if (!Remaining)
	{
		// Item not in the per-node map — config didn't set a range, so unlimited.
		return true;
	}
	return *Remaining > 0;
}

bool UMOResourceDepletionSubsystem::ConsumeYield(const FString& NodeKey, FName ItemId)
{
	FMOResourceNodeDepletion& State = DepletionMap.FindOrAdd(NodeKey);

	const bool bWasFresh = (State.RemainingByItem.Num() == 0);

	// Lazy init: if this node hasn't been touched, roll initial counts for every
	// configured ItemId. Unconfigured items aren't tracked (= unlimited).
	if (bWasFresh)
	{
		FString RollSummary;
		for (const TPair<FName, FMOResourceYieldRange>& Pair : InitialCountByItem)
		{
			const int32 Rolled = RollInitialCount(Pair.Key);
			State.RemainingByItem.Add(Pair.Key, Rolled);
			RollSummary += FString::Printf(TEXT("%s=%d "), *Pair.Key.ToString(), Rolled);
		}
		MOHARVEST_LOG(this, "Depletion", "Initialized node '%s' with: %s",
			*NodeKey, *RollSummary);
	}

	int32* Remaining = State.RemainingByItem.Find(ItemId);
	if (!Remaining)
	{
		// No config entry for this item — treat as unlimited, do nothing.
		MOHARVEST_LOG(this, "Depletion",
			"  ConsumeYield('%s') node '%s' -> UNCONFIGURED ItemId (unlimited). Map keys: %s",
			*ItemId.ToString(), *NodeKey,
			*FString::JoinBy(InitialCountByItem, TEXT(", "),
				[](const TPair<FName, FMOResourceYieldRange>& P) { return P.Key.ToString(); }));
		return true;
	}

	if (*Remaining <= 0)
	{
		MOHARVEST_LOG(this, "Depletion",
			"  ConsumeYield('%s') node '%s' -> DEPLETED (remaining=0)",
			*ItemId.ToString(), *NodeKey);
		return false;
	}

	--(*Remaining);
	MOHARVEST_LOG(this, "Depletion",
		"  ConsumeYield('%s') node '%s' -> consumed (remaining=%d)",
		*ItemId.ToString(), *NodeKey, *Remaining);

	// All yields exhausted? Stamp the depletion time for respawn tracking.
	if (State.IsFullyDepleted())
	{
		State.FullyDepletedAt = FDateTime::Now();
		MOHARVEST_LOG(this, "Depletion",
			"  Node '%s' FULLY DEPLETED — will respawn in %.1fh",
			*NodeKey, RespawnHoursReal);
	}

	return true;
}

FString UMOResourceDepletionSubsystem::MakeNodeKey(UInstancedStaticMeshComponent* MeshComp, int32 InstanceIndex)
{
	if (!MeshComp)
	{
		return FString();
	}

	FTransform Xform;
	if (!MeshComp->GetInstanceTransform(InstanceIndex, Xform, /*bWorldSpace=*/true))
	{
		return FString();
	}

	// Position-based key (rounded to integers so float jitter doesn't change the key
	// across sessions). Combined with a component-class identifier so two different
	// resource types at the same location don't collide.
	const FVector Loc = Xform.GetLocation();
	return FString::Printf(TEXT("%s|%d_%d_%d"),
		*MeshComp->GetClass()->GetName(),
		FMath::RoundToInt(Loc.X),
		FMath::RoundToInt(Loc.Y),
		FMath::RoundToInt(Loc.Z));
}

int32 UMOResourceDepletionSubsystem::RollInitialCount(FName ItemId) const
{
	const FMOResourceYieldRange* Range = InitialCountByItem.Find(ItemId);
	if (!Range)
	{
		return 0;
	}
	return FMath::RandRange(Range->MinCount, Range->MaxCount);
}

// ============================================================================
// SAVE / LOAD  (H37)
// ============================================================================

void UMOResourceDepletionSubsystem::BuildSaveData(FMOResourceDepletionSaveData& OutSaveData) const
{
	OutSaveData.Entries.Reset();

	// Same expiry rule as CheckRespawns: don't persist entries that have already
	// respawned — they'd just be pruned on next sweep anyway, and persisting them
	// would needlessly re-suppress a node that should be fresh.
	const FDateTime Now = FDateTime::Now();
	const FTimespan RespawnDuration = FTimespan::FromHours(RespawnHoursReal);

	for (const TPair<FString, FMOResourceNodeDepletion>& Pair : DepletionMap)
	{
		const FMOResourceNodeDepletion& State = Pair.Value;

		if (State.FullyDepletedAt != FDateTime::MinValue() &&
			(Now - State.FullyDepletedAt) >= RespawnDuration)
		{
			continue; // already respawned — skip
		}

		FMOResourceNodeDepletionSaveEntry Entry;
		Entry.NodeKey = Pair.Key;
		Entry.RemainingByItem = State.RemainingByItem;
		Entry.FullyDepletedAt = State.FullyDepletedAt;
		OutSaveData.Entries.Add(MoveTemp(Entry));
	}

	OutSaveData.bHasValidData = true;

	UE_LOG(LogMOFramework, Log, TEXT("[ResourceDepletion] BuildSaveData: %d node(s) captured"), OutSaveData.Entries.Num());
}

void UMOResourceDepletionSubsystem::ApplySaveDataAuthority(const FMOResourceDepletionSaveData& InSaveData)
{
	if (!InSaveData.bHasValidData)
	{
		// Legacy save (no depletion field) — leave the fresh runtime map untouched.
		return;
	}

	DepletionMap.Empty(InSaveData.Entries.Num());

	const FDateTime Now = FDateTime::Now();
	const FTimespan RespawnDuration = FTimespan::FromHours(RespawnHoursReal);
	int32 Restored = 0;

	for (const FMOResourceNodeDepletionSaveEntry& Entry : InSaveData.Entries)
	{
		// Drop entries that aged past respawn while the game was closed — they
		// come back as "not tracked" (fresh), matching the header's documented
		// "game closed -> reopened" behavior.
		if (Entry.FullyDepletedAt != FDateTime::MinValue() &&
			(Now - Entry.FullyDepletedAt) >= RespawnDuration)
		{
			continue;
		}

		FMOResourceNodeDepletion& State = DepletionMap.FindOrAdd(Entry.NodeKey);
		State.RemainingByItem = Entry.RemainingByItem;
		State.FullyDepletedAt = Entry.FullyDepletedAt;
		++Restored;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[ResourceDepletion] ApplySaveDataAuthority: restored %d of %d saved node(s)"),
		Restored, InSaveData.Entries.Num());
}

void UMOResourceDepletionSubsystem::CheckRespawns()
{
	if (DepletionMap.Num() == 0) return;

	const FDateTime Now = FDateTime::Now();
	const FTimespan RespawnDuration = FTimespan::FromHours(RespawnHoursReal);

	TArray<FString> ToRemove;
	for (const TPair<FString, FMOResourceNodeDepletion>& Pair : DepletionMap)
	{
		const FMOResourceNodeDepletion& State = Pair.Value;
		if (State.FullyDepletedAt == FDateTime::MinValue()) continue;
		if (Now - State.FullyDepletedAt >= RespawnDuration)
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FString& Key : ToRemove)
	{
		DepletionMap.Remove(Key);
	}

	if (ToRemove.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log,
			TEXT("[ResourceDepletion] Respawned %d node(s); %d still depleted"),
			ToRemove.Num(), DepletionMap.Num());
	}
}

// ---- IMOSaveDomain (ResourceDepletion) — see MOSaveDomainInterface.h ----

void UMOResourceDepletionSubsystem::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	BuildSaveData(Save.ResourceDepletionData);
}

void UMOResourceDepletionSubsystem::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	ApplySaveDataAuthority(Save.ResourceDepletionData);
}

void UMOResourceDepletionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Persist->RegisterSaveDomain(this);
		}
	}
}
