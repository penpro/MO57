#pragma once

#include "CoreMinimal.h"
#include "MORecipeDefinitionRow.h"
#include "MOCraftingExecutionContext.generated.h"

UENUM(BlueprintType)
enum class EMOCraftingContextFailure : uint8
{
	None, InvalidRecipe, HarvestActionRequired, InvalidCrafter, MissingStation, WrongStation,
	IncompleteStation, OutOfRange, Obstructed, InactiveStation, InsufficientTemperature, InvalidDuration
};

/** Resolved from world actors at execution time. A client's station enum is never execution evidence. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCraftingExecutionContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting") EMOCraftingStation StationType = EMOCraftingStation::None;
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting") FGuid StationGuid;
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting") float AchievableTemperatureCelsius = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting") EMOCraftingContextFailure Failure = EMOCraftingContextFailure::None;
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting") FText FailureReason;
	bool IsUsable() const { return Failure == EMOCraftingContextFailure::None; }
};
