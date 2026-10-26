// A creature's animation without an AnimBlueprint graph: a species is added as data (N-096, Wes Oct 26: "let's get ... some other
// animals going"). Its Blueprint sets the mesh, the capsule, AnimClass = MOCreatureAnimInstance and AMOCreature's IdleAnimation /
// WalkAnimation / RunAnimation; this blends them by the body's speed and plays the creature's cues (hit reactions, a bite: AMOCreature::
// PlayAnimationEverywhere) over them in "DefaultSlot", as an AnimBlueprint's slot node would. Deer and wolf keep their AnimBlueprints.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimTypes.h"
#include "MOCreatureAnimInstance.generated.h"

class UAnimSequence;
struct FPoseContext;

/** How much of each locomotion cycle shows at a speed, and how fast the run plays (pure: MOFramework.Creature.LocomotionBlend). */
struct MOFRAMEWORK_API FMOCreatureLocomotionBlend
{
	float Idle = 1.0f;
	float Walk = 0.0f;
	float Run = 0.0f;
	/** The run cycle's play rate past its own speed (clamped: beyond it the feet slide rather than blur). */
	float RunPlayRate = 1.0f;

	/** Standing still -> the walk cycle at its own speed -> the run cycle at its own, then the run faster up to MaxRunPlayRate. */
	static FMOCreatureLocomotionBlend ForSpeed(float SpeedCm, float WalkCycleSpeedCm, float RunCycleSpeedCm, float MaxRunPlayRate);
};

/** The creature's speed and cycles are copied on the game thread (PreUpdate); the blend advances and the pose is built on a worker. */
USTRUCT()
struct MOFRAMEWORK_API FMOCreatureAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FMOCreatureAnimInstanceProxy() = default;
	explicit FMOCreatureAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void EvaluateLocomotion(FPoseContext& Output) const;

	// The cycles (held by the creature class's defaults, so alive while it is), null when missing or for another skeleton; Sources are
	// the creature's own, so a rejected one is checked (and reported) once, not every frame.
	const UAnimSequence* Sources[3] = { nullptr, nullptr, nullptr };
	const UAnimSequence* IdleCycle = nullptr;
	const UAnimSequence* WalkCycle = nullptr;
	const UAnimSequence* RunCycle = nullptr;
	float WalkCycleSpeedCm = 100.0f;
	float RunCycleSpeedCm = 500.0f;
	float MaxRunPlayRate = 2.0f;
	float SpeedCm = 0.0f;

	FMOCreatureLocomotionBlend Blend;
	double IdleTime = 0.0;
	/** Where the walk and the run are in their stride (0..1), shared so a walk-to-run blend keeps the feet together. */
	float StridePhase = 0.0f;

	// The slot the cues play in (what FAnimNode_Slot keeps).
	FGraphTraversalCounter SlotRegistration;
	float SlotNodeWeight = 0.0f;
	float SlotSourceWeight = 1.0f;
	float SlotTotalNodeWeight = 0.0f;
};

UCLASS(Transient)
class MOFRAMEWORK_API UMOCreatureAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The slot the creature's cues play in: PlaySlotAnimationAsDynamicMontage(..., "DefaultSlot") and montages authored on it. */
	static const FName CueSlot;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FMOCreatureAnimInstanceProxy(this); }
};
