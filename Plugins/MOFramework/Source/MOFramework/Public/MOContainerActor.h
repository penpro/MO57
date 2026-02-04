#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"
#include "MOInventoryHolderInterface.h"
#include "MOMaterialSourceInterface.h"
#include "MOContainerActor.generated.h"

class UMOInventoryComponent;

/**
 * A buildable container actor that provides storage slots.
 * When complete, interaction opens an inventory UI.
 */
UCLASS()
class MOFRAMEWORK_API AMOContainerActor : public AMOBuildableActor,
	public IMOInventoryHolderInterface,
	public IMOMaterialSourceInterface
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

	// ============================================================================
	// IMOInventoryHolderInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOInventoryComponent* GetInventory_Implementation() const override;
	virtual bool HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const override;
	virtual int32 GetInventoryItemCount_Implementation(FName ItemDefinitionId) const override;

	// ============================================================================
	// IMOMaterialSourceInterface IMPLEMENTATION
	// ============================================================================

	virtual bool CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const override;
	virtual int32 GatherMaterial_Implementation(FName MaterialId, int32 Quantity) override;
	virtual int32 GetMaterialSourcePriority_Implementation() const override;

protected:
	virtual void BeginPlay() override;

	/** Override to open container UI when complete. */
	virtual void OnCompleteInteracted_Implementation(AController* Controller) override;

	/** Initialize slot count from recipe data. */
	virtual void InitializeBuilding(FName InRecipeId) override;
};
