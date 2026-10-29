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

	// The world's rock tags (Oct 29: PCG rocks named by an id -- ukopfbks_tier_2, wgosajs_tier_2 -- were not climbable).
	TestTrue(TEXT("a harvestable rock's tag is rock"), FMOTraversal::IsRockTag(TEXT("MOResource_Rock")));
	TestTrue(TEXT("a biome's rock cover tag is rock"), FMOTraversal::IsRockTag(TEXT("MOBiomeCover_Rock")));
	TestTrue(TEXT("the landmark tier is rock"), FMOTraversal::IsRockTag(TEXT("MOLandmark")));
	TestFalse(TEXT("CONTROL: ground debris (small stones) is not"), FMOTraversal::IsRockTag(TEXT("MOBiomeCover_Debris")));
	TestFalse(TEXT("CONTROL: a tree resource is not"), FMOTraversal::IsRockTag(TEXT("MOResource_Tree")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTraversal_PickTop,
	"MOFramework.Traversal.PickTop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOTraversal_PickTop::RunTest(const FString& Parameters)
{
	// Probes dropped onto an obstacle at 8, 25, 45, 70, 100 cm past its face (UMOCharacterMovementComponent::FindTraversal), heights above the feet.
	using FSample = FMOTraversal::FTopSample;
	using EKind = FSample::EKind;
	auto S = [](float Along, float Z, bool bWalkable) { FSample Out; Out.Kind = EKind::Surface; Out.AlongCm = Along; Out.Z = Z; Out.bWalkable = bWalkable; return Out; };
	auto Miss = [](float Along) { FSample Out; Out.AlongCm = Along; return Out; };
	auto High = [](float Along) { FSample Out; Out.Kind = EKind::AboveReach; Out.AlongCm = Along; return Out; };
	bool bAbove = true;

	const FSample Block[] = { S(8, 100, true), S(25, 100, true), S(45, 100, true), S(70, 100, true), S(100, 100, true) };
	TestEqual(TEXT("a block (the test ledges): its edge, the first probe, as before"), FMOTraversal::PickTop(MakeArrayView(Block), bAbove), 0);
	TestFalse(TEXT("... and it is no climb"), bAbove);

	const FSample Dome[] = { S(8, 77, false), S(25, 110, true), S(45, 131, true), S(70, 124, true), S(100, 96, false) };
	TestEqual(TEXT("a domed boulder: its crest, not the steep shoulder just past the face (the 135 cm rock read as a 77 cm ledge)"),
		FMOTraversal::PickTop(MakeArrayView(Dome), bAbove), 2);

	const FSample Fence[] = { S(8, 95, true), Miss(25), S(45, 60, true), Miss(70), Miss(100) };
	TestEqual(TEXT("a fence: its top; what lies past its far side is not counted"), FMOTraversal::PickTop(MakeArrayView(Fence), bAbove), 0);

	const FSample Wall[] = { High(8), High(25), High(45), High(70), High(100) };
	TestEqual(TEXT("a wall taller than the reach: nothing to stand on"), FMOTraversal::PickTop(MakeArrayView(Wall), bAbove), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("... it is a climb"), bAbove);

	const FSample LedgeThenWall[] = { S(8, 110, true), High(25), S(45, 150, true), Miss(70), Miss(100) };
	TestEqual(TEXT("a ledge at the foot of a higher face: the ledge, never past the face that rises out of reach"), FMOTraversal::PickTop(MakeArrayView(LedgeThenWall), bAbove), 0);
	TestFalse(TEXT("... something to stand on, so no climb"), bAbove);

	const FSample Steep[] = { S(8, 90, false), S(25, 130, false), High(45), Miss(70), Miss(100) };
	TestEqual(TEXT("a steep slab rising out of reach: nothing to stand on"), FMOTraversal::PickTop(MakeArrayView(Steep), bAbove), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("... a climb"), bAbove);

	const FSample Nothing[] = { Miss(8), Miss(25), Miss(45), Miss(70), Miss(100) };
	TestEqual(TEXT("CONTROL: nothing past the face"), FMOTraversal::PickTop(MakeArrayView(Nothing), bAbove), static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("... and no climb either"), bAbove);

	// RankTops: every top to try for room, best first (a lumpy top's highest spot may leave no room for the body where a lower one does).
	TArray<int32> Ranked;
	FMOTraversal::RankTops(MakeArrayView(Dome), Ranked, bAbove);
	TestTrue(TEXT("a dome: its crest, then the next highest walkable spots (2, 3, 1)"), Ranked == TArray<int32>({ 2, 3, 1 }));
	const FSample Lumpy[] = { S(8, 100, true), S(25, 140, true), S(45, 139, true), S(70, 120, true), Miss(100) };
	FMOTraversal::RankTops(MakeArrayView(Lumpy), Ranked, bAbove);
	TestTrue(TEXT("a lumpy top: the nearer of the two highest (within 2 cm), then down (1, 2, 3, 0)"), Ranked == TArray<int32>({ 1, 2, 3, 0 }));
	FMOTraversal::RankTops(MakeArrayView(Block), Ranked, bAbove);
	TestTrue(TEXT("a block: its edge first, then the rest of its flat top near to far (0..4)"), Ranked == TArray<int32>({ 0, 1, 2, 3, 4 }));
	FMOTraversal::RankTops(MakeArrayView(Wall), Ranked, bAbove);
	TestTrue(TEXT("CONTROL: a wall out of reach has none (a climb)"), Ranked.Num() == 0 && bAbove);
	return true;
}

#endif
