// What the air feels like (FMOFeelsLike::ComputeTerms) has no steps: the air warms and cools by degrees, and the body's heat balance reads this number.
// Until Nov 2026 a term switched on at its threshold: in a full wind the feels-like fell 20 C as the air cooled through 10 C (expected here: at most 0.6 C
// between two airs 0.1 C apart; failing before: 20.1). Run: python Tools/ue.py auto --filter "MOFramework.Weather.FeelsLike"

#include "Misc/AutomationTest.h"
#include "MOWeatherIntegrationSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOFeelsLikeTests
{
	FMOFeelsLike Feel(float AirC, float Wind, float Rain, float Humidity = 0.5f)
	{
		FMOFeelsLike Out;
		Out.AirC = AirC;
		Out.Exposure.Wind = Wind;
		Out.Exposure.Rain = Rain;
		Out.ComputeTerms([Humidity]() { return Humidity; });
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWeather_FeelsLikeHasNoSteps,
	"MOFramework.Weather.FeelsLikeHasNoSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWeather_FeelsLikeHasNoSteps::RunTest(const FString& Parameters)
{
	using MOFeelsLikeTests::Feel;

	// Across the air, in every weather: wind chill's slope at full wind (5 C per degree) is the steepest thing here.
	float WorstJump = 0.0f;
	for (const float Wind : { 0.0f, 0.05f, 0.1f, 0.11f, 0.28f, 0.5f, 1.0f })
	{
		for (const float Rain : { 0.0f, 0.05f, 0.1f, 0.11f, 0.7f, 1.0f })
		{
			for (float Air = -20.0f; Air < 45.0f; Air += 0.1f)
			{
				WorstJump = FMath::Max(WorstJump, FMath::Abs(Feel(Air + 0.1f, Wind, Rain).FeelsLikeC - Feel(Air, Wind, Rain).FeelsLikeC));
			}
		}
	}
	TestTrue(*FString::Printf(TEXT("the worst change for 0.1 C of air is %.2f C (a term switching on was 20 C)"), WorstJump), WorstJump <= 0.65f);

	// Across the wind and the rain: nothing switches on at 10 % any more.
	for (float Exposure = 0.0f; Exposure < 1.0f; Exposure += 0.01f)
	{
		TestTrue(TEXT("a little more wind is a little colder"), FMath::Abs(Feel(0.0f, Exposure + 0.01f, 0.0f).FeelsLikeC - Feel(0.0f, Exposure, 0.0f).FeelsLikeC) <= 0.25f);
		TestTrue(TEXT("a little more rain is a little colder"), FMath::Abs(Feel(0.0f, 0.0f, Exposure + 0.01f).FeelsLikeC - Feel(0.0f, 0.0f, Exposure).FeelsLikeC) <= 0.1f);
	}

	// The terms are what they were where they are in full, and absent where they do not apply (N-175's measurement: "Rain" at 11.2 C feels 7.7 C).
	TestEqual(TEXT("at 11.2 C, wind 0.28, rain 0.7: no wind chill above 10 C"), Feel(11.2f, 0.28f, 0.7f).WindChillC, 0.0f);
	TestEqual(TEXT("at 11.2 C, wind 0.28, rain 0.7: feels 7.7 C"), Feel(11.2f, 0.28f, 0.7f).FeelsLikeC, 7.7f, 0.01f);
	TestEqual(TEXT("at 0 C the wind chill is in full"), Feel(0.0f, 0.5f, 0.0f).WindChillC, -10.0f, 0.001f);
	TestEqual(TEXT("at 10 C it has not started"), Feel(10.0f, 1.0f, 0.0f).WindChillC, 0.0f, 0.001f);
	TestEqual(TEXT("at 7.5 C it is half on"), Feel(7.5f, 1.0f, 0.0f).WindChillC, -10.0f, 0.001f);
	TestEqual(TEXT("at 20 C rain does not chill"), Feel(20.0f, 0.0f, 1.0f).WetColdC, 0.0f, 0.001f);
	TestEqual(TEXT("in the heat the humidity adds"), Feel(40.0f, 0.0f, 0.0f, 0.5f).HeatIndexC, 5.0f, 0.001f);
	TestEqual(TEXT("at 25 C it adds nothing"), Feel(25.0f, 0.0f, 0.0f, 1.0f).HeatIndexC, 0.0f, 0.001f);
	return true;
}

#endif
