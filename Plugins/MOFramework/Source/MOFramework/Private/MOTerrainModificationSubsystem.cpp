/**
 * MOTerrainModificationSubsystem.cpp - World-scoped modified-zone tracker
 *
 * See MOTerrainModificationSubsystem.h for the architecture & rationale.
 */

#include "MOTerrainModificationSubsystem.h"
#include "MOFramework.h"
#include "MOHarvestDebugSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void UMOTerrainModificationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Kick off the periodic sweep. The sweep is a no-op when Zones is empty
	// so this is cheap before any terraforming has happened.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SweepTimerHandle,
			this,
			&UMOTerrainModificationSubsystem::SweepTimerTick,
			FMath::Max(0.25f, SweepInterval),
			/*bLoop=*/true);
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOTerrainMod] Initialized (sweep=%.2fs, maxZones=%d, gridCell=%.0fcm)"),
		SweepInterval, MaxZones, GridCellSize);
}

void UMOTerrainModificationSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SweepTimerHandle);
	}
	Zones.Empty();
	ZoneGrid.Empty();
	Super::Deinitialize();
}

bool UMOTerrainModificationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Game worlds only — skip editor preview / PIE-template worlds.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

UMOTerrainModificationSubsystem* UMOTerrainModificationSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UMOTerrainModificationSubsystem>();
}

// ============================================================================
// PRODUCER API
// ============================================================================

void UMOTerrainModificationSubsystem::RegisterModifiedZone(const FVector& Location, float Radius)
{
	Radius = FMath::Max(1.0f, Radius);

	// Merge: scan ALL zones for a close-enough match. We don't use the grid
	// here because the merge candidate's grid cells are also probably this
	// zone's cells, but iterating Zones directly is fine — RegisterModifiedZone
	// runs at most once per terraform action, not per frame.
	for (FMOTerrainModifiedZone& Existing : Zones)
	{
		const float Threshold = FMath::Max(Existing.Radius, Radius) * MergeDistanceFactor;
		if (FVector::Dist(Existing.Location, Location) <= Threshold)
		{
			// Merge: expand to cover both.
			const float OldRadius = Existing.Radius;
			Existing.Radius = FMath::Max(Existing.Radius, Radius);
			// Center stays — keeps the grid index valid. Re-indexing is only
			// needed if the radius grew enough to touch new cells; since
			// MergeDistanceFactor < 1, the new bounds stay near the old.
			// For simplicity and correctness, re-index this zone if it grew.
			if (Existing.Radius > OldRadius)
			{
				const int32 ZoneIdx = static_cast<int32>(&Existing - Zones.GetData());
				IndexZone(ZoneIdx);
			}
			MOHARVEST_LOG(this, "Terrain-Zone",
				"RegisterModifiedZone MERGED into existing zone idx=%d at %s radius=%.1f->%.1f (total zones: %d)",
				static_cast<int32>(&Existing - Zones.GetData()),
				*Existing.Location.ToString(), OldRadius, Existing.Radius, Zones.Num());
			ExtendBurstSweepWindow();
			return;
		}
	}

	// New zone — append and index.
	const int32 NewIdx = Zones.Add(FMOTerrainModifiedZone(Location, Radius));
	IndexZone(NewIdx);

	// Bound the list. Drop oldest entries beyond MaxZones — rebuilds index
	// because indices shift after removal.
	if (Zones.Num() > MaxZones)
	{
		const int32 ToRemove = Zones.Num() - MaxZones;
		Zones.RemoveAt(0, ToRemove);
		RebuildSpatialIndex();
	}

	MOHARVEST_LOG(this, "Terrain-Zone",
		"RegisterModifiedZone ADDED zone idx=%d at %s radius=%.1f (total zones: %d, grid cells: %d)",
		NewIdx, *Location.ToString(), Radius, Zones.Num(), ZoneGrid.Num());

	ExtendBurstSweepWindow();
}

// ============================================================================
// QUERY API
// ============================================================================

bool UMOTerrainModificationSubsystem::IsLocationModified(const FVector& WorldLocation) const
{
	if (Zones.Num() == 0)
	{
		return false;
	}

	const FIntPoint Cell = WorldToCell(WorldLocation);
	const TArray<int32>* CellZones = ZoneGrid.Find(Cell);
	if (!CellZones)
	{
		return false;
	}

	for (int32 ZoneIdx : *CellZones)
	{
		if (!Zones.IsValidIndex(ZoneIdx))
		{
			continue;
		}
		const FMOTerrainModifiedZone& Z = Zones[ZoneIdx];
		// Use squared distance to avoid sqrt — XY-only since terraforming
		// is mostly horizontal and a tall column above a modified spot is
		// also considered "above worked ground" for foliage purposes.
		const float DX = WorldLocation.X - Z.Location.X;
		const float DY = WorldLocation.Y - Z.Location.Y;
		if (DX * DX + DY * DY <= Z.Radius * Z.Radius)
		{
			return true;
		}
	}
	return false;
}

void UMOTerrainModificationSubsystem::GetModifiedZonesNear(const FVector& WorldLocation, float SearchRadius,
	TArray<FMOTerrainModifiedZone>& OutZones) const
{
	OutZones.Reset();
	if (Zones.Num() == 0)
	{
		return;
	}

	// Walk all cells touched by a SearchRadius circle around WorldLocation.
	const FIntPoint MinCell(
		FMath::FloorToInt((WorldLocation.X - SearchRadius) / GridCellSize),
		FMath::FloorToInt((WorldLocation.Y - SearchRadius) / GridCellSize));
	const FIntPoint MaxCell(
		FMath::FloorToInt((WorldLocation.X + SearchRadius) / GridCellSize),
		FMath::FloorToInt((WorldLocation.Y + SearchRadius) / GridCellSize));

	TSet<int32> Seen;
	const float SearchRadiusSq = SearchRadius * SearchRadius;

	for (int32 CX = MinCell.X; CX <= MaxCell.X; ++CX)
	{
		for (int32 CY = MinCell.Y; CY <= MaxCell.Y; ++CY)
		{
			const TArray<int32>* CellZones = ZoneGrid.Find(FIntPoint(CX, CY));
			if (!CellZones) continue;

			for (int32 ZoneIdx : *CellZones)
			{
				if (Seen.Contains(ZoneIdx)) continue;
				Seen.Add(ZoneIdx);
				if (!Zones.IsValidIndex(ZoneIdx)) continue;

				const FMOTerrainModifiedZone& Z = Zones[ZoneIdx];
				const float DX = WorldLocation.X - Z.Location.X;
				const float DY = WorldLocation.Y - Z.Location.Y;
				if (DX * DX + DY * DY <= SearchRadiusSq)
				{
					OutZones.Add(Z);
				}
			}
		}
	}
}

// ============================================================================
// SAVE / LOAD
// ============================================================================

void UMOTerrainModificationSubsystem::BuildSaveData(FMOTerrainModificationSaveData& OutData) const
{
	OutData.Zones = Zones;
	OutData.Version = 1;
}

bool UMOTerrainModificationSubsystem::ApplySaveDataAuthority(const FMOTerrainModificationSaveData& InData)
{
	AActor* Owner = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr;
	// World subsystems don't have a clean "HasAuthority" check the way an
	// actor does. Persistence's caller is expected to be on authority — the
	// codebase pattern allows this method to be called from there. We treat
	// it as a trusted call.

	Zones.Reset();
	Zones.Reserve(InData.Zones.Num());

	// Future: if InData.Version != 1, run migration here.
	for (const FMOTerrainModifiedZone& Z : InData.Zones)
	{
		// Sanity-clamp the radius — refuses zero/negative-radius zones that
		// could otherwise be saved by a buggy producer.
		if (Z.Radius <= 0.0f)
		{
			continue;
		}
		Zones.Add(Z);
	}

	RebuildSpatialIndex();

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOTerrainMod] Loaded %d modified zone(s) from save (v%d)"),
		Zones.Num(), InData.Version);
	return true;
}

// ============================================================================
// SWEEP
// ============================================================================

int32 UMOTerrainModificationSubsystem::SweepModifiedZones()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MOTerrainSweep);

	if (Zones.Num() == 0)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	// PERF NOTE (2026-05): Profiling showed 3fps during sweep — most of the
	// cost was in walking 5400+ actors via TActorIterator to find ~30 ISMs.
	// Now we iterate ONLY UInstancedStaticMeshComponent objects directly,
	// then use the ISM's spatial query (GetInstancesOverlappingSphere) per
	// zone instead of brute-force checking every instance's transform.
	// Result: actor walk eliminated, instance check goes from N (all
	// instances in any ISM near a zone) to actual-overlap-only.

	// Filter zones to ones near a local player — foliage outside ~25m of any
	// player isn't visible (grass fade distance is far less than 25m), so
	// respawned instances there don't matter until the player gets close,
	// at which point the next sweep tick catches them.
	TArray<const FMOTerrainModifiedZone*, TInlineAllocator<8>> ActiveZones;
	{
		TArray<FVector, TInlineAllocator<4>> PlayerLocs;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					PlayerLocs.Add(Pawn->GetActorLocation());
				}
				else if (PC->PlayerCameraManager)
				{
					PlayerLocs.Add(PC->PlayerCameraManager->GetCameraLocation());
				}
			}
		}

		if (PlayerLocs.Num() == 0)
		{
			// No local players (e.g. dedicated server transition) — sweep
			// all zones rather than skip entirely.
			for (const FMOTerrainModifiedZone& Z : Zones)
			{
				ActiveZones.Add(&Z);
			}
		}
		else
		{
			const float RangeSq = SweepPlayerRange * SweepPlayerRange;
			for (const FMOTerrainModifiedZone& Z : Zones)
			{
				for (const FVector& PL : PlayerLocs)
				{
					if (FVector::DistSquared(Z.Location, PL) <= RangeSq)
					{
						ActiveZones.Add(&Z);
						break;
					}
				}
			}
		}
	}

	if (ActiveZones.Num() == 0)
	{
		// All zones too far from any player — nothing visible to keep clear.
		return 0;
	}

	struct FRemoval
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Comp;
		TSet<int32> Indices;  // set so duplicate zones don't double-add
	};
	TMap<UInstancedStaticMeshComponent*, FRemoval> Removals;

	int32 ActorsScanned = 0;      // kept for log compat; no longer driving iteration
	int32 ISMsTotal = 0;          // every ISM/HISM we walked past
	int32 ISMsConsidered = 0;     // passed the zone-cell bounds reject
	int32 ISMsSkippedNotTagged = 0; // matched zone-cell but no AutoSweepTag on actor/component
	int32 InstancesRemoved = 0;
	int32 InstancesChecked = 0;   // approximated as total overlap-query returns

	// Iterate ISM components directly — skips all actors that have no ISMs.
	for (TObjectIterator<UInstancedStaticMeshComponent> ISMIt; ISMIt; ++ISMIt)
	{
		UInstancedStaticMeshComponent* ISM = *ISMIt;
		if (!IsValid(ISM)) continue;

		// Filter to in-world components only (skip CDOs, archetypes, editor preview, etc.)
		AActor* Actor = ISM->GetOwner();
		if (!IsValid(Actor)) continue;
		if (Actor->GetWorld() != World) continue;
		++ISMsTotal;
		++ActorsScanned;

		// Quick reject: if the ISM's bounds don't overlap any ACTIVE zone
		// (one near a player), skip. With the player-distance filter applied
		// to zones first, this typically narrows ActiveZones to 0-3 entries
		// so the test is cheap — no grid lookup needed.
		const FBoxSphereBounds CompBounds = ISM->Bounds;
		bool bAnyZoneNear = false;
		for (const FMOTerrainModifiedZone* AZ : ActiveZones)
		{
			// Sphere-sphere overlap: dist(centers) <= sum(radii).
			const float CombinedR = CompBounds.SphereRadius + AZ->Radius;
			if (FVector::DistSquared(CompBounds.Origin, AZ->Location) <= CombinedR * CombinedR)
			{
				bAnyZoneNear = true;
				break;
			}
		}
		if (!bAnyZoneNear) continue;
		++ISMsConsidered;

		// Tag gate: only auto-sweep HISMs that are explicitly opted-in
		// via AutoSweepTags. Trees / rocks / bushes are HARVESTABLE and
		// must persist for the player to interact with explicitly — the
		// sweep would otherwise yank them out the moment a zone registers.
		//
		// Match is OR across:
		//   - the HISM's ComponentTags
		//   - the owning Actor's Tags
		// Either is fine — lets the designer tag at whichever granularity
		// is convenient (broad actor-level for whole-scatter foliage,
		// per-component for mixed actors).
		//
		// Empty AutoSweepTags = sweep nothing (safe default if the world
		// has no decorative tagging configured yet).
		if (AutoSweepTags.Num() == 0)
		{
			++ISMsSkippedNotTagged;
			continue;
		}
		bool bTagMatch = false;
		for (const FName& Tag : AutoSweepTags)
		{
			if (Tag.IsNone()) continue;
			if (ISM->ComponentHasTag(Tag) || Actor->ActorHasTag(Tag))
			{
				bTagMatch = true;
				break;
			}
		}
		if (!bTagMatch)
		{
			++ISMsSkippedNotTagged;
			continue;
		}

		// Spatial query per ACTIVE zone: ask the ISM for instance indices
		// that overlap the zone sphere. Way faster than iterating every
		// instance and computing transforms — the ISM's internal indexing
		// handles the overlap test. Inactive zones (far from any player)
		// are already filtered out by ActiveZones, so we don't waste a
		// query on them.
		for (const FMOTerrainModifiedZone* Z : ActiveZones)
		{
			const TArray<int32> Indices = ISM->GetInstancesOverlappingSphere(
				Z->Location, Z->Radius, /*bSphereInWorldSpace=*/true);
			if (Indices.Num() == 0) continue;
			InstancesChecked += Indices.Num();
			FRemoval& R = Removals.FindOrAdd(ISM);
			R.Comp = ISM;
			for (int32 Idx : Indices)
			{
				R.Indices.Add(Idx);
			}
		}
	}

	// Apply removals. Descending sort per component so RemoveInstance(i)
	// doesn't shift the identity of indices we haven't removed yet.
	for (auto& Pair : Removals)
	{
		UInstancedStaticMeshComponent* ISM = Pair.Key;
		if (!IsValid(ISM)) continue;

		TArray<int32> SortedIndices = Pair.Value.Indices.Array();
		SortedIndices.Sort([](const int32& A, const int32& B) { return A > B; });
		for (int32 Idx : SortedIndices)
		{
			if (ISM->RemoveInstance(Idx))
			{
				++InstancesRemoved;
			}
		}
	}

	// Always log sweep counts — even zero-removal sweeps tell us "we ran and
	// found nothing", which is the diagnostic for "PCG isn't respawning yet
	// when we sweep". Logging only positive removals would hide that case.
	MOHARVEST_LOG(this, "Terrain-Sweep",
		"Sweep done: zones=%d actors=%d ISMsTotal=%d ISMsInZoneCells=%d ISMsSkippedNotTagged=%d instancesChecked=%d instancesRemoved=%d",
		Zones.Num(), ActorsScanned, ISMsTotal, ISMsConsidered, ISMsSkippedNotTagged, InstancesChecked, InstancesRemoved);

	return InstancesRemoved;
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOTerrainModificationSubsystem::SweepTimerTick()
{
	SweepModifiedZones();
}

void UMOTerrainModificationSubsystem::ExtendBurstSweepWindow()
{
	UWorld* World = GetWorld();
	if (!World || PostRegisterBurstSweepDuration <= 0.0f)
	{
		return;
	}

	// Extend (not replace) the burst window. If a new zone is registered
	// mid-burst, push the end-time out — keeps continuous per-frame coverage
	// across rapid sequential actions instead of resetting and creating gaps.
	const double NewEnd = World->GetTimeSeconds() + PostRegisterBurstSweepDuration;
	const double Now = World->GetTimeSeconds();
	if (NewEnd > BurstSweepEndTime)
	{
		const bool bWasInBurst = (BurstSweepEndTime > Now);
		BurstSweepEndTime = NewEnd;
		MOHARVEST_LOG(this, "Terrain-Burst",
			"%s burst sweep window: now=%.3f endsAt=%.3f duration=%.3fs",
			bWasInBurst ? TEXT("Extended") : TEXT("Started"),
			Now, BurstSweepEndTime, PostRegisterBurstSweepDuration);
	}
}

// ============================================================================
// FTickableGameObject — per-frame burst sweep
// ============================================================================
//
// We tick ONLY during a brief window after each RegisterModifiedZone. While
// in the burst window, every frame runs a sweep to catch PCG respawn at the
// smallest possible latency (one frame). Outside the burst window, IsTickable
// returns false and the engine skips the Tick call entirely — periodic
// cleanup falls back to the SweepInterval timer.

void UMOTerrainModificationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (UWorld* World = GetWorld())
	{
		if (World->GetTimeSeconds() < BurstSweepEndTime)
		{
			SweepModifiedZones();
		}
	}
}

bool UMOTerrainModificationSubsystem::IsTickable() const
{
	// Don't tick before Initialize finishes or during world teardown.
	if (!IsInitialized())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Only tick while inside an active burst window. The check is cheap:
	// one timestamp comparison. Saves the engine from a per-frame virtual
	// dispatch when nothing's happening.
	return World->GetTimeSeconds() < BurstSweepEndTime;
}

TStatId UMOTerrainModificationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOTerrainModificationSubsystem, STATGROUP_Tickables);
}

FIntPoint UMOTerrainModificationSubsystem::WorldToCell(const FVector& Loc) const
{
	return FIntPoint(
		FMath::FloorToInt(Loc.X / GridCellSize),
		FMath::FloorToInt(Loc.Y / GridCellSize));
}

void UMOTerrainModificationSubsystem::IndexZone(int32 ZoneIndex)
{
	if (!Zones.IsValidIndex(ZoneIndex))
	{
		return;
	}

	const FMOTerrainModifiedZone& Z = Zones[ZoneIndex];

	// Compute the cell range the zone's bounding box covers.
	const FIntPoint MinCell(
		FMath::FloorToInt((Z.Location.X - Z.Radius) / GridCellSize),
		FMath::FloorToInt((Z.Location.Y - Z.Radius) / GridCellSize));
	const FIntPoint MaxCell(
		FMath::FloorToInt((Z.Location.X + Z.Radius) / GridCellSize),
		FMath::FloorToInt((Z.Location.Y + Z.Radius) / GridCellSize));

	for (int32 CX = MinCell.X; CX <= MaxCell.X; ++CX)
	{
		for (int32 CY = MinCell.Y; CY <= MaxCell.Y; ++CY)
		{
			TArray<int32>& CellZones = ZoneGrid.FindOrAdd(FIntPoint(CX, CY));
			CellZones.AddUnique(ZoneIndex);
		}
	}
}

void UMOTerrainModificationSubsystem::RebuildSpatialIndex()
{
	ZoneGrid.Reset();
	for (int32 i = 0; i < Zones.Num(); ++i)
	{
		IndexZone(i);
	}
}
