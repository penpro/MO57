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

void FMOBodyRetargetAnimInstanceProxy::ApplyHandGrips(FPoseContext& Output)
{
	bAimed[0] = bAimed[1] = false;
	SupportWeight[0] = SupportWeight[1] = 0.0f;
	TrackWeight[0] = TrackWeight[1] = TrackHold[0] = TrackHold[1] = 0.0f;
	TrackDir[0] = TrackDir[1] = FVector::ZeroVector;
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
	// A hand's fingers closed on a handle (component space), blended over the animation's by Weight
	auto CloseFingers = [&](int32 Hand, const FMOHandGripIK& Grip, const FMOHandGrip::FShape& Handle, float Weight)
	{
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
				Local.SetRotation(FQuat::Slerp(Local.GetRotation(), FMOHandGrip::JointRotation(Finger, k, Bend[k]), Weight).GetNormalized());
			}
		}
	};
	// A hand's palm (its metacarpals to their knuckles, component space: the fingers do not move them), as the seat and Tools/check_grip.py see it
	auto ReadPalm = [&](int32 Hand, float HandScale, FMOHandGrip::FPalm& Out)
	{
		static const FName PalmBones[2][8] = {
			{TEXT("index_metacarpal_l"), TEXT("middle_metacarpal_l"), TEXT("ring_metacarpal_l"), TEXT("pinky_metacarpal_l"),
				TEXT("index_01_l"), TEXT("middle_01_l"), TEXT("ring_01_l"), TEXT("pinky_01_l")},
			{TEXT("index_metacarpal_r"), TEXT("middle_metacarpal_r"), TEXT("ring_metacarpal_r"), TEXT("pinky_metacarpal_r"),
				TEXT("index_01_r"), TEXT("middle_01_r"), TEXT("ring_01_r"), TEXT("pinky_01_r")}};
		for (int32 i = 0; i < 4; ++i)
		{
			const FCompactPoseBoneIndex Metacarpal = Compact(PalmBones[Hand][i]);
			const FCompactPoseBoneIndex Knuckle = Compact(PalmBones[Hand][4 + i]);
			if (!Metacarpal.IsValid() || !Knuckle.IsValid())
			{
				return false;
			}
			Out.Metacarpal[i] = Pose.GetComponentSpaceTransform(Metacarpal).GetLocation();
			Out.Knuckle[i] = Pose.GetComponentSpaceTransform(Knuckle).GetLocation();
		}
		Out.Radius = FMOHandGrip::PalmRadius * HandScale;
		return true;
	};
	const FCompactPoseBoneIndex HandIndex[2] = {Compact(TEXT("hand_l")), Compact(TEXT("hand_r"))};
	// What each hand's fingers close on, and how far: planned first, because a free hand pulled onto a tracked spear moves its arm, and the arm
	// has to be in the pose before any finger is read off it.
	struct FFingers
	{
		int32 Hand = 0;
		const FMOHandGripIK* Grip = nullptr;   // the fist and hand scale they close with (the item's, for a free hand too)
		FMOHandGrip::FShape On;
		float Weight = 0.0f;
	};
	TArray<FFingers, TInlineAllocator<4>> Fingers;   // in order: a free hand's over what its own grip closed (an item blending into it)
	int32 Reach = INDEX_NONE;   // the free hand pulled onto a tracked handle (one at most: two carrying hands leave none free)
	FTransform ReachCS;
	float ReachWeight = 0.0f;
	bool bPoseChanged = false;   // a hand turned at the wrist or an arm pulled onto the spear: back into the local pose before the fingers
	static const FName ArmBones[2][2] = {{TEXT("upperarm_l"), TEXT("lowerarm_l")}, {TEXT("upperarm_r"), TEXT("lowerarm_r")}};
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const FMOHandGripIK& Grip = HandGrips[Hand];
		if (Grip.Alpha <= ZERO_ANIMWEIGHT_THRESH || !HandIndex[Hand].IsValid() || Grip.HalfSize.GetMin() <= 0.0f)
		{
			continue;
		}
		FTransform HandCS = Pose.GetComponentSpaceTransform(HandIndex[Hand]);
		const bool bLong = Grip.Run.Y > Grip.Run.X;   // a long handle: a second hand can hold it, and it may turn from its seat

		// A clip carrying its spear's own track (the spear set, N-269): the curves come weighted by the clip (TrackCurve: they blend in and out
		// with it). Read first: past what a fist takes, the hand turns toward the clip's spear at the wrist before anything is seated in it.
		const float TrackIn = bLong ? Output.Curve.Get(FMOHandGrip::TrackCurve) : 0.0f;
		auto ClipValue = [&Output, TrackIn](const FName& Curve) { return TrackIn > 0.0f ? Output.Curve.Get(Curve) / TrackIn : 0.0f; };   // as the clip has it
		FVector Dir = FVector::ZeroVector;
		float Track = 0.0f;
		if (TrackIn > ZERO_ANIMWEIGHT_THRESH)
		{
			Dir = FVector(Output.Curve.Get(FMOHandGrip::TrackDirCurves[0]), Output.Curve.Get(FMOHandGrip::TrackDirCurves[1]),
				Output.Curve.Get(FMOHandGrip::TrackDirCurves[2])).GetSafeNormal();
			Track = Dir.IsNearlyZero() ? 0.0f
				: FMath::Min(TrackIn, 1.0f) * FMath::Clamp(ClipValue(FMOHandGrip::TrackHoldCurves[Hand]), 0.0f, 1.0f) * Grip.Alpha;
		}
		if (Track > ZERO_ANIMWEIGHT_THRESH)
		{
			const FQuat Wrist = FMOHandGrip::WristTurn((Grip.HandleInHand * HandCS).GetUnitAxis(EAxis::Z), Dir, Track);
			if (!Wrist.Equals(FQuat::Identity, 1.0e-6f))
			{
				HandCS.SetRotation((Wrist * HandCS.GetRotation()).GetNormalized());
				const FBoneTransform Turned[] = {FBoneTransform(HandIndex[Hand], HandCS)};
				Pose.LocalBlendCSBoneTransforms(Turned, 1.0f);
				bPoseChanged = true;
			}
		}
		FMOHandGrip::FShape Handle;
		Handle.Frame = Grip.HandleInHand * HandCS;
		Handle.HalfSize = Grip.HalfSize;
		Handle.Style = Grip.Style;
		const int32 Other = 1 - Hand;
		const float Free = HandIndex[Other].IsValid() ? 1.0f - HandGrips[Other].Alpha : 0.0f;
		FMOHandGrip::FPalm Palms[2];
		bool bPalms[2] = {false, false};
		if (bLong)
		{
			bPalms[0] = ReadPalm(Hand, Grip.HandScale, Palms[0]);
			bPalms[1] = HandIndex[Other].IsValid() && ReadPalm(Other, Grip.HandScale, Palms[1]);
		}

		// The other hand, carrying nothing, on a long handle where the clip puts it (a spear's front hand): the handle turned through that
		// hand's own seat and out of both palms, its fingers closed on it. Neither hand moves; the item follows them (AMOCharacter::AimHandGrip,
		// after this).
		FMOHandGrip::FShape Stretch;
		float FreeWeight = 0.0f;
		if (Free > ZERO_ANIMWEIGHT_THRESH && bLong)
		{
			FreeWeight = FMOHandGrip::Support(Handle, Grip.SupportInHand * Pose.GetComponentSpaceTransform(HandIndex[Other]),
				bPalms[0] ? &Palms[0] : nullptr, bPalms[1] ? &Palms[1] : nullptr, Grip.Run, Grip.Alpha * Free, Stretch);
		}

		// The track: the handle onto the clip's spear through this hand's grip, and the free hand pulled onto it where the clip's hand holds it.
		// It takes over from Support as the clip comes in.
		FMOHandGrip::FShape CarryOn = Handle;
		if (Track > ZERO_ANIMWEIGHT_THRESH)
		{
			const float Slide = ClipValue(FMOHandGrip::TrackSlideCurves[Hand]);
			FMOHandGrip::Track(Handle, Dir, Slide, bPalms[0] ? &Palms[0] : nullptr, Grip.Run, Track, CarryOn);
			TrackWeight[Hand] = Track;
			TrackDir[Hand] = Dir;
			TrackHold[Hand] = FMath::Clamp(ClipValue(FMOHandGrip::TrackHoldCurves[Other]), 0.0f, 1.0f);
			FreeWeight *= 1.0f - Track;
			if (Free > ZERO_ANIMWEIGHT_THRESH)
			{
				FMOHandGrip::FFreeHand FreeHand;
				FreeHand.HandCS = Pose.GetComponentSpaceTransform(HandIndex[Other]);
				FreeHand.SeatCS = Grip.SupportInHand * FreeHand.HandCS;
				FreeHand.Palm = bPalms[1] ? &Palms[1] : nullptr;
				const FCompactPoseBoneIndex Upper = Compact(ArmBones[Other][0]);
				const FCompactPoseBoneIndex Lower = Compact(ArmBones[Other][1]);
				if (Upper.IsValid() && Lower.IsValid())
				{
					const FVector Shoulder = Pose.GetComponentSpaceTransform(Upper).GetLocation();
					const FVector Elbow = Pose.GetComponentSpaceTransform(Lower).GetLocation();
					FreeHand.Shoulder = Shoulder;
					FreeHand.ArmCm = FVector::Dist(Shoulder, Elbow) + FVector::Dist(Elbow, FreeHand.HandCS.GetLocation());
				}
				const FVector CarryGrip = Handle.Frame.TransformPosition(FVector(0.0f, 0.0f, Slide * Track));
				FTransform Target;
				FMOHandGrip::FShape OnIt;
				const float Hold = FMOHandGrip::HoldOn(Handle, CarryGrip, FreeHand, Grip.Run, Track * TrackHold[Hand] * Free, Target, OnIt);
				if (Hold > 0.0f)
				{
					Reach = Other;
					ReachCS = Target;
					ReachWeight = Hold;
					Stretch = OnIt;
					FreeWeight += Hold;
				}
			}
		}
		SupportWeight[Hand] = FreeWeight;
		if (FreeWeight > 0.0f)
		{
			Fingers.Add({Other, &Grip, Stretch, FreeWeight});
		}
		if (bLong)
		{
			AimedHandleInHand[Hand] = Handle.Frame.GetRelativeTransform(HandCS);
			bAimed[Hand] = true;
		}
		Fingers.Add({Hand, &Grip, CarryOn, Grip.Alpha});
	}

	// The free hand onto the tracked spear: its arm by two-bone IK (the elbow bending the way the clip has it), the hand turned onto the shaft
	if (Reach != INDEX_NONE)
	{
		const FCompactPoseBoneIndex Upper = Compact(ArmBones[Reach][0]);
		const FCompactPoseBoneIndex Lower = Compact(ArmBones[Reach][1]);
		if (Upper.IsValid() && Lower.IsValid())
		{
			FTransform U = Pose.GetComponentSpaceTransform(Upper);
			FTransform L = Pose.GetComponentSpaceTransform(Lower);
			FTransform E = Pose.GetComponentSpaceTransform(HandIndex[Reach]);
			const FVector Mid = 0.5f * (U.GetLocation() + E.GetLocation());
			const FVector Out = L.GetLocation() - Mid;
			const FVector JointTarget = L.GetLocation() + (Out.SizeSquared() > 1.0f ? Out.GetSafeNormal() : -FVector::ZAxisVector) * 30.0f;
			AnimationCore::SolveTwoBoneIK(U, L, E, JointTarget, ReachCS.GetLocation(), false, 1.0, 1.0);
			E.SetRotation(ReachCS.GetRotation());
			const FBoneTransform Chain[] = {FBoneTransform(Upper, U), FBoneTransform(Lower, L), FBoneTransform(HandIndex[Reach], E)};   // root to tip
			Pose.LocalBlendCSBoneTransforms(Chain, ReachWeight);
			bPoseChanged = true;
		}
	}
	if (bPoseChanged)
	{
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(Pose, Output.Pose);
		Pose.InitPose(Output.Pose);   // the fingers read the turned hand and the moved arm
	}
	for (const FFingers& Close : Fingers)
	{
		CloseFingers(Close.Hand, *Close.Grip, Close.On, Close.Weight);
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

void UMOBodyRetargetAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
	// A long handle turned through the other hand this evaluation: the item goes where the hands hold it, before the attachments update
	if (AMOCharacter* Owner = Cast<AMOCharacter>(GetOwningActor()))
	{
		const FMOBodyRetargetAnimInstanceProxy& Proxy = GetProxyOnGameThread<FMOBodyRetargetAnimInstanceProxy>();
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			if (Proxy.bAimed[Hand])
			{
				Owner->AimHandGrip(Hand, Proxy.AimedHandleInHand[Hand], Proxy.SupportWeight[Hand], Proxy.TrackWeight[Hand], Proxy.TrackHold[Hand],
					Proxy.TrackDir[Hand]);
			}
		}
	}
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
			// Where the item hangs; a long handle from its seat (the item itself may be turned through the other hand: AimHandGrip)
			Grip.HandleInHand = Held.Run.Y > Held.Run.X ? Held.HandleInHand : GripTransform * ItemInHand;
			Grip.SupportInHand = Held.SupportInHand;
			Grip.Run = Held.Run;
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
