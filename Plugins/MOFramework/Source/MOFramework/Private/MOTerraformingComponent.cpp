#include "MOTerraformingComponent.h"
#include "MOFramework.h"
#include "MOViewpointUtils.h"
#include "MOCharacter.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOHarvestDebugSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

// Voxel plugin includes
#include "MOVoxelAlias.h"  // all Voxel sculpt calls go through this facade
#include "Sculpt/VoxelSculptMode.h"
#include "Sculpt/VoxelLevelToolType.h"

UMOTerraformingComponent::UMOTerraformingComponent()
{
	// We tick only while a terraform action is in-flight. bCanEverTick must
	// be true so SetComponentTickEnabled(true) actually works; we start
	// disabled so we don't waste a per-frame call when idle.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMOTerraformingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!PendingAction.bActive)
	{
		// Defensive — shouldn't be ticking if not active, but if we are,
		// disable the tick and bail.
		SetComponentTickEnabled(false);
		return;
	}

	PendingAction.ElapsedTime += DeltaTime;

	// Broadcast progress so any UI display can update.
	OnTerraformProgress.Broadcast(PendingAction.GetProgress01(), PendingAction.GetTimeRemaining());

	// Completion: apply the sculpt, then tear down state. We use >= so even
	// a small frame-time overshoot (TotalTime + 0.001 etc.) triggers exactly
	// once instead of being skipped.
	if (PendingAction.ElapsedTime >= PendingAction.TotalTime)
	{
		// Snapshot the bool BEFORE Reset so we can still broadcast Completed
		// reflecting whether the sculpt actually went through.
		const bool bSuccess = ApplyPendingTerraform();

		PendingAction.Reset();
		SetComponentTickEnabled(false);
		UnregisterFromInterrupts();

		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Timer completed, sculpt applied=%s"),
			bSuccess ? TEXT("true") : TEXT("false"));

		OnTerraformCompleted.Broadcast(bSuccess);
	}
}

void UMOTerraformingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find sculpt actors if not set
	if (!HeightSculptActor.IsValid() && !VolumeSculptActor.IsValid())
	{
		AutoFindSculptActors();
	}

	// Note: the periodic foliage-sweep timer lives on
	// UMOTerrainModificationSubsystem now. The component is just a producer.
}

void UMOTerraformingComponent::AutoFindSculptActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find height sculpt actor (facade hides the Voxel actor type)
	if (AActor* Height = MOVoxel::FindHeightSculptActor(World))
	{
		HeightSculptActor = Height;
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Auto-found HeightSculptActor: %s"), *Height->GetName());
	}

	// Find volume sculpt actor
	if (AActor* Volume = MOVoxel::FindVolumeSculptActor(World))
	{
		VolumeSculptActor = Volume;
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Auto-found VolumeSculptActor: %s"), *Volume->GetName());
	}
}

bool UMOTerraformingComponent::HasValidSculptActor() const
{
	if (bUseVolumeSculpting)
	{
		return VolumeSculptActor.IsValid();
	}
	return HeightSculptActor.IsValid();
}

bool UMOTerraformingComponent::ResolveViewpoint(FVector& OutLocation, FRotator& OutRotation) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn))
	{
		return false;
	}

	// Try controller first (handles both player and AI controllers)
	AController* OwnerController = OwnerPawn->GetController();
	if (UMOViewpointUtils::ResolveViewpointForController(OwnerController, OutLocation, OutRotation))
	{
		return true;
	}

	// Fall back to pawn eyes if no controller
	return UMOViewpointUtils::ResolveViewpointForPawn(OwnerPawn, OutLocation, OutRotation);
}

bool UMOTerraformingComponent::FindTerraformTarget(FVector& OutLocation, FVector& OutNormal) const
{
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!ResolveViewpoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * MaxDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, TraceChannel, QueryParams))
	{
		OutLocation = HitResult.ImpactPoint;
		OutNormal = HitResult.ImpactNormal;
		return true;
	}

	return false;
}

bool UMOTerraformingComponent::TryTerraform()
{
	FVector HitLocation;
	FVector HitNormal;
	if (!FindTerraformTarget(HitLocation, HitNormal))
	{
		return false;
	}

	return TerraformAtLocation(HitLocation, CurrentMode);
}

bool UMOTerraformingComponent::TerraformAtLocation(const FVector& WorldLocation, EMOTerraformMode Mode)
{
	// RemoveFoliage doesn't need a voxel sculpt actor — it works on world
	// HISM/ISM components — so let it through before the HasValidSculptActor gate.
	if (Mode != EMOTerraformMode::RemoveFoliage && !HasValidSculptActor())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTerraforming] No valid sculpt actor for current mode"));
		return false;
	}

	switch (Mode)
	{
	case EMOTerraformMode::Dig:
		return Dig(WorldLocation);
	case EMOTerraformMode::Raise:
		return Raise(WorldLocation);
	case EMOTerraformMode::Flatten:
		{
			// Resolve the flatten target Z. Default: character's feet (the
			// ground they're standing on). Falls back to ray-hit Z if the
			// owner isn't a character, or to Config.FlattenTargetHeight if
			// the feet option is disabled.
			float TargetHeight = Config.FlattenTargetHeight;
			if (Config.bUseCharacterFeetForFlatten)
			{
				float GroundZ = 0.0f;
				if (ResolveCharacterGroundZ(GroundZ))
				{
					TargetHeight = GroundZ;
				}
				else
				{
					// Owner isn't an AMOCharacter — fall back to ray-hit Z.
					TargetHeight = WorldLocation.Z;
				}
			}
			return Flatten(WorldLocation, TargetHeight);
		}
	case EMOTerraformMode::Smooth:
		return Smooth(WorldLocation);
	case EMOTerraformMode::RemoveFoliage:
		return RemoveFoliage(WorldLocation);
	default:
		return false;
	}
}

bool UMOTerraformingComponent::Dig(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Dig at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), Config.Radius, Config.RaiseLowerStrength);

	if (bUseVolumeSculpting)
	{
		return VolumeDig(WorldLocation);
	}
	return HeightDig(FVector2D(WorldLocation.X, WorldLocation.Y));
}

bool UMOTerraformingComponent::Raise(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Raise at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), Config.Radius, Config.RaiseLowerStrength);

	if (bUseVolumeSculpting)
	{
		return VolumeRaise(WorldLocation);
	}
	return HeightRaise(FVector2D(WorldLocation.X, WorldLocation.Y));
}

bool UMOTerraformingComponent::Flatten(const FVector& WorldLocation, float TargetHeight)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Flatten at %s to height %.1f (Radius: %.1f)"),
		*WorldLocation.ToString(), TargetHeight, Config.Radius);

	if (bUseVolumeSculpting)
	{
		return VolumeFlatten(WorldLocation, FVector::UpVector, TargetHeight);
	}
	return HeightFlatten(FVector2D(WorldLocation.X, WorldLocation.Y), TargetHeight);
}

bool UMOTerraformingComponent::Smooth(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Smooth at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), Config.Radius, Config.SmoothStrength);

	if (bUseVolumeSculpting)
	{
		return VolumeSmooth(WorldLocation);
	}
	return HeightSmooth(FVector2D(WorldLocation.X, WorldLocation.Y));
}

// ============================================================================
// HEIGHT SCULPTING OPERATIONS
// ============================================================================

bool UMOTerraformingComponent::HeightDig(const FVector2D& Location)
{
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::HeightSculpt(Actor, Location, Config.Radius, Config.RaiseLowerStrength, /*bAdd*/ false);
	return true;
}

bool UMOTerraformingComponent::HeightRaise(const FVector2D& Location)
{
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::HeightSculpt(Actor, Location, Config.Radius, Config.RaiseLowerStrength, /*bAdd*/ true);
	return true;
}

bool UMOTerraformingComponent::HeightFlatten(const FVector2D& Location, float TargetHeight)
{
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::HeightFlatten(Actor, Location, Config.Radius, Config.Falloff, TargetHeight);
	return true;
}

bool UMOTerraformingComponent::HeightSmooth(const FVector2D& Location)
{
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::HeightSmooth(Actor, Location, Config.Radius, Config.SmoothStrength);
	return true;
}

// ============================================================================
// VOLUME SCULPTING OPERATIONS
// ============================================================================

bool UMOTerraformingComponent::VolumeDig(const FVector& Location)
{
	AActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::VolumeSphere(Actor, Location, Config.Radius, /*bAdd*/ false, /*Smoothness*/ 0.0f);
	return true;
}

bool UMOTerraformingComponent::VolumeRaise(const FVector& Location)
{
	AActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::VolumeSphere(Actor, Location, Config.Radius, /*bAdd*/ true, /*Smoothness*/ 0.0f);
	return true;
}

bool UMOTerraformingComponent::VolumeFlatten(const FVector& Location, const FVector& Normal, float TargetHeight)
{
	AActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	// TargetHeight is unused for volume flatten — the Voxel API flattens toward the
	// Location/Normal plane; Height is the vertical reach of the brush.
	MOVoxel::VolumeFlatten(Actor, Location, Normal, Config.Radius, Config.Radius * 2.0f, Config.Falloff);
	return true;
}

bool UMOTerraformingComponent::VolumeSmooth(const FVector& Location)
{
	AActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::VolumeSmooth(Actor, Location, Config.Radius, Config.SmoothStrength);
	return true;
}

// ============================================================================
// MODE MANAGEMENT
// ============================================================================

void UMOTerraformingComponent::CycleMode()
{
	EMOTerraformMode OldMode = CurrentMode;
	const int32 CurrentIndex = static_cast<int32>(CurrentMode);
	const int32 ModeCount = static_cast<int32>(EMOTerraformMode::MAX);
	const int32 NextIndex = (CurrentIndex + 1) % ModeCount;
	CurrentMode = static_cast<EMOTerraformMode>(NextIndex);

	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Mode changed to: %s"), *GetModeDisplayName().ToString());
	OnModeChanged.Broadcast(OldMode, CurrentMode);
}

void UMOTerraformingComponent::SetMode(EMOTerraformMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	EMOTerraformMode OldMode = CurrentMode;
	CurrentMode = NewMode;
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Mode set to: %s"), *GetModeDisplayName().ToString());
	OnModeChanged.Broadcast(OldMode, CurrentMode);
}

FText UMOTerraformingComponent::GetModeDisplayName() const
{
	switch (CurrentMode)
	{
	case EMOTerraformMode::Dig:
		return NSLOCTEXT("MO", "TerraformDig", "Dig");
	case EMOTerraformMode::Raise:
		return NSLOCTEXT("MO", "TerraformRaise", "Raise");
	case EMOTerraformMode::Flatten:
		return NSLOCTEXT("MO", "TerraformFlatten", "Flatten");
	case EMOTerraformMode::Smooth:
		return NSLOCTEXT("MO", "TerraformSmooth", "Smooth");
	case EMOTerraformMode::RemoveFoliage:
		return NSLOCTEXT("MO", "TerraformRemoveFoliage", "Remove Foliage");
	default:
		return NSLOCTEXT("MO", "TerraformUnknown", "Unknown");
	}
}

// ============================================================================
// FOLIAGE REMOVAL
// ============================================================================

bool UMOTerraformingComponent::RemoveFoliage(const FVector& WorldLocation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float RadiusSq = Config.Radius * Config.Radius;

	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] RemoveFoliage at %s (Radius: %.1f)"),
		*WorldLocation.ToString(), Config.Radius);

	// SCOPE: removes only HISM instances explicitly opted-in via tags on the
	// owning Actor OR the HISM ComponentTags. Tag list is sourced from
	// UMOTerrainModificationSubsystem::AutoSweepTags so this code path and
	// the post-zone-register sweep agree on "what's auto-removable".
	//
	// DESIGN: harvestable content (trees, rocks, berry bushes) MUST persist
	// for the player to interact with explicitly. Falling open here would
	// let one RemoveFoliage swing strip a region of every chop-able and
	// mine-able resource.
	//
	// Notes:
	//   - Uses TActorIterator (not OverlapMulti) because PCG HISMs are often
	//     configured for line-trace collision only, which doesn't show up
	//     in object-channel overlap queries.
	//   - Filters per-COMPONENT bounds, not per-actor bounds. Actor bounds
	//     don't always aggregate HISM extents correctly after dynamic
	//     instance changes; component bounds are accurate.
	//   - Foliage system: AInstancedFoliageActor's components are subclasses
	//     of UInstancedStaticMeshComponent, so they're caught by this loop
	//     automatically (still gated by tags).

	// Resolve tag list once. Subsystem unavailable or empty list = safe
	// fallback to "remove nothing" — see design comment above.
	UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(this);
	const TArray<FName> AutoSweepTags = TerrainMod ? TerrainMod->AutoSweepTags : TArray<FName>();
	if (AutoSweepTags.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOTerraforming] RemoveFoliage: AutoSweepTags is empty (subsystem=%s). Nothing eligible — configure tags in Project Settings → MO|TerrainMod|Sweep."),
			TerrainMod ? TEXT("FOUND") : TEXT("NULL"));
		return false;
	}

	struct FFoliageRemoval
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Comp;
		TArray<int32> InstanceIndices;
	};
	TMap<UInstancedStaticMeshComponent*, FFoliageRemoval> Removals;

	int32 ActorsScanned = 0;
	int32 ISMsScanned = 0;
	int32 ISMsConsidered = 0;
	int32 ISMsSkippedNotTagged = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == GetOwner())
		{
			continue;
		}
		++ActorsScanned;

		// Drill into every ISM/HISM on the actor. We DON'T pre-filter by
		// actor bounds — for huge PCG actors covering a whole biome, the
		// actor bounds might be massive while individual HISMs have tighter
		// bounds; the per-component check below is both more accurate and
		// faster (single sphere math per HISM is cheap).
		TArray<UInstancedStaticMeshComponent*> ISMs;
		Actor->GetComponents<UInstancedStaticMeshComponent>(ISMs);

		for (UInstancedStaticMeshComponent* ISM : ISMs)
		{
			if (!IsValid(ISM))
			{
				continue;
			}
			++ISMsScanned;

			// Per-component sphere reject: skip if this HISM's bounding
			// sphere doesn't overlap our radius. HISMs maintain their own
			// bounds and these are reliable.
			const FBoxSphereBounds CompBounds = ISM->Bounds;
			const float ClosestPossibleDist = FVector::Dist(CompBounds.Origin, WorldLocation) - CompBounds.SphereRadius;
			if (ClosestPossibleDist > Config.Radius)
			{
				continue;
			}
			++ISMsConsidered;

			// Tag gate (same definition as the subsystem sweep): match if
			// any tag in AutoSweepTags appears on either the HISM's
			// ComponentTags or the owning Actor's Tags.
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

			const int32 NumInstances = ISM->GetInstanceCount();
			for (int32 i = 0; i < NumInstances; ++i)
			{
				FTransform InstanceXform;
				if (!ISM->GetInstanceTransform(i, InstanceXform, /*bWorldSpace*/ true))
				{
					continue;
				}

				const float DistSq = FVector::DistSquared(InstanceXform.GetLocation(), WorldLocation);
				if (DistSq <= RadiusSq)
				{
					FFoliageRemoval& Entry = Removals.FindOrAdd(ISM);
					Entry.Comp = ISM;
					Entry.InstanceIndices.Add(i);
				}
			}
		}
	}

	int32 InstancesRemoved = 0;
	for (auto& Pair : Removals)
	{
		UInstancedStaticMeshComponent* ISM = Pair.Key;
		if (!IsValid(ISM))
		{
			continue;
		}

		// Descending sort so removing index N doesn't shift index N+1's identity.
		Pair.Value.InstanceIndices.Sort([](const int32& A, const int32& B) { return A > B; });
		for (int32 Idx : Pair.Value.InstanceIndices)
		{
			if (ISM->RemoveInstance(Idx))
			{
				++InstancesRemoved;
			}
		}
	}

	MOHARVEST_LOG(this, "Terrain-Sculpt",
		"RemoveFoliage: actors=%d ISMsTotal=%d ISMsInRadius=%d ISMsSkippedNotTagged=%d removed=%d acrossComponents=%d",
		ActorsScanned, ISMsScanned, ISMsConsidered, ISMsSkippedNotTagged, InstancesRemoved, Removals.Num());

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOTerraforming] RemoveFoliage: scanned %d actors, %d ISM/HISM components total (%d in radius, %d skipped untagged), removed %d instance(s) across %d components"),
		ActorsScanned, ISMsScanned, ISMsConsidered, ISMsSkippedNotTagged, InstancesRemoved, Removals.Num());

	return InstancesRemoved > 0;
}

// ============================================================================
// TIMED ACTION (5-second progress, with interrupt support)
// ============================================================================

bool UMOTerraformingComponent::BeginTerraform()
{
	// One pending action at a time. If a previous one is still running,
	// cancel it (which fires OnTerraformCancelled so the UI can tear down).
	if (PendingAction.bActive)
	{
		CancelTerraform();
	}

	FVector HitLocation;
	FVector HitNormal;
	if (!FindTerraformTarget(HitLocation, HitNormal))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOTerraforming] BeginTerraform: no valid target"));
		return false;
	}

	// Capture the action intent. The Flatten height is resolved NOW (not at
	// completion) so the locked-in target reflects where the player was
	// standing at the moment they started the action.
	PendingAction.Reset();
	PendingAction.bActive = true;
	PendingAction.Mode = CurrentMode;
	PendingAction.TargetLocation = HitLocation;
	PendingAction.TargetNormal = HitNormal;
	PendingAction.ElapsedTime = 0.0f;
	PendingAction.TotalTime = FMath::Max(0.1f, TerraformDurationSeconds);

	if (CurrentMode == EMOTerraformMode::Flatten)
	{
		float GroundZ = 0.0f;
		if (Config.bUseCharacterFeetForFlatten && ResolveCharacterGroundZ(GroundZ))
		{
			PendingAction.FlattenTargetHeight = GroundZ;
		}
		else
		{
			PendingAction.FlattenTargetHeight = Config.bUseCharacterFeetForFlatten
				? HitLocation.Z   // owner not a character — fall back to ray-hit Z
				: Config.FlattenTargetHeight;
		}
	}

	// Subscribe to the owner's interrupt broadcast. Movement, damage, etc.
	// will fire NotifyInterrupt -> CancelTerraform automatically.
	RegisterForInterrupts();

	// Enable our own tick so we advance ElapsedTime each frame. The component
	// is now the authoritative source of progress — the widget is just a
	// view. This is the key fix for "no progress widget Blueprint set up =
	// terraform never completes": the action completes regardless of UI.
	SetComponentTickEnabled(true);

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOTerraforming] BeginTerraform mode=%s at %s, duration=%.1fs"),
		*GetModeDisplayName().ToString(), *HitLocation.ToString(), PendingAction.TotalTime);

	OnTerraformStarted.Broadcast(CurrentMode, PendingAction.TotalTime);
	return true;
}

bool UMOTerraformingComponent::CompleteTerraform()
{
	// Manual completion API. Normally the TickComponent auto-completes when
	// the timer expires, so this is mostly used for explicit "skip the timer"
	// paths (tools, AI, console commands, tests). Calling it a second time
	// after auto-completion is a safe no-op because bActive is already false.
	if (!PendingAction.bActive)
	{
		return false;
	}

	const bool bSuccess = ApplyPendingTerraform();

	PendingAction.Reset();
	SetComponentTickEnabled(false);
	UnregisterFromInterrupts();

	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] CompleteTerraform applied=%s"),
		bSuccess ? TEXT("true") : TEXT("false"));

	OnTerraformCompleted.Broadcast(bSuccess);
	return bSuccess;
}

void UMOTerraformingComponent::CancelTerraform()
{
	if (!PendingAction.bActive)
	{
		return;
	}

	PendingAction.Reset();
	SetComponentTickEnabled(false);
	UnregisterFromInterrupts();

	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] CancelTerraform"));
	OnTerraformCancelled.Broadcast();
}

bool UMOTerraformingComponent::ApplyPendingTerraform()
{
	if (!PendingAction.bActive)
	{
		return false;
	}

	// Bypass the bUseCharacterFeetForFlatten branch in TerraformAtLocation —
	// we've already locked in the Z. Apply directly via the per-mode methods.
	bool bSuccess = false;
	switch (PendingAction.Mode)
	{
	case EMOTerraformMode::Dig:
		bSuccess = Dig(PendingAction.TargetLocation);
		break;
	case EMOTerraformMode::Raise:
		bSuccess = Raise(PendingAction.TargetLocation);
		break;
	case EMOTerraformMode::Flatten:
		bSuccess = Flatten(PendingAction.TargetLocation, PendingAction.FlattenTargetHeight);
		break;
	case EMOTerraformMode::Smooth:
		bSuccess = Smooth(PendingAction.TargetLocation);
		break;
	case EMOTerraformMode::RemoveFoliage:
		bSuccess = RemoveFoliage(PendingAction.TargetLocation);
		break;
	default:
		return false;
	}

	MOHARVEST_LOG(this, "Terrain-Sculpt",
		"ApplyPendingTerraform: mode=%d location=%s radius=%.1f flattenZ=%.1f sculptApplied=%s",
		(int32)PendingAction.Mode,
		*PendingAction.TargetLocation.ToString(),
		Config.Radius,
		PendingAction.FlattenTargetHeight,
		bSuccess ? TEXT("true") : TEXT("false"));

	if (bSuccess)
	{
		// Any successful terraform action turns the affected area into "worked
		// ground." Report it to the world's terrain-modification subsystem,
		// which owns the zone list, the sweep timer, and save/load.
		if (UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(this))
		{
			TerrainMod->RegisterModifiedZone(PendingAction.TargetLocation, Config.Radius);

			// Immediate sweep: catches any foliage currently in radius and
			// any PCG instances that PCG already had on the ground before the
			// player got here. This is also why we don't need to call
			// RemoveFoliage directly — the subsystem's sweep covers it.
			TerrainMod->SweepModifiedZones();
		}
		else
		{
			MOHARVEST_LOG(this, "Terrain-Sculpt",
				"  WARNING: UMOTerrainModificationSubsystem not found — falling back to one-shot foliage clear");
			// Subsystem missing — fall back to a one-shot foliage clear in
			// the local radius so at least the immediate action looks right.
			// This shouldn't happen in normal gameplay (game worlds always
			// have the subsystem) but it's a defensive path for tools/tests.
			if (PendingAction.Mode != EMOTerraformMode::RemoveFoliage)
			{
				RemoveFoliage(PendingAction.TargetLocation);
			}
		}
	}

	return bSuccess;
}

bool UMOTerraformingComponent::ResolveCharacterGroundZ(float& OutGroundZ) const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!IsValid(OwnerCharacter))
	{
		return false;
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		return false;
	}

	// Feet = capsule center minus half-height. This is the surface Z the
	// character is currently standing on (or floating just above, in a frame
	// where the capsule is settling). Far more intuitive than the ray-hit Z
	// for flatten targets — when aiming at a hillside, the player wants
	// "flatten this area to where I'm standing", not "flatten to where my
	// cursor happened to land on the slope".
	const FVector Loc = OwnerCharacter->GetActorLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	OutGroundZ = Loc.Z - HalfHeight;
	return true;
}

void UMOTerraformingComponent::RegisterForInterrupts()
{
	AMOCharacter* OwnerChar = Cast<AMOCharacter>(GetOwner());
	if (!IsValid(OwnerChar))
	{
		return;
	}
	OwnerChar->RegisterInterruptListener(this);
	RegisteredInterruptCharacter = OwnerChar;
}

void UMOTerraformingComponent::UnregisterFromInterrupts()
{
	if (AMOCharacter* OwnerChar = RegisteredInterruptCharacter.Get())
	{
		OwnerChar->UnregisterInterruptListener(this);
	}
	RegisteredInterruptCharacter.Reset();
}

// ============================================================================
// INTERRUPT HANDLING (IMOInterruptibleInterface)
// ============================================================================

void UMOTerraformingComponent::NotifyInterrupt_Implementation(const FMOInterruptContext& Context)
{
	if (!PendingAction.bActive)
	{
		return;
	}

	// Terraforming requires concentration; cancel on any meaningful interrupt.
	// UserCancel is intentionally skipped — UI calls CancelTerraform directly.
	switch (Context.Reason)
	{
	case EMOInterruptReason::Movement:
	case EMOInterruptReason::Damage:
	case EMOInterruptReason::Knockdown:
	case EMOInterruptReason::Unconscious:
	case EMOInterruptReason::Death:
	case EMOInterruptReason::EnteredCombat:
	case EMOInterruptReason::LostControl:
	case EMOInterruptReason::External:
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOTerraforming] Interrupt reason=%d — cancelling pending terraform"),
			(int32)Context.Reason);
		CancelTerraform();
		break;

	case EMOInterruptReason::UserCancel:
	case EMOInterruptReason::None:
	default:
		break;
	}
}
