#include "MOTerraformingComponent.h"
#include "MOWorldCreationTypes.h"
#include "MODevMode.h"
#include "HAL/IConsoleManager.h"
#include "MOInventoryComponent.h"
#include "MOGeologyYield.h"
#include "MOFramework.h"
#include "MOViewpointUtils.h"
#include "MOCharacter.h"
#include "MOTerrainModificationSubsystem.h"
#include "MODesignationSubsystem.h"
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
#include "MOSpawnClearance.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "MOPlayerController.h"

#define LOCTEXT_NAMESPACE "MOTerraforming"

namespace MOTerraformDev
{
	// Wes, Oct 18 2026: "make terraforming take 1s while in dev mode". A developer's shortcut, never the simulation: off unless the world's host is in
	// developer mode (FMODevMode::IsHostInDevMode). Development builds are always in developer mode, so check the simulated times with 0.
	TAutoConsoleVariable<float> CVarDevTerraformSeconds(
		TEXT("MO.Dev.TerraformSeconds"), 1.0f,
		TEXT("Developer mode only: every terraform action takes this many seconds. 0 = the simulated time (volume and rock) even in developer mode."),
		ECVF_Default);

	/** The developer override for Owner's world, or 0 (the simulated time). */
	float SecondsFor(const UObject* Owner)
	{
		return FMODevMode::IsHostInDevMode(Owner) ? FMath::Max(0.0f, CVarDevTerraformSeconds.GetValueOnGameThread()) : 0.0f;
	}
}

UMOTerraformingComponent::UMOTerraformingComponent()
{
	// We tick only while a terraform action is in-flight. bCanEverTick must
	// be true so SetComponentTickEnabled(true) actually works; we start
	// disabled so we don't waste a per-frame call when idle.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Replicated so the co-op ServerApplyTerraform RPC routes to the host (#132/H17).
	SetIsReplicatedByDefault(true);
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

	// Incremental dig/raise (either sculpt) and a height flatten's bite: move a little earth each time progress crosses the
	// next step threshold, so the ground changes gradually AND an interruption
	// keeps the partial work already moved (see CancelTerraform). Authority-only:
	// a remote client's tick must never edit the voxel world — it forwards the
	// whole op at completion via ApplyPendingTerraform (H17). Smooth, a volume
	// flatten and remote clients apply atomically at completion below.
	const bool bIncremental = IsIncrementalTerraformMode(PendingAction.Mode)
		&& GetOwner()->HasAuthority();
	if (bIncremental)
	{
		const int32 N = FMath::Max(1, TerraformIncrementSteps);
		const int32 TargetStep = FMath::FloorToInt(PendingAction.GetProgress01() * N);
		while (PendingAction.AppliedSteps < TargetStep && PendingAction.AppliedSteps < N)
		{
			PendingAction.DugVolumeM3 += ApplyTerraformIncrement(++PendingAction.AppliedSteps, N);
		}
	}

	// Completion: finish/apply the sculpt, then tear down state. We use >= so even
	// a small frame-time overshoot triggers exactly once instead of being skipped.
	if (PendingAction.ElapsedTime >= PendingAction.TotalTime)
	{
		bool bSuccess = false;
		if (bIncremental)
		{
			// Flush any steps rounding left unapplied, then register the worked
			// zone once (the increments already edited the voxels each step).
			const int32 N = FMath::Max(1, TerraformIncrementSteps);
			while (PendingAction.AppliedSteps < N)
			{
				PendingAction.DugVolumeM3 += ApplyTerraformIncrement(++PendingAction.AppliedSteps, N);
			}
			RegisterTerraformZone();
			if (PendingAction.Mode == EMOTerraformMode::Dig || (PendingAction.Mode == EMOTerraformMode::Flatten && !PendingAction.bFlattenFill))
			{
				GrantDigYield(PendingAction.DugVolumeM3, /*bSlab=*/!PendingAction.bVolume);   // a flatten with no low ground in the brush: its cut earth is spoil too
			}
			bSuccess = true;
		}
		else
		{
			// Atomic modes (flatten/smooth/volume) + remote-client forward.
			bSuccess = ApplyPendingTerraform();
		}

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
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMOTerraformingComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
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

bool UMOTerraformingComponent::EnsureVolumeSculptActor()
{
	if (!VolumeSculptActor.IsValid())
	{
		VolumeSculptActor = MOVoxel::EnsureVolumeSculptActor(GetWorld());
	}
	return VolumeSculptActor.IsValid();
}

bool UMOTerraformingComponent::IsOnVolumeRock(const FVector& At) const
{
	float HeightCm = 0.0f;
	const bool bKnown = MOVoxel::HeightLayerSurfaceCm(GetWorld(), At.X, At.Y, HeightCm);
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] volume rock? spot z %.0f, height layer %s"), At.Z,
		bKnown ? *FString::Printf(TEXT("%.0f (%.0f cm over it)"), HeightCm, At.Z - HeightCm) : TEXT("unknown"));
	return bKnown && At.Z > HeightCm + VolumeRockMarginCm;
}

bool UMOTerraformingComponent::UsesVolumeAt(const FVector& At)
{
	if (bUseVolumeSculpting || (PendingAction.bActive && PendingAction.bVolume))
	{
		return EnsureVolumeSculptActor();
	}
	return (HasRockOverhead(At) || IsOnVolumeRock(At)) && EnsureVolumeSculptActor();
}

float UMOTerraformingComponent::GetVolumeBiteRadius() const
{
	// The half-sphere holding one open-ground bite's earth, floored at what the voxel world draws: 1.5 of its voxels (the generator's size).
	const double CubicCm = (double)ComputeMovedVolumeCubicMeters(GetBrushRadius(), GetBrushStrength()) * 1.0e6;
	const double EarthRadius = FMath::Pow(3.0 * CubicCm / (2.0 * UE_DOUBLE_PI), 1.0 / 3.0);
	const double VoxelCm = FMOWorldGenParamsProvider::HasPublished() ? (double)FMOWorldGenParamsProvider::Get()->Tuning.VoxelSizeCm : 50.0;
	return (float)FMath::Max(EarthRadius, 1.5 * VoxelCm);
}

float UMOTerraformingComponent::GetBrushRadius() const
{
	return Config.Radius * (FMOWorldRules::IsCreative(this) ? FMath::Max(0.1f, CreativeRadiusScale) : 1.0f);
}

float UMOTerraformingComponent::GetBrushStrength() const
{
	return FMath::Clamp(Config.RaiseLowerStrength * (FMOWorldRules::IsCreative(this) ? FMath::Max(0.1f, CreativeStrengthScale) : 1.0f), 0.0f, 1.0f);
}

float UMOTerraformingComponent::GetVolumeBiteM3() const
{
	const double R = GetVolumeBiteRadius();
	return (float)(0.5 * (4.0 / 3.0) * UE_DOUBLE_PI * R * R * R / 1.0e6);
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
	// Server-authoritative: voxel edits + the RegisterModifiedZone persistence record
	// must only run on the server; a non-authority caller no-ops (H17). Remote-client
	// terraform transport (ServerBeginTerraform + multicast re-apply) is a flagged follow-on.
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	// RemoveFoliage doesn't need a voxel sculpt actor — it works on world
	// HISM/ISM components — so let it through before the HasValidSculptActor gate.
	if (Mode != EMOTerraformMode::RemoveFoliage && !HasValidSculptActor())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTerraforming] No valid sculpt actor for current mode"));
		return false;
	}

	bool bSuccess = false;
	switch (Mode)
	{
	case EMOTerraformMode::Dig:
		bSuccess = Dig(WorldLocation);
		break;
	case EMOTerraformMode::Raise:
		bSuccess = Raise(WorldLocation);
		break;
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
			bSuccess = Flatten(WorldLocation, TargetHeight);
			break;
		}
	case EMOTerraformMode::Smooth:
		bSuccess = Smooth(WorldLocation);
		break;
	case EMOTerraformMode::RemoveFoliage:
		bSuccess = RemoveFoliage(WorldLocation);
		break;
	default:
		return false;
	}

	// Every successful sculpt is "worked ground" — register + sweep here, at
	// the PUBLIC entry, not only in ApplyPendingTerraform. Before this,
	// BlueprintCallable callers (BP, mods, test harness) mutated voxels with
	// NO RegisterModifiedZone: the edit never persisted and PCG scatter
	// respawned in the crater — the exact bug class the ServerApplyTerraform
	// comment documents for the old remote-client path.
	if (bSuccess)
	{
		RegisterWorkedGround(WorldLocation);
	}
	return bSuccess;
}

void UMOTerraformingComponent::RegisterWorkedGround(const FVector& Location)
{
	if (UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(this))
	{
		TerrainMod->RegisterModifiedZone(Location, GetBrushRadius());
		// Immediate sweep: clears scatter already standing in the new zone.
		TerrainMod->SweepModifiedZones();
	}
}

bool UMOTerraformingComponent::Dig(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Dig at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), GetBrushRadius(), GetBrushStrength());

	if (UsesVolumeAt(WorldLocation))
	{
		return VolumeDig(WorldLocation);
	}
	return HeightDig(FVector2D(WorldLocation.X, WorldLocation.Y));
}

bool UMOTerraformingComponent::Raise(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Raise at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), GetBrushRadius(), GetBrushStrength());

	if (UsesVolumeAt(WorldLocation))
	{
		return VolumeRaise(WorldLocation);
	}
	return HeightRaise(FVector2D(WorldLocation.X, WorldLocation.Y));
}

bool UMOTerraformingComponent::Flatten(const FVector& WorldLocation, float TargetHeight)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Flatten at %s to height %.1f (Radius: %.1f)"),
		*WorldLocation.ToString(), TargetHeight, GetBrushRadius());

	if (UsesVolumeAt(WorldLocation))
	{
		return VolumeFlatten(WorldLocation, FVector::UpVector, TargetHeight);
	}
	LastFlattenSpoilM3 = 0.0f;
	FVector DigAt, FillAt;
	bool bFill = false;
	if (!FindFlattenBite(WorldLocation, TargetHeight, DigAt, FillAt, bFill))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Flatten at %s: nothing in the brush stands over %.0f -- nothing to cut"), *WorldLocation.ToString(), TargetHeight);
		return false;
	}
	const float CutM3 = ApplyFlattenStep(DigAt, FillAt, bFill, GetBrushStrength());
	LastFlattenSpoilM3 = bFill ? 0.0f : CutM3;   // with no low ground in the brush the cut earth is spoil (the timed action puts it on the pile)
	// The bite's own ground is worked ground too (the caller registers the brush centre; a bite at the brush's edge reaches a brush further).
	if (UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(this))
	{
		TerrainMod->RegisterModifiedZone(DigAt, GetBrushRadius());
		if (bFill)
		{
			TerrainMod->RegisterModifiedZone(FillAt, GetBrushRadius());
		}
	}
	return true;
}

bool UMOTerraformingComponent::FindFlattenBite(const FVector& Center, float TargetZ, FVector& OutDig, FVector& OutFill, bool& bOutFill) const
{
	bOutFill = false;
	const UMODesignationSubsystem* Desig = UMODesignationSubsystem::Get(this);
	if (!Desig)
	{
		return false;
	}
	FMODesignationZone Zone;
	Zone.ZoneId = FGuid::NewGuid();   // a zone of this action only, never designated
	Zone.Kind = EMODesignationKind::Flatten;
	Zone.Center = Center;
	Zone.Radius = GetBrushRadius();
	Zone.TargetHeightZ = TargetZ;
	int32 Samples = 0;
	if (!Desig->FindBitePoint(Zone, /*bDig=*/true, OutDig, Samples))
	{
		return false;
	}
	bOutFill = Desig->FindBitePoint(Zone, /*bDig=*/false, OutFill, Samples);
	return true;
}

float UMOTerraformingComponent::ApplyFlattenStep(const FVector& DigAt, const FVector& FillAt, bool bFill, float Strength)
{
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor) || Strength <= 0.0f)
	{
		return 0.0f;
	}
	MOVoxel::HeightSculpt(Actor, FVector2D(DigAt.X, DigAt.Y), GetBrushRadius(), Strength, /*bAdd*/ false);
	if (bFill)
	{
		MOVoxel::HeightSculpt(Actor, FVector2D(FillAt.X, FillAt.Y), GetBrushRadius(), Strength, /*bAdd*/ true);
	}
	return ComputeMovedVolumeCubicMeters(GetBrushRadius(), Strength);
}

bool UMOTerraformingComponent::Smooth(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Smooth at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), GetBrushRadius(), Config.SmoothStrength);

	if (UsesVolumeAt(WorldLocation))
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
	MOVoxel::HeightSculpt(Actor, Location, GetBrushRadius(), GetBrushStrength(), /*bAdd*/ false);
	return true;
}

bool UMOTerraformingComponent::HeightRaise(const FVector2D& Location)
{
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::HeightSculpt(Actor, Location, GetBrushRadius(), GetBrushStrength(), /*bAdd*/ true);
	return true;
}

bool UMOTerraformingComponent::HeightSmooth(const FVector2D& Location)
{
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::HeightSmooth(Actor, Location, GetBrushRadius(), Config.SmoothStrength);
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
	MOVoxel::VolumeSphere(Actor, Location, GetVolumeBiteRadius(), /*bAdd*/ false, /*Smoothness*/ 0.0f);
	return true;
}

bool UMOTerraformingComponent::VolumeRaise(const FVector& Location)
{
	AActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::VolumeSphere(Actor, Location, GetVolumeBiteRadius(), /*bAdd*/ true, /*Smoothness*/ 0.0f);
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
	MOVoxel::VolumeFlatten(Actor, Location, Normal, GetBrushRadius(), GetBrushRadius() * 2.0f, Config.Falloff);
	return true;
}

bool UMOTerraformingComponent::VolumeSmooth(const FVector& Location)
{
	AActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}
	MOVoxel::VolumeSmooth(Actor, Location, GetBrushRadius(), Config.SmoothStrength);
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

	const float RadiusSq = GetBrushRadius() * GetBrushRadius();

	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] RemoveFoliage at %s (Radius: %.1f)"),
		*WorldLocation.ToString(), GetBrushRadius());

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
			if (ClosestPossibleDist > GetBrushRadius())
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
// VOLUME-BASED DURATION
// ============================================================================

float UMOTerraformingComponent::ComputeTerraformDurationSeconds(
	float RadiusUU, float DepthMeters, float SecondsPerCubicMeter, float MinSeconds)
{
	const float RadiusMeters = FMath::Max(0.0f, RadiusUU) * 0.01f;   // UU (cm) -> m
	const float FootprintM2 = PI * RadiusMeters * RadiusMeters;
	const float VolumeM3 = FootprintM2 * FMath::Max(0.0f, DepthMeters);
	const float Duration = VolumeM3 * FMath::Max(0.0f, SecondsPerCubicMeter);
	return FMath::Max(MinSeconds, Duration);
}

float UMOTerraformingComponent::GetActionDepthMeters(EMOTerraformMode Mode) const
{
	switch (Mode)
	{
	case EMOTerraformMode::Dig:
	case EMOTerraformMode::Raise:
		// Surface displacement scales with brush strength.
		return GetBrushStrength() * TerraformDepthPerStrengthMeters;
	case EMOTerraformMode::Flatten:
		// Cut+fill moves a terrain-dependent amount; use half the full-strength
		// displacement as a representative earth-moved estimate.
		return 0.5f * TerraformDepthPerStrengthMeters;
	case EMOTerraformMode::Smooth:
		return 0.1f * TerraformDepthPerStrengthMeters;
	case EMOTerraformMode::RemoveFoliage:
	default:
		return 0.0f;   // no earth moved -> min-seconds floor
	}
}

// ============================================================================
// EXCAVATION PRIMITIVES (unit 3 stage 1)
// ============================================================================

float UMOTerraformingComponent::TerraformAtLocationEx(
	const FVector& WorldLocation, EMOTerraformMode Mode, float RadiusUU, float Strength)
{
	// Authority-only — same contract as TerraformAtLocation: the voxel edit and the
	// RegisterModifiedZone persistence record must run server-side (H17).
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return 0.0f;
	}

	// Only Dig/Raise compose the excavation loop. Flatten decomposes into paired
	// Dig/Raise at the job layer; the other modes aren't bounded earth-moves.
	if (Mode != EMOTerraformMode::Dig && Mode != EMOTerraformMode::Raise)
	{
		return 0.0f;
	}

	AActor* SculptActor = HeightSculptActor.Get();
	if (!IsValid(SculptActor))
	{
		// Lazy resolve in case BeginPlay hadn't found the world sculpt actor yet
		// (e.g. an AI pawn's component driven before its own BeginPlay ran).
		AutoFindSculptActors();
		SculptActor = HeightSculptActor.Get();
		if (!IsValid(SculptActor))
		{
			return 0.0f;
		}
	}

	const float SafeRadius = FMath::Max(1.0f, RadiusUU);
	const float SafeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	const bool bAdd = (Mode == EMOTerraformMode::Raise);

	// Explicit radius/strength straight into the facade — does NOT read or mutate
	// Config, so a pawn job driving precise increments can't stomp the component's
	// player-facing brush settings.
	MOVoxel::HeightSculpt(SculptActor, FVector2D(WorldLocation.X, WorldLocation.Y),
		SafeRadius, SafeStrength, bAdd);

	// Persist the worked zone + clear foliage inside it, like TerraformAtLocation.
	// NOTE: this synchronous sweep is the per-op perf cost the excavation job bounds
	// by batching ops (see Docs/Terraform_Excavation_Plan.md gotchas).
	if (UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(this))
	{
		TerrainMod->RegisterModifiedZone(WorldLocation, SafeRadius);
		TerrainMod->SweepModifiedZones();
	}

	return ComputeMovedVolumeCubicMeters(SafeRadius, SafeStrength);
}

float UMOTerraformingComponent::ComputeMovedVolumeCubicMeters(float RadiusUU, float Strength)
{
	return MOVoxel::HeightSculptMovedVolumeM3(FMath::Max(0.0f, RadiusUU), FMath::Clamp(Strength, 0.0f, 1.0f));
}

float UMOTerraformingComponent::ComputeDurationForVolumeSeconds(float VolumeM3, float SecondsPerCubicMeter, float MinSeconds)
{
	return FMath::Max(MinSeconds, FMath::Max(0.0f, VolumeM3) * FMath::Max(0.0f, SecondsPerCubicMeter));
}

int32 UMOTerraformingComponent::SpoilItemsForVolume(float VolumeM3, float ItemsPerCubicMeter)
{
	return FMath::FloorToInt(FMath::Max(0.0f, VolumeM3) * FMath::Max(0.0f, ItemsPerCubicMeter));
}

// ============================================================================
// TIMED ACTION (volume-based progress, with interrupt support)
// ============================================================================

bool UMOTerraformingComponent::BeginTerraform()
{
	// One pending action at a time. The same tool pressed again while it works is the same work going on: it used to cancel the action and start
	// again, and the cancel put the earth already dug on the pile -- dirt before the bar was done, every press (BR1101, Nov 1: "if you start another
	// dig it spawns a dirt item before the bar completes"). Another tool takes over (the cancel fires OnTerraformCancelled so the UI can tear down).
	if (PendingAction.bActive)
	{
		if (PendingAction.Mode == CurrentMode)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] %s is already under way (%.0f of %.0f s): the press keeps it going"), *GetModeDisplayName().ToString(),
				PendingAction.ElapsedTime, PendingAction.TotalTime);
			return true;
		}
		CancelTerraform();
	}

	FVector HitLocation;
	FVector HitNormal;
	if (!FindTerraformTarget(HitLocation, HitNormal))
	{
		// DEBUG (MOTerraforming, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOTerraforming] BeginTerraform: no valid target"));
		return false;
	}

	// The height sculpt works the ground's top surface over (X, Y): under rock -- in a cave, under an overhang, into a wall -- it would dig, raise
	// or level the hill ABOVE, not the spot aimed at. A flatten in a cave levelled the hill down to the cave floor and opened the roof: square
	// patches of sun on the floor (BR1101, Nov 1: "lighting problems when using flatten in a cave"). Under rock the volume sculpt works the spot
	// itself (N-256), made if the level has none; only without one is it refused.
	// The volume layer's own rock is worked the same way (IsOnVolumeRock: a stamped cliff's face, an island).
	const bool bOverhead = !bUseVolumeSculpting && CurrentMode != EMOTerraformMode::RemoveFoliage && HasRockOverhead(HitLocation);
	const bool bVolumeRock = !bUseVolumeSculpting && CurrentMode != EMOTerraformMode::RemoveFoliage && !bOverhead && IsOnVolumeRock(HitLocation);
	const bool bUnderRock = bOverhead || bVolumeRock;
	if (bUnderRock && !EnsureVolumeSculptActor())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] %s at %s refused: rock overhead and no volume sculpt"), *GetModeDisplayName().ToString(),
			*HitLocation.ToString());
		NotifyOwner(LOCTEXT("RockOverhead", "There is rock above this spot: the ground can only be worked in the open."));
		return false;
	}
	if (bUnderRock)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] %s at %s: %s -- the volume sculpt works the spot itself"), *GetModeDisplayName().ToString(),
			*HitLocation.ToString(), bVolumeRock ? TEXT("on the volume layer's rock (over the height terrain)") : TEXT("rock overhead"));
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
	PendingAction.bVolume = bUseVolumeSculpting || bUnderRock;
	// Volume-based duration: moving earth takes time proportional to how much earth this brush moves, floored at the legacy duration so
	// trivial ops aren't instant. A Dig/Raise moves exactly one bite (a volume bite: the half of its sphere behind the hit point, GetVolumeBiteM3);
	// Smooth (and a volume flatten) keep the nominal footprint x depth estimate -- their earth depends on the ground under the brush; a height
	// Flatten is timed by its bite (below).
	const bool bDigRaise = CurrentMode == EMOTerraformMode::Dig || CurrentMode == EMOTerraformMode::Raise;
	const float BiteM3 = PendingAction.bVolume ? GetVolumeBiteM3() : ComputeMovedVolumeCubicMeters(GetBrushRadius(), GetBrushStrength());
	PendingAction.TotalTime = bDigRaise
		? ComputeDurationForVolumeSeconds(BiteM3, TerraformSecondsPerCubicMeter, FMath::Max(0.1f, TerraformDurationSeconds))
		: ComputeTerraformDurationSeconds(GetBrushRadius(), GetActionDepthMeters(CurrentMode),
			TerraformSecondsPerCubicMeter, FMath::Max(0.1f, TerraformDurationSeconds));
	// What the brush cuts decides how long it takes (Wes, Oct 17 2026): earth at the base rate, limestone ~5x, granite ~40x -- the same points the yield is
	// shared over (FMOGeologyYield). Raising places loose earth; smooth keeps the earth rate.
	float DigHardness = 1.0f;
	if (CurrentMode == EMOTerraformMode::Dig)
	{
		DigHardness = FMOGeologyYield::DigTimeMultiplierInWorld(this, HitLocation, PendingAction.bVolume ? GetVolumeBiteRadius() : GetBrushRadius(), BiteM3,
			!PendingAction.bVolume);
		PendingAction.TotalTime *= DigHardness;
	}

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
		if (!PendingAction.bVolume)
		{
			// One bite (Flatten's header): the brush's earth off the highest ground over the target, onto the lowest under it. Its time is that earth
			// dug (at the rock's hardness) and, when it has somewhere to go, laid again -- not a nominal estimate of the whole brush.
			if (!FindFlattenBite(HitLocation, PendingAction.FlattenTargetHeight, PendingAction.FlattenDigPoint, PendingAction.FlattenFillPoint, PendingAction.bFlattenFill))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Flatten at %s: nothing in the brush stands more than the tolerance over %.0f (the feet) -- nothing to cut"),
					*HitLocation.ToString(), PendingAction.FlattenTargetHeight);
				NotifyOwner(LOCTEXT("NothingToLevel", "Nothing here stands above your feet to cut down; the Raise tool builds low ground up."));
				PendingAction.Reset();
				return false;
			}
			DigHardness = FMOGeologyYield::DigTimeMultiplierInWorld(this, PendingAction.FlattenDigPoint, GetBrushRadius(), BiteM3, /*bSlab=*/true);
			PendingAction.TotalTime = FMath::Max(FMath::Max(0.1f, TerraformDurationSeconds),
				BiteM3 * TerraformSecondsPerCubicMeter * (DigHardness + (PendingAction.bFlattenFill ? 1.0f : 0.0f)));
		}
	}
	// This world's digging pace (MOWorldCreationTypes.h): chosen when the world was made, never zero.
	PendingAction.TotalTime = FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Terraforming, PendingAction.TotalTime);
	// A developer's shortcut (MO.Dev.TerraformSeconds), only while the world's host is in developer mode.
	if (const float DevSeconds = MOTerraformDev::SecondsFor(this); DevSeconds > 0.0f && PendingAction.TotalTime > 0.0f)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] developer mode: %.1f s instead of the simulated %.1f s (MO.Dev.TerraformSeconds 0 = simulated)"), DevSeconds,
			PendingAction.TotalTime);
		PendingAction.TotalTime = DevSeconds;
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
		TEXT("[MOTerraforming] BeginTerraform mode=%s at %s, duration=%.1fs (ground x%.1f)"),
		*GetModeDisplayName().ToString(), *HitLocation.ToString(), PendingAction.TotalTime, DigHardness);

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

	// Interrupted mid-dig: keep the earth already moved. The incremental steps
	// already edited the voxels; register the worked zone so the partial change
	// persists and clears foliage, exactly as a completed action would.
	if (PendingAction.AppliedSteps > 0)
	{
		RegisterTerraformZone();
		if (PendingAction.Mode == EMOTerraformMode::Dig || (PendingAction.Mode == EMOTerraformMode::Flatten && !PendingAction.bFlattenFill))
		{
			GrantDigYield(PendingAction.DugVolumeM3, /*bSlab=*/!PendingAction.bVolume);   // the earth already moved is in the pile
		}
	}

	PendingAction.Reset();
	SetComponentTickEnabled(false);
	UnregisterFromInterrupts();

	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] CancelTerraform"));
	OnTerraformCancelled.Broadcast();
}

void UMOTerraformingComponent::ServerApplyTerraform_Implementation(EMOTerraformMode Mode, FVector Location, float FlattenHeight)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	// Validate the client-resolved target before honoring it. The requesting client
	// resolved Location from its own viewpoint trace, so bound how far from the pawn the
	// server will act (generous margin over the client trace distance for tools/latency),
	// and clamp FlattenHeight to a sane band around the target so a bad/hostile value
	// can't spike terrain to +/-1e9 (audit -- terraform got no server validation in H17).
	const float MaxServerReachSq = FMath::Square(2500.0f);
	if (FVector::DistSquared(OwnerActor->GetActorLocation(), Location) > MaxServerReachSq)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTerraforming] ServerApplyTerraform: target out of reach -- rejected"));
		return;
	}
	FlattenHeight = FMath::Clamp(FlattenHeight, Location.Z - 5000.0f, Location.Z + 5000.0f);

	// Rate: a remote client forwards each WHOLE action once, at completion, and every action takes at least TerraformDurationSeconds (the volume-based
	// duration is floored there). So two applies from one pawn closer together than most of that floor are not two real actions: refuse the second
	// (H17 residual -- a modified client could otherwise fire instant craters as fast as it liked).
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float DevSeconds = MOTerraformDev::SecondsFor(this);
	const double MinInterval = FMOWorldRules::IsInstant(this, EMOWorldPace::Terraforming) ? 0.1
		: 0.8 * FMath::Max(0.1f, DevSeconds > 0.0f ? DevSeconds : TerraformDurationSeconds);   // a creative world's actions are instant
	if (LastServerTerraformTime >= 0.0 && Now - LastServerTerraformTime < MinInterval)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTerraforming] ServerApplyTerraform: %.2f s after the last apply (min %.1f) -- rejected"), Now - LastServerTerraformTime, MinInterval);
		return;
	}
	LastServerTerraformTime = Now;

	if (Mode != EMOTerraformMode::RemoveFoliage && !HasValidSculptActor())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTerraforming] ServerApplyTerraform: no valid sculpt actor"));
		return;
	}

	// Route through the SHARED authoritative apply so the voxel edit AND its
	// RegisterModifiedZone + foliage sweep (persistence!) all run -- the true host path.
	// (Previously this duplicated only the sculpt switch and silently skipped zone
	// registration, so remote-client terraform never saved and PCG foliage respawned in
	// the dug crater. Populate the pending action the server component doesn't otherwise
	// have -- the timed tick ran on the client -- then run + clear it.)
	PendingAction.Reset();
	PendingAction.bActive = true;
	PendingAction.Mode = Mode;
	PendingAction.TargetLocation = Location;
	PendingAction.FlattenTargetHeight = FlattenHeight;
	ApplyPendingTerraform();
	PendingAction.Reset();
}

bool UMOTerraformingComponent::ApplyPendingTerraform()
{
	if (!PendingAction.bActive)
	{
		return false;
	}

	// Server-authoritative: the actual voxel mutation + RegisterModifiedZone run only on
	// the server. The timed action's tick runs wherever BeginTerraform was called, so gate
	// the apply here so a client-side tick can never mutate the voxel world / persistence (H17).
	if (!GetOwner()->HasAuthority())
	{
		// Co-op: a remote client that locally controls this pawn forwards the resolved apply
		// to the host so the terraform reaches the authoritative voxel world + persistence.
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			ServerApplyTerraform(PendingAction.Mode, PendingAction.TargetLocation, PendingAction.FlattenTargetHeight);
			return true; // dispatched to the host; authoritative apply + persistence happen there
		}
		return false;
	}

	// The sculpt this apply works, decided once before it runs (a joiner's dig arrives here undecided: the host's own rock check decides it), so
	// the per-mode method and the yield below agree.
	if (PendingAction.Mode != EMOTerraformMode::RemoveFoliage && !PendingAction.bVolume)
	{
		PendingAction.bVolume = UsesVolumeAt(PendingAction.TargetLocation);
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
		GetBrushRadius(),
		PendingAction.FlattenTargetHeight,
		bSuccess ? TEXT("true") : TEXT("false"));

	if (bSuccess)
	{
		RegisterTerraformZone();
		if (PendingAction.Mode == EMOTerraformMode::Flatten && LastFlattenSpoilM3 > 0.0f)
		{
			GrantDigYield(LastFlattenSpoilM3, /*bSlab=*/true);   // a joiner's flatten applied here, with no low ground in the brush: spoil
		}
		if (PendingAction.Mode == EMOTerraformMode::Dig)
		{
			// The atomic dig (a joiner's dig applied here): its whole bite at once -- a volume bite is the half of its sphere behind the hit point.
			GrantDigYield(PendingAction.bVolume ? GetVolumeBiteM3() : ComputeMovedVolumeCubicMeters(GetBrushRadius(), GetBrushStrength()),
				/*bSlab=*/!PendingAction.bVolume);
		}
	}

	return bSuccess;
}

void UMOTerraformingComponent::GrantDigYield(float VolumeM3, bool bSlab)
{
	AActor* OwnerActor = GetOwner();
	if (!bYieldFromGeology || VolumeM3 <= 0.0f || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}
	if (FMOWorldRules::IsCreative(this))
	{
		return;   // a creative world's digs make no spoil (Wes, Nov 3: "make digging not produce dirt ... in creative mode")
	}
	// Whole items only: the part of an item's worth of earth a dig leaves over (an interrupted dig's few litres) is carried to this pawn's next dig,
	// not lost -- earth is conserved (Docs/Terraform_Excavation_Plan.md).
	const float WithCarry = VolumeM3 + DigYieldCarryM3;
	TArray<FMOItemYield> Yields;
	if (!FMOGeologyYield::ForDigInWorld(this, PendingAction.TargetLocation, GetBrushRadius(), WithCarry, ExcavationItemsPerCubicMeter, bSlab, ExcavationSpoilItemId, Yields))
	{
		Yields.Add({ ExcavationSpoilItemId, SpoilItemsForVolume(WithCarry, ExcavationItemsPerCubicMeter), true });   // no world seed: plain earth
	}
	int32 Items = 0;
	for (const FMOItemYield& Y : Yields)
	{
		Items += Y.Count;
	}
	DigYieldCarryM3 = FMath::Max(0.0f, WithCarry - Items / FMath::Max(ExcavationItemsPerCubicMeter, 1.0f));
	// The soil stays beside the hole as a spoil pile (earth is conserved, and a dig would otherwise fill the pack with dirt in a few strokes); stone and ore go to
	// the pack, the rest on the pile too when it is full. (The excavation job hauls its earth to a fill instead -- see AMOSurvivorController.)
	TArray<FMOItemYield> Soil, Kept;
	for (const FMOItemYield& Y : Yields)
	{
		(Y.ItemId == ExcavationSpoilItemId ? Soil : Kept).Add(Y);
	}
	FMOGeologyYield::GiveOrDrop(nullptr, Soil, PendingAction.TargetLocation, this);
	const int32 Dropped = FMOGeologyYield::GiveOrDrop(OwnerActor->FindComponentByClass<UMOInventoryComponent>(), Kept, PendingAction.TargetLocation);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOTerraforming] %s dug %.3fm3 at %s: %s (soil left on the spoil pile)%s"), *OwnerActor->GetName(), VolumeM3,
		*PendingAction.TargetLocation.ToString(), *FMOGeologyYield::Describe(Yields), Dropped > 0 ? *FString::Printf(TEXT(", %d more dropped: no room"), Dropped) : TEXT(""));
}

void UMOTerraformingComponent::RegisterTerraformZone()
{
	// Any successful terraform action turns the affected area into "worked
	// ground." Report it to the world's terrain-modification subsystem, which
	// owns the zone list, the sweep timer, and save/load.
	if (UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(this))
	{
		TerrainMod->RegisterModifiedZone(PendingAction.TargetLocation, GetBrushRadius());
		if (PendingAction.Mode == EMOTerraformMode::Flatten && PendingAction.AppliedSteps > 0)
		{
			// The incremental bite worked its own two spots (a bite at the brush's edge reaches a brush further than the centre's zone).
			TerrainMod->RegisterModifiedZone(PendingAction.FlattenDigPoint, GetBrushRadius());
			if (PendingAction.bFlattenFill)
			{
				TerrainMod->RegisterModifiedZone(PendingAction.FlattenFillPoint, GetBrushRadius());
			}
		}

		// Immediate sweep: catches any foliage currently in radius and any PCG
		// instances that PCG already had on the ground before the player got
		// here. This is also why we don't need to call RemoveFoliage directly —
		// the subsystem's sweep covers it.
		TerrainMod->SweepModifiedZones();
	}
	else
	{
		MOHARVEST_LOG(this, "Terrain-Sculpt",
			"  WARNING: UMOTerrainModificationSubsystem not found — falling back to one-shot foliage clear");
		// Subsystem missing — fall back to a one-shot foliage clear in the local
		// radius so at least the immediate action looks right. Shouldn't happen in
		// normal gameplay (game worlds always have the subsystem); defensive path.
		if (PendingAction.Mode != EMOTerraformMode::RemoveFoliage)
		{
			RemoveFoliage(PendingAction.TargetLocation);
		}
	}
}

bool UMOTerraformingComponent::IsIncrementalTerraformMode(EMOTerraformMode Mode) const
{
	// A dig or a raise moves earth that can be split into steps on either sculpt (a height strength fractioned, a volume sphere grown), and so
	// does a height flatten's bite. Smooth converges rather than accumulating, and a volume flatten has no bite: those stay atomic-at-completion.
	if (Mode == EMOTerraformMode::Dig || Mode == EMOTerraformMode::Raise)
	{
		return true;
	}
	const bool bVolume = PendingAction.bActive ? PendingAction.bVolume : bUseVolumeSculpting;
	return !bVolume && Mode == EMOTerraformMode::Flatten;
}

float UMOTerraformingComponent::ApplyTerraformIncrement(int32 Step, int32 TotalSteps)
{
	if (TotalSteps <= 0)
	{
		return 0.0f;
	}
	const bool bDig = PendingAction.Mode == EMOTerraformMode::Dig;
	if (PendingAction.bVolume)
	{
		// The sphere grows to the radius holding Step/TotalSteps of the bite (volume ~ r^3): each step takes the shell between it and the last.
		if (PendingAction.Mode == EMOTerraformMode::Flatten || !EnsureVolumeSculptActor())
		{
			return 0.0f;
		}
		AActor* Actor = VolumeSculptActor.Get();
		const float Fraction = FMath::Clamp((float)Step / (float)TotalSteps, 0.0f, 1.0f);
		MOVoxel::VolumeSphere(Actor, PendingAction.TargetLocation, GetVolumeBiteRadius() * FMath::Pow(Fraction, 1.0f / 3.0f), /*bAdd*/ !bDig,
			/*Smoothness*/ 0.0f);
		return bDig ? GetVolumeBiteM3() / (float)TotalSteps : 0.0f;
	}
	AActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return 0.0f;
	}
	const float StepStrength = GetBrushStrength() / static_cast<float>(TotalSteps);
	if (PendingAction.Mode == EMOTerraformMode::Flatten)
	{
		return ApplyFlattenStep(PendingAction.FlattenDigPoint, PendingAction.FlattenFillPoint, PendingAction.bFlattenFill, StepStrength);
	}
	const FVector2D Loc(PendingAction.TargetLocation.X, PendingAction.TargetLocation.Y);
	MOVoxel::HeightSculpt(Actor, Loc, GetBrushRadius(), StepStrength, /*bAdd*/ !bDig);
	return bDig ? ComputeMovedVolumeCubicMeters(GetBrushRadius(), StepStrength) : 0.0f;
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

bool UMOTerraformingComponent::HasRockOverhead(const FVector& At) const
{
	// In a cave: the generator's cave field a metre over the aimed spot (tunnels, caverns, the sinkholes that reach the ground). No collision
	// needed -- a cave's roof can be 60 m of rock thick, and the top of that hill is beyond the collision built round the pawn (Nov 1: in a cave
	// 60 m down the collision trace below found nothing and the dig began).
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	if (Seeds && Seeds->HasActiveSeed() && FMOWorldGenParamsProvider::HasPublished()
		&& FMOWorldGen::SampleCaveDistanceCm(*FMOWorldGenParamsProvider::Get(), At.X, At.Y, At.Z + 100.0, Seeds->GetActiveTerrainSeed()) < 0.0f)
	{
		return true;
	}
	// Under any other rock (an overhang, a wall the spot is on): the topmost voxel surface over the point's column, from 50 m up to half a metre
	// over it -- on open ground the column is air down to the point.
	FHitResult Above;
	return MOSpawnClearance::TraceVoxelGround(GetWorld(), At.X, At.Y, At.Z + 5000.0f, At.Z + 50.0f, GetOwner(), Above);
}

void UMOTerraformingComponent::NotifyOwner(const FText& Message) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (AMOPlayerController* PC = Pawn ? Cast<AMOPlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->ClientShowNotification(Message, 3.0f, EMONotificationType::Warning);
	}
}

#undef LOCTEXT_NAMESPACE
