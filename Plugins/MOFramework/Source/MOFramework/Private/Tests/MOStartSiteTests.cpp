/**
 * The start site's placement maths (MOStartSiteTypes.h): the site frame turns a prop's mesh +Y (the clone egg's opening) toward the survivor,
 * the candidate ring starts at the configured bearing and distance, and the default layout keeps the hatch between the egg and the survivor.
 * Run: python Tools/ue.py auto --filter "MOFramework.StartSite"
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MOStartSiteSubsystem.h"
#include "MOStartSiteTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOStartSite_FrameFacesTheSurvivor,
	"MOFramework.StartSite.FrameFacesTheSurvivor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOStartSite_FrameFacesTheSurvivor::RunTest(const FString& Parameters)
{
	const FVector2D Site(1000.0, -2000.0);
	for (const FVector2D& Dir : { FVector2D(0.0, 1.0), FVector2D(1.0, 0.0), FVector2D(0.0, -1.0), FVector2D(-1.0, 0.0), FVector2D(0.6, -0.8) })
	{
		const FVector2D Survivor = Site + Dir * 330.0;
		const float Yaw = MOStartSite::FrameYawDeg(Site, Survivor);
		// The prop's mesh +Y, turned by the frame yaw as the engine turns it, points at the survivor.
		const FVector MeshY = FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector::YAxisVector);
		TestTrue(FString::Printf(TEXT("mesh +Y faces a survivor toward (%.1f, %.1f)"), Dir.X, Dir.Y), FVector2D(MeshY).Equals(Dir, 1e-4));
		// A frame offset straight down +Y by the distance lands on the survivor.
		TestTrue(TEXT("offset (0, distance) is the survivor"), MOStartSite::ToWorld(Site, Yaw, FVector2D(0.0, 330.0)).Equals(Survivor, 1e-2));
		// ToWorld agrees with the engine's own rotation for an offset off both axes.
		const FVector2D Offset(-30.0, 175.0);
		const FVector2D Engine = Site + FVector2D(FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector(Offset, 0.0)));
		TestTrue(TEXT("ToWorld matches FRotator::RotateVector"), MOStartSite::ToWorld(Site, Yaw, Offset).Equals(Engine, 1e-2));
	}
	// CONTROL: the mirrored formula (atan2(x, y)) would turn the opening away from a survivor to the east.
	const FVector2D East = FVector2D(1.0, 0.0);
	const float Wrong = FMath::RadiansToDegrees(FMath::Atan2(East.X, East.Y));
	TestFalse(TEXT("CONTROL: a mirrored yaw does not face the survivor"), FVector2D(FRotator(0.0f, Wrong, 0.0f).RotateVector(FVector::YAxisVector)).Equals(East, 1e-3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOStartSite_CandidatesStartAtTheBearing,
	"MOFramework.StartSite.CandidatesStartAtTheBearing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOStartSite_CandidatesStartAtTheBearing::RunTest(const FString& Parameters)
{
	const FVector2D Survivor(500.0, 500.0);
	TArray<FVector2D> Candidates;
	MOStartSite::Candidates(Survivor, /*Facing=*/90.0f, /*Bearing=*/140.0f, /*Distance=*/330.0f, Candidates);
	TestEqual(TEXT("24 candidates (12 bearings, two reaches)"), Candidates.Num(), 24);
	if (Candidates.Num() != 24)
	{
		return false;
	}
	// The first is at facing + bearing (230 degrees), 330 cm out.
	const FVector2D First = Candidates[0] - Survivor;
	TestEqual(TEXT("first candidate 330 cm out"), First.Size(), 330.0, 0.01);
	TestEqual(TEXT("first candidate at facing + bearing"), FMath::RadiansToDegrees(FMath::Atan2(First.Y, First.X)), -130.0, 0.01);
	TestEqual(TEXT("the second ring is half as far again"), (Candidates[12] - Survivor).Size(), 495.0, 0.01);
	TestEqual(TEXT("the last of the first ring is straight opposite the first"), (Candidates[11] - Survivor).GetSafeNormal().Dot(First.GetSafeNormal()), -1.0, 1e-4);
	for (int32 A = 0; A < Candidates.Num(); ++A)
	{
		for (int32 B = A + 1; B < Candidates.Num(); ++B)
		{
			if (Candidates[A].Equals(Candidates[B], 1.0))
			{
				AddError(FString::Printf(TEXT("candidates %d and %d are the same spot"), A, B));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOStartSite_DefaultLayout,
	"MOFramework.StartSite.DefaultLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOStartSite_DefaultLayout::RunTest(const FString& Parameters)
{
	const UMOStartSiteSettings* Settings = GetDefault<UMOStartSiteSettings>();
	TestEqual(TEXT("two props: the egg and its hatch"), Settings->Props.Num(), 2);
	if (Settings->Props.Num() != 2)
	{
		return false;
	}
	TestTrue(TEXT("the egg is first (the log and the save name it)"), Settings->Props[0].Mesh.ToString().Contains(TEXT("CloneEggPod01.CloneEggPod01")));
	TestTrue(TEXT("the egg sits at the site origin"), Settings->Props[0].OffsetCm.IsNearlyZero());
	const float HatchY = Settings->Props[1].OffsetCm.Y;
	TestTrue(TEXT("the hatch lies between the egg and the survivor, at least a metre short of them"), HatchY > 0.0f && HatchY < Settings->DistanceCm - 100.0f);
	TestTrue(TEXT("both meshes exist"), Settings->Props[0].Mesh.LoadSynchronous() != nullptr && Settings->Props[1].Mesh.LoadSynchronous() != nullptr);
	return true;
}

#endif
