/**
 * =============================================================================
 * MOMenuFlyby.h - The main menu's living backdrop: a slow camera flight over the generated coast
 * =============================================================================
 *
 * PURPOSE:
 * The main menu used to be an empty level. LoadingLevel (the menu map) now holds the same terrain stack as the game level (voxel world + the MO terrain graph, sky, water,
 * PCG scatter) and ONE of these actors. On BeginPlay it
 *   1. gives the backdrop world a seed (UMOWorldSeedSubsystem::ApplyBackdropSeed: the terrain is generated and scattered exactly as in a game with that seed),
 *   2. asks the world generator for a good coast of that seed (the same search a new game uses) and lays a closed loop over it: half over the land, half out over the sea,
 *   3. becomes the player's view target and flies the loop at walking-jog speed, keeping a fixed clearance over the ground (the generator knows the ground, so there is
 *      nothing to hand-tune per seed), looking ahead and a little toward the sea.
 * Nothing here is gameplay: starting a game travels to another level and the actor goes with the menu world.
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * - The menu level must contain the terrain actors (VoxelWorld, the terrain stamps, PCGVolume, sky, water). Without a VoxelWorld the camera flies over nothing and the log says so.
 * - The menu's player controller may set its own view target; Tick re-asserts ours every 2 s.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOMenuFlyby.generated.h"

class UCameraComponent;
class USplineComponent;

UCLASS(Blueprintable)
class MOFRAMEWORK_API AMOMenuFlyby : public AActor
{
	GENERATED_BODY()

public:
	AMOMenuFlyby();

	/** The world seed the backdrop terrain is generated from (any seed works; this one has a coast worth flying over). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MO|Menu")
	int32 BackdropWorldSeed = 12345;

	/** The Voxel graph's seed parameter name (the same one the game uses). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MO|Menu")
	FName VoxelSeedParameterName = TEXT("Seed");

	/** Flight speed (cm/s). 900 = 9 m/s: a lap of the default loop takes about ten minutes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MO|Menu", meta = (ClampMin = "100"))
	float SpeedCmPerSecond = 900.0f;

	/** Height over the ground (or the sea) the camera keeps (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MO|Menu", meta = (ClampMin = "500"))
	float ClearanceCm = 7000.0f;

	/** The loop is about this long and wide (cm), laid over the coast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MO|Menu", meta = (ClampMin = "100000"))
	float LoopSizeCm = 400000.0f;

	/** How far the view turns from the direction of flight toward the sea (0 = straight ahead, 1 = at the sea). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MO|Menu", meta = (ClampMin = "0", ClampMax = "1"))
	float LookTowardSea = 0.6f;

	/** Camera pitch (degrees, negative = down). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MO|Menu")
	float PitchDegrees = -7.0f;

	/** Where the camera is along the loop right now (cm), for tests and tools. */
	float GetDistanceAlongLoop() const { return DistanceAlongLoop; }

	/** True once the loop is laid (the seed has a coast, or the fallback loop is used). */
	bool HasLoop() const { return bLoopReady; }

	/** Put the camera at this fraction (0..1) of the lap. For tools and screenshots: `unreal.MOMenuFlyby.jump_to_fraction(0.3)` from the editor Python bridge. */
	UFUNCTION(BlueprintCallable, Category = "MO|Menu")
	void JumpToFraction(float Fraction);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildLoop();
	float GroundAt(const FVector2D& XY) const;

	UPROPERTY(VisibleAnywhere, Category = "MO|Menu")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "MO|Menu")
	TObjectPtr<USplineComponent> Loop;

	FVector2D SeaDirection = FVector2D(1.0, 0.0);
	FVector2D LoopCentre = FVector2D::ZeroVector;
	float DistanceAlongLoop = 0.0f;
	float SmoothedZ = 0.0f;
	float TimeSinceViewCheck = 0.0f;
	float Age = 0.0f;
	int32 TerrainSeed = 0;
	bool bLoopReady = false;
	bool bHaveZ = false;
};
