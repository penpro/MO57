/**
 * The character creator's colours: UMOAppearanceSubsystem::ComputeSkinParameters / eye presets and FMOCharacterAppearance::CopyColorsFrom.
 * Run: python Tools/ue.py auto --filter "MOFramework.Appearance"
 *
 * Oct 15 2026: the appearance record's colours were three RGB values nothing applied (ApplyColors was a TODO); they are now the MetaHuman
 * lookdev's own parameters, and the default must leave the MetaHuman exactly as shipped.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MOAppearanceSubsystem.h"
#include "MOCharacterAppearance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAppearance_Colors,
	"MOFramework.Appearance.Colors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAppearance_Colors::RunTest(const FString& Parameters)
{
	// The default skin tone is the skin as baked: the material's own neutral values (value 1, saturation offset 0, white multiply).
	float Value = 0.0f, Saturation = 1.0f;
	FLinearColor Multiply;
	UMOAppearanceSubsystem::ComputeSkinParameters(FMOCharacterAppearance::DefaultSkinTone, Value, Saturation, Multiply);
	TestEqual(TEXT("baked tone: value 1"), Value, 1.0f);
	TestEqual(TEXT("baked tone: no saturation change"), Saturation, 0.0f);
	TestTrue(TEXT("baked tone: white multiply"), Multiply.Equals(FLinearColor::White));

	// Darker the higher the tone; lighter (a little) below the baked one.
	float Previous = 10.0f;
	for (float Tone = 0.0f; Tone <= 1.0f; Tone += 0.1f)
	{
		UMOAppearanceSubsystem::ComputeSkinParameters(Tone, Value, Saturation, Multiply);
		TestTrue(FString::Printf(TEXT("value falls as the tone darkens (%.1f)"), Tone), Value < Previous);
		Previous = Value;
	}
	UMOAppearanceSubsystem::ComputeSkinParameters(0.0f, Value, Saturation, Multiply);
	TestTrue(TEXT("the lightest tone is only a little lighter than baked"), Value > 1.0f && Value <= 1.1f);
	UMOAppearanceSubsystem::ComputeSkinParameters(1.0f, Value, Saturation, Multiply);
	TestTrue(TEXT("the darkest tone is much darker, never black"), Value >= 0.15f && Value <= 0.3f);

	// Eye presets (the procedural iris); None leaves the shipped baked eye.
	const TArray<FMOEyeColorPreset>& Presets = UMOAppearanceSubsystem::GetEyeColorPresets();
	TestTrue(TEXT("several eye colours"), Presets.Num() >= 5);
	TestEqual(TEXT("the creator starts on Brown"), Presets[0].Id, FName(TEXT("Brown")));
	TestTrue(TEXT("blue sits on the blue end of the procedural iris hue (0.15)"), FMath::IsNearlyEqual(UMOAppearanceSubsystem::FindEyeColorPreset(TEXT("Blue"))->PrimaryHue, 0.15f));
	TestNull(TEXT("None = no preset"), UMOAppearanceSubsystem::FindEyeColorPreset(NAME_None));
	TestNotNull(TEXT("Blue exists"), UMOAppearanceSubsystem::FindEyeColorPreset(TEXT("Blue")));

	// Colours only: the creator edits colours over a pawn's own meshes.
	FMOCharacterAppearance Pawn;
	Pawn.BodyAssetPath = TEXT("/Game/MetaHumans/MH_F_1/Body/SK_Body");
	FMOCharacterAppearance Chosen;
	Chosen.SkinTone = 0.7f;
	Chosen.HairMelanin = 0.2f;
	Chosen.EyeColor = TEXT("Green");
	Pawn.CopyColorsFrom(Chosen);
	TestEqual(TEXT("skin copied"), Pawn.SkinTone, 0.7f);
	TestEqual(TEXT("eyes copied"), Pawn.EyeColor, FName(TEXT("Green")));
	TestEqual(TEXT("the pawn keeps its own body mesh"), Pawn.BodyAssetPath, FString(TEXT("/Game/MetaHumans/MH_F_1/Body/SK_Body")));
	return true;
}

#endif
