// The weather director's chain (UMOWeatherDirectorSubsystem::ChooseNextPreset / ChooseSpellHours): pure functions, no world.

#include "Misc/AutomationTest.h"
#include "MOWeatherDirector.h"
#include "MOWeatherTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWeatherDirector_Chain,
	"MOFramework.Weather.DirectorChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWeatherDirector_Chain::RunTest(const FString& Parameters)
{
	FRandomStream Rng(2026);
	const TArray<FName> All = { "Clear_Skies", "Partly_Cloudy", "Cloudy", "Overcast", "Foggy", "Rain_Light", "Rain", "Rain_Thunderstorm", "Snow_Light", "Snow", "Snow_Blizzard" };

	// 1. Temperature decides rain or snow, from any starting weather.
	int32 SnowWhenWarm = 0, RainWhenCold = 0, Wet = 0;
	for (const FName& From : All)
	{
		for (int32 I = 0; I < 3000; ++I)
		{
			const FName Warm = UMOWeatherDirectorSubsystem::ChooseNextPreset(From, 14.0f, Rng);
			const FName Cold = UMOWeatherDirectorSubsystem::ChooseNextPreset(From, -6.0f, Rng);
			SnowWhenWarm += Warm.ToString().StartsWith(TEXT("Snow")) ? 1 : 0;
			RainWhenCold += Cold.ToString().StartsWith(TEXT("Rain")) ? 1 : 0;
			Wet += UMOWeatherDirectorSubsystem::IsWet(Warm) ? 1 : 0;
		}
	}
	TestEqual(TEXT("no snow at 14 C"), SnowWhenWarm, 0);
	TestEqual(TEXT("no rain at -6 C"), RainWhenCold, 0);
	TestTrue(TEXT("wet fronts do happen (the chain is not stuck dry)"), Wet > 0);

	// 2. A long run: every base state is visited, and the sky is mostly fair (time-weighted by the spell lengths).
	TSet<FString> Visited;
	double Hours = 0.0, FairHours = 0.0, StormHours = 0.0, WetHours = 0.0;
	FName Current = "Clear_Skies";
	for (int32 I = 0; I < 6000; ++I)
	{
		Current = UMOWeatherDirectorSubsystem::ChooseNextPreset(Current, 12.0f, Rng);
		const float Spell = UMOWeatherDirectorSubsystem::ChooseSpellHours(Current, Rng);
		Visited.Add(Current.ToString());
		Hours += Spell;
		FairHours += (Current == "Clear_Skies" || Current == "Partly_Cloudy") ? Spell : 0.0;
		StormHours += (Current == "Rain_Thunderstorm") ? Spell : 0.0;
		WetHours += UMOWeatherDirectorSubsystem::IsWet(Current) ? Spell : 0.0;
		TestTrue(TEXT("spells last between half an hour and ten hours"), Spell >= 0.5f && Spell <= 10.0f);
	}
	AddInfo(FString::Printf(TEXT("6000 fronts over %.0f game days at 12 C: %.0f%% fair, %.0f%% wet, %.1f%% thunderstorm; %d of 8 warm presets visited"), Hours / 24.0, 100.0 * FairHours / Hours,
		100.0 * WetHours / Hours, 100.0 * StormHours / Hours, Visited.Num()));
	TestEqual(TEXT("all eight warm-weather presets turn up"), Visited.Num(), 8);
	TestTrue(TEXT("fair weather is the most common sky (35%..65% of the time)"), FairHours / Hours > 0.35 && FairHours / Hours < 0.65);
	TestTrue(TEXT("rain is a minority of the time (10%..35%)"), WetHours / Hours > 0.10 && WetHours / Hours < 0.35);
	TestTrue(TEXT("thunderstorms are rare (under 6% of the time)"), StormHours / Hours < 0.06);

	// 3. Deterministic for a given stream.
	FRandomStream A(77), B(77);
	TestEqual(TEXT("the same stream gives the same front"), UMOWeatherDirectorSubsystem::ChooseNextPreset("Overcast", 8.0f, A), UMOWeatherDirectorSubsystem::ChooseNextPreset("Overcast", 8.0f, B));
	return true;
}

// The ground's answer to the weather (FMOGroundWeather::Advance, BR18-16): real rates on game time, not a weather plugin's demo ones.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWeather_GroundAnswersTheWeather,
	"MOFramework.Weather.GroundAnswersTheWeather",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWeather_GroundAnswersTheWeather::RunTest(const FString& Parameters)
{
	const FMOGroundWeatherRates Rates;   // the shipped defaults
	FMOGroundWeatherInputs Rain;
	Rain.Rain01 = 0.7f;   // UDW "Rain" (7 of 10)
	Rain.Cloud01 = 1.0f;
	Rain.TemperatureC = 10.0f;
	FMOGroundWeatherInputs Sunny;
	Sunny.TemperatureC = 15.0f;
	FMOGroundWeatherInputs Night = Sunny;
	Night.bSunUp = false;

	// 1. A steady rain soaks the surface in ten minutes, half in five; heavier rain faster.
	FMOGroundWeatherState G;
	FMOGroundWeather::Advance(G, Rain, 5.0 * 60.0, Rates);
	TestTrue(TEXT("half soaked after 5 min of rain"), FMath::IsNearlyEqual(G.Wetness, 0.5f, 0.01f));
	FMOGroundWeather::Advance(G, Rain, 5.0 * 60.0, Rates);
	TestTrue(TEXT("soaked after 10 min"), FMath::IsNearlyEqual(G.Wetness, 1.0f, 0.001f));
	TestEqual(TEXT("rain lays no snow"), G.SnowCover, 0.0f);

	// 2. The sun dries it in two hours (half in one); without the sun at 0.3 of that. Not the 90 s the weather plugin used.
	FMOGroundWeatherState Day = G, Dark = G;
	FMOGroundWeather::Advance(Day, Sunny, 3600.0, Rates);
	TestTrue(TEXT("half dry after an hour of sun"), FMath::IsNearlyEqual(Day.Wetness, 0.5f, 0.01f));
	FMOGroundWeather::Advance(Dark, Night, 3600.0, Rates);
	TestTrue(TEXT("a night hour dries 0.3 of that"), FMath::IsNearlyEqual(Dark.Wetness, 1.0f - 0.15f, 0.01f));
	FMOGroundWeatherState Short = G;
	FMOGroundWeather::Advance(Short, Sunny, 90.0, Rates);
	TestTrue(TEXT("CONTROL: 90 s of sun leaves it nearly soaked"), Short.Wetness > 0.95f);

	// 3. Snow at -3 C covers the ground in 45 min; in frost it stays (no melt below freezing), then melts at +5 C in 6 h and wets the ground.
	FMOGroundWeatherInputs Snow;
	Snow.Snow01 = 0.7f;
	Snow.Cloud01 = 1.0f;
	Snow.TemperatureC = -3.0f;
	FMOGroundWeatherState S;
	FMOGroundWeather::Advance(S, Snow, 45.0 * 60.0, Rates);
	TestTrue(TEXT("covered after 45 min of snow"), FMath::IsNearlyEqual(S.SnowCover, 1.0f, 0.001f));
	FMOGroundWeatherInputs Frost = Sunny;
	Frost.TemperatureC = -5.0f;
	FMOGroundWeather::Advance(S, Frost, 10.0 * 3600.0, Rates);
	TestEqual(TEXT("a frosty day melts none"), S.SnowCover, 1.0f);
	FMOGroundWeatherInputs Thaw = Sunny;
	Thaw.TemperatureC = 5.0f;
	FMOGroundWeather::Advance(S, Thaw, 3.0 * 3600.0, Rates);
	TestTrue(TEXT("half melted after 3 h at +5 C"), FMath::IsNearlyEqual(S.SnowCover, 0.5f, 0.01f));
	TestTrue(TEXT("the melt wets the ground"), S.Wetness > 0.1f);

	// 4. Snow falling above freezing melts as it lands: it wets, it does not cover.
	FMOGroundWeatherInputs Sleet = Snow;
	Sleet.TemperatureC = 3.0f;
	FMOGroundWeatherState W;
	FMOGroundWeather::Advance(W, Sleet, 10.0 * 60.0, Rates);
	TestEqual(TEXT("no cover from snow at +3 C"), W.SnowCover, 0.0f);
	TestTrue(TEXT("it wets instead"), W.Wetness > 0.9f);

	// 5. One long step equals many short ones (an offline catch-up of 4 h).
	FMOGroundWeatherState A = G, B = G;
	FMOGroundWeather::Advance(A, Sunny, 4.0 * 3600.0, Rates);
	for (int32 I = 0; I < 240; ++I)
	{
		FMOGroundWeather::Advance(B, Sunny, 60.0, Rates);
	}
	TestTrue(TEXT("4 h in one step = 240 one-minute steps"), FMath::IsNearlyEqual(A.Wetness, B.Wetness, 0.001f) && FMath::IsNearlyEqual(A.SnowCover, B.SnowCover, 0.001f));
	TestEqual(TEXT("dry after 4 h of sun"), A.Wetness, 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
