// The ambience driver's classification (UMOAmbientEnvironmentSubsystem::Classify): pure, driven with the generator's own ground.

#include "Misc/AutomationTest.h"
#include "MOAmbientEnvironmentSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAmbientEnvironment_Classify,
	"MOFramework.Audio.AmbientEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAmbientEnvironment_Classify::RunTest(const FString& Parameters)
{
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Built = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Built->Tuning.bEnableUplandLakes = false;   // sea-level water only: the shore geometry below is built from the ground heights
	Built->Finalize();
	const FMOWorldGenParams& P = *Built;
	const float Sea = P.Tuning.SeaLevelCm;
	const float R = UMOAmbientEnvironmentSubsystem::WaterRadiusCm;

	// A shore built from the generator's own heights, found by scanning east across open water in a world that has some: walk from dry land into the water and stop at the first wet
	// sample (the shore edge E). Keep it only if the water goes on (30 m of it ahead) and the land behind is dry in EVERY direction at the radii the classifier looks at, so the
	// expectations below follow from the geometry, not from the classifier under test.
	struct FShore { int32 Seed; double Ex, Ey; };
	TOptional<FShore> Shore;
	for (const int32 Seed : { 7, 18, 19, 1000, 12345, 31337 })
	{
		auto H = [&](double X, double Y) { return FMOWorldGen::SampleHeightCm(P, X, Y, Seed, P.Tuning.VoxelSizeCm); };
		auto Wet = [&](double X, double Y) { return H(X, Y) < Sea - 20.0f; };
		for (int32 I = 0; I < 900 && !Shore.IsSet(); ++I)
		{
			const double X0 = -2.6e6 + 5900.0 * I, Y0 = -2.0e6 + 7100.0 * (I % 53);
			if (Wet(X0, Y0) || H(X0, Y0) > Sea + 1500.0f)
			{
				continue;   // start on low dry land
			}
			for (double D = 0.0; D < 60000.0; D += 100.0)
			{
				if (!Wet(X0 + D, Y0))
				{
					continue;
				}
				const double Ex = X0 + D, Ey = Y0;
				bool bOk = true;
				for (double A = 100.0; A <= 3000.0 && bOk; A += 100.0)
				{
					bOk = Wet(Ex + A, Ey);   // 30 m of water ahead
				}
				// The point 45 m inland of the edge must be "outside the entry distance" for real: dry on the classifier's own rings (every 7 m out to 35 m) in every direction. The point 120 m
				// inland must be dry on all the rings out to the EXIT distance (56 m) too, so a Water that started at the shore really ends there.
				auto DryRings = [&](double InlandCm, TArrayView<const double> Radii)
				{
					for (int32 K = 0; K < 16; ++K)
					{
						const double Dx = FMath::Cos(K * UE_PI / 8.0), Dy = FMath::Sin(K * UE_PI / 8.0);
						for (const double Rad : Radii)
						{
							if (Wet(Ex - InlandCm + Dx * Rad, Ey + Dy * Rad))
							{
								return false;
							}
						}
					}
					return true;
				};
				static constexpr double EntryRings[] = { 700.0, 1400.0, 2100.0, 2800.0, 3500.0 };
				static constexpr double ExitRings[] = { 700.0, 1400.0, 2100.0, 2800.0, 3500.0, 4200.0, 4900.0, 5600.0 };
				bOk = bOk && DryRings(4500.0, EntryRings) && DryRings(12000.0, ExitRings);
				if (bOk)
				{
					Shore = FShore{ Seed, Ex, Ey };
				}
				break;
			}
		}
		if (Shore.IsSet())
		{
			break;
		}
	}
	TestTrue(TEXT("the test worlds have a shore with the needed geometry"), Shore.IsSet());
	if (!Shore.IsSet())
	{
		return true;
	}
	const int32 Seed = Shore->Seed;
	const double Ex = Shore->Ex, Ey = Shore->Ey;
	auto Ground = [&](double X, double Y) { return FMOWorldGen::SampleHeightCm(P, X, Y, Seed, P.Tuning.VoxelSizeCm); };
	const auto At = [&](double InlandCm, double AboveGround, EMOAmbientState Previous)
	{
		const double X = Ex - InlandCm;
		return UMOAmbientEnvironmentSubsystem::Classify(P, Seed, FVector(X, Ey, Ground(X, Ey) + AboveGround), Previous);
	};

	// Beside the water: the shore edge's own ring point is wet by construction.
	TestEqual(TEXT("17 m from the water (a ring sample lands in it): Water"), At(1750.0, 100.0, EMOAmbientState::Outdoor_Day), EMOAmbientState::Water);
	TestEqual(TEXT("far above the water (a cliff top 60 m up): Outdoor"), At(1750.0, 6000.0, EMOAmbientState::Outdoor_Day), EMOAmbientState::Outdoor_Day);
	// Hysteresis: 45 m from the water is outside the 35 m entry radius but inside the 56 m exit radius.
	TestEqual(TEXT("45 m from the water, coming from open ground: stays Outdoor"), At(4500.0, 100.0, EMOAmbientState::Outdoor_Day), EMOAmbientState::Outdoor_Day);
	TestEqual(TEXT("45 m from the water, already Water: stays Water"), At(4500.0, 100.0, EMOAmbientState::Water), EMOAmbientState::Water);
	TestEqual(TEXT("120 m inland, already Water: leaves it"), At(12000.0, 100.0, EMOAmbientState::Water), EMOAmbientState::Outdoor_Day);
	TestEqual(TEXT("120 m inland from the open: Outdoor"), At(12000.0, 100.0, EMOAmbientState::Outdoor_Day), EMOAmbientState::Outdoor_Day);

	// Underground: 20 m under the surface is a cave; 10 m under is not unless we were already in one (it ends at two thirds of the entry depth).
	TestEqual(TEXT("20 m under the ground: Cave"), At(12000.0, -2000.0, EMOAmbientState::Outdoor_Day), EMOAmbientState::Cave);
	TestEqual(TEXT("10 m under the ground from the open: not a cave yet"), At(12000.0, -1000.0, EMOAmbientState::Outdoor_Day), EMOAmbientState::Outdoor_Day);
	TestEqual(TEXT("10 m under the ground, already in a cave: still Cave"), At(12000.0, -1000.0, EMOAmbientState::Cave), EMOAmbientState::Cave);
	TestEqual(TEXT("3 m under the ground, was in a cave: out"), At(12000.0, -300.0, EMOAmbientState::Cave), EMOAmbientState::Outdoor_Day);

	// SETTLES: a fixed position must reach its answer in one step whichever state it is asked from (the first version flapped Water <-> Outdoor once a second on a narrow river, restarting
	// every ambient layer each time). Walk from 20 m into the water to 130 m inland and check the fixed-point property from both starting states, at ground level and 10 m up.
	int32 Unsettled = 0, Probes = 0;
	for (double InlandCm = -2000.0; InlandCm <= 13000.0; InlandCm += 250.0)
	{
		for (const double AboveGround : { 100.0, 1000.0, -2000.0 })
		{
			for (const EMOAmbientState Start : { EMOAmbientState::Outdoor_Day, EMOAmbientState::Water, EMOAmbientState::Cave })
			{
				const EMOAmbientState One = At(InlandCm, AboveGround, Start);
				const EMOAmbientState Two = At(InlandCm, AboveGround, One);
				Unsettled += (One != Two) ? 1 : 0;
				++Probes;
			}
		}
	}
	TestEqual(FString::Printf(TEXT("a fixed position settles in one step from every starting state (%d probes)"), Probes), Unsettled, 0);
	AddInfo(FString::Printf(TEXT("shore edge at (%.0f, %.0f) of seed %d"), Ex, Ey, Seed));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
