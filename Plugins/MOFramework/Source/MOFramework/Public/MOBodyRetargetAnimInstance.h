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
#include "MOItemDefinitionRow.h"
#include "MOBodyRetargetAnimInstance.generated.h"

/** A climber's hand or foot on the rock (Wes, Oct 29: "we can probably reuse the hand/foot IK rigging to follow the cliff"): Titan Climbing's
 *  approach on our own climb. The climbing clips carry a contact curve per limb (c_lefthand ...): rising past ClimbIKTraceThreshold a hand reaches
 *  for the rock (a sweep from where the clip puts it), past ClimbIKLockThreshold it stays on that hold, kept on the thing it touched. A foot finds
 *  its own rock and steps when the clip has moved on (ClimbFootStepCm). The chain bends to its hold by two-bone IK after the slot. Every machine
 *  works out its own (the position, the climbing mode and the clips replicate; where a hand lands is presentation, never movement). */
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

/** A hand closing on what it holds (Nov 2 2026, Wes: "her fingers are back to being all fucked up"; Docs/Held_Grip.md): the held item's
 *  handle (AMOCharacter::FHandGrip: the item's GripTransform on its mesh, GripHalfSize, GripStyle) in the hand bone's space, and the fist that
 *  sets which way each finger joint flexes (the item's GripPose, its left hand's finger joints). Each finger closes as a tendon closes it
 *  (FMOHandGrip::Wrap): all its joints together from the open hand (the reference pose) toward full flexion, until a phalanx touches the
 *  handle; the joints up to that phalanx stop, the ones past it go on. The animation's own fingers are replaced, so the hand holds it in every
 *  clip (a walk's loose hand too).
 *  A bow also limits the other hand's draw: drawn (nocked) past the bow's full draw -- a longer arm than the one it was built for: Harvey's
 *  draw is 85 cm, the bows' 64 -- the drawing hand stops there (two-bone IK along the draw), as an archer short-draws a bow too short for him. */
struct FMOHandGripIK
{
	FTransform HandleInHand = FTransform::Identity;
	FVector HalfSize = FVector::ZeroVector;
	EMOGripStyle Style = EMOGripStyle::None;
	float HandScale = 1.0f;   // the hand's knuckle span over FMOHandGrip::RefSpanCm: its fingers' thickness
	FQuat Fist[15];           // thumb, index, middle, ring, pinky x the 01 / 02 / 03 joints: local rotations
	float Alpha = 0.0f;       // 0 the animation's fingers .. 1 the grip (blends over GripBlendSeconds)
	FTransform ItemInHand = FTransform::Identity;   // a bow: the bow (its component, unscaled) in the hand bone's space
	FVector DrawAxis = FVector::YAxisVector;        // ... its draw, bow space (UMOBowMeshComponent)
	float MaxDrawCm = 0.0f;                         // ... its full draw (0: not a bow)
	bool bDrawing = false;                          // ... the string is in the other hand's fingers

	FMOHandGripIK()
	{
		for (FQuat& Q : Fist)
		{
			Q = FQuat::Identity;
		}
	}
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

	/** Each hand's grip ([0] left, [1] right), written by the instance before each update. */
	FMOHandGripIK HandGrips[2];

protected:
	virtual FAnimNode_Base* GetCustomRootNode() override { return RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RetargetNode); }   // its PreUpdate copies the driver's pose
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	/**
	 * A human ankle's range, from the foot's reference pose (anatomical neutral: standing on flat ground the feet sit 5-20 deg from it) in the calf's
	 * frame, whose Z is the ankle's flexion axis (a flat-ground crouch turns both feet about it alone, -42 deg; negative = toes toward the shin). The
	 * rest of the foot's turn -- rolling onto its edge, turning about the shin -- is one cone. GASP's foot placement turns the foot to the ground it
	 * stands on with no limit, so on a rock's slope it rolled the foot 56-65 deg onto its side (BR31-6, Oct 30 2026: "my ankle is bending like a
	 * twig"; a human foot inverts ~35 and everts ~20 deg). Crouching takes 42-58 deg of dorsiflexion on the ground round that rock, hence 60 (a
	 * weight-bearing deep squat; the textbook 20 is the unloaded range). Measured on Trey; every base shares the skeleton.
	 */
	static constexpr float AnkleDorsiflexionLimitDeg = 60.0f;
	static constexpr float AnklePlantarflexionLimitDeg = 50.0f;
	static constexpr float AnkleRollLimitDeg = 30.0f;

private:
	/** The climbing limbs onto their holds (component space, after the slot). */
	void ApplyClimbIK(FPoseContext& Output);

	/** Each foot back inside the ankle's range (local space, last: after the retarget, the slot and the climbing IK). */
	void LimitAnkles(FPoseContext& Output) const;

	/** The holding hands' fingers round their handles (local space, after the slot and the climbing IK). */
	void ApplyHandGrips(FPoseContext& Output) const;

	/** A drawn bow's drawing hand kept within the bow's full draw (after the grips). */
	void LimitDraw(FPoseContext& Output) const;

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

	/** Titan Climbing's rig profile defaults: a hand reaches for the rock past the first, stays on its hold past the second. */
	static constexpr float ClimbIKTraceThreshold = 0.6f;
	static constexpr float ClimbIKLockThreshold = 0.95f;

	/** A foot finds its own rock (Wes, Oct 31: "get the foot ik's to land on geometry for climbing instead of just keeping them lined up with the
	 *  hands"), not by the clip's contact curves (the down and side clips have none): it holds a point on the rock until the clip has moved that
	 *  foot ClimbFootStepCm away across the face, then steps to the rock in front of where the clip has it now. The sweep starts ClimbFootOutCm out
	 *  from the clip's foot (rock bulging out at the feet) and reaches ClimbFootReachCm in (rock standing back under a ledge). */
	static constexpr float ClimbFootStepCm = 25.0f;
	static constexpr float ClimbFootOutCm = 40.0f;
	static constexpr float ClimbFootReachCm = 70.0f;
	/** How fast a limb's target follows a new hold (1/s): a step to new rock takes ~0.15 s instead of a jump. */
	static constexpr float ClimbHoldFollowRate = 20.0f;

	/** A hand closes on a newly held item, or lets its fingers go back to the animation, over this long. */
	static constexpr float GripBlendSeconds = 0.15f;

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
		FVector Target = FVector::ZeroVector;   // where the limb is going, world space: follows the hold, so a step slides there quickly
		bool bHeld = false;
		bool bLocked = false;
		float Alpha = 0.0f;
	};
	FLimbHold LimbHolds[4];
	FMOClimbLimbIK ClimbLimbs[4];

	/** The hands' grips (game thread, every update): the owner's held handles in the hand bones' space, the fists sampled once per pose. */
	void UpdateHandGrips(float DeltaSeconds);
	FMOHandGripIK HandGrips[2];
	TWeakObjectPtr<const UAnimSequence> SampledFist[2];
	float SampledFistTime[2] = {-1.0f, -1.0f};

	/** A foot on the rock: keeps its hold until the clip has moved it ClimbFootStepCm across the face, then finds rock in front of the clip's foot. */
	void UpdateClimbFoot(FLimbHold& Hold, const FMOClimbLimbIK& Limb, bool bClimbing, const FVector& ClipFoot, const FVector& Out, const AActor* Owner) const;
	bool bClimbIK = false;

	friend struct FMOBodyRetargetAnimInstanceProxy;

	// On the instance (UPROPERTY: the retargeter it holds stays referenced), run by the proxy, as UIKRetargetAnimInstance does.
	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;
};
