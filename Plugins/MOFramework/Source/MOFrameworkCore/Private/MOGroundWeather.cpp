// The ground's answer to the weather: surface wetness and snow cover, advanced on game time (BR18-16). See FMOGroundWeather in MOWeatherTypes.h.

#include "MOWeatherTypes.h"

void FMOGroundWeather::Advance(FMOGroundWeatherState& State, const FMOGroundWeatherInputs& In, double GameSeconds, const FMOGroundWeatherRates& Rates)
{
	const float Reference = FMath::Max(Rates.ReferenceIntensity, 0.05f);
	const bool bSnowSticks = In.TemperatureC < Rates.SnowSticksBelowC;
	// Snow that melts as it lands is water on the ground like rain.
	const float Rain = FMath::Clamp(In.Rain01, 0.0f, 1.0f) + (bSnowSticks ? 0.0f : FMath::Clamp(In.Snow01, 0.0f, 1.0f));
	const float Snowfall = bSnowSticks ? FMath::Clamp(In.Snow01, 0.0f, 1.0f) : 0.0f;

	// Degree-hours: 1 = the +5 C melt rate (a full cover in MeltHoursAt5C), twice that at +10 C or under rain, nil at or below freezing.
	const float MeltFactor = In.TemperatureC > 0.0f ? (In.TemperatureC / 5.0f) * (Rain > 0.02f ? 2.0f : 1.0f) : 0.0f;
	const float MeltPerSecond = MeltFactor / FMath::Max(Rates.MeltHoursAt5C * 3600.0f, 1.0f);
	const float CoverPerSecond = Snowfall / Reference / FMath::Max(Rates.SnowCoverMinutes * 60.0f, 1.0f);
	const float SoakPerSecond = 1.0f / Reference / FMath::Max(Rates.SoakMinutes * 60.0f, 1.0f);   // per unit of water intensity
	const float Sun = In.bSunUp ? FMath::Lerp(1.0f, Rates.NoSunDryFactor, FMath::Clamp(In.Cloud01, 0.0f, 1.0f)) : Rates.NoSunDryFactor;
	const float DryPerSecond = Sun * (In.TemperatureC <= 0.0f ? Rates.FrozenDryFactor : 1.0f) / FMath::Max(Rates.DryHoursInSun * 3600.0f, 1.0f);

	// One-minute steps with the weather held, so a cover that melts away mid-step stops feeding the ground where it would in a continuous run.
	double Left = FMath::Max(GameSeconds, 0.0);
	while (Left > 0.0)
	{
		const float Dt = static_cast<float>(FMath::Min(Left, 60.0));
		Left -= Dt;

		float Cover = FMath::Clamp(State.SnowCover + CoverPerSecond * Dt, 0.0f, 1.0f);
		const bool bMelting = Cover > 0.0f && MeltPerSecond > 0.0f;
		Cover = FMath::Max(Cover - MeltPerSecond * Dt, 0.0f);

		// The water reaching the ground: the rain, and a melting cover's (like a light rain while it lasts). With none, the ground dries.
		const float Water = Rain + (bMelting ? Rates.MeltWaterAsRain * MeltFactor : 0.0f);
		const float Wet = State.Wetness + (Water > 0.02f ? Water * SoakPerSecond * Dt : -DryPerSecond * Dt);
		State.SnowCover = Cover;
		State.Wetness = FMath::Clamp(Wet, 0.0f, 1.0f);

		// Nothing left to change: skip the rest of a long catch-up.
		if (Water <= 0.02f && Snowfall <= 0.0f && State.Wetness <= 0.0f && (State.SnowCover <= 0.0f || MeltPerSecond <= 0.0f))
		{
			break;
		}
	}
}
