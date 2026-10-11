// The weather director's chain (UMOWeatherDirectorSubsystem::ChooseNextPreset / ChooseSpellHours): pure functions, no world.

#include "Misc/AutomationTest.h"
#include "MOWeatherDirector.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
