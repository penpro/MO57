// A hand closing on what it holds (FMOBodyRetargetAnimInstanceProxy::WrapFinger / HandleClearance; Nov 2 2026, Wes: "her fingers are back to being
// all fucked up"): each finger bends from open toward the fist until a phalanx touches the handle -- never into it, never stopping short of it.

#include "Misc/AutomationTest.h"
#include "MOBodyRetargetAnimInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOHandGripTest
{
	// A finger along +X from the origin: phalanges 4, 2.5 and 2 cm, curling about +Y toward -Z (the palm's side), 80 deg a joint in the fist
	const FTransform Open[3] = {FTransform(FVector::ZeroVector), FTransform(FVector(4.0, 0.0, 0.0)), FTransform(FVector(2.5, 0.0, 0.0))};
	const FQuat Fist[3] = {FQuat(FVector::YAxisVector, FMath::DegreesToRadians(80.0f)), FQuat(FVector::YAxisVector, FMath::DegreesToRadians(80.0f)),
		FQuat(FVector::YAxisVector, FMath::DegreesToRadians(80.0f))};
	const float Radii[3] = {0.85f, 0.78f, 0.68f};

	/** A handle across the finger's path (its axis along Y, the finger's flexion axis), centred at C, radius R, 5 cm each side. */
	FTransform Handle(const FVector& C)
	{
		return FTransform(FRotationMatrix::MakeFromXZ(FVector::XAxisVector, FVector::YAxisVector).ToQuat(), C);
	}

	/** The finger's nearest approach to the handle for a bend (the same joints and samples WrapFinger walks). */
	float Nearest(const FTransform& H, const FVector& Half, const float Bend[3])
	{
		FVector J[4];
		FTransform T = FTransform::Identity;
		for (int32 k = 0; k < 3; ++k)
		{
			T = FTransform(FQuat::Slerp(Open[k].GetRotation(), Fist[k], Bend[k]), Open[k].GetTranslation()) * T;
			J[k] = T.GetLocation();
		}
		J[3] = J[2] + (J[2] - J[1]) * FMOBodyRetargetAnimInstanceProxy::GripTipFraction;
		float Min = 99.0f;
		for (int32 k = 0; k < 3; ++k)
		{
			const float R1 = k < 2 ? Radii[k + 1] : Radii[2] * 0.9f;
			for (int32 i = 1; i <= 4; ++i)
			{
				const float F = i / 4.0f;
				Min = FMath::Min(Min, FMOBodyRetargetAnimInstanceProxy::HandleClearance(H, Half, FMath::Lerp(J[k], J[k + 1], F), FMath::Lerp(Radii[k], R1, F)));
			}
		}
		return Min;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHandGrip_Clearance,
	"MOFramework.Animation.HandGrip.Clearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHandGrip_Clearance::RunTest(const FString& Parameters)
{
	const FTransform H = FTransform::Identity;   // a handle along Z at the origin, 1 x 2 cm, 5 cm each way
	const FVector Half(1.0, 2.0, 5.0);
	TestEqual(TEXT("3 cm out across the narrow side, a 0.5 cm finger: 1.5 cm clear"),
		FMOBodyRetargetAnimInstanceProxy::HandleClearance(H, Half, FVector(3.0, 0.0, 0.0), 0.5f), 1.5f, 1e-4f);
	TestEqual(TEXT("4 cm out across the deep side: 1.5 cm clear"),
		FMOBodyRetargetAnimInstanceProxy::HandleClearance(H, Half, FVector(0.0, 4.0, 2.0), 0.5f), 1.5f, 1e-4f);
	TestTrue(TEXT("inside it: negative"), FMOBodyRetargetAnimInstanceProxy::HandleClearance(H, Half, FVector(0.5, 0.0, 0.0), 0.5f) < 0.0f);
	TestEqual(TEXT("1 cm past its end, over the axis: 1 cm less the finger"),
		FMOBodyRetargetAnimInstanceProxy::HandleClearance(H, Half, FVector(0.0, 0.0, 6.0), 0.5f), 0.5f, 1e-4f);
	TestTrue(TEXT("far past its end: nowhere near"), FMOBodyRetargetAnimInstanceProxy::HandleClearance(H, Half, FVector(0.0, 0.0, 20.0), 0.5f) > 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHandGrip_Wrap,
	"MOFramework.Animation.HandGrip.Wrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHandGrip_Wrap::RunTest(const FString& Parameters)
{
	using namespace MOHandGripTest;
	const FVector Half(1.5, 1.5, 5.0);
	float Bend[3];

	// A handle under the finger: open it is clear; closed on it, the finger touches without going in, and stops short of the fist
	const FTransform Under = Handle(FVector(3.5, 0.0, -2.8));
	const float OpenBend[3] = {0.0f, 0.0f, 0.0f};
	TestTrue(TEXT("the open finger clears the handle"), Nearest(Under, Half, OpenBend) > 0.0f);
	FMOBodyRetargetAnimInstanceProxy::WrapFinger(FTransform::Identity, Open, Fist, Radii, Under, Half, Bend);
	const float Touch = Nearest(Under, Half, Bend);
	TestTrue(FString::Printf(TEXT("closed on it: touching (%.3f cm)"), Touch), Touch >= -0.01f && Touch <= 0.05f);
	TestTrue(TEXT("the knuckle bends"), Bend[0] > 0.05f);
	TestTrue(TEXT("a joint stops short of the fist"), Bend[0] < 1.0f || Bend[1] < 1.0f || Bend[2] < 1.0f);
	TestTrue(TEXT("the joints past the first contact bend at least as far (a tendon closes them together)"), Bend[1] >= Bend[0] - 1e-4f && Bend[2] >= Bend[1] - 1e-4f);

	// A thick handle stops the fingers sooner than a thin one in the same place
	float Thin[3];
	FMOBodyRetargetAnimInstanceProxy::WrapFinger(FTransform::Identity, Open, Fist, Radii, Under, FVector(0.8, 0.8, 5.0), Thin);
	TestTrue(TEXT("a thinner handle: the finger closes further"), Thin[0] + Thin[1] + Thin[2] > Bend[0] + Bend[1] + Bend[2]);

	// Nothing in reach: the full fist
	FMOBodyRetargetAnimInstanceProxy::WrapFinger(FTransform::Identity, Open, Fist, Radii, Handle(FVector(3.0, 0.0, -40.0)), Half, Bend);
	TestTrue(TEXT("no handle in reach: the fist"), Bend[0] == 1.0f && Bend[1] == 1.0f && Bend[2] == 1.0f);

	// The open finger already through the handle (a bad seat): nothing bends, nothing pretends
	FMOBodyRetargetAnimInstanceProxy::WrapFinger(FTransform::Identity, Open, Fist, Radii, Handle(FVector(3.0, 0.0, 0.0)), Half, Bend);
	TestTrue(TEXT("through the open finger: it stays open"), Bend[0] == 0.0f && Bend[1] == 0.0f && Bend[2] == 0.0f);
	return true;
}

#endif
