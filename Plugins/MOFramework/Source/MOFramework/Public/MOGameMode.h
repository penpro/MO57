#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MOGameMode.generated.h"

/**
 * Entry for mapping PCG component tags to item IDs.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTagItemMapping
{
	GENERATED_BODY()

	/** The component/actor tag to match (e.g., "GivesStick"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FName Tag;

	/** The item definition ID to give when harvested (e.g., "Stick01"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FName ItemId;
};

/**
 * Base game mode for MO Framework.
 * Handles initialization of PCG tag-to-item mappings and other framework setup.
 */
UCLASS()
class MOFRAMEWORK_API AMOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMOGameMode();

protected:
	virtual void BeginPlay() override;

	// ============================================================================
	// PCG TAG MAPPINGS
	// ============================================================================

	/**
	 * Tag-to-item mappings for PCG-spawned objects.
	 * When an ISM/HISM component has a matching tag, harvesting gives the specified item.
	 * Example: Tag="GivesStick", ItemId="Stick01"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	TArray<FMOTagItemMapping> PCGTagItemMappings;

private:
	/** Register all configured tag mappings with the PCG interaction subsystem. */
	void RegisterPCGTagMappings();
};
