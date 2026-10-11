// The character movement component of every MO pawn: stock UCharacterMovementComponent plus WATER.
//
// The engine decides "swimming" from the APhysicsVolume a character overlaps; MO's water is generated (see UMOWaterSubsystem), so this component asks the water oracle instead:
//   - Immersion = the fraction of the capsule below the water surface at its position (feet .. feet + capsule height), refreshed at the start of every movement update (so a client replaying a
//     saved move gets the same answer as the server: the oracle is deterministic in the position).
//   - WADING: walking in water is slowed by the drag of the water (about half speed at full immersion).
//   - SWIMMING starts when the water reaches the chest (SwimEnterImmersion) and ends when the character can stand again (SwimExitImmersion; the gap is hysteresis so the mode does not flicker at the
//     chest line). While swimming GetPhysicsVolume() returns the world's shared water volume, which is what makes the engine's PhysSwimming keep the mode, apply fluid friction and buoyancy.
//   - BUOYANCY: stock swimming floats a body whose Buoyancy x Immersion reaches 1. A human with lungs full floats a little (Buoyancy 1.12: head and shoulders out of the water at rest); a heavy load
//     lowers Buoyancy (the owning character sets it) until the swimmer sinks.
//   - The capsule stays UPRIGHT while swimming (ShouldRemainVertical): immersion math and collision stay simple; the character mesh is what lies down (AMOCharacter).
//
// The pure functions (ComputeImmersion, ShouldSwim) are static so the automation tests exercise the decisions without a world.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MOCharacterMovementComponent.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UMOCharacterMovementComponent();

	// ---- pure decisions (tested) ----

	/** Fraction (0..1) of a capsule whose feet are at FeetZ and which is CapsuleHeight tall that lies under a water surface at SurfaceZ. */
	static float ComputeImmersion(float SurfaceZ, float FeetZ, float CapsuleHeight);

	/**
	 * Swim-mode decision with hysteresis: not swimming -> start at Immersion >= Enter; swimming -> stop only when the water has dropped below Exit AND there is walkable ground under the feet (bStandable).
	 * A swimmer at the surface with nothing to stand on stays a swimmer however little of the capsule is in the water, so holding the rise key at the surface cannot pop in and out of the mode.
	 */
	static bool ShouldSwim(bool bWasSwimming, float Immersion, float EnterFraction, float ExitFraction, bool bStandable);

	// ---- water state (valid after the first movement update) ----

	/** Fraction of the capsule under water right now (0 on dry land or when no water is known here). */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	float GetImmersion() const { return Immersion; }

	/** The water surface height under the character, if there is open water there. */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	bool GetWaterSurfaceZ(float& OutSurfaceZ) const
	{
		OutSurfaceZ = WaterSurfaceZ;
		return bWaterKnown;
	}

	/** True when the eyes are below the surface (the head is under: the airway is closed, the underwater look applies). */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	bool IsHeadUnderwater() const;

	/** Depth (cm) of the water over the character's feet; 0 when dry. */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	float GetWaterDepthCm() const;

	/** Effort scale on swimming speed (the character lowers it when exhausted, raises it for a hustle). 1 = the nominal MaxSwimSpeed. */
	UPROPERTY(BlueprintReadWrite, Category="MO|Water")
	float SwimSpeedScale = 1.0f;

	// ---- tuning ----

	/** The water reaches this fraction of the capsule (chest height is about 0.7) and the character starts to swim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="0.3", ClampMax="1"))
	float SwimEnterImmersion = 0.70f;

	/** Swimming ends below this fraction (the feet can reach the bottom; lower than the enter fraction = hysteresis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="0.1", ClampMax="0.95"))
	float SwimExitImmersion = 0.50f;

	/** Walking speed lost to the drag of the water at full immersion (0.55 = about half speed when chest deep). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="0", ClampMax="0.9"))
	float WadeDragAtFullImmersion = 0.55f;

	/**
	 * Buoyancy of an unloaded body (stock CMC: 1 = neutral at full immersion; a floating body settles where Buoyancy x Immersion = 1). 1.27 settles at 0.79 immersion: the eyes about 10 cm above the water,
	 * the pose of a person treading water (the capsule is 192 cm with the eyes at 0.83 of it). A heavier load lowers it until the swimmer sinks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="0", ClampMax="2"))
	float UnloadedBuoyancy = 1.27f;

	//~ Begin UCharacterMovementComponent interface
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual float ImmersionDepth() const override { return Immersion; }
	virtual bool IsInWater() const override { return Immersion > 0.0f; }
	virtual float GetMaxSpeed() const override;
	virtual bool ShouldRemainVertical() const override { return true; }
	virtual APhysicsVolume* GetPhysicsVolume() const override;
	//~ End UCharacterMovementComponent interface

private:
	void RefreshWater();

	float Immersion = 0.0f;
	float WaterSurfaceZ = 0.0f;
	bool bWaterKnown = false;
};
