#include "MOBodyRetargetAnimInstance.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "MOBowMeshComponent.h"
#include "MOHandGrip.h"
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
		FMOHandGrip::FShape Handle;
		Handle.Frame = Grip.HandleInHand * Pose.GetComponentSpaceTransform(HandIndex);
		Handle.HalfSize = Grip.HalfSize;
		Handle.Style = Grip.Style;
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
			FMOHandGrip::FFinger Finger;
			FMOHandGrip::MakeFinger(Open, &Grip.Fist[f * 3], f, Grip.HandScale, Finger);
			float Bend[3];
			FMOHandGrip::Wrap(ParentCS, Finger, Handle, Bend);
			for (int32 k = 0; k < 3; ++k)
			{
				FTransform& Local = Output.Pose[Joint[k]];
				Local.SetRotation(FQuat::Slerp(Local.GetRotation(), FMOHandGrip::JointRotation(Finger, k, Bend[k]), Grip.Alpha).GetNormalized());
			}
		}
	}
}

void FMOBodyRetargetAnimInstanceProxy::LimitDraw(FPoseContext& Output) const
{
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const FMOHandGripIK& Grip = HandGrips[Hand];
		if (Grip.Alpha <= ZERO_ANIMWEIGHT_THRESH || Grip.MaxDrawCm <= 0.0f || !Grip.bDrawing)
		{
			continue;
		}
		// The bow in this hand, the drawing arm the other: where its fingers hold the string (UMOBowMeshComponent's pair)
		static const FName Bones[2][6] = {
			{TEXT("hand_l"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("index_02_r"), TEXT("middle_02_r")},
			{TEXT("hand_r"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("index_02_l"), TEXT("middle_02_l")}};
		const FBoneContainer& Container = Output.Pose.GetBoneContainer();
		FCompactPoseBoneIndex Index[6] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE),
			FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
		bool bAll = true;
		for (int32 i = 0; i < 6; ++i)
		{
			const int32 MeshIndex = Container.GetPoseBoneIndexForBoneName(Bones[Hand][i]);
			Index[i] = MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			bAll &= Index[i].IsValid();
		}
		if (!bAll)
		{
			continue;
		}
		FCSPose<FCompactPose> Pose;
		Pose.InitPose(Output.Pose);
		const FTransform BowCS = Grip.ItemInHand * Pose.GetComponentSpaceTransform(Index[0]);
		const FVector Axis = BowCS.TransformVectorNoScale(Grip.DrawAxis).GetSafeNormal();
		const FVector Fingers = 0.5f * (Pose.GetComponentSpaceTransform(Index[4]).GetLocation() + Pose.GetComponentSpaceTransform(Index[5]).GetLocation());
		const float Over = FVector::DotProduct(Fingers - BowCS.GetLocation(), Axis) - Grip.MaxDrawCm;
		if (Over <= 0.0f)
		{
			continue;   // within the bow's draw: the arm as animated
		}
		// The drawing hand back along the draw to the bow's full draw, its turn kept; the elbow bends the way the clip has it
		FTransform Upper = Pose.GetComponentSpaceTransform(Index[1]);
		FTransform Lower = Pose.GetComponentSpaceTransform(Index[2]);
		FTransform End = Pose.GetComponentSpaceTransform(Index[3]);
		const FVector Mid = 0.5f * (Upper.GetLocation() + End.GetLocation());
		const FVector Out = Lower.GetLocation() - Mid;
		const FVector JointTarget = Lower.GetLocation() + (Out.SizeSquared() > 1.0f ? Out.GetSafeNormal() : -Axis) * 30.0f;
		AnimationCore::SolveTwoBoneIK(Upper, Lower, End, JointTarget, End.GetLocation() - Axis * Over, false, 1.0, 1.0);
		const FBoneTransform Chain[] = {FBoneTransform(Index[1], Upper), FBoneTransform(Index[2], Lower), FBoneTransform(Index[3], End)};
		Pose.LocalBlendCSBoneTransforms(Chain, Grip.Alpha);
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
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
		LimitDraw(Output);
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
	LimitDraw(Output);
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
			FTransform ItemInHand = Held.Handle->GetComponentTransform().GetRelativeTransform(Mesh->GetSocketTransform(HandBone, RTS_World));
			ItemInHand.SetScale3D(FVector::OneVector);   // GripTransform is in cm as held
			FTransform GripTransform = Held.GripTransform;
			GripTransform.SetScale3D(FVector::OneVector);
			Grip.HandleInHand = GripTransform * ItemInHand;
			Grip.HalfSize = Held.HalfSize;
			Grip.Style = Held.Style;
			Grip.HandScale = Held.HandScale;
			const UMOBowMeshComponent* Bow = Cast<UMOBowMeshComponent>(Held.Handle.Get());
			Grip.ItemInHand = ItemInHand;
			Grip.DrawAxis = Bow ? Bow->GetDrawAxis() : FVector::YAxisVector;
			Grip.MaxDrawCm = Bow && Bow->HasDraw() ? Bow->GetDrawRangeCm().Z : 0.0f;
			Grip.bDrawing = Bow && Bow->IsNocked();
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
