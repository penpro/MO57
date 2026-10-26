// Crops (Docs/Farming_And_Husbandry.md §2; Wes, Oct 26: "let's get farming ... going"): one DT_Crops row per crop, and the pure rules
// for where a sown plot is in its growth. A plot (AMOCropPlotActor) keeps only what was sown and when, on the game clock; its stage is
// worked out from the time that has passed, so a field grows while nobody is near and after a load, and every machine agrees.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOCropTypes.generated.h"

class UStaticMesh;

/** Where a plot is in its growth. Fallow = nothing sown. */
UENUM(BlueprintType)
enum class EMOCropStage : uint8
{
	Fallow,
	Sown,        // in the ground, nothing showing
	Sprouted,    // the first green
	Growing,     // leaves and stems, up to heading / flowering
	Ripe,        // ready to harvest
	Overripe     // past it: grain shatters onto the ground, pods split; less to take each day
};

/** One crop. Durations are real days on the game clock (scaled by the world's Growing pace, FMOWorldRules). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCropDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop")
	FText DisplayName;

	/** The seed sown (an item; kept back from a harvest or gathered from a wild stand). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop")
	FName SeedItemId;

	/** Seed a plot takes (items): ~150 kg/ha for barley is ~250 g on a 4 x 4 m plot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop", meta=(ClampMin="1"))
	int32 SeedPerPlot = 1;

	/** Days from sowing to the first green. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Growth", meta=(ClampMin="0"))
	float DaysToSprout = 7.0f;

	/** Days from sowing to ripe (barley ~90-100, flax ~100, pea ~70). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Growth", meta=(ClampMin="0"))
	float DaysToRipe = 95.0f;

	/** Days a ripe crop stands before it starts to shatter (overripe: less to take each day after). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Growth", meta=(ClampMin="0"))
	float DaysRipe = 14.0f;

	/** Days after which an overripe crop has nothing left to take. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Growth", meta=(ClampMin="0"))
	float DaysOverripeToLost = 30.0f;

	/** What a harvest gives (sheaves / pods / pulled stems), and how many from a ripe plot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Harvest")
	FName HarvestItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Harvest", meta=(ClampMin="0"))
	int32 HarvestQuantity = 1;

	/** Seed back from a ripe plot (a harvest feeds people AND sows the next field). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Harvest", meta=(ClampMin="0"))
	int32 SeedReturn = 0;

	/** The tool that harvests it ("Sickle"; "None" = by hand, e.g. pulling flax). A knife does a sickle's work slower (tool effectiveness). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Harvest")
	FName HarvestToolType = TEXT("Sickle");

	/** Sowing and harvesting a plot (seconds at pace 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Work", meta=(ClampMin="0"))
	float SowSeconds = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Work", meta=(ClampMin="0"))
	float HarvestSeconds = 900.0f;

	/** The plant at each stage, from Sprouted (index 0) to Ripe (index 2); scaled up through Growing. Missing = the last one given. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Look")
	TArray<TSoftObjectPtr<UStaticMesh>> StageMeshes;

	/** Plants drawn on a plot (a look, not a count of anything). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Look", meta=(ClampMin="1"))
	int32 PlantsPerPlot = 36;

	/** A full-grown plant's height (cm): the stage meshes are scaled to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crop|Look", meta=(ClampMin="1"))
	float RipeHeightCm = 80.0f;
};

/** The pure rules (MOFramework.Crop.*). */
struct MOFRAMEWORK_API FMOCropGrowth
{
	/** The stage after DaysGrown (game days since sowing, already scaled by the world's pace). Negative = not sown. */
	static EMOCropStage StageAt(const FMOCropDefinitionRow& Crop, double DaysGrown);

	/** 0 at sowing -> 1 at ripe (the plants' size). */
	static float GrowthFraction(const FMOCropDefinitionRow& Crop, double DaysGrown);

	/** What is left to take: 1 while ripe, falling to 0 over the overripe days (grain on the ground). 0 before ripe. */
	static float HarvestableFraction(const FMOCropDefinitionRow& Crop, double DaysGrown);

	/** Seconds in a game day. */
	static constexpr double SecondsPerDay = 86400.0;
};
