#include "MOHandGrip.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

const TCHAR* const FMOHandGrip::FingerNames[5] = {TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")};

const float FMOHandGrip::FingerRadii[5][3] = {
	{1.0f, 0.92f, 0.8f},     // thumb
	{0.85f, 0.78f, 0.68f},   // index
	{0.86f, 0.8f, 0.7f},     // middle
	{0.8f, 0.74f, 0.65f},    // ring
	{0.72f, 0.66f, 0.58f}};  // pinky

const float FMOHandGrip::FlexLimitDeg[5][3] = {
	{50.0f, 60.0f, 80.0f},    // thumb: its base (the MetaHuman's thumb_01, the metacarpal), knuckle, end joint
	{90.0f, 110.0f, 80.0f},   // index
	{90.0f, 110.0f, 80.0f},   // middle
	{90.0f, 110.0f, 80.0f},   // ring
	{90.0f, 110.0f, 80.0f}};  // pinky

// held_grip.derive_standard (Nov 2 2026): Femme's seat of CompositeBow01 in Sparrow's fist, the centre slid along the axis to e1 = 0
const FVector FMOHandGrip::PowerCentre(0.0, 0.5619, 0.2897);
const FVector FMOHandGrip::PowerAxis(0.9998, 0.0037, -0.019);
const FVector FMOHandGrip::PowerFace(-0.0191, 0.374, -0.9272);

float FMOHandGrip::Clearance(const FShape& Shape, const FVector& Point, float Radius)
{
	const FVector P = Shape.Frame.InverseTransformPositionNoScale(Point);
	const FVector& H = Shape.HalfSize;
	if (Shape.Style == EMOGripStyle::Sphere)
	{
		// Out from the surface along the ray from the centre (exact for a ball, close for these rounded stones and fruit)
		const float Rn = FMath::Sqrt(FMath::Square(P.X / H.X) + FMath::Square(P.Y / H.Y) + FMath::Square(P.Z / H.Z));
		return (Rn > UE_KINDA_SMALL_NUMBER ? P.Size() * (1.0f - 1.0f / Rn) : -H.GetMin()) - Radius;
	}
	if (FMath::Abs(P.Z) > H.Z + 2.0f)
	{
		return 99.0f;   // nowhere near it
	}
	// Out from the side along the ray from the axis (an ellipse's radial distance: exact for a circle, close for these near-round handles)
	const float Rn = FMath::Sqrt(FMath::Square(P.X / H.X) + FMath::Square(P.Y / H.Y));
	const float Radial = FMath::Sqrt(P.X * P.X + P.Y * P.Y);
	float Out = Rn > UE_KINDA_SMALL_NUMBER ? Radial * (1.0f - 1.0f / Rn) : -FMath::Min(H.X, H.Y);
	if (FMath::Abs(P.Z) > H.Z)
	{
		Out = FMath::Max(Out, FMath::Abs(P.Z) - H.Z);   // past an end: a finger may lie over it
	}
	return Out - Radius;
}

void FMOHandGrip::MakeFinger(const FTransform OpenLocal[3], const FQuat FistLocal[3], int32 Finger, float HandScale, FFinger& Out)
{
	for (int32 k = 0; k < 3; ++k)
	{
		Out.Open[k] = OpenLocal[k];
		// The fist's turn from open, in the parent's frame (Fist = Delta * Open): its axis is the way this joint flexes
		FQuat Delta = (FistLocal[k] * OpenLocal[k].GetRotation().Inverse()).GetNormalized();
		if (Delta.W < 0.0f)
		{
			Delta = FQuat(-Delta.X, -Delta.Y, -Delta.Z, -Delta.W);   // the short way round
		}
		FVector Axis;
		float Angle;
		Delta.ToAxisAndAngle(Axis, Angle);
		const bool bFlexes = Angle > 1.0e-3f;   // a joint the fist leaves open has no way to flex: it stays open
		Out.Axis[k] = bFlexes ? Axis : FVector::YAxisVector;
		Out.LimitRad[k] = bFlexes ? FMath::Max(Angle, FMath::DegreesToRadians(FlexLimitDeg[Finger][k])) : 0.0f;
		Out.Radii[k] = FingerRadii[Finger][k] * HandScale;
	}
	Out.bThumb = Finger == 0;
}

FQuat FMOHandGrip::JointRotation(const FFinger& Finger, int32 Joint, float Bend)
{
	const FQuat Flexed = FQuat(Finger.Axis[Joint], Finger.LimitRad[Joint] * Bend) * Finger.Open[Joint].GetRotation();
	return (Joint == 0 ? Finger.BaseSwing * Flexed : Flexed).GetNormalized();
}

void FMOHandGrip::Wrap(const FTransform& ParentCS, FFinger& Finger, const FShape& Shape, float OutBend[3])
{
	constexpr int32 Samples = 4;      // points along each phalanx past its base
	constexpr int32 Steps = 16;       // the sweep walked toward full flexion for the first touch (a thin handle could be passed clean through)
	constexpr int32 Iterations = 10;  // then bisection inside that step

	// The joints for a bend, the fingertip past the last; then a phalanx's clearance (k: 0 proximal .. 2 distal)
	auto Joints = [&](const float Bend[3], FVector Out[4])
	{
		FTransform T = ParentCS;
		for (int32 k = 0; k < 3; ++k)
		{
			T = FTransform(JointRotation(Finger, k, Bend[k]), Finger.Open[k].GetTranslation()) * T;
			Out[k] = T.GetLocation();
		}
		Out[3] = Out[2] + (Out[2] - Out[1]) * TipFraction;
	};
	auto Phalanx = [&](const FVector J[4], int32 k)
	{
		const float R0 = Finger.Radii[k];
		const float R1 = k < 2 ? Finger.Radii[k + 1] : Finger.Radii[2] * 0.9f;
		float Min = 99.0f;
		for (int32 i = 1; i <= Samples; ++i)
		{
			const float F = static_cast<float>(i) / Samples;
			Min = FMath::Min(Min, Clearance(Shape, FMath::Lerp(J[k], J[k + 1], F), FMath::Lerp(R0, R1, F)));
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

	float Bend[3] = {0.0f, 0.0f, 0.0f};
	int32 Free = 0;
	if (ClearFrom(Bend, 0) < 0.0f)
	{
		// The open finger is in the handle's way. A thumb goes over it: along its closing to where it is clear on the far side, and closes
		// from there (the open MetaHuman thumb lies across a handle seated against the palm). A finger straightens until clear (as far as a
		// finger extends): its base lies on the handle and the joints past the phalanx touching it close round it, as on a thick handle.
		// Neither: it rests where along its range it is least in the handle, the joints past its deepest phalanx still closing.
		bool bOver = false;
		if (Finger.bThumb)
		{
			for (int32 Step = 1; Step <= Steps && !bOver; ++Step)
			{
				const float X = static_cast<float>(Step) / Steps;
				const float Try[3] = {X, X, X};
				if (ClearFrom(Try, 0) >= 0.0f)
				{
					Bend[0] = Bend[1] = Bend[2] = X;
					bOver = true;
				}
			}
			if (!bOver)
			{
				// A handle too thick to go over: the thumb's base swings out from it (the joint at its root abducts) until the whole thumb is
				// clear, then it closes onto the handle from outside
				FVector J[4];
				Joints(Bend, J);
				const FVector Local = Shape.Frame.InverseTransformPositionNoScale(J[1]);
				const FVector Radial = Shape.Style == EMOGripStyle::Sphere ? Local : FVector(Local.X, Local.Y, 0.0f);
				const FVector Out = Shape.Frame.TransformVectorNoScale(Radial).GetSafeNormal();
				const FVector Axis = FVector::CrossProduct(J[3] - J[0], Out).GetSafeNormal();
				if (!Axis.IsNearlyZero())
				{
					const FVector AxisInParent = ParentCS.InverseTransformVectorNoScale(Axis).GetSafeNormal();
					for (int32 Step = 1; Step <= ThumbSwingSteps && !bOver; ++Step)
					{
						Finger.BaseSwing = FQuat(AxisInParent, FMath::DegreesToRadians(ThumbSwingMaxDeg) * Step / ThumbSwingSteps);
						bOver = ClearFrom(Bend, 0) >= 0.0f;
					}
					if (!bOver)
					{
						Finger.BaseSwing = FQuat::Identity;
					}
				}
			}
		}
		if (!bOver)
		{
			const float Straight[3] = {-MaxExtension, -MaxExtension, -MaxExtension};
			if (ClearFrom(Straight, 0) >= 0.0f)
			{
				float Lo = -MaxExtension, Hi = 0.0f;
				for (int32 It = 0; It < Iterations; ++It)
				{
					const float Mid = 0.5f * (Lo + Hi);
					const float Try[3] = {Mid, Mid, Mid};
					(ClearFrom(Try, 0) >= 0.0f ? Lo : Hi) = Mid;
				}
				Bend[0] = Bend[1] = Bend[2] = Lo;
			}
			else
			{
				float Best = -99.0f;
				for (int32 Step = 0; Step <= 2 * Steps; ++Step)
				{
					const float X = FMath::Lerp(-MaxExtension, 1.0f, static_cast<float>(Step) / (2 * Steps));
					const float Try[3] = {X, X, X};
					const float Clear = ClearFrom(Try, 0);
					if (Clear > Best)
					{
						Best = Clear;
						Bend[0] = Bend[1] = Bend[2] = X;
					}
				}
			}
			// The phalanx on (or deepest in) the handle stops with the joints before it; the ones past it go on closing below
			FVector J[4];
			Joints(Bend, J);
			int32 Pinned = 0;
			for (int32 k = 1; k < 3; ++k)
			{
				if (Phalanx(J, k) < Phalanx(J, Pinned))
				{
					Pinned = k;
				}
			}
			Free = Pinned + 1;
		}
	}

	// A tendon closes it: the joints from Free on bend together; when a phalanx touches, it and every joint before it stop
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
			break;
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
			At(1.0f, Bend);   // closes all the way without touching: a handle thinner than the closed hand
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

bool FMOHandGrip::ReadHand(const USkeletalMesh* Body, const UAnimSequence* Fist, float FistTime, bool bLeftHand, FHandInFist& Out, FString* OutWhy)
{
	auto Fail = [OutWhy](const FString& Why)
	{
		if (OutWhy)
		{
			*OutWhy = Why;
		}
		return false;
	};
	if (!Body || !Fist)
	{
		return Fail(TEXT("no body or fist"));
	}
	const USkeleton* FistSkeleton = Fist->GetSkeleton();
	if (!FistSkeleton)
	{
		return Fail(FString::Printf(TEXT("the fist %s has no skeleton"), *Fist->GetName()));
	}
	const FReferenceSkeleton& Ref = Body->GetRefSkeleton();
	const FReferenceSkeleton& FistRef = FistSkeleton->GetReferenceSkeleton();
	const TCHAR* Side = bLeftHand ? TEXT("l") : TEXT("r");
	const int32 HandIndex = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("hand_%s"), Side)));
	if (HandIndex == INDEX_NONE)
	{
		return Fail(FString::Printf(TEXT("%s has no hand_%s"), *Body->GetName(), Side));
	}
	const FAnimExtractContext Context(static_cast<double>(FistTime));

	// A bone's local transform in the fist: the fist's rotation (read off its LEFT hand: the bones are mirrored) on this body's bone
	auto FistLocal = [&](int32 Index)
	{
		FTransform Local = Ref.GetRefBonePose()[Index];
		const FString Name = Ref.GetBoneName(Index).ToString();
		const int32 FistIndex = FistRef.FindBoneIndex(FName(*(Name.LeftChop(2) + TEXT("_l"))));
		if (FistIndex != INDEX_NONE)
		{
			FTransform Sampled;
			Fist->GetBoneTransform(Sampled, FSkeletonPoseBoneIndex(FistIndex), Context, false);
			Local.SetRotation(Sampled.GetRotation());
		}
		return Local;
	};
	// A bone in the hand's space in the fist
	auto InHand = [&](int32 Index, FTransform& T)
	{
		T = FTransform::Identity;
		while (Index != HandIndex)
		{
			if (Index == INDEX_NONE)
			{
				return false;   // missing, or not under the hand
			}
			T = T * FistLocal(Index);   // the bone in its next ancestor's space
			Index = Ref.GetParentIndex(Index);
		}
		return true;
	};
	auto Bone = [&](const FString& Name) { return Ref.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%s"), *Name, Side))); };
	for (int32 f = 0; f < 5; ++f)
	{
		const FString Finger = FingerNames[f];
		int32 Joint[3];
		for (int32 k = 0; k < 3; ++k)
		{
			Joint[k] = Bone(FString::Printf(TEXT("%s_%02d"), *Finger, k + 1));
		}
		FTransform Parent, J3, Metacarpal;
		if (Joint[0] == INDEX_NONE || Joint[1] == INDEX_NONE || Joint[2] == INDEX_NONE || !InHand(Ref.GetParentIndex(Joint[0]), Parent) || !InHand(Joint[2], J3)
			|| (f > 0 && !InHand(Bone(Finger + TEXT("_metacarpal")), Metacarpal)))
		{
			return Fail(FString::Printf(TEXT("%s lacks the %s finger's bones under hand_%s"), *Body->GetName(), *Finger, Side));
		}
		Out.FingerParent[f] = Parent;
		for (int32 k = 0; k < 3; ++k)
		{
			Out.Open[f][k] = Ref.GetRefBonePose()[Joint[k]];
			Out.Fist[f][k] = FistLocal(Joint[k]).GetRotation();
		}
		if (f == 0)
		{
			const FTransform Base = FistLocal(Joint[0]) * Parent;
			Out.Thenar[0] = Base.GetLocation();
			Out.Thenar[1] = (FistLocal(Joint[1]) * Base).GetLocation();
		}
		else
		{
			Out.Knuckle[f - 1] = (FistLocal(Joint[0]) * Parent).GetLocation();
			Out.Tip[f - 1] = J3.GetLocation();
			Out.Metacarpal[f - 1] = Metacarpal.GetLocation();
		}
	}
	return true;
}

bool FMOHandGrip::SeatInHand(const FHandInFist& Hand, bool bLeftHand, EMOGripStyle Style, const FVector& HalfSize, float HandOffset,
	FTransform& OutHandleInHand, float& OutHandScale, FString* OutWhy)
{
	auto Fail = [OutWhy](const FString& Why)
	{
		if (OutWhy)
		{
			*OutWhy = Why;
		}
		return false;
	};
	if (Style == EMOGripStyle::None || HalfSize.GetMin() <= 0.0f)
	{
		return Fail(TEXT("no style or handle size"));
	}
	const FVector* Knuckle = Hand.Knuckle;
	const FVector* Metacarpal = Hand.Metacarpal;

	// The four knuckles' frame: e1 little finger -> index, e2 toward the curled fingertips, e3 the same side of either hand
	const FVector O = (Knuckle[0] + Knuckle[1] + Knuckle[2] + Knuckle[3]) * 0.25f;
	const float Span = FVector::Dist(Knuckle[0], Knuckle[3]);
	const FVector E1 = (Knuckle[0] - Knuckle[3]).GetSafeNormal();
	const FVector C = (Hand.Tip[0] + Hand.Tip[1] + Hand.Tip[2] + Hand.Tip[3]) * 0.25f - O;
	const FVector E2 = (C - E1 * FVector::DotProduct(C, E1)).GetSafeNormal();
	const FVector E3 = bLeftHand ? FVector::CrossProduct(E1, E2) : FVector::CrossProduct(E2, E1);
	if (Span < 1.0f || E2.IsNearlyZero())
	{
		return Fail(TEXT("the hand has no knuckle span in the fist"));
	}
	auto FromKnuckles = [&](const FVector& V) { return E1 * V.X + E2 * V.Y + E3 * V.Z; };
	OutHandScale = Span / RefSpanCm;

	const FVector Z = FromKnuckles(PowerAxis).GetSafeNormal();
	const FVector Face = FromKnuckles(PowerFace);
	const FVector X = (Face - Z * FVector::DotProduct(Face, Z)).GetSafeNormal();
	const FQuat Rotation = FMatrix(X, FVector::CrossProduct(Z, X), Z, FVector::ZeroVector).ToQuat();
	const FVector PowerAt = O + FromKnuckles(PowerCentre) * Span;
	FVector PalmCentre = FVector::ZeroVector;
	for (int32 i = 0; i < 4; ++i)
	{
		PalmCentre += (Metacarpal[i] + Knuckle[i]) * 0.125f;
	}
	FVector ToPalm = PalmCentre - PowerAt;
	ToPalm = (ToPalm - Z * FVector::DotProduct(ToPalm, Z)).GetSafeNormal();

	// The palm: a capsule along each metacarpal to its knuckle, and the ball of the thumb (a big stone or a thick handle rests on it, not in it)
	FShape Shape;
	Shape.HalfSize = HalfSize;
	Shape.Style = Style;
	const float PalmR = PalmRadius * OutHandScale;
	const FVector ToHandle = -Z * HandOffset;   // the hand's centre to the handle's
	auto PalmClearance = [&](const FVector& At)
	{
		Shape.Frame = FTransform(Rotation, At + ToHandle);
		float Min = 99.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			for (int32 s = 0; s <= 4; ++s)
			{
				Min = FMath::Min(Min, Clearance(Shape, FMath::Lerp(Metacarpal[i], Knuckle[i], s / 4.0f), PalmR));
			}
		}
		for (int32 s = 0; s <= 4; ++s)
		{
			Min = FMath::Min(Min, Clearance(Shape, FMath::Lerp(Hand.Thenar[0], Hand.Thenar[1], s / 4.0f), FingerRadii[0][0] * OutHandScale));
		}
		return Min;
	};
	// Power: from in front of the palm toward it until the handle lies against it; Sphere: out of the palm's hollow until it does
	const bool bSphere = Style == EMOGripStyle::Sphere;
	const FVector Start = bSphere ? PalmCentre : PowerAt;
	const FVector Along = bSphere ? -ToPalm : ToPalm;
	const float Reach = 10.0f * OutHandScale;
	const float Deepest = bSphere ? 0.0f : FVector::DotProduct(PalmCentre - PowerAt, ToPalm);   // the handle's centre on the palm's
	float ClearS = bSphere ? Reach : Deepest - Reach;
	float InS = Deepest;
	if (PalmClearance(Start + Along * ClearS) < 0.0f || PalmClearance(Start + Along * InS) >= 0.0f)
	{
		return Fail(FString::Printf(TEXT("the handle (%s) never meets the palm"), *HalfSize.ToCompactString()));
	}
	for (int32 It = 0; It < 24; ++It)
	{
		const float Mid = 0.5f * (ClearS + InS);
		(PalmClearance(Start + Along * Mid) >= 0.0f ? ClearS : InS) = Mid;
	}
	OutHandleInHand = FTransform(Rotation, Start + Along * ClearS + ToHandle);
	return true;
}

bool FMOHandGrip::Seat(const USkeletalMesh* Body, const UAnimSequence* Fist, float FistTime, bool bLeftHand, EMOGripStyle Style, const FVector& HalfSize,
	float HandOffset, FTransform& OutHandleInHand, float& OutHandScale, FString* OutWhy)
{
	FHandInFist Hand;
	return ReadHand(Body, Fist, FistTime, bLeftHand, Hand, OutWhy)
		&& SeatInHand(Hand, bLeftHand, Style, HalfSize, HandOffset, OutHandleInHand, OutHandScale, OutWhy);
}

bool FMOHandGrip::Hold(const USkeletalMesh* Body, const UAnimSequence* Fist, float FistTime, bool bLeftHand, EMOGripStyle Style, const FVector& HalfSize,
	float HandOffset, FTransform& OutHandleInHand, float OutClearance[5], float OutBend[5][3], FString* OutWhy)
{
	FHandInFist Hand;
	float HandScale = 1.0f;
	if (!ReadHand(Body, Fist, FistTime, bLeftHand, Hand, OutWhy) || !SeatInHand(Hand, bLeftHand, Style, HalfSize, HandOffset, OutHandleInHand, HandScale, OutWhy))
	{
		return false;
	}
	FShape Shape;
	Shape.Frame = OutHandleInHand;
	Shape.HalfSize = HalfSize;
	Shape.Style = Style;
	for (int32 f = 0; f < 5; ++f)
	{
		FFinger Finger;
		MakeFinger(Hand.Open[f], Hand.Fist[f], f, HandScale, Finger);
		Wrap(Hand.FingerParent[f], Finger, Shape, OutBend[f]);
		OutClearance[f] = FingerClearance(Hand.FingerParent[f], Finger, Shape, OutBend[f]);
	}
	return true;
}

float FMOHandGrip::FingerClearance(const FTransform& ParentCS, const FFinger& Finger, const FShape& Shape, const float Bend[3])
{
	FVector J[4];
	FTransform T = ParentCS;
	for (int32 k = 0; k < 3; ++k)
	{
		T = FTransform(JointRotation(Finger, k, Bend[k]), Finger.Open[k].GetTranslation()) * T;
		J[k] = T.GetLocation();
	}
	J[3] = J[2] + (J[2] - J[1]) * TipFraction;
	float Min = 99.0f;
	for (int32 k = 0; k < 3; ++k)
	{
		const float R1 = k < 2 ? Finger.Radii[k + 1] : Finger.Radii[2] * 0.9f;
		for (int32 i = 1; i <= 4; ++i)
		{
			const float F = i / 4.0f;
			Min = FMath::Min(Min, Clearance(Shape, FMath::Lerp(J[k], J[k + 1], F), FMath::Lerp(Finger.Radii[k], R1, F)));
		}
	}
	return Min;
}
