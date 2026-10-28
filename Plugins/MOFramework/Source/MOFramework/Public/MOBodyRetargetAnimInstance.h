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

/** The retarget node is the graph's root; the slot blends the body's montages over what it produces. */
USTRUCT()
struct MOFRAMEWORK_API FMOBodyRetargetAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FMOBodyRetargetAnimInstanceProxy() = default;
	FMOBodyRetargetAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode)
		: FAnimInstanceProxy(InAnimInstance), RetargetNode(InRetargetNode) {}

protected:
	virtual FAnimNode_Base* GetCustomRootNode() override { return RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RetargetNode); }   // its PreUpdate copies the driver's pose
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
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

private:
	/** Point the retarget at the owner's driver and retargeter. False while the owner has none (the body then holds its reference pose). */
	bool BindToOwnerDriver();

	// On the instance (UPROPERTY: the retargeter it holds stays referenced), run by the proxy, as UIKRetargetAnimInstance does.
	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;
};
