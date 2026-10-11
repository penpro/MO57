/**
 * =============================================================================
 * MOWeatherDirector.h - Weather that changes by itself: fronts that come and go
 * =============================================================================
 *
 * PURPOSE:
 * Until now the weather was whatever the game started with (clear skies) or a developer set. A world that stays under the same sky for a whole session is a still photograph. The
 * director makes the weather a slow simulation: it holds one weather for a spell of GAME time, then moves to the next according to a Markov chain over the UDS presets (clear ->
 * partly cloudy -> cloudy -> overcast -> rain -> ... with storms rare and fog mostly after clear spells), with the temperature deciding whether a wet front falls as rain or snow.
 * It runs on the server only (weather is server-authoritative; clients follow the replicated sky) and works through UMOWeatherIntegrationSubsystem like every other weather change.
 *
 * RULES:
 * - Spells are measured in GAME time (UMOGameClockSubsystem), so they stretch and shrink with the time scale and survive a time skip (`time` popup): a skip past the end of a
 *   spell simply moves the weather on once.
 * - A weather set by anyone else (the `weather` popup, a cheat, a loaded save) is respected: the director notices that the sky is no longer what it applied, adopts it, and leaves
 *   it alone for ManualHoldHours before the chain continues.
 * - MO.Weather.Director 0 switches it off; MO.Weather.Director.Next moves on immediately (dev).
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * - The default clock runs in real time (TimeScale 1): a "4 hour" spell is four real hours. Raise the time scale and the weather speeds up with it.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWeatherDirector.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOWeatherDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * The chain: the preset that follows Current (a UDS preset name such as "Rain"), at this ambient temperature (Celsius), drawing from Rng. Wet fronts fall as snow below
	 * ~0.5 C and as rain above ~3 C (in between the front keeps the family of the current weather). Pure: no world needed, so tests drive it directly.
	 */
	static FName ChooseNextPreset(FName Current, float TemperatureCelsius, FRandomStream& Rng);

	/** How long (game hours) a spell of this preset lasts, drawn from Rng. */
	static float ChooseSpellHours(FName Preset, FRandomStream& Rng);

	/** True for the presets that bring precipitation. */
	static bool IsWet(FName Preset);

	/** Move to the next weather now (dev tool; the schedule restarts from it). */
	void AdvanceNow();

	void SetDirectorEnabled(bool bEnable);
	bool IsDirectorEnabled() const { return bEnabled; }

	/** The preset the director is holding, and when (game time) it moves on. */
	FName GetCurrentPreset() const { return CurrentPreset; }
	FDateTime GetNextChange() const { return NextChange; }

	/** After a manual weather change the director holds that weather this long (game hours) before the chain continues. */
	UPROPERTY(EditAnywhere, Category = "MO|Weather|Director", meta = (ClampMin = "0"))
	float ManualHoldHours = 6.0f;

private:
	void Evaluate();
	void ApplyNext(const FDateTime& Now);

	bool bEnabled = true;
	bool bScheduled = false;
	FName CurrentPreset;
	FString LastAppliedAssetName;
	FDateTime NextChange;
	int32 Changes = 0;
	float TimeSinceEvaluate = 100.0f;
};
