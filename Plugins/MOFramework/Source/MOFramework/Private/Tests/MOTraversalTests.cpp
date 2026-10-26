// The traversal decisions (MOTraversal.h): which move a ledge allows, how long it takes, the path's shape, what is climbable. Pure: no world.

#include "Misc/AutomationTest.h"
#include "MOTraversal.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTraversal_ClassifyLedges,
	"MOFramework.Traversal.ClassifyLedges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOTraversal_ClassifyLedges::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("a 40 cm ledge is a step, not a traversal"), FMOTraversal::Classify(40.0f, true, false), EMOTraversal::None);
	TestEqual(TEXT("a 90 cm platform: hop up"), FMOTraversal::Classify(90.0f, true, false), EMOTraversal::HopUp);
	TestEqual(TEXT("a 90 cm fence with ground behind: vault"), FMOTraversal::Classify(90.0f, false, true), EMOTraversal::Vault);
	TestEqual(TEXT("a 90 cm thin log one could also stand on: vault (the lower landing)"), FMOTraversal::Classify(90.0f, true, true), EMOTraversal::Vault);
	TestEqual(TEXT("waist high with no room on top or behind: nothing"), FMOTraversal::Classify(100.0f, false, false), EMOTraversal::None);
	TestEqual(TEXT("a 180 cm ledge with room on top: pull up"), FMOTraversal::Classify(180.0f, true, false), EMOTraversal::PullUp);
	TestEqual(TEXT("a 180 cm wall: no vault over it (too high)"), FMOTraversal::Classify(180.0f, false, true), EMOTraversal::None);
	TestEqual(TEXT("2.5 m: out of reach (a climb, if climbable)"), FMOTraversal::Classify(250.0f, true, false), EMOTraversal::None);

	TestEqual(TEXT("a vault takes 0.55 s"), FMOTraversal::DurationSeconds(EMOTraversal::Vault, 90.0f), 0.55f);
	TestTrue(TEXT("a higher hop takes longer"), FMOTraversal::DurationSeconds(EMOTraversal::HopUp, 115.0f) > FMOTraversal::DurationSeconds(EMOTraversal::HopUp, 60.0f));
	TestTrue(TEXT("a pull-up takes longer than a hop"), FMOTraversal::DurationSeconds(EMOTraversal::PullUp, 125.0f) > FMOTraversal::DurationSeconds(EMOTraversal::HopUp, 115.0f));
	TestTrue(TEXT("a full-reach pull-up takes about 1.6 s"), FMath::IsNearlyEqual(FMOTraversal::DurationSeconds(EMOTraversal::PullUp, 220.0f), 1.6f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTraversal_PathShape,
	"MOFramework.Traversal.PathShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOTraversal_PathShape::RunTest(const FString& Parameters)
{
	// A pull-up onto a 150 cm ledge 60 cm ahead: rise 158 cm straight up beside the face, over 60 cm, settle 8 cm onto it.
	FMOTraversal::FPath Path;
	Path.Points[0] = FVector(0, 0, 0);
	Path.Points[1] = FVector(0, 0, 158);
	Path.Points[2] = FVector(60, 0, 158);
	Path.Points[3] = FVector(60, 0, 150);
	TestTrue(TEXT("starts where the body is"), FMOTraversal::Evaluate(Path, 0.0f).Equals(Path.Points[0]));
	TestTrue(TEXT("ends standing on the ledge"), FMOTraversal::Evaluate(Path, 1.0f).Equals(Path.Points[3]));
	const float Total = 158.0f + 60.0f + 8.0f;
	const FVector MidRise = FMOTraversal::Evaluate(Path, 79.0f / Total);
	TestTrue(TEXT("half way up the rise the body is still beside the face, not in it"), MidRise.Equals(FVector(0, 0, 79), 0.01f));
	const FVector Over = FMOTraversal::Evaluate(Path, (158.0f + 30.0f) / Total);
	TestTrue(TEXT("over the edge it travels at the risen height"), Over.Equals(FVector(30, 0, 158), 0.01f));
	TestTrue(TEXT("a fraction past the end clamps"), FMOTraversal::Evaluate(Path, 1.5f).Equals(Path.Points[3]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTraversal_Climbable,
	"MOFramework.Traversal.Climbable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOTraversal_Climbable::RunTest(const FString& Parameters)
{
	auto Classify = [](bool bTree, bool bRock, bool bClimbable, bool bVoxel, bool bBuilt, const TCHAR* Path, float NormalZ)
	{
		return FMOTraversal::ClassifyClimbSurface(bTree, bRock, bClimbable, bVoxel, bBuilt, Path, NormalZ);
	};
	TestEqual(TEXT("a PCG tree trunk"), Classify(true, false, false, false, false, TEXT(""), 0.0f), EMOClimbSurface::Tree);
	TestEqual(TEXT("a landmark crag"), Classify(false, true, false, false, false, TEXT(""), 0.1f), EMOClimbSurface::Rock);
	TestEqual(TEXT("an explicitly climbable thing"), Classify(false, false, true, false, false, TEXT(""), 0.0f), EMOClimbSurface::Rock);
	TestEqual(TEXT("a cliff face of the terrain (75 degrees)"), Classify(false, false, false, true, false, TEXT(""), 0.26f), EMOClimbSurface::Rock);
	TestEqual(TEXT("a walkable terrain slope (30 degrees) is not climbed"), Classify(false, false, false, true, false, TEXT(""), 0.87f), EMOClimbSurface::None);
	TestEqual(TEXT("a boulder by its mesh"), Classify(false, false, false, false, false, TEXT("/Game/Megascans/Boulder_Large_01"), 0.0f), EMOClimbSurface::Rock);
	TestEqual(TEXT("a tree by its mesh"), Classify(false, false, false, false, false, TEXT("/Game/BlackAlder/SM_BlackAlder_Tree_03"), 0.0f), EMOClimbSurface::Tree);
	TestEqual(TEXT("a built wall is not climbed, whatever its mesh"), Classify(false, true, false, false, true, TEXT("/Game/Stone_Wall"), 0.0f), EMOClimbSurface::None);
	TestEqual(TEXT("CONTROL: a plain mesh is not climbable"), Classify(false, false, false, false, false, TEXT("/Engine/BasicShapes/Cube"), 0.0f), EMOClimbSurface::None);
	TestTrue(TEXT("a trunk climbs slower than rock"), FMOTraversal::ClimbSpeedCm(EMOClimbSurface::Tree) < FMOTraversal::ClimbSpeedCm(EMOClimbSurface::Rock));
	TestEqual(TEXT("nothing to hold: no speed"), FMOTraversal::ClimbSpeedCm(EMOClimbSurface::None), 0.0f);
	return true;
}

#endif
