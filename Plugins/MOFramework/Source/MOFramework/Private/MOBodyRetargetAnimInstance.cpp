#include "MOBodyRetargetAnimInstance.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "BonePose.h"
#include "Animation/AnimSequence.h"
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
		HandGrips[0] = Instance->HandGrips[0];
		HandGrips[1] = Instance->HandGrips[1];
	}
}

const float FMOBodyRetargetAnimInstanceProxy::GripFingerRadii[5][3] = {
	{1.0f, 0.92f, 0.8f},     // thumb
	{0.85f, 0.78f, 0.68f},   // index
	{0.86f, 0.8f, 0.7f},     // middle
	{0.8f, 0.74f, 0.65f},    // ring
	{0.72f, 0.66f, 0.58f}};  // pinky

float FMOBodyRetargetAnimInstanceProxy::HandleClearance(const FTransform& HandleCS, const FVector& HalfSize, const FVector& PointCS, float Radius)
{
	const FVector P = HandleCS.InverseTransformPositionNoScale(PointCS);
	if (FMath::Abs(P.Z) > HalfSize.Z + 2.0f)
	{
		return 99.0f;   // nowhere near it
	}
	// Out from the side along the ray from the axis (an ellipse's radial distance: exact for a circle, close for these near-round handles)
	const float Rn = FMath::Sqrt(FMath::Square(P.X / HalfSize.X) + FMath::Square(P.Y / HalfSize.Y));
	const float Radial = FMath::Sqrt(P.X * P.X + P.Y * P.Y);
	float Out = Rn > UE_KINDA_SMALL_NUMBER ? Radial * (1.0f - 1.0f / Rn) : -FMath::Min(HalfSize.X, HalfSize.Y);
	if (FMath::Abs(P.Z) > HalfSize.Z)
	{
		Out = FMath::Max(Out, FMath::Abs(P.Z) - HalfSize.Z);   // past an end: a finger may lie over it
	}
	return Out - Radius;
}

void FMOBodyRetargetAnimInstanceProxy::WrapFinger(const FTransform& ParentCS, const FTransform OpenLocal[3], const FQuat FistLocal[3], const float Radii[3],
	const FTransform& HandleCS, const FVector& HalfSize, float OutBend[3])
{
	constexpr int32 Samples = 4;      // points along each phalanx past its base
	constexpr int32 Steps = 16;       // the sweep walked toward the fist for the first touch (a thin handle could be passed clean through)
	constexpr int32 Iterations = 10;  // then bisection inside that step: 1/16384 of the way from open to the fist

	// The joints for a bend (0 open .. 1 the fist per joint), the fingertip past the last; then a phalanx's clearance (k: 0 proximal .. 2 distal)
	auto Joints = [&](const float Bend[3], FVector Out[4])
	{
		FTransform T = ParentCS;
		for (int32 k = 0; k < 3; ++k)
		{
			T = FTransform(FQuat::Slerp(OpenLocal[k].GetRotation(), FistLocal[k], Bend[k]), OpenLocal[k].GetTranslation()) * T;
			Out[k] = T.GetLocation();
		}
		Out[3] = Out[2] + (Out[2] - Out[1]) * GripTipFraction;
	};
	auto Phalanx = [&](const FVector J[4], int32 k)
	{
		const float R0 = Radii[k];
		const float R1 = k < 2 ? Radii[k + 1] : Radii[2] * 0.9f;
		float Min = 99.0f;
		for (int32 i = 1; i <= Samples; ++i)
		{
			const float F = static_cast<float>(i) / Samples;
			Min = FMath::Min(Min, HandleClearance(HandleCS, HalfSize, FMath::Lerp(J[k], J[k + 1], F), FMath::Lerp(R0, R1, F)));
		}
		return Min;
	};
	auto ClearFrom = [&](const float Bend[3], int32 First)
	{
		FVector J[4];
		Joints(Bend, J);
		float Min = 99.0f;
		for (int32 k = First; k < 3; ++k)
		{
			Min = FMath::Min(Min, Phalanx(J, k));
		}
		return Min;
	};

	// A tendon closes it: the joints from Free on bend together; when a phalanx touches, it and every joint before it stop
	float Bend[3] = {0.0f, 0.0f, 0.0f};
	int32 Free = 0;
	while (Free < 3)
	{
		auto At = [&](float X, float Out[3])
		{
			for (int32 i = 0; i < 3; ++i)
			{
				Out[i] = i >= Free ? FMath::Max(Bend[i], X) : Bend[i];
			}
		};
		float Try[3];
		At(Bend[Free], Try);
		if (ClearFrom(Try, Free) < 0.0f)
		{
			break;   // already against it (the open hand runs into the handle: a seat to fix, not a finger)
		}
		// Walk the sweep for the first touch (testing only its end missed a handle the finger passes through and out of: Nov 2, the test)
		float Lo = Bend[Free], Hi = -1.0f;
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float X = FMath::Lerp(Bend[Free], 1.0f, static_cast<float>(Step) / Steps);
			At(X, Try);
			if (ClearFrom(Try, Free) < 0.0f)
			{
				Hi = X;
				break;
			}
			Lo = X;
		}
		if (Hi < 0.0f)
		{
			At(1.0f, Bend);   // closes all the way without touching: a thin handle
			break;
		}
		for (int32 It = 0; It < Iterations; ++It)
		{
			const float Mid = 0.5f * (Lo + Hi);
			At(Mid, Try);
			(ClearFrom(Try, Free) >= 0.0f ? Lo : Hi) = Mid;
		}
		At(Hi, Try);
		FVector J[4];
		Joints(Try, J);
		int32 Touching = 2;
		for (int32 k = Free; k < 3; ++k)
		{
			if (Phalanx(J, k) < 0.0f)
			{
				Touching = k;
				break;
			}
		}
		At(Lo, Bend);
		Free = Touching + 1;
	}
	FMemory::Memcpy(OutBend, Bend, sizeof(Bend));
}

void FMOBodyRetargetAnimInstanceProxy::ApplyHandGrips(FPoseContext& Output) const
{
	if (HandGrips[0].Alpha <= ZERO_ANIMWEIGHT_THRESH && HandGrips[1].Alpha <= ZERO_ANIMWEIGHT_THRESH)
	{
		return;
	}
	static const TArray<FName> FingerBones = []()   // [hand][finger][joint]: hand_l's thumb_01_l .. hand_r's pinky_03_r
	{
		static const TCHAR* Fingers[5] = {TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")};
		TArray<FName> Names;
		for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
		{
			for (const TCHAR* Finger : Fingers)
			{
				for (int32 k = 1; k <= 3; ++k)
				{
					Names.Add(FName(*FString::Printf(TEXT("%s_%02d_%s"), Finger, k, Side)));
				}
			}
		}
		return Names;
	}();
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	auto Compact = [&Bones](const FName Name)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};
	FCSPose<FCompactPose> Pose;
	Pose.InitPose(Output.Pose);
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const FMOHandGripIK& Grip = HandGrips[Hand];
		const FCompactPoseBoneIndex HandIndex = Compact(Hand == 0 ? TEXT("hand_l") : TEXT("hand_r"));
		if (Grip.Alpha <= ZERO_ANIMWEIGHT_THRESH || !HandIndex.IsValid() || Grip.HalfSize.GetMin() <= 0.0f)
		{
			continue;
		}
		const FTransform HandleCS = Grip.HandleInHand * Pose.GetComponentSpaceTransform(HandIndex);
		for (int32 f = 0; f < 5; ++f)
		{
			FCompactPoseBoneIndex Joint[3];
			for (int32 k = 0; k < 3; ++k)
			{
				Joint[k] = Compact(FingerBones[Hand * 15 + f * 3 + k]);
			}
			if (!Joint[0].IsValid() || !Joint[1].IsValid() || !Joint[2].IsValid())
			{
				continue;   // a LOD without the fingers
			}
			// The parents above the finger are as animated (the metacarpals, the hand): read their component space before any finger changes
			const FTransform ParentCS = Pose.GetComponentSpaceTransform(Output.Pose.GetParentBoneIndex(Joint[0]));
			const FTransform Open[3] = {Output.Pose.GetRefPose(Joint[0]), Output.Pose.GetRefPose(Joint[1]), Output.Pose.GetRefPose(Joint[2])};
			const FQuat* Fist = &Grip.Fist[f * 3];
			float Bend[3];
			WrapFinger(ParentCS, Open, Fist, GripFingerRadii[f], HandleCS, Grip.HalfSize, Bend);
			for (int32 k = 0; k < 3; ++k)
			{
				FTransform& Local = Output.Pose[Joint[k]];
				const FQuat Held = FQuat::Slerp(Open[k].GetRotation(), Fist[k], Bend[k]);
				Local.SetRotation(FQuat::Slerp(Local.GetRotation(), Held, Grip.Alpha).GetNormalized());
			}
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
		ApplyHandGrips(Output);
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
	ApplyHandGrips(Output);
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
	UpdateHandGrips(DeltaSeconds);
}

void UMOBodyRetargetAnimInstance::UpdateHandGrips(float DeltaSeconds)
{
	const AMOCharacter* Owner = Cast<AMOCharacter>(GetOwningActor());
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		FMOHandGripIK& Grip = HandGrips[Hand];
		AMOCharacter::FHandGrip Held;
		const bool bHolding = Owner && Mesh && Owner->GetHandGrip(Hand == 0, Held);
		if (bHolding)
		{
			const FName HandBone = Hand == 0 ? TEXT("hand_l") : TEXT("hand_r");
			Grip.HandleInHand = Held.Handle->GetComponentTransform().GetRelativeTransform(Mesh->GetSocketTransform(HandBone, RTS_World));
			Grip.HalfSize = Held.HalfSize;
			const UAnimSequence* Fist = Held.Fist.Get();
			if (SampledFist[Hand].Get() != Fist || SampledFistTime[Hand] != Held.FistTime)
			{
				// The fist's left-hand finger joints, once per pose: the same local rotations close either hand (mirrored bones)
				const USkeleton* Skeleton = Fist->GetSkeleton();
				static const TCHAR* Fingers[5] = {TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")};
				for (int32 i = 0; i < 15; ++i)
				{
					const FName Bone(*FString::Printf(TEXT("%s_%02d_l"), Fingers[i / 3], i % 3 + 1));
					const int32 BoneIndex = Skeleton ? Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
					FTransform Local = FTransform::Identity;
					if (BoneIndex != INDEX_NONE)
					{
						Fist->GetBoneTransform(Local, FSkeletonPoseBoneIndex(BoneIndex), FAnimExtractContext(static_cast<double>(Held.FistTime)), false);
					}
					Grip.Fist[i] = Local.GetRotation();
				}
				SampledFist[Hand] = Fist;
				SampledFistTime[Hand] = Held.FistTime;
			}
		}
		// Closes on a new handle, lets go back to the animation's fingers (the last handle kept while it fades)
		Grip.Alpha = FMath::FInterpConstantTo(Grip.Alpha, bHolding ? 1.0f : 0.0f, DeltaSeconds, 1.0f / GripBlendSeconds);
	}
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
		const bool bFoot = i >= 2;
		const float Contact = bClimbing ? GetCurveValue(Limb.ContactCurve) : 0.0f;
		if (bFoot)
		{
			UpdateClimbFoot(Hold, Limb, bClimbing, ToWorld.TransformPosition(Proxy.AnimatedEndCS[i]), Out, Owner);
		}
		else if (Contact < ClimbIKTraceThreshold)
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
			const FVector HoldAt = On->GetComponentTransform().TransformPosition(Hold.LocalPoint);
			// A foot slides to a new hold in ~0.15 s (a step), from where it was; a hand goes straight to its hold, as the clip's reach brings it.
			Hold.Target = bFoot && Hold.Alpha > 0.05f ? FMath::VInterpTo(Hold.Target, HoldAt, DeltaSeconds, ClimbHoldFollowRate) : HoldAt;
			Limb.TargetCS = ToWorld.InverseTransformPosition(Hold.Target);
		}
		// Elbows bend down, knees toward the rock, when the clip's own limb is straight.
		Limb.BendHintCS = ToWorld.InverseTransformVectorNoScale(i < 2 ? FVector::DownVector : -Out);
		bAny |= Limb.Alpha > ZERO_ANIMWEIGHT_THRESH;
	}
	bClimbIK = bClimbing || bAny;   // the proxy measures the clip's limbs while climbing (the sweeps start there) and blends the holds out after
}

void UMOBodyRetargetAnimInstance::UpdateClimbFoot(FLimbHold& Hold, const FMOClimbLimbIK& Limb, bool bClimbing, const FVector& ClipFoot, const FVector& Out,
	const AActor* Owner) const
{
	// Until Oct 31 a foot reached for rock only while the clip's foot contact curve was up (the idle and up clips; the down and side clips have
	// none) and only 45 cm straight in from the clip's foot, so it stayed in the clip's flat-wall plane under the hands: on SM_Cliff_09 side 3 the
	// feet were on the rock in 40 % of samples and off it (more than 30 cm, or no rock within 60) in 48 % (the hands: 72 % on).
	const UWorld* World = GetWorld();
	if (!bClimbing || !World)
	{
		Hold.bHeld = Hold.bLocked = false;
		return;
	}
	if (Hold.bLocked)
	{
		const UPrimitiveComponent* On = Hold.On.Get();
		const FVector HoldAt = On ? On->GetComponentTransform().TransformPosition(Hold.LocalPoint) : ClipFoot;
		// The clip has moved this foot on across the face (a step up, down or along): it lets go of this hold and finds the next.
		if (!On || FVector::VectorPlaneProject(HoldAt - ClipFoot, Out).Size() > ClimbFootStepCm)
		{
			Hold.bLocked = false;
		}
	}
	if (Hold.bLocked)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MOClimbFootIK), false, Owner);
	FHitResult Hit;
	const bool bFound = World->SweepSingleByChannel(Hit, ClipFoot + Out * ClimbFootOutCm, ClipFoot - Out * ClimbFootReachCm, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(4.0f), Params) && Hit.GetComponent() && !Hit.bStartPenetrating;
	Hold.bHeld = Hold.bLocked = bFound;
	if (bFound)
	{
		Hold.On = Hit.GetComponent();
		Hold.LocalPoint = Hit.GetComponent()->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint + Hit.ImpactNormal * Limb.SurfaceOffset);
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
