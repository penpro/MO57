// Footsteps: the pure decisions (gait, sound ids, which kind of ground), the foot-plant detector against a synthetic walk cycle (including the cases that must NOT make a sound: standing, a tiptoe too
// shallow to read, a ledge), the walked-distance fallback, and the DATA check that every biome's footstep set has sound-bank rows (a typo in a biome row would otherwise be a silent walk).

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "MOAudioSettings.h"
#include "MOAudioTypes.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOFootstepComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * A foot's world horizontal speed over a gait cycle, shaped like the measured MO animations: half the cycle the foot is a swing (a half-sine peaking at PeakCm/s), the other half it is the stance
	 * foot, nearly still (StanceCm/s). Jitter is a deterministic +-JitterCm wobble.
	 */
	float FootSpeedAt(float TimeSeconds, float PeriodSeconds, float StanceCm, float PeakCm, float JitterCm, float PhaseOffset = 0.0f)
	{
		const float Phase = FMath::Fmod(TimeSeconds / PeriodSeconds + PhaseOffset, 1.0f);
		const float Wobble = JitterCm * FMath::Sin(TimeSeconds * 97.0f);
		return (Phase < 0.5f ? StanceCm + (PeakCm - StanceCm) * FMath::Sin(Phase * 2.0f * PI) : StanceCm) + Wobble;
	}

	int32 CountPlants(float DurationSeconds, float PeriodSeconds, float BodyCm, float StanceCm, float PeakCm, float JitterCm = 0.0f)
	{
		FMOFootPlantDetector Detector;
		int32 Plants = 0;
		const float Dt = 1.0f / 60.0f;
		for (float T = 0.0f; T < DurationSeconds; T += Dt)
		{
			if (Detector.Update(FMath::Max(0.0f, FootSpeedAt(T, PeriodSeconds, StanceCm, PeakCm, JitterCm)), BodyCm, Dt))
			{
				++Plants;
			}
		}
		return Plants;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFootstep_Decisions,
	"MOFramework.Footsteps.Decisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOFootstep_Decisions::RunTest(const FString& Parameters)
{
	auto Same = [this](const TCHAR* What, auto Actual, auto Expected)
	{
		TestTrue(What, Actual == Expected);
	};

	// Gait.
	Same(TEXT("crouched is a sneak at any speed"), FMOFootstepMath::GaitFor(true, 700.0f, 475.0f), EMOFootstepGait::Sneak);
	Same(TEXT("a walk"), FMOFootstepMath::GaitFor(false, 400.0f, 475.0f), EMOFootstepGait::Walk);
	Same(TEXT("standing is a walk (it makes no steps anyway)"), FMOFootstepMath::GaitFor(false, 0.0f, 475.0f), EMOFootstepGait::Walk);
	Same(TEXT("a jog is a run"), FMOFootstepMath::GaitFor(false, 550.0f, 475.0f), EMOFootstepGait::Run);
	TestTrue(TEXT("louder as the gait speeds up"), FMOFootstepMath::VolumeFor(EMOFootstepGait::Sneak) < FMOFootstepMath::VolumeFor(EMOFootstepGait::Walk)
		&& FMOFootstepMath::VolumeFor(EMOFootstepGait::Walk) < FMOFootstepMath::VolumeFor(EMOFootstepGait::Run));
	TestTrue(TEXT("longer steps as the gait speeds up"), FMOFootstepMath::StepLengthCm(EMOFootstepGait::Sneak) < FMOFootstepMath::StepLengthCm(EMOFootstepGait::Walk)
		&& FMOFootstepMath::StepLengthCm(EMOFootstepGait::Walk) < FMOFootstepMath::StepLengthCm(EMOFootstepGait::Run));

	// Sound ids: the set is the middle of the bank id, two variants.
	Same(TEXT("variant 0"), FMOFootstepMath::SoundIdFor(FName("LeafCrunch"), 0), FName("SFX.Footsteps.LeafCrunch1"));
	Same(TEXT("variant 1"), FMOFootstepMath::SoundIdFor(FName("LeafCrunch"), 1), FName("SFX.Footsteps.LeafCrunch2"));
	Same(TEXT("variants wrap"), FMOFootstepMath::SoundIdFor(FName("LeafCrunch"), 2), FName("SFX.Footsteps.LeafCrunch1"));
	Same(TEXT("a negative variant does not break the id"), FMOFootstepMath::SoundIdFor(FName("SandTrail"), -1), FName("SFX.Footsteps.SandTrail2"));

	// Ground: on the surface the biome decides, well above is built, well below is a cave or pit.
	using EGround = FMOFootstepMath::EGround;
	Same(TEXT("on the surface"), FMOFootstepMath::ClassifyGround(1000.0f, 1000.0f), EGround::Terrain);
	Same(TEXT("a voxel above (terrain mesh tolerance)"), FMOFootstepMath::ClassifyGround(1059.0f, 1000.0f), EGround::Terrain);
	Same(TEXT("a floor a metre up"), FMOFootstepMath::ClassifyGround(1100.0f, 1000.0f), EGround::Structure);
	Same(TEXT("a hillside's slope error is tolerated downhill"), FMOFootstepMath::ClassifyGround(860.0f, 1000.0f), EGround::Terrain);
	Same(TEXT("a cave floor"), FMOFootstepMath::ClassifyGround(500.0f, 1000.0f), EGround::Underground);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFootstep_PlantDetector,
	"MOFramework.Footsteps.PlantDetector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOFootstep_PlantDetector::RunTest(const FString& Parameters)
{
	// The measured walk: body 250 cm/s, one swing per foot every 0.71 s, swing peak ~750, stance ~20. Ten seconds hold 14 cycles.
	const int32 Walk = CountPlants(10.0f, 0.71f, 250.0f, 20.0f, 750.0f);
	TestTrue(*FString::Printf(TEXT("a walk plants once per cycle (got %d in 10 s, expected 13-14)"), Walk), Walk >= 13 && Walk <= 14);

	// With +-15 cm/s of jitter on the signal the count must not change (the real signal is noisy).
	const int32 NoisyWalk = CountPlants(10.0f, 0.71f, 250.0f, 20.0f, 750.0f, 15.0f);
	TestEqual(TEXT("jitter on the signal makes no extra steps"), NoisyWalk, Walk);

	// The measured sprint: body 700 cm/s, cycle ~0.5 s, swing peak ~1300, the stance foot slides at ~100-230 (the animation's speed does not match the body's exactly).
	const int32 Sprint = CountPlants(10.0f, 0.5f, 700.0f, 150.0f, 1300.0f, 40.0f);
	TestTrue(*FString::Printf(TEXT("a sprint plants once per cycle despite the sliding stance foot (got %d in 10 s, expected 19-20)"), Sprint), Sprint >= 19 && Sprint <= 20);

	// A slow creep: body 60 cm/s (below the reference floor), swing peak 150.
	const int32 Creep = CountPlants(10.0f, 1.4f, 60.0f, 10.0f, 150.0f);
	TestTrue(*FString::Printf(TEXT("a creep still plants (got %d in 10 s, expected 6-7)"), Creep), Creep >= 6 && Creep <= 7);

	// Not a step: standing, and a foot shuffling on the spot (speed below the reference floor's swing threshold).
	TestEqual(TEXT("standing makes no step"), CountPlants(10.0f, 0.71f, 0.0f, 0.0f, 0.0f), 0);
	TestEqual(TEXT("shuffling on the spot (a 55 cm/s foot twitch) makes no step"), CountPlants(10.0f, 0.71f, 0.0f, 5.0f, 55.0f), 0);

	// A dip in the swinging foot's speed is not a plant (measured in the crouch walk: 227 -> 44 -> 198 cm/s within 0.1 s, mid-swing): exactly one plant for the whole swing.
	{
		FMOFootPlantDetector Dip;
		int32 DipPlants = 0;
		const float DipDt = 1.0f / 60.0f;
		auto Feed = [&](float Speed, float Seconds)
		{
			for (float T = 0.0f; T < Seconds; T += DipDt)
			{
				DipPlants += Dip.Update(Speed, 200.0f, DipDt) ? 1 : 0;
			}
		};
		Feed(300.0f, 0.05f);   // the swing starts
		Feed(40.0f, 0.04f);    // a dip well under the plant threshold (60 cm/s at this body speed), too short to be a stance
		Feed(350.0f, 0.25f);   // and the swing carries on
		Feed(15.0f, 0.3f);     // the real plant
		TestEqual(TEXT("a mid-swing dip makes no extra step"), DipPlants, 1);
	}

	// A swing that never ends is dropped quietly: the foot hovers between the swing and plant thresholds for 3 s (animation frozen, pawn dragged); no step comes out of it, and the detector recovers.
	FMOFootPlantDetector Detector;
	int32 Plants = 0;
	const float Dt = 1.0f / 60.0f;
	Detector.Update(400.0f, 250.0f, Dt);   // the swing begins
	for (float T = 0.0f; T < 3.0f; T += Dt)
	{
		Plants += Detector.Update(110.0f, 250.0f, Dt) ? 1 : 0;   // neither fast enough to be a swing nor slow enough to be a plant
	}
	TestEqual(TEXT("a swing that never ends makes no step"), Plants, 0);
	TestFalse(TEXT("... and is dropped"), Detector.IsSwinging());
	for (float T = 0.0f; T < 0.25f; T += Dt)
	{
		Detector.Update(400.0f, 250.0f, Dt);   // a fresh swing, long enough to be a real one
	}
	TestTrue(TEXT("the detector recovers: the next swing is seen"), Detector.IsSwinging());
	TestTrue(TEXT("... and plants"), Detector.Update(10.0f, 250.0f, Dt));

	Detector.Reset();
	TestFalse(TEXT("a reset detector is not mid-swing"), Detector.IsSwinging());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFootstep_StrideFallback,
	"MOFramework.Footsteps.StrideFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOFootstep_StrideFallback::RunTest(const FString& Parameters)
{
	const float Step = FMOFootstepMath::StepLengthCm(EMOFootstepGait::Walk);
	const float Trigger = Step * FMOFootstepMath::FallbackFactor;

	FMOStrideFallback Fallback;
	TestFalse(TEXT("a short stretch is silent"), Fallback.Advance(Trigger * 0.5f, Step));
	TestTrue(TEXT("enough ground without a plant fires a step"), Fallback.Advance(Trigger * 0.6f, Step));
	TestFalse(TEXT("and starts counting again"), Fallback.Advance(Trigger * 0.5f, Step));

	// A plant restarts the count: the fallback never doubles an animation step.
	Fallback.Advance(Trigger * 0.9f, Step);
	Fallback.NotePlant();
	TestFalse(TEXT("a plant resets the distance"), Fallback.Advance(Trigger * 0.2f, Step));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFootstep_BiomeData,
	"MOFramework.Footsteps.BiomeSetsHaveSounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOFootstep_BiomeData::RunTest(const FString& Parameters)
{
	auto Same = [this](const TCHAR* What, auto Actual, auto Expected)
	{
		TestTrue(What, Actual == Expected);
	};

	const UMOBiomeDatabaseSettings* BiomeSettings = GetDefault<UMOBiomeDatabaseSettings>();
	UDataTable* Biomes = BiomeSettings ? BiomeSettings->GetBiomeDefinitionsDataTable() : nullptr;
	const UMOAudioSettings* AudioSettings = GetDefault<UMOAudioSettings>();
	UDataTable* Bank = AudioSettings ? Cast<UDataTable>(AudioSettings->DefaultAudioBank.TryLoad()) : nullptr;
	if (!Biomes || !Bank)
	{
		AddWarning(TEXT("the biome table or the audio bank is not configured in this context: nothing to check"));
		return true;
	}

	FMOBiomeDefinitionRow Defaults;
	Same(TEXT("a new biome row walks on grass until told otherwise"), Defaults.FootstepSet, FName("GrassTread"));

	for (const FName& RowName : Biomes->GetRowNames())
	{
		const FMOBiomeDefinitionRow* Row = Biomes->FindRow<FMOBiomeDefinitionRow>(RowName, TEXT("footstep test"));
		if (!Row)
		{
			continue;
		}
		TestFalse(*FString::Printf(TEXT("biome %s names a footstep set"), *RowName.ToString()), Row->FootstepSet.IsNone());
		for (int32 Variant = 0; Variant < 2; ++Variant)
		{
			const FName Id = FMOFootstepMath::SoundIdFor(Row->FootstepSet, Variant);
			TestNotNull(*FString::Printf(TEXT("biome %s: sound bank row %s exists"), *RowName.ToString(), *Id.ToString()), Bank->FindRow<FMOAudioBankRow>(Id, TEXT("footstep test"), false));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
