// What digging a patch of ground turns up: one roll per entry. Used by the foraging subsystem's default table (UMOForagingSubsystem::DigDropTable) and by a
// biome's own (FMOBiomeDefinitionRow::DigYields: a beach turns up sand, not roots). Lives in Core so the biome row can carry it.

#pragma once

#include "CoreMinimal.h"
#include "MODigDropEntry.generated.h"

/**
 * Entry in a dig drop table - defines what items can be found when digging.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMODigDropEntry
{
	GENERATED_BODY()

	/** Item definition ID to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging")
	FName ItemId;

	/** Minimum quantity to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging")
	int32 MinQuantity = 1;

	/** Maximum quantity to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging")
	int32 MaxQuantity = 1;

	/** Chance to find this item (0.0 - 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropChance = 0.1f;

	/** Minimum foraging level required to find this item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging", meta=(ClampMin="0"))
	int32 MinForagingLevel = 0;
};
