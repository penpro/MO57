#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOPCGInteractionSubsystem.generated.h"

class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMOInventoryComponent;

/**
 * World subsystem that handles interaction with PCG-spawned HISM instances.
 *
 * Automatically maps static meshes to item definitions by doing a reverse lookup
 * in the item database. When a player interacts with an HISM instance, this subsystem:
 * 1. Gets the static mesh from the HISM
 * 2. Finds which item definition uses that mesh
 * 3. Removes the instance and adds the item to inventory
 *
 * No configuration needed on the PCG side - just ensure item definitions have
 * their StaticMesh property set correctly.
 */
UCLASS()
class MOFRAMEWORK_API UMOPCGInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============================================================================
	// MESH TO ITEM LOOKUP
	// ============================================================================

	/**
	 * Find the item definition ID for a given static mesh.
	 * @param Mesh The static mesh to look up
	 * @return The item definition ID, or NAME_None if not found
	 */
	UFUNCTION(BlueprintPure, Category="MO|PCG")
	FName GetItemIdForMesh(UStaticMesh* Mesh) const;

	/**
	 * Check if a mesh has an associated item definition.
	 */
	UFUNCTION(BlueprintPure, Category="MO|PCG")
	bool IsMeshHarvestable(UStaticMesh* Mesh) const;

	/**
	 * Rebuild the mesh-to-item lookup cache.
	 * Called automatically on Initialize, but can be called manually if item database changes.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|PCG")
	void RebuildMeshLookupCache();

	// ============================================================================
	// HISM INTERACTION
	// ============================================================================

	/**
	 * Try to harvest an HISM instance.
	 * @param HISMComponent The HISM component containing the instance
	 * @param InstanceIndex The index of the instance to harvest
	 * @param Harvester The actor doing the harvesting (pawn with inventory)
	 * @param OutItemId The item ID that was harvested (if successful)
	 * @return True if harvest was successful
	 */
	UFUNCTION(BlueprintCallable, Category="MO|PCG")
	bool HarvestHISMInstance(UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex, AActor* Harvester, FName& OutItemId);

	/**
	 * Try to harvest an ISM instance (non-hierarchical).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|PCG")
	bool HarvestISMInstance(UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex, AActor* Harvester, FName& OutItemId);

	/**
	 * Check if an HISM component's mesh is harvestable.
	 */
	UFUNCTION(BlueprintPure, Category="MO|PCG")
	bool IsHISMHarvestable(UHierarchicalInstancedStaticMeshComponent* HISMComponent) const;

	// ============================================================================
	// TAG-BASED ITEM LOOKUP
	// ============================================================================

	/**
	 * Register a tag-to-item mapping for PCG components.
	 * When a component has this tag, interacting gives the specified item.
	 * @param Tag The component tag to check for (e.g., "GivesStick")
	 * @param ItemId The item to give when harvested (e.g., "stick01")
	 */
	UFUNCTION(BlueprintCallable, Category="MO|PCG")
	void RegisterTagItemMapping(FName Tag, FName ItemId);

	/**
	 * Get the item ID for a component based on its tags.
	 * @param Component The component to check tags on
	 * @return The item ID if a tag matches, NAME_None otherwise
	 */
	UFUNCTION(BlueprintPure, Category="MO|PCG")
	FName GetItemIdForComponentTags(UActorComponent* Component) const;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Default quantity to give when harvesting (if item doesn't specify). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	int32 DefaultHarvestQuantity = 1;

	/** Interaction prompt text for harvestable meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FText DefaultInteractionPrompt = NSLOCTEXT("MO", "PickUp", "Pick Up");

	/**
	 * Tag-to-item mappings. When an ISM/HISM component has a matching tag,
	 * harvesting gives the specified item instead of doing mesh lookup.
	 * Key: Component tag (e.g., "GivesStick")
	 * Value: Item definition ID (e.g., "stick01")
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	TMap<FName, FName> TagToItemMap;

	/**
	 * Get the interaction prompt for a mesh.
	 * Returns item display name if available, otherwise default prompt.
	 */
	UFUNCTION(BlueprintPure, Category="MO|PCG")
	FText GetInteractionPromptForMesh(UStaticMesh* Mesh) const;

private:
	/** Find inventory component on harvester (handles pawns and controllers). */
	UMOInventoryComponent* FindHarvesterInventory(AActor* Harvester) const;

	/** Cache mapping static mesh to item definition ID. */
	UPROPERTY(Transient)
	TMap<TSoftObjectPtr<UStaticMesh>, FName> MeshToItemCache;
};
