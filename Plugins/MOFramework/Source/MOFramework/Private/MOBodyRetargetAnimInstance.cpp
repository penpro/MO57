#include "MOBodyRetargetAnimInstance.h"
#include "MOCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProcessor.h"

const FName UMOBodyRetargetAnimInstance::MontageSlot(TEXT("DefaultSlot"));

void FMOBodyRetargetAnimInstanceProxy::Update(float DeltaSeconds)
{
	// The montage slot, as FAnimNode_Slot keeps it: registered once per initialization, its weights read every update. (The retarget node
	// itself updates as the graph's root, after this.)
	if (!SlotRegistration.IsSynchronized_Counter(GetSlotNodeInitializationCounter()))
	{
		SlotRegistration.SynchronizeWith(GetSlotNodeInitializationCounter());
		RegisterSlotNodeWithAnimInstance(UMOBodyRetargetAnimInstance::MontageSlot);
	}
	GetSlotWeight(UMOBodyRetargetAnimInstance::MontageSlot, SlotNodeWeight, SlotSourceWeight, SlotTotalNodeWeight);
	UpdateSlotNodeWeight(UMOBodyRetargetAnimInstance::MontageSlot, SlotNodeWeight, 1.0f);
}

bool FMOBodyRetargetAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		RetargetNode->Evaluate_AnyThread(Output);
		return true;
	}
	// A montage is playing: the retargeted locomotion is its source pose (what shows where it blends in or out).
	FPoseContext Source(Output);
	if (SlotSourceWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		RetargetNode->Evaluate_AnyThread(Source);
	}
	else
	{
		Source.ResetToRefPose();
	}
	const FAnimationPoseData SourceData(Source);
	FAnimationPoseData OutData(Output);
	SlotEvaluatePose(UMOBodyRetargetAnimInstance::MontageSlot, SourceData, SlotSourceWeight, OutData, SlotNodeWeight, SlotTotalNodeWeight);
	return true;
}

UMOBodyRetargetAnimInstance::UMOBodyRetargetAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RetargetNode.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
}

void UMOBodyRetargetAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	// Before the graph initializes (UAnimInstance::InitializeAnimation runs this, then InitializeRootNode).
	BindToOwnerDriver();
}

void UMOBodyRetargetAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	// The driver can arrive after the instance (the body's blueprint initializes first) or be replaced.
	if (!RetargetNode.SourceMeshComponent.IsValid())
	{
		BindToOwnerDriver();
	}
}

bool UMOBodyRetargetAnimInstance::BindToOwnerDriver()
{
	const AMOCharacter* Owner = Cast<AMOCharacter>(GetOwningActor());
	USkeletalMeshComponent* Driver = Owner ? Owner->GetLocomotionDriver() : nullptr;
	UIKRetargeter* Retargeter = Owner ? Owner->GetBodyRetargeter() : nullptr;
	if (!Driver || !Retargeter)
	{
		return false;
	}
	RetargetNode.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
	RetargetNode.SourceMeshComponent = Driver;
	RetargetNode.IKRetargeterAsset = Retargeter;
	if (FIKRetargetProcessor* Processor = RetargetNode.GetRetargetProcessor())
	{
		Processor->SetNeedsInitialized();
	}
	return true;
}
