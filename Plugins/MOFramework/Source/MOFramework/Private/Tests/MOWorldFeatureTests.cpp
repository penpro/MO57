// World features (POIs): the placer is a pure function of (generator, seed, catalog) -- every co-op machine must compute the same sites -- and it keeps
// its promises: a row's own rules (biome, slope, dry), its spacing, and a higher-priority feature's footprint. Headless; real biomes, a synthetic catalog.

#include "Misc/AutomationTest.h"
#include "MOWorldFeatureSubsystem.h"
#include "MOWorldSeedSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldFeature_Placer,
	"MOFramework.WorldFeatures.Placer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldFeature_Placer::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
	if (!TestTrue(TEXT("the published params have biomes"), P->Biomes.Num() >= 2))
	{
		return false;
	}
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(-1675528670);
	const FName Forest = P->Biomes[0].Id;   // any real biome: the rule is what is tested

	FMOWorldFeatureDefinitionRow Big;
	Big.Biomes = { Forest };
	Big.DensityPerKm2 = 20.0f;
	Big.SpacingCm = 20000.0f;
	Big.FootprintRadiusCm = 4000.0f;
	Big.Priority = 10;
	Big.SlopeMaxDeg = 30.0f;
	FMOWorldFeatureDefinitionRow Small = Big;
	Small.Biomes.Reset();               // anywhere on land
	Small.DensityPerKm2 = 60.0f;
	Small.SpacingCm = 10000.0f;
	Small.FootprintRadiusCm = 1500.0f;
	Small.Priority = 0;
	const TArray<TPair<FName, FMOWorldFeatureDefinitionRow>> Rows = { { FName(TEXT("Small")), Small }, { FName(TEXT("Big")), Big } };

	FMOWorldFeaturePlacer A(P, Seed, Rows), B(P, Seed, Rows), OtherSeed(P, Seed + 1, Rows);
	TestEqual(TEXT("priority orders the features (Big first)"), A.GetFeatures()[0].Id, FName(TEXT("Big")));

	const FBox2D Area(FVector2D(-600000.0, -600000.0), FVector2D(600000.0, 600000.0));   // 12 x 12 km around the origin
	TArray<FMOWorldFeatureSite> SitesA, SitesB, SitesOther;
	A.SitesTouching(Area, SitesA);
	B.SitesTouching(Area, SitesB);
	OtherSeed.SitesTouching(Area, SitesOther);
	AddInfo(FString::Printf(TEXT("%d sites in 144 km^2 (another seed: %d)"), SitesA.Num(), SitesOther.Num()));
	if (!TestTrue(TEXT("sites exist"), SitesA.Num() > 20))
	{
		return false;
	}
	bool bSame = SitesA.Num() == SitesB.Num();
	for (int32 I = 0; bSame && I < SitesA.Num(); ++I)
	{
		bSame = SitesA[I].FeatureId == SitesB[I].FeatureId && SitesA[I].Location == SitesB[I].Location && SitesA[I].Seed == SitesB[I].Seed && SitesA[I].YawDegrees == SitesB[I].YawDegrees;
	}
	TestTrue(TEXT("deterministic: two placers on the same seed give identical sites"), bSame);
	TestTrue(TEXT("another seed places differently"), SitesOther.Num() != SitesA.Num() || SitesOther[0].Location != SitesA[0].Location);

	const FMOBiomeFinder Finder(P, Seed);
	int32 WrongBiome = 0, Steep = 0, Wet = 0, TooClose = 0, Overlap = 0, NumBig = 0;
	for (const FMOWorldFeatureSite& S : SitesA)
	{
		float Ground = 0.0f, Slope = 0.0f;
		const int32 Biome = Finder.BiomeAt(S.Location.X, S.Location.Y, Ground, Slope);
		const bool bBig = S.FeatureId == FName(TEXT("Big"));
		NumBig += bBig ? 1 : 0;
		WrongBiome += (bBig && P->GetBiomeId(Biome) != Forest) ? 1 : 0;
		Steep += Slope > 30.0f ? 1 : 0;
		Wet += Finder.IsInWater(S.Location.X, S.Location.Y, Ground) ? 1 : 0;
		for (const FMOWorldFeatureSite& T : SitesA)
		{
			if (&S == &T)
			{
				continue;
			}
			const double D = FVector2D::Distance(FVector2D(S.Location), FVector2D(T.Location));
			if (S.FeatureId == T.FeatureId)
			{
				const FMOWorldFeatureDefinitionRow& R = S.FeatureId == FName(TEXT("Big")) ? Big : Small;
				TooClose += D < R.SpacingCm * 0.5 - 1.0 ? 1 : 0;
			}
			else if (bBig)
			{
				Overlap += D < S.FootprintRadiusCm + T.FootprintRadiusCm ? 1 : 0;   // a Small inside a Big's footprint
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d Big, %d Small"), NumBig, SitesA.Num() - NumBig));
	TestEqual(TEXT("every Big site stands in its biome"), WrongBiome, 0);
	TestEqual(TEXT("no site is steeper than the row allows"), Steep, 0);
	TestEqual(TEXT("no site is in water"), Wet, 0);
	TestEqual(TEXT("two sites of one feature are never closer than half its spacing"), TooClose, 0);
	TestEqual(TEXT("no lower-priority site overlaps a higher-priority footprint"), Overlap, 0);

	// CONTROL: the same Small row alone (nothing above it) does place sites inside where the Bigs stand -- so the overlap check above can fail.
	FMOWorldFeaturePlacer SmallAlone(P, Seed, { { FName(TEXT("Small")), Small } });
	TArray<FMOWorldFeatureSite> Alone;
	SmallAlone.SitesTouching(Area, Alone);
	int32 Inside = 0;
	for (const FMOWorldFeatureSite& S : Alone)
	{
		for (const FMOWorldFeatureSite& T : SitesA)
		{
			Inside += (T.FeatureId == FName(TEXT("Big")) && FVector2D::Distance(FVector2D(S.Location), FVector2D(T.Location)) < S.FootprintRadiusCm + T.FootprintRadiusCm) ? 1 : 0;
		}
	}
	TestTrue(FString::Printf(TEXT("CONTROL: without the Big row, %d Small site(s) would overlap a Big footprint"), Inside), Inside > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
