// "Where is the water" for gameplay (swimming, floating objects, later boats): the generator's open-water surfaces, queried by world position.
//
// The visible water is generated, not placed: the infinite ocean mesh fills everything under SeaLevelCm (the sea, rivers, ponds, sea-level lake basins and any pit dug below the plane) and each upland
// lake is a plane over its ellipse at its own surface height. FMOWorldGen::GetWaterSurfaceCm answers with exactly those surfaces, so gameplay and visuals cannot disagree.
//
// The swim volume: UCharacterMovementComponent decides "in water" from its APhysicsVolume (bWaterVolume). The world's placed volumes cannot describe generated water, so this subsystem owns ONE brushless
// APhysicsVolume with bWaterVolume set, which UMOCharacterMovementComponent::GetPhysicsVolume returns while a character swims (nothing else ever overlaps it: it has no shape).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldGen.h"
#include "MOWaterSubsystem.generated.h"

class APhysicsVolume;

UCLASS()
class MOFRAMEWORK_API UMOWaterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	static UMOWaterSubsystem* Get(const UObject* WorldContext);

	/** The open-water surface height (cm) at X, Y. False when there is no active world seed or water is off (the main menu, a world without a generator). Cheap: safe to call every movement update. */
	bool GetSurfaceZ(double X, double Y, float& OutSurfaceZ) const;

	/** The generator's ground height at (X, Y) (what the voxel terrain converges to). False without a seed / generator. */
	bool GetGroundZ(double X, double Y, float& OutGroundZ) const;

	/** Which body of water (X, Y) is in (FMOWorldGen::ClassifyWaterBody): what drinking there does. None on land or without a generator. */
	EMOWaterBodyKind ClassifyWaterAt(double X, double Y) const;

	/**
	 * The first open water along a view ray (the crosshair): marches Start + Dir x t up to MaxCm (and no further than BlockedAtCm, where the caller's trace hit something
	 * solid -- water surfaces do not block world traces, the lake bed does) and returns the point where the ray meets a real water surface, with its kind. False if the ray
	 * reaches nothing but land first.
	 */
	bool FindWaterAlongRay(const FVector& Start, const FVector& Dir, float MaxCm, float BlockedAtCm, FVector& OutPoint, EMOWaterBodyKind& OutKind) const;

	/** The world's one shared brushless water volume (bWaterVolume, a high fluid friction so a swimmer's motion damps like a swimmer's). Spawned on first use; null if it cannot be spawned. */
	APhysicsVolume* GetSwimVolume();

private:
	UPROPERTY(Transient)
	TObjectPtr<APhysicsVolume> SwimVolume;
};
