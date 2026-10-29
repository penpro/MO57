// The MetaHuman body's animation while Epic's Game Animation Sample drives the locomotion (GASP phase 1, Oct 27 2026; Docs/Animation_GASP.md).
// The owner's hidden locomotion driver (AMOCharacter::GetLocomotionDriver: a UEFN mannequin running GASP's motion-matching blueprint) is retargeted
// onto the body (AMOCharacter::GetBodyRetargeter), then "DefaultSlot" plays over it, so a montage on the body's own skeleton -- an attack, a swim
// stroke -- still shows over the retargeted walk. Native rather than GASP's ABP_GenericRetarget: that blueprint has no slot and finds its source by
// attachment. The retarget is the engine's node run as this instance's graph (the pattern of IKRigEditor's UIKRetargetAnimInstance); the slot is
// kept by hand as UMOCreatureAnimInstance keeps it, not by FAnimNode_Slot, whose 5.8 Sequencer-mixer hook (MovieSceneAnimMixer, on for GASP)
// asserts that the instance has a blueprint's anim subsystems.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "MOBodyRetargetAnimInstance.generated.h"

/** A climber's hand or foot on the rock (Wes, Oct 29: "we can probably reuse the hand/foot IK rigging to follow the cliff"): Titan Climbing's
 *  approach on our own climb. The climbing clips carry a contact curve per limb (c_lefthand ...): rising past ClimbIKTraceThreshold the limb reaches
 *  for the rock (a sweep from where the clip puts it), past ClimbIKLockThreshold it stays on that hold, kept on the thing it touched; the chain
 *  bends to it by two-bone IK after the slot. Every machine works out its own (the position, the climbing mode and the clips replicate; where a
 *  hand lands is presentation, never movement). */
struct FMOClimbLimbIK
{
	FName Upper;            // the chain (the MetaHuman body's bones)
	FName Lower;
	FName End;
	FName ContactCurve;     // the clip's contact curve
	float SurfaceOffset;    // how far off the rock the end bone sits (the palm, the ball of the foot), cm
	FVector TargetCS = FVector::ZeroVector;   // where the end bone goes (component space)
	FVector BendHintCS = FVector::ZeroVector; // which way the elbow / knee bends when the clip's limb is straight (component space)
	float Alpha = 0.0f;
};

/** The retarget node is the graph's root; the slot blends the body's montages over what it produces; then a climber's hands and feet go onto the rock. */
USTRUCT()
struct MOFRAMEWORK_API FMOBodyRetargetAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FMOBodyRetargetAnimInstanceProxy() = default;
	FMOBodyRetargetAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode)
		: FAnimInstanceProxy(InAnimInstance), RetargetNode(InRetargetNode) {}

	/** The climbing limbs, written by the instance before each update (PreUpdate); and where the clip itself put each end bone in the last
	 *  evaluation, before the IK (component space), which the next update's sweeps start from. */
	FMOClimbLimbIK ClimbLimbs[4];
	FVector AnimatedEndCS[4] = {FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector};
	bool bClimbIK = false;

protected:
	virtual FAnimNode_Base* GetCustomRootNode() override { return RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RetargetNode); }   // its PreUpdate copies the driver's pose
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	/** The climbing limbs onto their holds (component space, after the slot). */
	void ApplyClimbIK(FPoseContext& Output);

	FAnimNode_RetargetPoseFromMesh* RetargetNode = nullptr;

	// The montage slot (what FAnimNode_Slot keeps).
	FGraphTraversalCounter SlotRegistration;
	float SlotNodeWeight = 0.0f;
	float SlotSourceWeight = 1.0f;
	float SlotTotalNodeWeight = 0.0f;
};

UCLASS(Transient, NotBlueprintable)
class MOFRAMEWORK_API UMOBodyRetargetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMOBodyRetargetAnimInstance(const FObjectInitializer& ObjectInitializer);

	/** The slot the body's montages play in (the locomotion blueprint's slot before GASP: combat, swim). */
	static const FName MontageSlot;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FMOBodyRetargetAnimInstanceProxy(this, &RetargetNode); }
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Titan Climbing's rig profile defaults: a limb reaches for the rock past the first, stays on its hold past the second. */
	static constexpr float ClimbIKTraceThreshold = 0.6f;
	static constexpr float ClimbIKLockThreshold = 0.95f;

private:
	/** Point the retarget at the owner's driver and retargeter. False while the owner has none (the body then holds its reference pose). */
	bool BindToOwnerDriver();

	/** The climbing limbs' holds (game thread, every update): read the contact curves, sweep for the rock, keep the locked holds. */
	void UpdateClimbIK(float DeltaSeconds);

	/** One limb's hold on the game thread: the thing it touched and where on it (its own space, so a hold stays on a moving thing). */
	struct FLimbHold
	{
		TWeakObjectPtr<UPrimitiveComponent> On;
		FVector LocalPoint = FVector::ZeroVector;
		bool bHeld = false;
		bool bLocked = false;
		float Alpha = 0.0f;
	};
	FLimbHold LimbHolds[4];
	FMOClimbLimbIK ClimbLimbs[4];
	bool bClimbIK = false;

	friend struct FMOBodyRetargetAnimInstanceProxy;

	// On the instance (UPROPERTY: the retargeter it holds stays referenced), run by the proxy, as UIKRetargetAnimInstance does.
	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;
};
