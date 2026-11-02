// A hand closing on what it holds (FMOHandGrip; Nov 2 2026, Wes: "her fingers are back to being all fucked up", then "make sure it works on the
// other metahumans and make sure we have a standard for how they grip other items"): each finger bends from open toward full flexion until a
// phalanx touches the handle -- never into it, never stopping short of it -- and every base's hand holds a handle against its palm.

#include "Misc/AutomationTest.h"
#include "MOHandGrip.h"
#include "MOSurvivorBase.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOHandGripTest
{
	// An index finger along +X from the origin: phalanges 4, 2.5 and 2 cm, curling about +Y toward -Z (the palm's side), 80 deg a joint in the fist
	FMOHandGrip::FFinger Finger()
	{
		const FTransform Open[3] = {FTransform(FVector::ZeroVector), FTransform(FVector(4.0, 0.0, 0.0)), FTransform(FVector(2.5, 0.0, 0.0))};
		const FQuat Fist[3] = {FQuat(FVector::YAxisVector, FMath::DegreesToRadians(80.0f)), FQuat(FVector::YAxisVector, FMath::DegreesToRadians(80.0f)),
			FQuat(FVector::YAxisVector, FMath::DegreesToRadians(80.0f))};
		FMOHandGrip::FFinger Out;
		FMOHandGrip::MakeFinger(Open, Fist, 1, 1.0f, Out);
		return Out;
	}

	/** A handle across the finger's path (its axis along Y, the finger's flexion axis), centred at C. */
	FMOHandGrip::FShape Handle(const FVector& C, const FVector& Half)
	{
		FMOHandGrip::FShape Shape;
		Shape.Frame = FTransform(FRotationMatrix::MakeFromXZ(FVector::XAxisVector, FVector::YAxisVector).ToQuat(), C);
		Shape.HalfSize = Half;
		Shape.Style = EMOGripStyle::Power;
		return Shape;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHandGrip_Clearance,
	"MOFramework.Animation.HandGrip.Clearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHandGrip_Clearance::RunTest(const FString& Parameters)
{
	FMOHandGrip::FShape H;   // a handle along Z at the origin, 1 x 2 cm, 5 cm each way
	H.HalfSize = FVector(1.0, 2.0, 5.0);
	H.Style = EMOGripStyle::Power;
	TestEqual(TEXT("3 cm out across the narrow side, a 0.5 cm finger: 1.5 cm clear"), FMOHandGrip::Clearance(H, FVector(3.0, 0.0, 0.0), 0.5f), 1.5f, 1e-4f);
	TestEqual(TEXT("4 cm out across the deep side: 1.5 cm clear"), FMOHandGrip::Clearance(H, FVector(0.0, 4.0, 2.0), 0.5f), 1.5f, 1e-4f);
	TestTrue(TEXT("inside it: negative"), FMOHandGrip::Clearance(H, FVector(0.5, 0.0, 0.0), 0.5f) < 0.0f);
	TestEqual(TEXT("1 cm past its end, over the axis: 1 cm less the finger"), FMOHandGrip::Clearance(H, FVector(0.0, 0.0, 6.0), 0.5f), 0.5f, 1e-4f);
	TestTrue(TEXT("far past its end: nowhere near"), FMOHandGrip::Clearance(H, FVector(0.0, 0.0, 20.0), 0.5f) > 50.0f);

	FMOHandGrip::FShape S;   // a stone: 3 x 2 x 4 cm half sizes
	S.HalfSize = FVector(3.0, 2.0, 4.0);
	S.Style = EMOGripStyle::Sphere;
	TestEqual(TEXT("a stone, 5 cm out along its X: 2 cm less the finger"), FMOHandGrip::Clearance(S, FVector(5.0, 0.0, 0.0), 0.5f), 1.5f, 1e-4f);
	TestEqual(TEXT("... along its thin Y: 3 cm clear"), FMOHandGrip::Clearance(S, FVector(0.0, 5.0, 0.0), 0.0f), 3.0f, 1e-4f);
	TestTrue(TEXT("... past where a cylinder would end, a stone is round: 1 cm past its Z tip is 1 cm out"),
		FMath::IsNearlyEqual(FMOHandGrip::Clearance(S, FVector(0.0, 0.0, 5.0), 0.0f), 1.0f, 1e-4f));
	TestTrue(TEXT("inside the stone: negative"), FMOHandGrip::Clearance(S, FVector(1.0, 1.0, 1.0), 0.0f) < 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHandGrip_Wrap,
	"MOFramework.Animation.HandGrip.Wrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHandGrip_Wrap::RunTest(const FString& Parameters)
{
	using namespace MOHandGripTest;
	FMOHandGrip::FFinger F = Finger();
	const FVector Half(1.5, 1.5, 5.0);
	float Bend[3];

	TestEqual(TEXT("full flexion: the knuckle 90 deg (the fist's 80 is not its limit)"), FMath::RadiansToDegrees(F.LimitRad[0]), 90.0f, 1e-3f);
	TestEqual(TEXT("the middle joint 110"), FMath::RadiansToDegrees(F.LimitRad[1]), 110.0f, 1e-3f);
	TestEqual(TEXT("the end joint keeps the fist's 80"), FMath::RadiansToDegrees(F.LimitRad[2]), 80.0f, 1e-3f);

	// A handle under the finger: open it is clear; closed on it, the finger touches without going in, and stops short of full flexion
	const FMOHandGrip::FShape Under = Handle(FVector(3.5, 0.0, -2.8), Half);
	const float Open[3] = {0.0f, 0.0f, 0.0f};
	TestTrue(TEXT("the open finger clears the handle"), FMOHandGrip::FingerClearance(FTransform::Identity, F, Under, Open) > 0.0f);
	FMOHandGrip::Wrap(FTransform::Identity, F, Under, Bend);
	const float Touch = FMOHandGrip::FingerClearance(FTransform::Identity, F, Under, Bend);
	TestTrue(FString::Printf(TEXT("closed on it: touching (%.3f cm)"), Touch), Touch >= -0.01f && Touch <= 0.05f);
	TestTrue(TEXT("the knuckle bends"), Bend[0] > 0.05f);
	TestTrue(TEXT("a joint stops short of full flexion"), Bend[0] < 1.0f || Bend[1] < 1.0f || Bend[2] < 1.0f);
	TestTrue(TEXT("the joints past the first contact bend at least as far (a tendon closes them together)"), Bend[1] >= Bend[0] - 1e-4f && Bend[2] >= Bend[1] - 1e-4f);

	// A thick handle stops the fingers sooner than a thin one in the same place
	float Thin[3];
	FMOHandGrip::Wrap(FTransform::Identity, F, Handle(FVector(3.5, 0.0, -2.8), FVector(0.8, 0.8, 5.0)), Thin);
	TestTrue(TEXT("a thinner handle: the finger closes further"), Thin[0] + Thin[1] + Thin[2] > Bend[0] + Bend[1] + Bend[2]);

	// Nothing in reach: full flexion
	FMOHandGrip::Wrap(FTransform::Identity, F, Handle(FVector(3.0, 0.0, -40.0), Half), Bend);
	TestTrue(TEXT("no handle in reach: full flexion"), Bend[0] == 1.0f && Bend[1] == 1.0f && Bend[2] == 1.0f);

	// An open finger grazing the handle straightens off it (it lies along it instead of in it)
	const FMOHandGrip::FShape Graze = Handle(FVector(3.5, 0.0, -2.2), Half);
	TestTrue(TEXT("the open finger grazes into it"), FMOHandGrip::FingerClearance(FTransform::Identity, F, Graze, Open) < 0.0f);
	FMOHandGrip::Wrap(FTransform::Identity, F, Graze, Bend);
	TestTrue(TEXT("... it straightens"), Bend[0] < 0.0f && Bend[0] >= -FMOHandGrip::MaxExtension);
	TestTrue(TEXT("... and clears it"), FMOHandGrip::FingerClearance(FTransform::Identity, F, Graze, Bend) >= 0.0f);
	TestTrue(FString::Printf(TEXT("... its base on the handle, the joints past it close round it (bends %.2f / %.2f / %.2f)"), Bend[0], Bend[1], Bend[2]),
		Bend[1] > Bend[0] + 0.05f || Bend[2] > Bend[0] + 0.05f);

	// A thumb open across a thin handle goes over it and closes on it from the far side (the MetaHuman's open thumb lies across a handle
	// seated against the palm); a finger in the same place straightens off it instead
	const FMOHandGrip::FShape Across = Handle(FVector(3.5, 0.0, -1.4), FVector(0.8, 0.8, 5.0));
	FMOHandGrip::FFinger Thumb = F;
	Thumb.bThumb = true;
	TestTrue(TEXT("the open thumb lies in the handle"), FMOHandGrip::FingerClearance(FTransform::Identity, Thumb, Across, Open) < 0.0f);
	FMOHandGrip::Wrap(FTransform::Identity, Thumb, Across, Bend);
	const float Over = FMOHandGrip::FingerClearance(FTransform::Identity, Thumb, Across, Bend);
	TestTrue(FString::Printf(TEXT("... it goes over it: closed past it, out of it (bend %.2f, %.3f cm)"), Bend[0], Over), Bend[0] > 0.3f && Over >= -0.01f);
	FMOHandGrip::Wrap(FTransform::Identity, F, Across, Bend);
	TestTrue(TEXT("a finger there straightens off it"), Bend[0] < 0.0f && FMOHandGrip::FingerClearance(FTransform::Identity, F, Across, Bend) >= 0.0f);

	// The open finger right through the handle (a bad seat), too deep to straighten off: it rests where along its range it is least in it
	const FMOHandGrip::FShape Through = Handle(FVector(3.0, 0.0, 0.0), Half);
	FMOHandGrip::Wrap(FTransform::Identity, F, Through, Bend);
	const float Least = FMOHandGrip::FingerClearance(FTransform::Identity, F, Through, Bend);
	TestTrue(FString::Printf(TEXT("through the open finger: no deeper than open (%.2f cm against %.2f)"), Least,
		FMOHandGrip::FingerClearance(FTransform::Identity, F, Through, Open)), Least > FMOHandGrip::FingerClearance(FTransform::Identity, F, Through, Open));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHandGrip_EveryBase,
	"MOFramework.Animation.HandGrip.EveryBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHandGrip_EveryBase::RunTest(const FString& Parameters)
{
	// The standard fist (a power grip: Sparrow's bow hand at full draw) is local content (Fab: Paragon Sparrow, retargeted by
	// Content/Python/retarget_weapon_sets.py); a clone without it skips this test.
	const UAnimSequence* Fist = LoadObject<UAnimSequence>(nullptr, TEXT("/MOFramework/Characters/Bow/RMB_Loop_MH.RMB_Loop_MH"));
	if (!Fist)
	{
		AddWarning(TEXT("no /MOFramework/Characters/Bow/RMB_Loop_MH (local Sparrow content): skipped"));
		return true;
	}
	struct FCase { const TCHAR* Name; EMOGripStyle Style; FVector Half; float HandOffset; };
	const FCase Cases[] = {
		{TEXT("a bow's grip"), EMOGripStyle::Power, FVector(2.15, 1.85, 5.0), 1.61f},   // CompositeBow01's
		{TEXT("an axe's haft"), EMOGripStyle::Power, FVector(1.5, 1.8, 6.0), 0.0f},
		{TEXT("a stone"), EMOGripStyle::Sphere, FVector(3.5, 2.5, 4.0), 0.0f}};
	int32 Bodies = 0;
	for (const FMOSurvivorBase& Base : UMOSurvivorBaseSettings::GetBases())
	{
		const USkeletalMesh* Body = Cast<USkeletalMesh>(Base.Body.TryLoad());
		if (!Body)
		{
			AddWarning(FString::Printf(TEXT("%s: body %s did not load"), *Base.Id.ToString(), *Base.Body.ToString()));
			continue;
		}
		++Bodies;
		for (const FCase& Case : Cases)
		{
			float Clear[2][5];
			for (int32 Hand = 0; Hand < 2; ++Hand)
			{
				FTransform HandleInHand;
				float Bend[5][3];
				FString Why;
				if (!TestTrue(FString::Printf(TEXT("%s %s hand holds %s"), *Base.Id.ToString(), Hand == 0 ? TEXT("left") : TEXT("right"), Case.Name),
					FMOHandGrip::Hold(Body, Fist, 0.33f, Hand == 0, Case.Style, Case.Half, Case.HandOffset, HandleInHand, Clear[Hand], Bend, &Why)))
				{
					AddError(Why);
					continue;
				}
				for (int32 f = 1; f < 5; ++f)   // the four fingers on it (the thumb only kept out of it: it lies over them on a thin handle)
				{
					TestTrue(FString::Printf(TEXT("%s %s hand, %s: the %s finger on it (%.3f cm)"), *Base.Id.ToString(), Hand == 0 ? TEXT("left") : TEXT("right"),
						Case.Name, FMOHandGrip::FingerNames[f], Clear[Hand][f]), Clear[Hand][f] >= -0.02f && Clear[Hand][f] <= 0.1f);
				}
				TestTrue(FString::Printf(TEXT("%s %s hand, %s: the thumb not in it (%.3f cm; Tools/check_grip.py's PEN_CM)"), *Base.Id.ToString(),
					Hand == 0 ? TEXT("left") : TEXT("right"), Case.Name, Clear[Hand][0]), Clear[Hand][0] >= -0.3f);
			}
			for (int32 f = 0; f < 5; ++f)   // the hands are mirror images: the right holds it as the left does (a Creator body's thumbs differ a little:
			{                               // Harvey's 0.27 cm round a stone)
				TestTrue(FString::Printf(TEXT("%s, %s: the %s finger the same in either hand (%.3f / %.3f)"), *Base.Id.ToString(), Case.Name,
					FMOHandGrip::FingerNames[f], Clear[0][f], Clear[1][f]), FMath::Abs(Clear[0][f] - Clear[1][f]) <= (f == 0 ? 0.5f : 0.05f));
			}
		}
	}
	TestTrue(TEXT("at least one base's body loaded"), Bodies > 0);
	return true;
}

#endif
