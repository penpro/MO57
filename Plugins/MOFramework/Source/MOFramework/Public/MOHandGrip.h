#pragma once

#include "CoreMinimal.h"
#include "MOItemDefinitionRow.h"

class UAnimSequence;
class USkeletalMesh;

/**
 * How a hand holds what it holds: one standard for every held item on every body (Docs/Held_Grip.md; Nov 2 2026, Wes: "make sure it works on
 * the other metahumans and make sure we have a standard for how they grip other items"). The item says where its handle is in its own mesh
 * (FMOItemHeldVisual::GripStyle / GripTransform / GripHalfSize); the body's hand does the rest, so the bases' different hands (a knuckle span of
 * 5.6 cm on Femme, 7.3 on Harvey) each hold it right:
 *  - Seat (AMOCharacter, when the item is equipped or the body changes): the handle's place in THIS body's hand, from its bones in the fist
 *    (the item's GripPose). A Power grip lies where Sparrow's bow lies in her fist, measured in the frame of the four knuckles and scaled by
 *    their span, then slid against the palm; a Sphere sits in the palm's hollow against it.
 *  - Fingers (UMOBodyRetargetAnimInstance, every evaluation): each closes from the open hand toward full flexion -- along the way the fist
 *    flexes it, on to a human joint's range -- until a phalanx touches the handle (a tendon's closing: the joints up to the touching phalanx
 *    stop, the rest go on). A hand bigger than the fist was made for closes further round a thin handle instead of stopping in the air.
 * Pure functions: the tests and Content/Python/held_grip.py (the same numbers offline) use them as the game does.
 */
struct MOFRAMEWORK_API FMOHandGrip
{
	/** The handle as the hand sees it: its frame (the item's GripTransform, placed), half sizes (cm) and shape. */
	struct FShape
	{
		FTransform Frame = FTransform::Identity;
		FVector HalfSize = FVector::ZeroVector;
		EMOGripStyle Style = EMOGripStyle::None;
	};

	/** One finger's closing: its joints' open (reference) transforms, each joint's flexion axis (the fist's turn from open) and how far it can
	 *  go (FlexLimitDeg, or the fist's own turn if that is further), and the phalanges' capsule radii for this hand. */
	struct FFinger
	{
		FTransform Open[3];
		FVector Axis[3] = {FVector::YAxisVector, FVector::YAxisVector, FVector::YAxisVector};
		float LimitRad[3] = {0.0f, 0.0f, 0.0f};
		float Radii[3] = {0.0f, 0.0f, 0.0f};
		bool bThumb = false;   // open in the handle's way, it goes over it (a finger straightens off it instead)
		FQuat BaseSwing = FQuat::Identity;   // a thumb swung out from a handle too thick to go over (Wrap sets it), before the base's flexion
	};

	/** The fingers (thumb first) as the MetaHuman names them; each has joints 01..03 (the thumb's 01 is its metacarpal). */
	static const TCHAR* const FingerNames[5];

	/** Capsule radii round each phalanx on the reference hand (cm; thumb .. pinky x proximal, middle, distal: a woman's fingers, 1.4-1.8 cm
	 *  thick at the proximal phalanx); a hand's are these times its knuckle span over RefSpanCm. The fingertip lies TipFraction of the middle
	 *  phalanx beyond the last joint. */
	static const float FingerRadii[5][3];
	static constexpr float TipFraction = 0.85f;

	/** Full flexion of each joint from the open hand (deg; thumb .. pinky x 01, 02, 03): a human hand's range (knuckle 90, middle joint 110,
	 *  end joint 80; the thumb's base 50, knuckle 60, end joint 80). */
	static const float FlexLimitDeg[5][3];

	/** The palm's soft side off the metacarpals (cm, reference hand) and the reference hand's knuckle span (index_01 to pinky_01: Femme's). */
	static constexpr float PalmRadius = 1.2f;
	static constexpr float RefSpanCm = 5.64f;

	/** An open finger already in the handle's way straightens, at most this fraction of its full flexion (a finger extends ~20-30 deg), and
	 *  the joints past the phalanx on the handle close round it (a thick handle); a thumb goes over it instead; neither clear, it rests where
	 *  along that range it is least in the handle. */
	static constexpr float MaxExtension = 0.25f;

	/** A point's clearance to the handle (cm, negative inside) for a sphere of Radius round it: out from a Power handle's side along the ray
	 *  from its axis (past its ends, to them; 99 well beyond them), out from a Sphere's surface along the ray from its centre. */
	static float Clearance(const FShape& Shape, const FVector& Point, float Radius);

	/** A finger's closing from its open joints (the body's reference pose, local) and the fist's local rotations. Finger: 0 thumb .. 4 pinky;
	 *  HandScale: the hand's knuckle span over RefSpanCm. */
	static void MakeFinger(const FTransform OpenLocal[3], const FQuat FistLocal[3], int32 Finger, float HandScale, FFinger& Out);

	/** A thumb too thick a handle's way swings out from it at its base, up to ThumbSwingMaxDeg (the root joint's abduction), in steps. */
	static constexpr float ThumbSwingMaxDeg = 60.0f;
	static constexpr int32 ThumbSwingSteps = 24;

	/** A joint's local rotation at Bend (0 open, 1 full flexion, negative straighter than open), a thumb's base swing included. */
	static FQuat JointRotation(const FFinger& Finger, int32 Joint, float Bend);

	/** Where each joint stops for this handle (bends in hand order; a thumb's BaseSwing set). ParentCS: the finger's parent (its metacarpal or
	 *  the hand) in the space the Shape is in. */
	static void Wrap(const FTransform& ParentCS, FFinger& Finger, const FShape& Shape, float OutBend[3]);

	/** A finger's nearest approach to the handle at a bend (cm, negative inside): the capsules Wrap closes. */
	static float FingerClearance(const FTransform& ParentCS, const FFinger& Finger, const FShape& Shape, const float Bend[3]);

	/** A body's hand in the fist, in the hand bone's space: each finger's parent and open / fist joints, the four fingers' knuckles (01), last
	 *  joints (03) and metacarpals (index .. pinky), and the thumb's metacarpal. The fist's local rotations on the body's own bone lengths. */
	struct FHandInFist
	{
		FTransform FingerParent[5];
		FTransform Open[5][3];
		FQuat Fist[5][3];
		FVector Knuckle[4];
		FVector Tip[4];
		FVector Metacarpal[4];
		FVector Thenar[2];   // the thumb's metacarpal (thumb_01 to thumb_02): the ball of the thumb, part of the palm a handle rests on
	};
	static bool ReadHand(const USkeletalMesh* Body, const UAnimSequence* Fist, float FistTime, bool bLeftHand, FHandInFist& Out, FString* OutWhy = nullptr);

	/** The handle's frame in the hand bone's space for this hand, style, size and HandOffset (the item's GripHandOffset: the hand's centre from
	 *  the handle's along its axis) -- Docs/Held_Grip.md, the seat -- and the hand's scale (its knuckle span over RefSpanCm). False, with the
	 *  reason, when the handle never meets the palm. */
	static bool SeatInHand(const FHandInFist& Hand, bool bLeftHand, EMOGripStyle Style, const FVector& HalfSize, float HandOffset,
		FTransform& OutHandleInHand, float& OutHandScale, FString* OutWhy = nullptr);

	/** ReadHand then SeatInHand: where a body's hand holds the handle. */
	static bool Seat(const USkeletalMesh* Body, const UAnimSequence* Fist, float FistTime, bool bLeftHand, EMOGripStyle Style, const FVector& HalfSize,
		float HandOffset, FTransform& OutHandleInHand, float& OutHandScale, FString* OutWhy = nullptr);

	/** The whole grip in the fist's palm, as the game closes it: the seat, then each finger wrapped (thumb .. pinky), its bends and clearance. */
	static bool Hold(const USkeletalMesh* Body, const UAnimSequence* Fist, float FistTime, bool bLeftHand, EMOGripStyle Style, const FVector& HalfSize,
		float HandOffset, FTransform& OutHandleInHand, float OutClearance[5], float OutBend[5][3], FString* OutWhy = nullptr);

	/** The standard Power seat in the four knuckles' frame (e1 little finger -> index, e2 toward the fist's curled fingertips, e3 their cross;
	 *  per cm of knuckle span): the handle's centre, its axis and its face. Measured Nov 2 2026 from Sparrow's bow in her fist (RMB_Loop at
	 *  0.33) as retargeted onto Femme, the centre moved along the axis to the middle of the knuckles (held_grip.derive_standard). */
	static const FVector PowerCentre;
	static const FVector PowerAxis;
	static const FVector PowerFace;
};
