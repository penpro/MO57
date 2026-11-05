#include "Misc/AutomationTest.h"
#include "MOTerrainModificationSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// A merged dig's far edge lay in no zone (N-307, the June audit's M23): the radius took the larger of the two and the center stayed, so plants stood over the newly dug ground.
// Expected: every dig along a trench is wholly inside some zone, and no zone runs away with the grass; failing before: a dig 40 cm along, with a 100 cm brush, reached 140 cm in a 100 cm zone.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTerrainMergedZoneCoversBoth, "MOFramework.Terrain.MergedZoneCoversBoth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOTerrainMergedZoneCoversBoth::RunTest(const FString& Parameters)
{
	const float Factor = GetDefault<UMOTerrainModificationSubsystem>()->MergeDistanceFactor;
	const float Brush = 100.0f;

	FMOTerrainModifiedZone Zone(FVector::ZeroVector, Brush);
	TestTrue(TEXT("a dig a tenth of a brush along merges"), UMOTerrainModificationSubsystem::TryMergeZone(Zone, FVector(10.0f, 0.0f, 0.0f), Brush, Factor));
	TestEqual(TEXT("and the zone reaches its far edge"), Zone.Radius, 110.0f);

	FMOTerrainModifiedZone Again(FVector::ZeroVector, Brush);
	TestTrue(TEXT("the same spot merges"), UMOTerrainModificationSubsystem::TryMergeZone(Again, FVector::ZeroVector, Brush, Factor));
	TestEqual(TEXT("and the zone does not grow"), Again.Radius, Brush);

	FMOTerrainModifiedZone Far(FVector::ZeroVector, Brush);
	TestFalse(TEXT("a dig well along is a zone of its own"), UMOTerrainModificationSubsystem::TryMergeZone(Far, FVector(90.0f, 0.0f, 0.0f), Brush, Factor));
	TestEqual(TEXT("and leaves this one as it was"), Far.Radius, Brush);

	// A trench: a dig every fifth of a brush for 20 m, each going where RegisterModifiedZone would put it.
	TArray<FMOTerrainModifiedZone> Zones;
	bool bAllCovered = true;
	float Largest = 0.0f;
	for (int32 Step = 0; Step < 100; ++Step)
	{
		const FVector Dig(Step * Brush * 0.2f, 0.0f, 0.0f);
		bool bMerged = false;
		for (FMOTerrainModifiedZone& Existing : Zones)
		{
			bMerged = UMOTerrainModificationSubsystem::TryMergeZone(Existing, Dig, Brush, Factor);
			if (bMerged)
			{
				break;
			}
		}
		if (!bMerged)
		{
			Zones.Add(FMOTerrainModifiedZone(Dig, Brush));
		}
		const FVector FarEdge = Dig + FVector(Brush, 0.0f, 0.0f);
		bool bCovered = false;
		for (const FMOTerrainModifiedZone& Existing : Zones)
		{
			bCovered |= FVector::Dist(Existing.Location, FarEdge) <= Existing.Radius + KINDA_SMALL_NUMBER;
			Largest = FMath::Max(Largest, Existing.Radius);
		}
		bAllCovered &= bCovered;
	}
	TestTrue(TEXT("every dig's far edge is in a zone"), bAllCovered);
	TestTrue(TEXT("no zone outgrows the brush by more than F / (1 - F)"), Largest <= Brush / (1.0f - Factor) + 1.0f);
	TestTrue(TEXT("a trench stays many zones, not one that eats the valley"), Zones.Num() > 5);
	return true;
}

#endif
