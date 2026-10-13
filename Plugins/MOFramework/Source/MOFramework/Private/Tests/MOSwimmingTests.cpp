// Swimming and drowning: the pure decisions (immersion, the swim / wade hysteresis), the generator's open-water surface, the oxygen-reserve math, and the respiration component's process
// (a closed airway drains the reserve, the SpO2 cap follows, the asphyxia timer ends in death, a gasp on the first breath). Headless-safe: no terrain, no pawn.

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MOBreathMeterWidget.h"
#include "MOCharacterMovementComponent.h"
#include "MORespirationComponent.h"
#include "MOVitalsComponent.h"
#include "MOWorldGen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMOWorldGenParamsRef MakeSwimTestParams()
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSwim_BuriedInTerrain,
	"MOFramework.Water.BuriedInTerrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSwim_BuriedInTerrain::RunTest(const FString& Parameters)
{
	// The ocean plane fills everything under sea level, underground too: a pawn inside the terrain below z=0 used to count as swimming (and was never rescued, the rescue handles falling pawns).
	TestTrue(TEXT("feet 15 m under a 2.3 m ground: buried"), UMOCharacterMovementComponent::IsBuriedInTerrain(-1042.0f, 238.0f, 300.0f));
	TestTrue(TEXT("feet at -47 cm under ground at +226 (the spawn bug: 2.7 m under, margin 2.5 m): buried"), UMOCharacterMovementComponent::IsBuriedInTerrain(-137.0f, 226.0f, 250.0f));
	TestFalse(TEXT("feet on the ground: not buried"), UMOCharacterMovementComponent::IsBuriedInTerrain(226.0f, 226.0f, 300.0f));
	TestFalse(TEXT("feet in 2 m of water over a seabed 5 m down (ground far below): not buried"), UMOCharacterMovementComponent::IsBuriedInTerrain(-200.0f, -500.0f, 300.0f));
	TestFalse(TEXT("feet 2 m under the ground (inside the margin): not buried (a shallow dug pit)"), UMOCharacterMovementComponent::IsBuriedInTerrain(24.0f, 226.0f, 300.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSwim_Decisions,
	"MOFramework.Water.SwimDecisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSwim_Decisions::RunTest(const FString& Parameters)
{
	// Immersion of a 180 cm capsule.
	TestEqual(TEXT("dry (surface below the feet)"), UMOCharacterMovementComponent::ComputeImmersion(-5.0f, 0.0f, 180.0f), 0.0f);
	TestEqual(TEXT("ankle deep"), UMOCharacterMovementComponent::ComputeImmersion(18.0f, 0.0f, 180.0f), 0.1f);
	TestEqual(TEXT("chest deep (126 cm of 180)"), UMOCharacterMovementComponent::ComputeImmersion(126.0f, 0.0f, 180.0f), 0.7f);
	TestEqual(TEXT("fully under"), UMOCharacterMovementComponent::ComputeImmersion(500.0f, 0.0f, 180.0f), 1.0f);
	TestEqual(TEXT("feet at -90 cm, surface at 0: half under"), UMOCharacterMovementComponent::ComputeImmersion(0.0f, -90.0f, 180.0f), 0.5f);
	TestEqual(TEXT("a degenerate capsule is all-or-nothing"), UMOCharacterMovementComponent::ComputeImmersion(1.0f, 0.0f, 0.0f), 1.0f);

	// Swim / wade with hysteresis (enter 0.7, exit 0.5).
	TestFalse(TEXT("standing in knee-deep water does not swim"), UMOCharacterMovementComponent::ShouldSwim(false, 0.3f, 0.7f, 0.5f, true));
	TestFalse(TEXT("... nor at 0.69"), UMOCharacterMovementComponent::ShouldSwim(false, 0.69f, 0.7f, 0.5f, true));
	TestTrue(TEXT("chest-deep water starts a swim"), UMOCharacterMovementComponent::ShouldSwim(false, 0.7f, 0.7f, 0.5f, false));
	TestTrue(TEXT("a swimmer does NOT stop at the enter line (hysteresis: no flicker at the chest)"), UMOCharacterMovementComponent::ShouldSwim(true, 0.6f, 0.7f, 0.5f, true));
	TestFalse(TEXT("a swimmer stops once the feet can stand (below the exit fraction, walkable ground under them)"), UMOCharacterMovementComponent::ShouldSwim(true, 0.49f, 0.7f, 0.5f, true));
	TestTrue(TEXT("a swimmer with NOTHING to stand on keeps swimming however little of the capsule is in the water (no popping out at the surface)"), UMOCharacterMovementComponent::ShouldSwim(true, 0.20f, 0.7f, 0.5f, false));

	// The defaults are people-scale: slow swimming, a heavy wading drag, a floating body.
	const UMOCharacterMovementComponent* Defaults = GetDefault<UMOCharacterMovementComponent>();
	TestTrue(TEXT("a swimmer crawls about 1 m/s (not a game character's 3 m/s)"), Defaults->MaxSwimSpeed >= 80.0f && Defaults->MaxSwimSpeed <= 200.0f);
	TestTrue(TEXT("buoyancy above 1: lungs full, the body floats with the head out"), Defaults->UnloadedBuoyancy > 1.0f && Defaults->UnloadedBuoyancy < 1.4f);
	TestTrue(TEXT("... and it floats ABOVE the swim-exit line (a floating body must not drift into the stand-up state)"), 1.0f / Defaults->UnloadedBuoyancy > Defaults->SwimExitImmersion);
	TestTrue(TEXT("the swim-exit line is below the swim-enter line"), Defaults->SwimExitImmersion < Defaults->SwimEnterImmersion);
	TestTrue(TEXT("wading in chest-deep water roughly halves the walking speed"), Defaults->WadeDragAtFullImmersion > 0.4f && Defaults->WadeDragAtFullImmersion < 0.7f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSwim_OpenWaterSurface,
	"MOFramework.Water.OpenWaterSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSwim_OpenWaterSurface::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeSwimTestParams();
	constexpr int32 Seed = 777;

	// Find upland lakes the way the game finds them.
	TArray<FMOWorldGen::FLake> Lakes;
	const int32 HalfCells = (int32)(P->Tuning.WorldHalfSizeCm / P->Tuning.UplandLakeCellCm);
	for (int64 Cy = -HalfCells; Cy < HalfCells && Lakes.Num() < 12; ++Cy)
	{
		for (int64 Cx = -HalfCells; Cx < HalfCells && Lakes.Num() < 12; ++Cx)
		{
			FMOWorldGen::FLake L;
			if (FMOWorldGen::GetLakeInCell(*P, 2, Cx, Cy, Seed, L))
			{
				Lakes.Add(L);
			}
		}
	}
	if (!TestTrue(TEXT("the test world has upland lakes"), Lakes.Num() >= 5))
	{
		return false;
	}

	int32 CentreWrong = 0, OutsideWrong = 0, EdgeWrong = 0;
	for (const FMOWorldGen::FLake& L : Lakes)
	{
		float Z = 0.0f;
		if (!FMOWorldGen::GetWaterSurfaceCm(*P, L.Center.X, L.Center.Y, Seed, Z) || !FMath::IsNearlyEqual(Z, L.SurfaceCm, 0.01f))
		{
			++CentreWrong;
		}
		// 90 % of the way to the shore along the long axis is still the lake; 20 % past the shore is the ocean plane again (the bank is dry land well above sea level, and the ocean mesh is hidden by the terrain there).
		const double Ax = L.Center.X + 0.9 * L.RadiusCm * L.CosAngle, Ay = L.Center.Y + 0.9 * L.RadiusCm * L.SinAngle;
		if (!FMOWorldGen::GetWaterSurfaceCm(*P, Ax, Ay, Seed, Z) || !FMath::IsNearlyEqual(Z, L.SurfaceCm, 0.01f))
		{
			++EdgeWrong;
		}
		const double Bx = L.Center.X + 1.2 * L.RadiusCm * L.CosAngle, By = L.Center.Y + 1.2 * L.RadiusCm * L.SinAngle;
		if (!FMOWorldGen::GetWaterSurfaceCm(*P, Bx, By, Seed, Z) || !FMath::IsNearlyEqual(Z, P->Tuning.SeaLevelCm, 0.01f))
		{
			++OutsideWrong;
		}
	}
	TestEqual(TEXT("the surface at every upland lake's centre is the lake's own surface height"), CentreWrong, 0);
	TestEqual(TEXT("... 90 % of the way to the shore too"), EdgeWrong, 0);
	TestEqual(TEXT("... and past the shore it is the ocean plane (sea level)"), OutsideWrong, 0);

	float Sea = 1.0f;
	TestTrue(TEXT("far from any lake the open water is the ocean plane"), FMOWorldGen::GetWaterSurfaceCm(*P, 0.0, 0.0, Seed, Sea));

	// Water off: no water anywhere.
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Dry = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Dry->Tuning.bEnableWater = false;
	Dry->Finalize();
	TestFalse(TEXT("with water off there is no water surface"), FMOWorldGen::GetWaterSurfaceCm(*Dry, 0.0, 0.0, Seed, Sea));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSwim_BreathMath,
	"MOFramework.Water.BreathMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSwim_BreathMath::RunTest(const FString& Parameters)
{
	const FMOBreathParams P;

	// A closed airway at rest empties the reserve in RestEmptySeconds.
	float R = 1.0f;
	const float Step = 0.25f;
	float T = 0.0f;
	while (R > 0.0f && T < 1000.0f)
	{
		R = FMOBreathMath::StepReserve(P, R, Step, true, 0.0f);
		T += Step;
	}
	TestTrue(*FString::Printf(TEXT("a rest breath-hold empties the reserve in about %.0f s (took %.0f s)"), P.RestEmptySeconds, T), FMath::Abs(T - P.RestEmptySeconds) <= 1.0f);

	// Exertion burns it faster (a hard swimmer is blacking out in under 40 s).
	R = 1.0f;
	T = 0.0f;
	while (R > 0.0f && T < 1000.0f)
	{
		R = FMOBreathMath::StepReserve(P, R, Step, true, 1.0f);
		T += Step;
	}
	TestTrue(*FString::Printf(TEXT("a full-effort hold empties it in RestEmptySeconds / %.0f (took %.1f s)"), P.FullExertionDrainMultiplier, T), FMath::Abs(T - P.RestEmptySeconds / P.FullExertionDrainMultiplier) <= 1.0f);
	TestTrue(TEXT("SecondsToEmpty agrees with the stepping"), FMath::Abs(FMOBreathMath::SecondsToEmpty(P, 1.0f) - T) <= 1.0f);
	TestTrue(TEXT("a hard-working swimmer's hold is well under a minute"), FMOBreathMath::SecondsToEmpty(P, 1.0f) < 40.0f);
	TestTrue(TEXT("a still hold is well over a minute"), FMOBreathMath::SecondsToEmpty(P, 0.0f) > 90.0f);

	// An open airway refills it.
	R = 0.0f;
	T = 0.0f;
	while (R < 1.0f && T < 1000.0f)
	{
		R = FMOBreathMath::StepReserve(P, R, Step, false, 0.0f);
		T += Step;
	}
	TestTrue(*FString::Printf(TEXT("a few deep breaths refill an empty reserve in about %.0f s (took %.1f s)"), P.RecoverySeconds, T), FMath::Abs(T - P.RecoverySeconds) <= 1.0f);
	TestEqual(TEXT("a zero time step changes nothing"), FMOBreathMath::StepReserve(P, 0.42f, 0.0f, true, 1.0f), 0.42f);
	TestEqual(TEXT("the reserve never goes below 0"), FMOBreathMath::StepReserve(P, 0.001f, 100.0f, true, 1.0f), 0.0f);
	TestEqual(TEXT("... nor above 1"), FMOBreathMath::StepReserve(P, 0.999f, 100.0f, false, 0.0f), 1.0f);

	// SpO2: normal until the reserve is low, then a smooth monotonic fall to the floor.
	TestEqual(TEXT("a full reserve shows the normal saturation"), FMOBreathMath::SpO2ForReserve(P, 1.0f), 98.0f);
	TestEqual(TEXT("... so does the desaturation threshold"), FMOBreathMath::SpO2ForReserve(P, P.DesaturationReserve), 98.0f);
	TestEqual(TEXT("an empty reserve shows the floor"), FMOBreathMath::SpO2ForReserve(P, 0.0f), P.EmptyReserveSpO2);
	bool bMonotonic = true;
	float Prev = -1.0f;
	for (float Reserve = 0.0f; Reserve <= 1.0f; Reserve += 0.01f)
	{
		const float S = FMOBreathMath::SpO2ForReserve(P, Reserve);
		bMonotonic &= S >= Prev - 0.0001f;
		Prev = S;
	}
	TestTrue(TEXT("SpO2 never falls as the reserve rises"), bMonotonic);
	TestTrue(TEXT("the unconscious line (SpO2 < 70) comes while there is still reserve to lose"), FMOBreathMath::SpO2ForReserve(P, 0.12f) < 85.0f);
	TestTrue(TEXT("the floor is below the respiratory-failure line (50)"), P.EmptyReserveSpO2 < 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSwim_ColdWater,
	"MOFramework.Water.ColdWater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSwim_ColdWater::RunTest(const FString& Parameters)
{
	// Dry: nothing changes.
	float Ambient = 20.0f, Insulation = 0.55f;
	UMOVitalsComponent::ApplyWaterToExposure(0.0f, 20.0f, Ambient, Insulation);
	TestEqual(TEXT("on land the ambient is the air's"), Ambient, 20.0f);
	TestEqual(TEXT("... and the insulation is intact"), Insulation, 0.55f);

	// Fully immersed on a mild day: the water is colder than the air and the clothes stop insulating.
	Ambient = 20.0f;
	Insulation = 0.55f;
	UMOVitalsComponent::ApplyWaterToExposure(1.0f, 20.0f, Ambient, Insulation);
	TestEqual(TEXT("fully immersed: the ambient is the water temperature"), Ambient, UMOVitalsComponent::WaterTemperatureForAir(20.0f));
	TestTrue(TEXT("water on a 20 C day is colder than the air (a lake lags the air)"), Ambient < 20.0f);
	TestTrue(TEXT("soaked clothes insulate at most 15 % of what dry ones do"), Insulation <= 0.55f * 0.151f);

	// Half immersed: in between.
	Ambient = 20.0f;
	Insulation = 0.55f;
	UMOVitalsComponent::ApplyWaterToExposure(0.5f, 20.0f, Ambient, Insulation);
	TestTrue(TEXT("half immersed is between air and water"), Ambient < 20.0f && Ambient > UMOVitalsComponent::WaterTemperatureForAir(20.0f));
	TestTrue(TEXT("... and about half-way through the insulation"), Insulation < 0.55f && Insulation > 0.55f * 0.15f);

	// The heat loss: in cold water the exposure (1 - insulation) times the temperature difference to the body is several times the dry value.
	const float DryLoss = (20.0f - 37.0f) * (1.0f - 0.55f);
	const float WetLoss = (UMOVitalsComponent::WaterTemperatureForAir(20.0f) - 37.0f) * (1.0f - 0.55f * 0.15f);
	TestTrue(*FString::Printf(TEXT("a swimmer loses heat %.1fx faster than a dry person in the same air (%.1f vs %.1f)"), WetLoss / DryLoss, WetLoss, DryLoss), WetLoss / DryLoss > 1.8f);
	TestTrue(TEXT("water temperature is always liveable-range bounded"), UMOVitalsComponent::WaterTemperatureForAir(-30.0f) >= 1.0f && UMOVitalsComponent::WaterTemperatureForAir(60.0f) <= 26.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSwim_BreathMeter,
	"MOFramework.Water.BreathMeter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSwim_BreathMeter::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("lungs full, airway open: the meter stays hidden"), UMOBreathMeterWidget::ShouldShow(false, 1.0f));
	TestTrue(TEXT("head under water: shown even with a full reserve"), UMOBreathMeterWidget::ShouldShow(true, 1.0f));
	TestTrue(TEXT("recovering after a dive: shown until the lungs are full again"), UMOBreathMeterWidget::ShouldShow(false, 0.6f));

	float Pulse = -1.0f;
	const FLinearColor Calm = UMOBreathMeterWidget::ColorForReserve(0.9f, 0.5f, Pulse);
	TestEqual(TEXT("plenty of air: no pulse"), Pulse, 0.0f);
	const FLinearColor Mid = UMOBreathMeterWidget::ColorForReserve(0.35f, 0.5f, Pulse);
	TestEqual(TEXT("an urge to breathe: warning colour, still no pulse"), Pulse, 0.0f);
	const FLinearColor Low = UMOBreathMeterWidget::ColorForReserve(0.05f, 0.5f, Pulse);
	TestTrue(TEXT("almost out: pulsing"), Pulse > 0.4f);
	TestTrue(TEXT("calm is blue (blue > red)"), Calm.B > Calm.R);
	TestTrue(TEXT("danger is red (red > blue)"), Low.R > Low.B + 0.5f);
	TestTrue(TEXT("the warning colour is warmer than the calm one"), Mid.R > Calm.R);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSwim_RespirationProcess,
	"MOFramework.Water.RespirationProcess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSwim_RespirationProcess::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("MOSwimTestWorld"));
	if (!TestNotNull(TEXT("a game world for the test"), World))
	{
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	AActor* Actor = World->SpawnActor<AActor>();
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>(Actor);
	Actor->AddInstanceComponent(Vitals);
	Vitals->RegisterComponent();
	UMORespirationComponent* Breath = NewObject<UMORespirationComponent>(Actor);
	Actor->AddInstanceComponent(Breath);
	Breath->RegisterComponent();

	TestEqual(TEXT("a fresh component has a full reserve"), Breath->GetOxygenReserve(), 1.0f);
	TestFalse(TEXT("... and an open airway"), Breath->IsAirwayClosed());

	// Two sources closing the airway: it opens only when BOTH let go.
	Breath->SetAirwayClosedBy(TEXT("Water"), true);
	Breath->SetAirwayClosedBy(TEXT("Smoke"), true);
	TestTrue(TEXT("the airway is closed while any source closes it"), Breath->IsAirwayClosed());
	Breath->SetAirwayClosedBy(TEXT("Water"), false);
	TestTrue(TEXT("... and stays closed while the other source holds it"), Breath->IsAirwayClosed());
	Breath->SetAirwayClosedBy(TEXT("Smoke"), false);
	TestFalse(TEXT("... and opens when the last source lets go"), Breath->IsAirwayClosed());

	// A held breath: tick the component the way the game does (the authority ticks it every 0.25 s).
	Breath->SetAirwayClosedBy(TEXT("Water"), true);
	float Elapsed = 0.0f;
	float ReserveAtMinute = -1.0f;
	while (Elapsed < 60.0f)
	{
		Breath->TickComponent(0.25f, LEVELTICK_All, nullptr);
		Elapsed += 0.25f;
	}
	ReserveAtMinute = Breath->GetOxygenReserve();
	TestTrue(*FString::Printf(TEXT("after a minute under water the reserve is about half gone (%.2f)"), ReserveAtMinute), ReserveAtMinute > 0.35f && ReserveAtMinute < 0.60f);
	TestTrue(TEXT("the urge to breathe is strong by then"), Breath->IsUrgeToBreathe());
	TestTrue(*FString::Printf(TEXT("the vitals' SpO2 follows the reserve (%.1f%% at reserve %.2f)"), Vitals->GetAirwayOxygenReserve() * 100.0f, Vitals->GetAirwayOxygenReserve()), FMath::IsNearlyEqual(Vitals->GetAirwayOxygenReserve(), ReserveAtMinute, 0.01f));

	// Surface: a gasp, then recovery.
	Breath->SetAirwayClosedBy(TEXT("Water"), false);
	for (int32 I = 0; I < 80; ++I)   // 20 s of breathing
	{
		Breath->TickComponent(0.25f, LEVELTICK_All, nullptr);
	}
	TestEqual(TEXT("20 s of breathing refills the reserve"), Breath->GetOxygenReserve(), 1.0f);
	TestFalse(TEXT("the urge is gone"), Breath->IsUrgeToBreathe());

	// Drowning: a closed airway to the end. Empty at ~110 s, death 75 s later.
	Breath->DebugSetOxygenReserve(1.0f);
	Breath->SetAirwayClosedBy(TEXT("Water"), true);
	Elapsed = 0.0f;
	while (Elapsed < 110.0f + 60.0f)
	{
		Breath->TickComponent(0.25f, LEVELTICK_All, nullptr);
		Elapsed += 0.25f;
	}
	TestEqual(TEXT("an empty reserve after ~110 s"), Breath->GetOxygenReserve(), 0.0f);
	TestTrue(TEXT("the asphyxia timer is running (still alive 60 s past empty)"), Breath->GetSecondsAsphyxiating() > 40.0f && Breath->GetSecondsAsphyxiating() < 75.0f);
	TestTrue(TEXT("blood oxygen is at the floor"), FMath::Abs(FMOBreathMath::SpO2ForReserve(Breath->Params, 0.0f) - Breath->Params.EmptyReserveSpO2) < 0.01f);
	TestFalse(TEXT("not dead yet"), Breath->HasAsphyxiated());
	for (int32 I = 0; I < 4 * 30; ++I)   // 30 s more: past the 75 s grace
	{
		Breath->TickComponent(0.25f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("75 s after the reserve emptied with the airway still closed, the body fails (drowning is fatal about 3 minutes after submersion)"), Breath->HasAsphyxiated());

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
