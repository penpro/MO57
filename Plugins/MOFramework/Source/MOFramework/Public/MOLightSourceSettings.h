// What each carried light gives (Project Settings > MO > Light Sources): an item that burns when held lit -- a torch, a fat lamp. Keyed by item id, so a new light
// source is one profile here plus its item row; UMOCarriedLightComponent reads it. Research and calibration: Docs/Light_Sources.md.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "MOLightSourceSettings.generated.h"

/** One kind of carried flame. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOLightSourceProfile
{
	GENERATED_BODY()

	/** Luminous intensity of the flame when it burns freely (cd). A juniper torch ~14, a fat lamp ~0.6, a candle ~1 (Medina-Alcaide et al. 2021). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0.01"))
	float LuminousIntensityCd = 10.0f;

	/** How far the light is computed (cm). Past it the flame gives well under 0.1 lux: a render budget, not a physical edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="100"))
	float AttenuationRadiusCm = 1500.0f;

	/** Flame colour (K): wood and fat flames burn ~1800-2000 K. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="1000", ClampMax="6500"))
	float ColorTemperatureK = 1900.0f;

	/** Size of the flame (cm): softens the shadows it casts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0"))
	float SourceRadiusCm = 4.0f;

	/** Whether the flame casts shadows (a torch does: the hand and the body throw them). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light")
	bool bCastShadows = true;

	/** Flicker: how far the intensity wanders around its mean (fraction), and how fast (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0", ClampMax="0.9"))
	float FlickerAmount = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0.1"))
	float FlickerHz = 6.0f;

	/**
	 * Intensity held still, as a fraction of a free burn. A torch starves of air when it is not moved: swinging one took the illuminance at 20 cm from 5 to
	 * 28 lux in the experiments. A lamp flame does not care (1).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0.05", ClampMax="1"))
	float StillIntensityScale = 0.6f;

	/** Speed at which the flame gets all the air it can use (cm/s): about a walk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="1"))
	float FullAirflowSpeedCmPerSec = 120.0f;

	/** Holding it in a burning fire until it catches (game s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0.5"))
	float LightSeconds = 6.0f;

	/** Game seconds of burning per point of the item's durability: the item row's MaxDurability x this = how long one lasts (a torch 90 x 30 s = 45 min). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="1"))
	float BurnSecondsPerDurability = 30.0f;

	/** Where the flame sits along the held mesh's long axis, from the grip (0) to the far end (1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0", ClampMax="1"))
	float FlamePositionAlongMesh = 0.92f;

	/** The visible flame (a Niagara system), placed where the light is; empty = light only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(AllowedClasses="/Script/Niagara.NiagaraSystem"))
	FSoftObjectPath FlameEffect;

	/** Uniform scale of the flame effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Light", meta=(ClampMin="0.01"))
	float FlameEffectScale = 1.0f;

	/** Rain at the flame (0-1: the weather's exposure there, so a roof, an overhang or a cave keeps it off) up to which it burns on: a resin torch
	 *  survives light rain. UDW's "Rain" reads 0.7 in the open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0", ClampMax="1"))
	float RainSafeIntensity = 0.4f;

	/** Mean game seconds the flame lasts in the heaviest rain (1); between RainSafeIntensity and 1 the chance scales with how far above it the
	 *  rain is (UDW "Rain", 0.7: ~4 min). An estimate -- no measurement found: a pitch torch gutters out in a downpour within minutes. 0 = rain
	 *  never puts it out (a covered lantern). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0"))
	float MeanSecondsInHeaviestRain = 120.0f;
};

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Light Sources"))
class MOFRAMEWORK_API UMOLightSourceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMOLightSourceSettings();

	/** Every item that burns as a carried light, by item id. */
	UPROPERTY(Config, EditAnywhere, Category="Sources")
	TMap<FName, FMOLightSourceProfile> Profiles;

	/** The profile for an item, or null when it is not a light source. */
	static const FMOLightSourceProfile* Find(FName ItemId);

	/**
	 * How much of a free burn the flame gives moving at SpeedCmPerSec: StillIntensityScale at rest, rising linearly to 1 at FullAirflowSpeedCmPerSec. Pure, for
	 * the tests.
	 */
	static float AirflowScale(const FMOLightSourceProfile& Profile, float SpeedCmPerSec);

	virtual FName GetCategoryName() const override { return FName(TEXT("MO")); }
};
