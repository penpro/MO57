// A world's creation settings (Wes, Oct 22 2026: per-world multipliers, world-shape knobs, a starting biome). The defaults must leave the generator exactly as
// authored (every existing world, save and golden checksum depends on it); every knob clamps into its range; a pace never reaches zero. Headless-safe.

#include "Misc/AutomationTest.h"
#include "MOWorldCreationTypes.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOBiomeDatabaseSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldCreation_DefaultIsAuthored,
	"MOFramework.WorldCreation.DefaultShapeKeepsTheGenerator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldCreation_DefaultIsAuthored::RunTest(const FString& Parameters)
{
	UMOWorldGenSettings::SetActiveWorldShape(FMOWorldShape());
	const FMOWorldGenParamsRef Authored = UMOWorldGenSettings::BuildParams();

	FMOWorldGenTuning T = Authored->Tuning;
	FMOWorldCreation::ApplyShape(FMOWorldShape(), T);
	TestEqual(TEXT("moisture wavelength"), T.MoistureWavelengthCm, Authored->Tuning.MoistureWavelengthCm);
	TestEqual(TEXT("vertical scale"), T.VerticalScale, Authored->Tuning.VerticalScale);
	TestEqual(TEXT("massif amplitude"), T.MassifAmplitudeM, Authored->Tuning.MassifAmplitudeM);
	TestEqual(TEXT("lake chance"), T.LakeChance, Authored->Tuning.LakeChance);
	TestEqual(TEXT("cave entrance chance"), T.CaveEntranceChance, Authored->Tuning.CaveEntranceChance);
	TestEqual(TEXT("vegetation"), T.VegetationDensityScale, 1.0f);

	// A shaped world is a different world (its own hash), and a default one is not.
	FMOWorldShape Wide;
	Wide.BiomeSize = 2.0f;
	Wide.WorldHeight = 1.25f;
	UMOWorldGenSettings::SetActiveWorldShape(Wide);
	const FMOWorldGenParamsRef Shaped = UMOWorldGenSettings::BuildParams();
	UMOWorldGenSettings::SetActiveWorldShape(FMOWorldShape());
	const FMOWorldGenParamsRef Again = UMOWorldGenSettings::BuildParams();
	TestEqual(TEXT("a default shape keeps the authored hash"), Again->GetHash(), Authored->GetHash());
	TestNotEqual(TEXT("a shaped world has its own hash"), Shaped->GetHash(), Authored->GetHash());
	TestEqual(TEXT("biome size 2 doubles the moisture wavelength"), Shaped->Tuning.MoistureWavelengthCm, 2.0f * Authored->Tuning.MoistureWavelengthCm);
	TestEqual(TEXT("biome size 2 doubles the temperature wavelength"), Shaped->Tuning.TemperatureWavelengthCm, 2.0f * Authored->Tuning.TemperatureWavelengthCm);
	TestTrue(TEXT("height 1.25 raises the highest ground"), Shaped->MaxHeightCm > Authored->MaxHeightCm * 1.2f);

	// Vegetation joins the hash only when it is not 1 (a default world's hash is unchanged by the new field).
	FMOWorldShape Sparse;
	Sparse.Vegetation = 0.5f;
	UMOWorldGenSettings::SetActiveWorldShape(Sparse);
	const FMOWorldGenParamsRef Thin = UMOWorldGenSettings::BuildParams();
	UMOWorldGenSettings::SetActiveWorldShape(FMOWorldShape());
	TestEqual(TEXT("vegetation 0.5 reaches the tuning"), Thin->Tuning.VegetationDensityScale, 0.5f);
	TestNotEqual(TEXT("vegetation 0.5 changes the hash"), Thin->GetHash(), Authored->GetHash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldCreation_Knobs,
	"MOFramework.WorldCreation.KnobsClampAndPaceNeverZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldCreation_Knobs::RunTest(const FString& Parameters)
{
	TSet<FName> Ids;
	TMap<EMOWorldKnobGroup, int32> PerGroup;
	FMOWorldCreationSettings S;
	for (const FMOWorldKnob& K : FMOWorldCreation::GetKnobs())
	{
		TestFalse(*FString::Printf(TEXT("%s: one entry per id"), *K.Id.ToString()), Ids.Contains(K.Id));
		Ids.Add(K.Id);
		TestNotNull(*FString::Printf(TEXT("%s: has a field"), *K.Id.ToString()), K.Field(S));
		TestTrue(*FString::Printf(TEXT("%s: default inside its range"), *K.Id.ToString()), K.Default >= K.Min && K.Default <= K.Max);
		TestEqual(*FString::Printf(TEXT("%s: a new world starts at the default"), *K.Id.ToString()), FMOWorldCreation::GetKnob(S, K.Id), K.Default);
		if (K.Group == EMOWorldKnobGroup::Pace)
		{
			TestTrue(*FString::Printf(TEXT("%s: a pace never reaches zero"), *K.Id.ToString()), K.Min > 0.0f);
			TestFalse(*FString::Printf(TEXT("%s: a pace is a multiplier, not a switch"), *K.Id.ToString()), K.bSwitch);
		}
		if (K.Group == EMOWorldKnobGroup::Rules)
		{
			TestTrue(*FString::Printf(TEXT("%s: a world rule is an on / off switch"), *K.Id.ToString()), K.bSwitch);
		}
		++PerGroup.FindOrAdd(K.Group);
	}
	TestEqual(TEXT("six shape knobs"), PerGroup.FindRef(EMOWorldKnobGroup::Shape), 6);
	TestEqual(TEXT("six pace knobs"), PerGroup.FindRef(EMOWorldKnobGroup::Pace), 6);
	TestEqual(TEXT("one world rule (creative mode)"), PerGroup.FindRef(EMOWorldKnobGroup::Rules), 1);

	TestTrue(TEXT("set a known knob"), FMOWorldCreation::SetKnob(S, TEXT("Crafting"), 0.0f));
	TestTrue(TEXT("crafting 0 clamps to the floor"), S.Pace.Crafting > 0.0f && S.Pace.Crafting <= 0.1f + 1e-6f);
	TestTrue(TEXT("set biome size"), FMOWorldCreation::SetKnob(S, TEXT("BiomeSize"), 9.0f));
	TestEqual(TEXT("biome size clamps to its top"), S.Shape.BiomeSize, 2.5f);
	TestTrue(TEXT("set world height"), FMOWorldCreation::SetKnob(S, TEXT("WorldHeight"), 1.13f));
	TestTrue(TEXT("world height snaps to its step"), FMath::IsNearlyEqual(S.Shape.WorldHeight, 1.15f, 1e-4f));
	TestFalse(TEXT("an unknown knob is refused"), FMOWorldCreation::SetKnob(S, TEXT("NoSuchKnob"), 1.0f));

	FMOWorldPace Bad;
	Bad.Gathering = 0.0f;
	Bad.Building = NAN;
	TestTrue(TEXT("a zero pace in a save reads as the floor"), Bad.For(EMOWorldPace::Gathering) >= 0.1f - 1e-6f);
	TestEqual(TEXT("a NaN pace reads as real time"), Bad.For(EMOWorldPace::Building), 1.0f);

	FMOWorldCreationSettings Wild;
	Wild.Shape.Mountains = -3.0f;
	Wild.Pace.Inspecting = 1000.0f;
	FMOWorldCreation::Sanitize(Wild);
	TestEqual(TEXT("sanitize: mountains back to 0"), Wild.Shape.Mountains, 0.0f);
	TestEqual(TEXT("sanitize: inspecting back to 4"), Wild.Pace.Inspecting, 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldCreation_StartingBiomes,
	"MOFramework.WorldCreation.StartingBiomesAreLowland",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldCreation_StartingBiomes::RunTest(const FString& Parameters)
{
	// Wes, Oct 23: "gate the higher elevation biomes, so no alpine heath, or alpine rock or highlands". The rule reads the band; the table is checked
	// too, so a retuned band that would let a summit biome back in (or shut a lowland one out) fails here.
	TestTrue(TEXT("a climate band is a start"), FMOWorldCreation::IsStartingBand(-100000.0f, 0.0f));
	TestTrue(TEXT("a strand band (floor under the sea) is a start"), FMOWorldCreation::IsStartingBand(-300.0f, 0.0f));
	TestFalse(TEXT("a band from 320 m up is not"), FMOWorldCreation::IsStartingBand(32000.0f, 0.0f));
	TestFalse(TEXT("a steep-slope band is not"), FMOWorldCreation::IsStartingBand(-100000.0f, 25.0f));
	for (const TCHAR* Id : { TEXT("AlpineHeath"), TEXT("AlpineRock"), TEXT("RockyHighland") })
	{
		const FMOBiomeDefinitionRow* Row = UMOBiomeDatabaseSettings::GetBiomeDefinition(Id);
		if (TestNotNull(FString::Printf(TEXT("%s in DT_Biomes"), Id), Row))
		{
			TestFalse(FString::Printf(TEXT("%s is not a start"), Id), FMOWorldCreation::IsStartingBand(Row->HeightMin, Row->SlopeMinDeg));
		}
	}
	for (const TCHAR* Id : { TEXT("Meadow"), TEXT("TemperateForest"), TEXT("Rainforest"), TEXT("ElectricForest"), TEXT("ShoreStrand"), TEXT("NordicStrand") })
	{
		const FMOBiomeDefinitionRow* Row = UMOBiomeDatabaseSettings::GetBiomeDefinition(Id);
		if (TestNotNull(FString::Printf(TEXT("%s in DT_Biomes"), Id), Row))
		{
			TestTrue(FString::Printf(TEXT("%s is a start"), Id), FMOWorldCreation::IsStartingBand(Row->HeightMin, Row->SlopeMinDeg));
		}
	}
	return true;
}

#endif
