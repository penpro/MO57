// "Where is the water" for gameplay (swimming, floating objects, later boats): the generator's open-water surfaces, queried by world position.
//
// The visible water is generated, not placed: the infinite ocean mesh fills everything under SeaLevelCm (the sea, rivers, ponds, sea-level lake basins and any pit dug below the plane) and each upland
// lake is a plane over its ellipse at its own surface height. FMOWorldGen::GetWaterSurfaceCm answers with exactly those surfaces' STILL level (GetStillSurfaceZ). The water is drawn with
// Oceanology's Gerstner waves on top of that level, so GetSurfaceZ adds the waves drawn there now (GetWaveOffsetCm): the sea's swell heaves the drawn ocean ~+-60 cm, and a swimmer
// floating on the still level hung in air in its troughs and went under at its crests (BR31-14, Oct 31 2026). Gameplay and visuals cannot disagree.
//
// The swim volume: UCharacterMovementComponent decides "in water" from its APhysicsVolume (bWaterVolume). The world's placed volumes cannot describe generated water, so this subsystem owns ONE brushless
// APhysicsVolume with bWaterVolume set, which UMOCharacterMovementComponent::GetPhysicsVolume returns while a character swims (nothing else ever overlaps it: it has no shape).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldGen.h"
#include "MOWaterSubsystem.generated.h"

class APhysicsVolume;
class AOceanologyWaterParent;
class AOceanologyInfiniteOcean;

UCLASS()
class MOFRAMEWORK_API UMOWaterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	static UMOWaterSubsystem* Get(const UObject* WorldContext);

	/**
	 * The open-water surface height (cm) at X, Y as the water is drawn NOW: the still level plus the waves (GetWaveOffsetCm). False when there is no active world seed or water is off (the main
	 * menu, a world without a generator). Cheap (one Gerstner sum a few times over): safe to call every movement update. Off the game thread it answers the still level (the water actors are
	 * game-thread objects).
	 */
	bool GetSurfaceZ(double X, double Y, float& OutSurfaceZ) const;

	/** The still water level (cm) at X, Y -- the sea plane or an upland lake's surface, no waves: for a decision that must not change with the waves (where a pawn may spawn). Any thread. */
	bool GetStillSurfaceZ(double X, double Y, float& OutSurfaceZ) const;

	/**
	 * How far the drawn waves lift (+) or drop (-) the water over X, Y now (cm): the waves of the Oceanology body drawn there (the infinite ocean over the sea, the lake's own actor over an
	 * upland lake), at the wave clock it is drawn with (the server's clock: host and joiners draw the same waves). 0 off the game thread, or with no body there (a world without an ocean,
	 * a lake not spawned yet).
	 */
	float GetWaveOffsetCm(double X, double Y, float StillSurfaceZ) const;

	/**
	 * The height of the water over (X, Y) given the waves' displacement of each still point (Displacement: a still point's (x, y) -> its sideways and up moves). A Gerstner wave moves each
	 * point of the surface sideways as well as up, so the water over (X, Y) is not the point that started there but the one that started at (X, Y) minus its own sideways move: found by
	 * fixed-point steps (each shrinks the error by the waves' steepness), then its height. Pure; MOFramework.Water.WaveHeightOver checks it against a brute-force search.
	 */
	static float WaveHeightOver(double X, double Y, TFunctionRef<FVector(const FVector&)> Displacement);
	static constexpr int32 WaveInversionSteps = 4;

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

	/**
	 * The world's one shared brushless water volume (bWaterVolume, a high fluid friction so a swimmer's motion damps like a swimmer's). Spawned on the game thread when the
	 * world begins play (or on a game-thread call before that); null if it cannot be spawned. Never spawns off the game thread: the movement component's GetPhysicsVolume
	 * reaches here from GetGravityZ, which GASP's trajectory (PoseSearchTrajectoryLibrary) calls from the animation worker threads -- two swimmers' graphs spawning it at
	 * once crashed a joiner on the second spawn's name (Oct 28).
	 */
	APhysicsVolume* GetSwimVolume();

private:
	/** The Oceanology body whose water is drawn over (X, Y) at that still level: the lake's actor over an upland lake, else the infinite ocean. Game thread. */
	AOceanologyWaterParent* FindWaterBody(double X, double Y, float StillSurfaceZ) const;

	UPROPERTY(Transient)
	TObjectPtr<APhysicsVolume> SwimVolume;

	/** The level's infinite ocean, found on first need (and looked for again at most once a second while there is none). */
	mutable TWeakObjectPtr<AOceanologyInfiniteOcean> Ocean;
	mutable double NextOceanSearchSeconds = 0.0;
};
