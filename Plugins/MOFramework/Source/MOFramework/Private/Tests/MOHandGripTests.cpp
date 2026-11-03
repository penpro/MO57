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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHandGrip_Support,
	"MOFramework.Animation.HandGrip.Support",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHandGrip_Support::RunTest(const FString& Parameters)
{
	// A spear's shaft held at its origin along +Z (the carrying hand's seat), 60 cm of it toward the butt and 140 toward the head; the free
	// hand's seat 60 cm up it and 10 cm off it (9.5 deg off the axis: the spear set's two-hand clips put it 8-14), its own axis along the line
	const FVector2D Run(-60.0, 140.0);
	auto Shaft = []()
	{
		FMOHandGrip::FShape S;
		S.HalfSize = FVector(1.3, 1.2, 40.0);
		S.Style = EMOGripStyle::Power;
		return S;
	};
	auto SeatAt = [](const FVector& At, const FVector& Along)
	{
		return FTransform(FRotationMatrix::MakeFromZX(Along, FVector::XAxisVector).ToQuat(), At);
	};
	auto OffAxis = [](const FMOHandGrip::FShape& S, const FVector& P)
	{
		const FVector Rel = P - S.Frame.GetLocation();
		const FVector Z = S.Frame.GetUnitAxis(EAxis::Z);
		return (Rel - Z * FVector::DotProduct(Rel, Z)).Size();
	};
	const FVector Seat(10.0, 0.0, 60.0);
	FMOHandGrip::FShape Stretch;

	FMOHandGrip::FShape H = Shaft();
	float W = FMOHandGrip::Support(H, SeatAt(Seat, Seat.GetSafeNormal()), nullptr, nullptr, Run, 1.0f, Stretch);
	TestTrue(FString::Printf(TEXT("the free hand along the shaft holds it fully (%.3f)"), W), W > 0.999f);
	TestTrue(FString::Printf(TEXT("... the shaft turned through its seat (%.3f cm off)"), OffAxis(H, Seat)), OffAxis(H, Seat) < 0.01f);
	TestTrue(TEXT("... about the carrying hand's grip (it stays where it was)"), H.Frame.GetLocation().Equals(FVector::ZeroVector, 1e-4));
	TestTrue(FString::Printf(TEXT("... the stretch the fingers close on is round the seat (%s, half %.1f)"), *Stretch.Frame.GetLocation().ToCompactString(),
		Stretch.HalfSize.Z), FVector::Dist(Stretch.Frame.GetLocation(), Seat) < 0.01f
		&& FMath::IsNearlyEqual(Stretch.HalfSize.Z, static_cast<double>(FMOHandGrip::SupportReachCm), 1e-3) && FMath::IsNearlyEqual(Stretch.HalfSize.X, 1.3, 1e-4));

	H = Shaft();
	W = FMOHandGrip::Support(H, SeatAt(Seat, -Seat.GetSafeNormal()), nullptr, nullptr, Run, 1.0f, Stretch);
	TestTrue(FString::Printf(TEXT("the free hand facing the other way round holds it too (%.3f)"), W), W > 0.999f && OffAxis(H, Seat) < 0.01f);

	const FVector Butt(0.0, 3.0, -40.0);
	H = Shaft();
	W = FMOHandGrip::Support(H, SeatAt(Butt, FVector::ZAxisVector), nullptr, nullptr, Run, 1.0f, Stretch);
	TestTrue(FString::Printf(TEXT("toward the butt: turned through it, held (%.3f, %.3f cm off)"), W, OffAxis(H, Butt)), W > 0.999f && OffAxis(H, Butt) < 0.01f);

	// Half the grip's blend (it closes or lets go): half the turn, the shaft still 7 cm off the hand, so its fingers not on it yet
	const FVector Wide(14.0, 0.0, 60.0);
	H = Shaft();
	W = FMOHandGrip::Support(H, SeatAt(Wide, Wide.GetSafeNormal()), nullptr, nullptr, Run, 0.5f, Stretch);
	const float HalfTurn = FMath::RadiansToDegrees(FMath::Acos(H.Frame.GetUnitAxis(EAxis::Z).Z));
	TestTrue(FString::Printf(TEXT("half the grip's blend: half the turn (%.2f deg of 13.13), the fingers not yet on it (%.3f)"), HalfTurn, W),
		FMath::IsNearlyEqual(HalfTurn, 0.5f * FMath::RadiansToDegrees(FMath::Atan(14.0f / 60.0f)), 0.05f) && W == 0.0f);

	// Where a hand does not hold it: the handle as it was, the fingers as animated
	struct FMiss { const TCHAR* Why; FVector At; FVector Along; FVector2D Run; EMOGripStyle Style; };
	const FMiss Misses[] = {
		{TEXT("the free hand 40 deg off the shaft (a one-hand clip: 39-46)"), FVector(50.0, 0.0, 60.0), FVector(50.0, 0.0, 60.0).GetSafeNormal(), Run, EMOGripStyle::Power},
		{TEXT("its knuckles across the line (a one-hand clip: 54-87 deg)"), Seat, FVector(1.0, 0.0, -0.17).GetSafeNormal(), Run, EMOGripStyle::Power},
		{TEXT("past the item's head"), FVector(10.0, 0.0, 150.0), FVector(10.0, 0.0, 150.0).GetSafeNormal(), Run, EMOGripStyle::Power},
		{TEXT("beside the carrying hand"), FVector(1.0, 0.0, 5.0), FVector::ZAxisVector, Run, EMOGripStyle::Power},
		{TEXT("an item no second hand holds (no run)"), Seat, Seat.GetSafeNormal(), FVector2D::ZeroVector, EMOGripStyle::Power},
		{TEXT("a stone"), Seat, Seat.GetSafeNormal(), Run, EMOGripStyle::Sphere}};
	for (const FMiss& Miss : Misses)
	{
		H = Shaft();
		H.Style = Miss.Style;
		W = FMOHandGrip::Support(H, SeatAt(Miss.At, Miss.Along), nullptr, nullptr, Miss.Run, 1.0f, Stretch);
		TestTrue(FString::Printf(TEXT("%s: not held (%.3f), the handle not turned"), Miss.Why, W), W == 0.0f && H.Frame.GetRotation().Equals(FQuat::Identity, 1e-5f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHandGrip_TwoHands,
	"MOFramework.Animation.HandGrip.TwoHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHandGrip_TwoHands::RunTest(const FString& Parameters)
{
	// Every base's free hand on a spear's shaft as the two-hand clips put it: its seat 55 cm up the line from the carrying hand's grip, the
	// shaft as that grip holds it 12 deg off the line (the clips: 8-14), the free hand's own axis 0 or 12 deg off it (the clips: 5-16). The
	// shaft turns through the free hand's seat and its four fingers close on it, in that hand's own space.
	const UAnimSequence* Fist = LoadObject<UAnimSequence>(nullptr, TEXT("/MOFramework/Characters/Bow/RMB_Loop_MH.RMB_Loop_MH"));
	if (!Fist)
	{
		AddWarning(TEXT("no /MOFramework/Characters/Bow/RMB_Loop_MH (local Sparrow content): skipped"));
		return true;
	}
	const FVector Half(1.34, 1.23, 40.0);   // FlintSpear01's shaft
	const FVector2D Run(-60.0, 140.0);
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
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			const bool bLeft = Hand == 0;
			FMOHandGrip::FHandInFist Fingers;
			FTransform Seat;
			float Scale = 1.0f;
			FString Why;
			if (!TestTrue(FString::Printf(TEXT("%s %s hand seats the shaft"), *Base.Id.ToString(), bLeft ? TEXT("left") : TEXT("right")),
				FMOHandGrip::ReadHand(Body, Fist, 0.33f, bLeft, Fingers, &Why) && FMOHandGrip::SeatInHand(Fingers, bLeft, EMOGripStyle::Power, Half, 0.0f, Seat, Scale, &Why)))
			{
				AddError(Why);
				continue;
			}
			for (const float TurnDeg : {0.0f, 12.0f})
			{
				const FVector SeatX = Seat.GetUnitAxis(EAxis::X);
				const FVector Line = FQuat(SeatX, FMath::DegreesToRadians(TurnDeg)).RotateVector(Seat.GetUnitAxis(EAxis::Z));
				const FVector Tilt = FVector::CrossProduct(Line, SeatX).GetSafeNormal();
				FMOHandGrip::FShape Handle;
				Handle.Frame = FTransform(FQuat(Tilt, FMath::DegreesToRadians(12.0f)) * FQuat(SeatX, FMath::DegreesToRadians(TurnDeg)) * Seat.GetRotation(),
					Seat.GetLocation() - Line * 55.0f);
				Handle.HalfSize = Half;
				Handle.Style = EMOGripStyle::Power;
				FMOHandGrip::FPalm Palm;
				for (int32 i = 0; i < 4; ++i)
				{
					Palm.Metacarpal[i] = Fingers.Metacarpal[i];
					Palm.Knuckle[i] = Fingers.Knuckle[i];
				}
				Palm.Radius = FMOHandGrip::PalmRadius * Scale;
				FMOHandGrip::FShape Stretch;
				const float W = FMOHandGrip::Support(Handle, Seat, nullptr, &Palm, Run, 1.0f, Stretch);
				const FString Label = FString::Printf(TEXT("%s %s hand, its axis %.0f deg off the line"), *Base.Id.ToString(), bLeft ? TEXT("left") : TEXT("right"), TurnDeg);
				if (!TestTrue(FString::Printf(TEXT("%s: holds it (%.3f)"), *Label, W), W > 0.999f))
				{
					continue;
				}
				float Clear[5];
				for (int32 f = 0; f < 5; ++f)
				{
					FMOHandGrip::FFinger Finger;
					FMOHandGrip::MakeFinger(Fingers.Open[f], Fingers.Fist[f], f, Scale, Finger);
					float Bend[3];
					FMOHandGrip::Wrap(Fingers.FingerParent[f], Finger, Stretch, Bend);
					Clear[f] = FMOHandGrip::FingerClearance(Fingers.FingerParent[f], Finger, Stretch, Bend);
				}
				for (int32 f = 1; f < 5; ++f)
				{
					TestTrue(FString::Printf(TEXT("%s: the %s finger on the shaft (%.3f cm)"), *Label, FMOHandGrip::FingerNames[f], Clear[f]),
						Clear[f] >= -0.02f && Clear[f] <= 0.1f);
				}
				TestTrue(FString::Printf(TEXT("%s: the thumb not in it (%.3f cm)"), *Label, Clear[0]), Clear[0] >= -0.3f);
				// Turned across the hand's own axis, a shaft through its seat's centre would cross the cupped palm at one edge (0.6-0.8 cm in on
				// every base at 12 deg, Nov 2): it slides out until it only touches
				const float PalmClear = FMOHandGrip::PalmClearance(Palm, Stretch);
				TestTrue(FString::Printf(TEXT("%s: the shaft on the palm, not in it (%.3f cm)"), *Label, PalmClear), PalmClear >= -0.02f && PalmClear <= 0.1f);
			}
		}
	}
	TestTrue(TEXT("at least one base's body loaded"), Bodies > 0);
	return true;
}

#endif
