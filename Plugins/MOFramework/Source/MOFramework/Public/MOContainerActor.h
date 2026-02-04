#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"
#include "MOContainerActor.generated.h"

class UMOInventoryComponent;

/**
 * A buildable container actor that provides storage slots.
 * When complete, interaction opens an inventory UI.
 */
UCLASS()
class MOFRAMEWORK_API AMOContainerActor : public AMOBuildableActor
{
	GENERATED_BODY()

public:
	AMOContainerActor();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Inventory component for storage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Container")
	TObjectPtr<UMOInventoryComponent> ContainerInventory;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Number of storage slots. Set from recipe data on initialize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Container")
	int32 SlotCount = 8;

	// ============================================================================
	// ACCESSORS
	// ============================================================================

	/** Get the container inventory component. */
	UFUNCTION(BlueprintPure, Category="MO|Container")
	UMOInventoryComponent* GetContainerInventory() const { return ContainerInventory; }

protected:
	virtual void BeginPlay() override;

	/** Override to open container UI when complete. */
	virtual void OnCompleteInteracted_Implementation(AController* Controller) override;

	/** Initialize slot count from recipe data. */
	virtual void InitializeBuilding(FName InRecipeId) override;
};
