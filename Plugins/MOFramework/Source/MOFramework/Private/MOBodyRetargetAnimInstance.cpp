#include "MOBodyRetargetAnimInstance.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
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

void FMOBodyRetargetAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UMOBodyRetargetAnimInstance* Instance = Cast<UMOBodyRetargetAnimInstance>(InAnimInstance))
	{
		bClimbIK = Instance->bClimbIK;
		for (int32 i = 0; i < 4; ++i)
		{
			ClimbLimbs[i] = Instance->ClimbLimbs[i];
		}
	}
}

void FMOBodyRetargetAnimInstanceProxy::ApplyClimbIK(FPoseContext& Output)
{
	if (!bClimbIK)
	{
		return;
	}
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	FCSPose<FCompactPose> Pose;
	Pose.InitPose(Output.Pose);
	bool bChanged = false;
	for (int32 i = 0; i < 4; ++i)
	{
		const FMOClimbLimbIK& Limb = ClimbLimbs[i];
		const int32 U = Bones.GetPoseBoneIndexForBoneName(Limb.Upper);
		const int32 L = Bones.GetPoseBoneIndexForBoneName(Limb.Lower);
		const int32 E = Bones.GetPoseBoneIndexForBoneName(Limb.End);
		if (U == INDEX_NONE || L == INDEX_NONE || E == INDEX_NONE)
		{
			continue;
		}
		const FCompactPoseBoneIndex UI = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(U));
		const FCompactPoseBoneIndex LI = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(L));
		const FCompactPoseBoneIndex EI = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(E));
		if (!UI.IsValid() || !LI.IsValid() || !EI.IsValid())
		{
			continue;
		}
		FTransform Upper = Pose.GetComponentSpaceTransform(UI);
		FTransform Lower = Pose.GetComponentSpaceTransform(LI);
		FTransform End = Pose.GetComponentSpaceTransform(EI);
		AnimatedEndCS[i] = End.GetLocation();   // where the clip put it: the next sweep starts here
		if (Limb.Alpha <= ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}
		// The elbow / knee keeps the clip's bend: pushed out from the limb's midline; a straight limb bends the way the hint says.
		const FVector Mid = 0.5f * (Upper.GetLocation() + End.GetLocation());
		const FVector Out = Lower.GetLocation() - Mid;
		const FVector JointTarget = Lower.GetLocation() + (Out.SizeSquared() > 4.0f ? Out.GetSafeNormal() : Limb.BendHintCS) * 30.0f;
		AnimationCore::SolveTwoBoneIK(Upper, Lower, End, JointTarget, Limb.TargetCS, false, 1.0, 1.0);
		const FBoneTransform Chain[] = {FBoneTransform(UI, Upper), FBoneTransform(LI, Lower), FBoneTransform(EI, End)};   // root to tip: sorted
		Pose.LocalBlendCSBoneTransforms(Chain, Limb.Alpha);
		bChanged = true;
	}
	if (bChanged)
	{
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
	}
}

void FMOBodyRetargetAnimInstanceProxy::LimitAnkles(FPoseContext& Output) const
{
	static const FName Feet[] = {TEXT("foot_l"), TEXT("foot_r")};
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	for (const FName& Foot : Feet)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Foot);
		const FCompactPoseBoneIndex Index = MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (!Index.IsValid())
		{
			continue;
		}
		// The foot's turn from neutral, in the calf's frame: flexion about Z (the twist), the rest as one swing.
		const FQuat Neutral = Output.Pose.GetRefPose(Index).GetRotation();
		FTransform& Local = Output.Pose[Index];
		FQuat Swing, Twist;
		(Local.GetRotation() * Neutral.Inverse()).GetNormalized().ToSwingTwist(FVector::ZAxisVector, Swing, Twist);
		const float Flex = FMath::RadiansToDegrees(Twist.GetTwistAngle(FVector::ZAxisVector));
		const float FlexLimited = FMath::Clamp(Flex, -AnkleDorsiflexionLimitDeg, AnklePlantarflexionLimitDeg);
		if (Swing.W < 0.0f)
		{
			Swing = FQuat(-Swing.X, -Swing.Y, -Swing.Z, -Swing.W);
		}
		FVector SwingAxis;
		float SwingRad;
		Swing.ToAxisAndAngle(SwingAxis, SwingRad);
		const float RollLimitRad = FMath::DegreesToRadians(AnkleRollLimitDeg);
		if (FlexLimited == Flex && SwingRad <= RollLimitRad)
		{
			continue;   // inside the range: the pose as animated
		}
		const FQuat SwingLimited = SwingRad > RollLimitRad ? FQuat(SwingAxis, RollLimitRad) : Swing;
		Local.SetRotation((SwingLimited * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(FlexLimited)) * Neutral).GetNormalized());
	}
}

bool FMOBodyRetargetAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		RetargetNode->Evaluate_AnyThread(Output);
		ApplyClimbIK(Output);
		LimitAnkles(Output);
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
	ApplyClimbIK(Output);
	LimitAnkles(Output);
	return true;
}

UMOBodyRetargetAnimInstance::UMOBodyRetargetAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RetargetNode.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
	// The climbing limbs (MetaHuman bones; Titan Climbing's contact curves on the climbing set): hands, then feet.
	ClimbLimbs[0] = {TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("c_lefthand"), 4.0f};
	ClimbLimbs[1] = {TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("c_righthand"), 4.0f};
	ClimbLimbs[2] = {TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("c_leftfoot"), 8.0f};
	ClimbLimbs[3] = {TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("c_rightfoot"), 8.0f};
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
	UpdateClimbIK(DeltaSeconds);
}

void UMOBodyRetargetAnimInstance::UpdateClimbIK(float DeltaSeconds)
{
	const AMOCharacter* Owner = Cast<AMOCharacter>(GetOwningActor());
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const UMOCharacterMovementComponent* Move = Owner ? Cast<UMOCharacterMovementComponent>(Owner->GetCharacterMovement()) : nullptr;
	const UWorld* World = GetWorld();
	const bool bClimbing = Mesh && Move && World && Move->IsClimbingAsSeen();
	const FMOBodyRetargetAnimInstanceProxy& Proxy = GetProxyOnGameThread<FMOBodyRetargetAnimInstanceProxy>();

	// The face's outward normal, near enough: the climber faces the rock (every machine has the facing; the movement's own normal is the owner's
	// and the server's only).
	const FVector Out = Owner ? -Owner->GetActorForwardVector() : FVector::ForwardVector;
	const FTransform ToWorld = Mesh ? Mesh->GetComponentTransform() : FTransform::Identity;
	bool bAny = false;
	for (int32 i = 0; i < 4; ++i)
	{
		FLimbHold& Hold = LimbHolds[i];
		FMOClimbLimbIK& Limb = ClimbLimbs[i];
		const float Contact = bClimbing ? GetCurveValue(Limb.ContactCurve) : 0.0f;
		if (Contact < ClimbIKTraceThreshold)
		{
			Hold.bHeld = Hold.bLocked = false;   // in the air: the clip's own reach
		}
		else if (!Hold.bLocked)
		{
			// Reaching: sweep for the rock from where the clip puts the hand / foot, along the face's normal.
			const FVector From = ToWorld.TransformPosition(Proxy.AnimatedEndCS[i]);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(MOClimbLimbIK), false, Owner);
			FHitResult Hit;
			Hold.bHeld = World->SweepSingleByChannel(Hit, From + Out * 30.0f, From - Out * 45.0f, FQuat::Identity, ECC_Visibility,
				FCollisionShape::MakeSphere(4.0f), Params) && Hit.GetComponent();
			if (Hold.bHeld)
			{
				Hold.On = Hit.GetComponent();
				Hold.LocalPoint = Hit.GetComponent()->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint + Hit.ImpactNormal * Limb.SurfaceOffset);
				Hold.bLocked = Contact >= ClimbIKLockThreshold;   // on its hold now: it stays there until the clip lifts it
			}
		}
		const UPrimitiveComponent* On = Hold.On.Get();
		if (Hold.bHeld && !On)
		{
			Hold.bHeld = Hold.bLocked = false;   // the thing it held is gone (harvested, streamed out)
		}
		Hold.Alpha = FMath::FInterpTo(Hold.Alpha, Hold.bHeld ? 1.0f : 0.0f, DeltaSeconds, 15.0f);
		Limb.Alpha = Hold.Alpha;
		if (On)
		{
			Limb.TargetCS = ToWorld.InverseTransformPosition(On->GetComponentTransform().TransformPosition(Hold.LocalPoint));
		}
		// Elbows bend down, knees toward the rock, when the clip's own limb is straight.
		Limb.BendHintCS = ToWorld.InverseTransformVectorNoScale(i < 2 ? FVector::DownVector : -Out);
		bAny |= Limb.Alpha > ZERO_ANIMWEIGHT_THRESH;
	}
	bClimbIK = bClimbing || bAny;   // the proxy measures the clip's limbs while climbing (the sweeps start there) and blends the holds out after
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
