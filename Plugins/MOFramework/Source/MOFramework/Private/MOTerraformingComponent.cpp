#include "MOTerraformingComponent.h"
#include "MOFramework.h"
#include "MOViewpointUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Voxel plugin includes
#include "Sculpt/Height/VoxelHeightSculptActor.h"
#include "Sculpt/Height/VoxelHeightSculptBlueprintLibrary.h"
#include "Sculpt/Volume/VoxelVolumeSculptActor.h"
#include "Sculpt/Volume/VoxelVolumeSculptBlueprintLibrary.h"
#include "Sculpt/VoxelSculptMode.h"
#include "Sculpt/VoxelLevelToolType.h"

UMOTerraformingComponent::UMOTerraformingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOTerraformingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find sculpt actors if not set
	if (!HeightSculptActor.IsValid() && !VolumeSculptActor.IsValid())
	{
		AutoFindSculptActors();
	}
}

void UMOTerraformingComponent::AutoFindSculptActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find height sculpt actor
	TArray<AActor*> HeightActors;
	UGameplayStatics::GetAllActorsOfClass(World, AVoxelHeightSculptActor::StaticClass(), HeightActors);
	if (HeightActors.Num() > 0)
	{
		HeightSculptActor = Cast<AVoxelHeightSculptActor>(HeightActors[0]);
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Auto-found HeightSculptActor: %s"), *HeightActors[0]->GetName());
	}

	// Find volume sculpt actor
	TArray<AActor*> VolumeActors;
	UGameplayStatics::GetAllActorsOfClass(World, AVoxelVolumeSculptActor::StaticClass(), VolumeActors);
	if (VolumeActors.Num() > 0)
	{
		VolumeSculptActor = Cast<AVoxelVolumeSculptActor>(VolumeActors[0]);
		UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Auto-found VolumeSculptActor: %s"), *VolumeActors[0]->GetName());
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
	if (!HasValidSculptActor())
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
			const float TargetHeight = Config.bUseFlattenHitHeight ? WorldLocation.Z : Config.FlattenTargetHeight;
			return Flatten(WorldLocation, TargetHeight);
		}
	case EMOTerraformMode::Smooth:
		return Smooth(WorldLocation);
	default:
		return false;
	}
}

bool UMOTerraformingComponent::Dig(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Dig at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), Config.Radius, Config.Strength);

	if (bUseVolumeSculpting)
	{
		return VolumeDig(WorldLocation);
	}
	return HeightDig(FVector2D(WorldLocation.X, WorldLocation.Y));
}

bool UMOTerraformingComponent::Raise(const FVector& WorldLocation)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerraforming] Raise at %s (Radius: %.1f, Strength: %.2f)"),
		*WorldLocation.ToString(), Config.Radius, Config.Strength);

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
		*WorldLocation.ToString(), Config.Radius, Config.Strength);

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
	AVoxelHeightSculptActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelHeightSculptBlueprintLibrary::SculptHeight(
		Actor,
		Location,
		Config.Radius,
		Config.Strength,
		EVoxelSculptMode::Remove
	);

	return true;
}

bool UMOTerraformingComponent::HeightRaise(const FVector2D& Location)
{
	AVoxelHeightSculptActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelHeightSculptBlueprintLibrary::SculptHeight(
		Actor,
		Location,
		Config.Radius,
		Config.Strength,
		EVoxelSculptMode::Add
	);

	return true;
}

bool UMOTerraformingComponent::HeightFlatten(const FVector2D& Location, float TargetHeight)
{
	AVoxelHeightSculptActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelHeightSculptBlueprintLibrary::Flatten(
		Actor,
		Location,
		Config.Radius,
		Config.Falloff,
		EVoxelLevelToolType::Both,
		TargetHeight
	);

	return true;
}

bool UMOTerraformingComponent::HeightSmooth(const FVector2D& Location)
{
	AVoxelHeightSculptActor* Actor = HeightSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelHeightSculptBlueprintLibrary::Smooth(
		Actor,
		Location,
		Config.Radius,
		Config.Strength
	);

	return true;
}

// ============================================================================
// VOLUME SCULPTING OPERATIONS
// ============================================================================

bool UMOTerraformingComponent::VolumeDig(const FVector& Location)
{
	AVoxelVolumeSculptActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
		Actor,
		Location,
		Config.Radius,
		EVoxelSculptMode::Remove,
		0.0f  // Smoothness
	);

	return true;
}

bool UMOTerraformingComponent::VolumeRaise(const FVector& Location)
{
	AVoxelVolumeSculptActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
		Actor,
		Location,
		Config.Radius,
		EVoxelSculptMode::Add,
		0.0f  // Smoothness
	);

	return true;
}

bool UMOTerraformingComponent::VolumeFlatten(const FVector& Location, const FVector& Normal, float TargetHeight)
{
	AVoxelVolumeSculptActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelVolumeSculptBlueprintLibrary::Flatten(
		Actor,
		Location,
		Normal,
		Config.Radius,
		Config.Radius * 2.0f,  // Height
		Config.Falloff,
		EVoxelLevelToolType::Both
	);

	return true;
}

bool UMOTerraformingComponent::VolumeSmooth(const FVector& Location)
{
	AVoxelVolumeSculptActor* Actor = VolumeSculptActor.Get();
	if (!IsValid(Actor))
	{
		return false;
	}

	UVoxelVolumeSculptBlueprintLibrary::Smooth(
		Actor,
		Location,
		Config.Radius,
		Config.Strength
	);

	return true;
}

// ============================================================================
// MODE MANAGEMENT
// ============================================================================

void UMOTerraformingComponent::CycleMode()
{
	EMOTerraformMode OldMode = CurrentMode;
	const int32 CurrentIndex = static_cast<int32>(CurrentMode);
	const int32 NextIndex = (CurrentIndex + 1) % 4;
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
	default:
		return NSLOCTEXT("MO", "TerraformUnknown", "Unknown");
	}
}
