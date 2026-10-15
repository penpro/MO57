/**
 * The world seed as players see and type it (UMOGameSettings::SeedToText / SeedFromString).
 * Run: python Tools/ue.py auto --filter "MOFramework.Settings.Seed"
 *
 * Found Oct 15 2026 by a first-ten-minutes pass: the New Game panel showed "Seed: 15,204" (FText::AsNumber groups digits), put the
 * same grouped text into its seed box, and Start parsed the box -- "15,204" is not numeric, so it was hashed as a word and the game
 * started world 1026818223. A player who wrote down 15204 to replay or share the world got a different one.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MOGameSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSettings_SeedTextRoundTrip,
	"MOFramework.Settings.SeedTextRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSettings_SeedTextRoundTrip::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("a seed is shown as plain digits (no grouping)"), UMOGameSettings::SeedToText(15204).ToString(), FString(TEXT("15204")));
	TestEqual(TEXT("a negative seed keeps its sign"), UMOGameSettings::SeedToText(-1829368329).ToString(), FString(TEXT("-1829368329")));

	for (const int32 Seed : { 0, 7, 15204, 1029024224, -1829368329, MAX_int32, MIN_int32 + 1 })
	{
		TestEqual(*FString::Printf(TEXT("round trip %d: what the panel shows is what Start parses"), Seed),
			UMOGameSettings::SeedFromString(UMOGameSettings::SeedToText(Seed).ToString()), Seed);
	}

	TestEqual(TEXT("\"15,204\" (pasted with grouping) is 15204, not a word"), UMOGameSettings::SeedFromString(TEXT("15,204")), 15204);
	TestEqual(TEXT("\"15 204\" is 15204"), UMOGameSettings::SeedFromString(TEXT("15 204")), 15204);
	TestEqual(TEXT("\"15.204\" (European grouping) is 15204"), UMOGameSettings::SeedFromString(TEXT("15.204")), 15204);
	TestEqual(TEXT("\"-1,234\" is -1234"), UMOGameSettings::SeedFromString(TEXT("-1,234")), -1234);
	TestEqual(TEXT("surrounding spaces are ignored"), UMOGameSettings::SeedFromString(TEXT("  42 ")), 42);
	TestEqual(TEXT("empty is 0"), UMOGameSettings::SeedFromString(TEXT("")), 0);

	// Word seeds still hash, and stably.
	const int32 Word = UMOGameSettings::SeedFromString(TEXT("Dragon Valley"));
	TestEqual(TEXT("a word seed is stable"), UMOGameSettings::SeedFromString(TEXT("Dragon Valley")), Word);
	TestNotEqual(TEXT("a word seed is not 0"), Word, 0);
	TestNotEqual(TEXT("digits mixed with letters are a word, not a number"), UMOGameSettings::SeedFromString(TEXT("12ab")), 12);
	TestNotEqual(TEXT("a lone minus is a word"), UMOGameSettings::SeedFromString(TEXT("-")), 0);
	return true;
}

#endif
