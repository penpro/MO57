// What drinking from each kind of natural water does (Project Settings > Water Sources). Research and calibration: Docs/Water_And_Illness.md.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOWorldGen.h"
#include "MOBodyPartTypes.h"
#include "MOWaterSourceSettings.generated.h"

/** One kind of natural water, untreated. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWaterSourceProfile
{
	GENERATED_BODY()

	/** "River water": what the player is told they drink. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
	FText DisplayName;

	/** Dissolved salts (g/L): the sea ~35, fresh water ~0. Drives the net body water and the sodium of a draught. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0", ClampMax="60"))
	float SalinityGPerL = 0.0f;

	/** Chance per litre drunk untreated of a symptomatic waterborne illness (0-1). Boiled water has none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0", ClampMax="1"))
	float IllnessChancePerLitre = 0.0f;
};

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Water Sources"))
class MOFRAMEWORK_API UMOWaterSourceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMOWaterSourceSettings();

	/** Every kind of open water the generator makes (EMOWaterBodyKind). */
	UPROPERTY(Config, EditAnywhere, Category="Sources")
	TMap<EMOWaterBodyKind, FMOWaterSourceProfile> Profiles;

	/** What drinking straight from a source takes in one go (ml): kneeling at the edge and scooping with the hands. */
	UPROPERTY(Config, EditAnywhere, Category="Drinking", meta=(ClampMin="10"))
	float DraughtMillilitres = 250.0f;

	/** How long that takes (game seconds): ~30-50 ml a handful, a few seconds a handful. */
	UPROPERTY(Config, EditAnywhere, Category="Drinking", meta=(ClampMin="1"))
	float DraughtSeconds = 20.0f;

	/** How far from the pawn the water's edge may be to drink from it (cm): arm's reach from kneeling, or standing in it. */
	UPROPERTY(Config, EditAnywhere, Category="Drinking", meta=(ClampMin="50"))
	float ReachCm = 250.0f;

	/** The condition untreated water can give (DT_Conditions row: its course and effects). */
	UPROPERTY(Config, EditAnywhere, Category="Drinking")
	EMOConditionType IllnessCondition = EMOConditionType::WaterborneDisease;

	/** The profile for a kind, or an all-zero "water" if it is not configured. */
	static FMOWaterSourceProfile GetProfile(EMOWaterBodyKind Kind);

	virtual FName GetCategoryName() const override { return FName(TEXT("MO")); }
};
