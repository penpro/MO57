/**
 * =============================================================================
 * MOHISMInteractableComponent.h - HISM Instance Interaction Handler
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Component enabling interaction with PCG-spawned HISM instances. Handles
 * item harvesting from hierarchical instanced static meshes. Attach to actors
 * that have HISM components needing player interaction.
 *
 * HARVEST FLOW:
 * 1. Player interacts with HISM instance
 * 2. HandlesHISMComponent() checks if this component manages the HISM
 * 3. HarvestInstance() removes instance and adds item to inventory
 * 4. OnInstanceHarvested fires for effects (particles, sounds)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] TARGET COMPONENT: If TargetHISMComponent is set, only that HISM
 *   is handled. Otherwise ALL HISMs on the actor are handled.
 *
 * [2024-02] QUANTITY RANGE: MinQuantity/MaxQuantity control random drops.
 *   Both should be >= 1. Set equal for fixed quantity.
 *
 * [2024-02] INVENTORY REQUIRED: HarvestInstance() requires inventory component
 *   on harvester. Returns false if no inventory found.
 *
 * =============================================================================
 * RELATED FILES: MOHarvestSubsystem.h, MOHISMHarvestHelper.h, MOForagingSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOHISMInteractableComponent.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMOInventoryComponent;

/**
 * Delegate fired when an HISM instance is harvested.
 * See file header for harvest flow.
 * @param InstanceIndex The index of the harvested instance
 * @param ItemId The item definition ID that was harvested
 * @param Quantity The quantity harvested
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOHISMInstanceHarvestedSignature, int32, InstanceIndex, FName, ItemId, int32, Quantity);

/**
 * Component that makes HISM (Hierarchical Instanced Static Mesh) instances interactable.
 * Attach to actors that have PCG-spawned HISM components to allow players to harvest items.
 *
 * When a player interacts with an HISM instance:
 * 1. The instance is removed from the HISM
 * 2. The corresponding item is added to the player's inventory
 * 3. OnInstanceHarvested delegate fires for any additional effects
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOHISMInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOHISMInteractableComponent();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** The item definition ID to give when harvesting instances from this HISM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HISM")
	FName ItemDefinitionId;

	/** Minimum quantity to give per harvest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HISM", meta=(ClampMin="1"))
	int32 MinQuantity = 1;

	/** Maximum quantity to give per harvest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HISM", meta=(ClampMin="1"))
	int32 MaxQuantity = 1;

	/** Interaction prompt shown to player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HISM")
	FText InteractionPrompt = NSLOCTEXT("MO", "PickUp", "Pick Up");

	/** If set, only this specific HISM component is interactable. Otherwise all HISMs on the actor are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HISM")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TargetHISMComponent;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Fired when an instance is harvested. */
	UPROPERTY(BlueprintAssignable, Category="MO|HISM")
	FMOHISMInstanceHarvestedSignature OnInstanceHarvested;

	// ============================================================================
	// API
	// ============================================================================

	/**
	 * Check if this component handles the given HISM component.
	 * @param HISMComponent The HISM component to check
	 * @return True if this component handles interactions for the given HISM
	 */
	UFUNCTION(BlueprintPure, Category="MO|HISM")
	bool HandlesHISMComponent(UHierarchicalInstancedStaticMeshComponent* HISMComponent) const;

	/**
	 * Harvest an instance from the HISM.
	 * @param HISMComponent The HISM component containing the instance
	 * @param InstanceIndex The index of the instance to harvest
	 * @param Harvester The actor doing the harvesting (used to find inventory)
	 * @return True if harvest was successful
	 */
	UFUNCTION(BlueprintCallable, Category="MO|HISM")
	bool HarvestInstance(UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex, AActor* Harvester);

	/**
	 * Get the interaction prompt for this HISM.
	 */
	UFUNCTION(BlueprintPure, Category="MO|HISM")
	FText GetInteractionPrompt() const { return InteractionPrompt; }

	/**
	 * Get the item definition ID for this HISM.
	 */
	UFUNCTION(BlueprintPure, Category="MO|HISM")
	FName GetItemDefinitionId() const { return ItemDefinitionId; }

protected:
	/**
	 * Calculate random quantity between min and max.
	 */
	int32 CalculateQuantity() const;

	/**
	 * Find inventory component on the harvester.
	 */
	UMOInventoryComponent* FindHarvesterInventory(AActor* Harvester) const;
};
