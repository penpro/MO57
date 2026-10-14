// World features (POIs): the placer is a pure function of (generator, seed, catalog) -- every co-op machine must compute the same sites -- and it keeps
// its promises: a row's own rules (biome, slope, dry), its spacing, and a higher-priority feature's footprint. Headless; real biomes, a synthetic catalog.

#include "Misc/AutomationTest.h"
#include "MOWorldFeatureSubsystem.h"
#include "MOWorldFeatureActor.h"
#include "MOWorldSeedSubsystem.h"
#include "MOWorldGen.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldFeature_ChooseVariant,
	"MOFramework.WorldFeatures.ChooseVariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldFeature_ChooseVariant::RunTest(const FString& Parameters)
{
	// The site seed picks the composition: in range for every seed (negative too), the same on every call, and every variant gets used.
	TestEqual(TEXT("no variants -> none"), AMOWorldFeatureActor::ChooseVariant(12345, 0), INDEX_NONE);
	int32 Counts[6] = {};
	int32 OutOfRange = 0, Unstable = 0;
	for (int32 I = 0; I < 6000; ++I)
	{
		const int32 Seed = static_cast<int32>(static_cast<uint32>(I) * 2654435761u);   // spread over the whole int32 range, half negative
		const int32 V = AMOWorldFeatureActor::ChooseVariant(Seed, 6);
		OutOfRange += (V < 0 || V >= 6) ? 1 : 0;
		Unstable += V != AMOWorldFeatureActor::ChooseVariant(Seed, 6) ? 1 : 0;
		Counts[FMath::Clamp(V, 0, 5)]++;
	}
	TestEqual(TEXT("always in range"), OutOfRange, 0);
	TestEqual(TEXT("deterministic"), Unstable, 0);
	for (int32 V = 0; V < 6; ++V)
	{
		TestTrue(FString::Printf(TEXT("variant %d used (%d of 6000)"), V, Counts[V]), Counts[V] > 600);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldFeature_SlopeAlign,
	"MOFramework.WorldFeatures.SlopeAlign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldFeature_SlopeAlign::RunTest(const FString& Parameters)
{
	// A hillside composition (bAlignToSlope) stands where the plane across its whole footprint has the row's slope, turned so its own downhill
	// direction faces the terrain's; an unaligned twin is the control (random yaw: the facing check must be able to fail).
	const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(-1675528670);
	FMOWorldFeatureDefinitionRow Hill;
	Hill.DensityPerKm2 = 50.0f;            // saturates: every square that qualifies has a site
	Hill.SpacingCm = 30000.0f;
	Hill.FootprintRadiusCm = 15000.0f;
	Hill.SlopeMinDeg = 8.0f;
	Hill.SlopeMaxDeg = 25.0f;
	Hill.MaxFootprintReliefCm = 2500.0f;
	Hill.bAlignToSlope = true;
	Hill.AuthoredDownhillYawDeg = 30.0f;
	Hill.AlignYawJitterDeg = 10.0f;
	FMOWorldFeatureDefinitionRow Loose = Hill;
	Loose.bAlignToSlope = false;
	Loose.SlopeMaxDeg = 90.0f;
	Loose.MaxFootprintReliefCm = 0.0f;
	Loose.SlopeMinDeg = 0.0f;

	const FBox2D Area(FVector2D(-1000000.0, -1000000.0), FVector2D(1000000.0, 1000000.0));   // 20 x 20 km
	auto Check = [&](const FMOWorldFeatureDefinitionRow& Row, int32& OutSites, int32& OutFacing, int32& OutSlopeOk, int32& OutFlat)
	{
		FMOWorldFeaturePlacer Placer(P, Seed, { { FName(TEXT("Hill")), Row } });
		TArray<FMOWorldFeatureSite> Sites;
		Placer.SitesTouching(Area, Sites);
		const FMOBiomeFinder Finder(P, Seed);
		OutSites = Sites.Num();
		OutFacing = OutSlopeOk = OutFlat = 0;
		for (const FMOWorldFeatureSite& S : Sites)
		{
			FVector2D G;
			float Dev = 0.0f;
			const float Ground = FMOWorldGen::SampleHeightCm(*P, S.Location.X, S.Location.Y, Seed, P->Tuning.VoxelSizeCm);
			if (!FMOWorldFeaturePlacer::MeasureFootprintPlane(*P, Seed, Finder, S.Location.X, S.Location.Y, Hill.FootprintRadiusCm, Ground, G, Dev))
			{
				continue;
			}
			const float Slope = FMath::RadiansToDegrees(FMath::Atan(G.Size()));
			OutSlopeOk += (Slope >= Hill.SlopeMinDeg - 0.01f && Slope <= Hill.SlopeMaxDeg + 0.01f && Dev <= Hill.MaxFootprintReliefCm + 1.0f) ? 1 : 0;
			OutFlat += Slope < 3.0f ? 1 : 0;
			const float Downhill = FMath::RadiansToDegrees(FMath::Atan2(-G.Y, -G.X));
			const float Facing = FMath::FindDeltaAngleDegrees(Downhill, S.YawDegrees + Hill.AuthoredDownhillYawDeg);
			OutFacing += FMath::Abs(Facing) <= Hill.AlignYawJitterDeg + 0.01f ? 1 : 0;
		}
	};
	int32 Sites = 0, Facing = 0, SlopeOk = 0, Flat = 0;
	Check(Hill, Sites, Facing, SlopeOk, Flat);
	AddInfo(FString::Printf(TEXT("aligned: %d sites in 400 km^2"), Sites));
	if (!TestTrue(TEXT("hillside sites exist"), Sites >= 10))
	{
		return false;
	}
	TestEqual(TEXT("every aligned site faces downhill (within the jitter)"), Facing, Sites);
	TestEqual(TEXT("every aligned site's footprint plane has the row's slope and relief"), SlopeOk, Sites);

	int32 LSites = 0, LFacing = 0, LSlopeOk = 0, LFlat = 0;
	Check(Loose, LSites, LFacing, LSlopeOk, LFlat);
	AddInfo(FString::Printf(TEXT("CONTROL unaligned: %d sites, %d facing downhill, %d on the slope range, %d flat"), LSites, LFacing, LSlopeOk, LFlat));
	TestTrue(TEXT("CONTROL: random yaw mostly does not face downhill"), LFacing < LSites / 2);
	TestTrue(TEXT("CONTROL: without the slope rule some sites stand on flat ground"), LFlat > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
