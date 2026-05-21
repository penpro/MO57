/**
 * =============================================================================
 * MOHarvestSubsystem.h - Resource Harvesting System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * World subsystem managing harvesting of PCG-spawned resources (trees, rocks,
 * plants via ISM/HISM components). Handles timed harvest operations, tool
 * requirements, skill checks, and output spawning.
 *
 * KEY RESPONSIBILITIES:
 * 1. Validate harvest operations (tools, skills, tags)
 * 2. Track harvest progress with FMOHarvestContext
 * 3. Execute harvests (remove ISM instance, spawn outputs)
 * 4. Grant XP and discovery unlocks
 *
 * HARVEST WORKFLOW:
 * 1. Player interacts with ISM/HISM instance
 * 2. MOKeepOnHarvestContextMenu shows available recipes (filtered by tags)
 * 3. Player selects recipe -> StartHarvest()
 * 4. Progress ticks -> OnHarvestProgress broadcasts
 * 5. Complete -> OnHarvestComplete, instance removed, items spawned
 *
 * TAG SYSTEM:
 * Harvest recipes match via SourceTags (e.g., "Wood", "Tree", "Hardwood").
 * ISM components are tagged by MOPCGResourceSpawnerSettings.
 * Recipe shows only if ALL its SourceTags exist on the target.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ISM VS HISM: Both are supported. Use ISMComponent or HISMComponent
 *   WeakObjectPtr - only one will be valid per context.
 *
 * [2024-02] INSTANCE INDEX STABILITY: ISM instance indices can change when
 *   other instances are removed. Store transform at harvest start for
 *   validation. If instance moves, harvest fails.
 *
 * [2024-02] TAG INHERITANCE: Tags come from both the ISM component AND its
 *   owner actor. Check TargetTags array in FMOHarvestContext.
 *
 * [2024-02] TOOL DEGRADATION: Tool durability is consumed during harvest.
 *   If tool breaks mid-harvest, operation continues (tool required only
 *   to START harvest).
 *
 * [2024-02] KEEP-ON-HARVEST: Some recipes have bKeepOnHarvest=true (e.g.,
 *   gather sticks doesn't destroy tree). Instance is only removed if
 *   recipe's bDestroysResource=true.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOPCGResourceSpawnerSettings.h - PCG node that spawns tagged resources
 * - MOKeepOnHarvestContextMenu.h - UI for selecting harvest recipe
 * - MORecipeDefinitionRow.h - Recipe definitions with SourceTags
 * - MOHISMHarvestHelper.h - Helper for HISM operations
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MORecipeDefinitionRow.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOInterruptibleInterface.h"
#include "MOHarvestSubsystem.generated.h"

class AMOCharacter;
class UMOKnowledgeComponent;
class UMOSkillsComponent;
class UMOInventoryComponent;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UDataTable;
enum class EMOLearningPotential : uint8;
struct FMOCraftResult;

/**
 * Context for an in-progress harvest operation.
 * See file header for harvest workflow details.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOHarvestContext
{
	GENERATED_BODY()

	/** The ISM component being harvested (if not HISM). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	TWeakObjectPtr<UInstancedStaticMeshComponent> ISMComponent;

	/** The HISM component being harvested (if HISM). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> HISMComponent;

	/** The instance index within the component. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	int32 InstanceIndex = INDEX_NONE;

	/** Transform of the instance at harvest start. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	FTransform InstanceTransform;

	/** All tags collected from the component and owner actor. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	TArray<FName> TargetTags;

	/** The harvest action ID being executed (from resource definition). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	FName ActiveActionId = NAME_None;

	/** The resource node ID (for looking up the action definition). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	FName ResourceNodeId = NAME_None;

	/** Legacy: Recipe ID (for backward compatibility, will be removed). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	FName ActiveRecipeId = NAME_None;

	/** Elapsed time into the harvest operation. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	float ElapsedTime = 0.0f;

	/** Total time required for this harvest. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Harvest")
	float TotalTime = 0.0f;

	/** Check if this context references a valid target. */
	bool IsValid() const
	{
		return (ISMComponent.IsValid() || HISMComponent.IsValid()) && InstanceIndex != INDEX_NONE;
	}

	/** Clear the context. */
	void Reset()
	{
		ISMComponent.Reset();
		HISMComponent.Reset();
		InstanceIndex = INDEX_NONE;
		InstanceTransform = FTransform::Identity;
		TargetTags.Empty();
		ActiveActionId = NAME_None;
		ResourceNodeId = NAME_None;
		ActiveRecipeId = NAME_None;
		ElapsedTime = 0.0f;
		TotalTime = 0.0f;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnHarvestComplete, FName, RecipeId, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnHarvestProgress, float, Progress, float, TimeRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnHarvestCancelled);

/**
 * World subsystem that manages harvest operations on ISM/HISM instances.
 *
 * Provides:
 * - Smart inspect logic (picks first item with learning potential)
 * - Harvest recipe filtering based on target object tags
 * - Harvest execution with progress tracking
 */
UCLASS()
class MOFRAMEWORK_API UMOHarvestSubsystem : public UWorldSubsystem,
	public IMOInterruptibleInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Interrupt policy for an active harvest:
	 *   Movement / Damage / Knockdown / Unconscious / Death / EnteredCombat
	 *     / LostControl / External  -> CANCEL (no progress preserved)
	 *   UserCancel  -> ignored (UI calls CancelHarvest directly)
	 *
	 * Gathering is a stay-put activity — no partial progress is meaningful.
	 */
	virtual void NotifyInterrupt_Implementation(const FMOInterruptContext& Context) override;

	// --- Delegates ---

	/** Broadcast when a harvest operation completes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|Events")
	FMOOnHarvestComplete OnHarvestComplete;

	/** Broadcast periodically during harvest with progress info. */
	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|Events")
	FMOOnHarvestProgress OnHarvestProgress;

	/** Broadcast when harvest is cancelled. */
	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|Events")
	FMOOnHarvestCancelled OnHarvestCancelled;

	// ============================================================================
	// RESOURCE DEFINITION LOOKUP
	// ============================================================================

	/**
	 * Extract the ResourceNodeId from component tags.
	 * Looks for tag in format "ResourceNode_{RowName}".
	 * @param Tags Array of tags to search
	 * @return Row name of the resource definition, or NAME_None if not found
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	FName ExtractResourceNodeId(const TArray<FName>& Tags) const;

	/**
	 * Get the resource definition row from the DataTable.
	 * @param ResourceNodeId Row name in the resource DataTable
	 * @return Pointer to the resource definition, or nullptr if not found
	 */
	const FMOResourceNodeDefinitionRow* GetResourceDefinition(FName ResourceNodeId) const;

	/**
	 * Get the resource definition for an ISM component based on its tags.
	 * @param ISMComponent The component to look up
	 * @return Pointer to the resource definition, or nullptr if not found
	 */
	const FMOResourceNodeDefinitionRow* GetResourceDefinitionForComponent(UInstancedStaticMeshComponent* ISMComponent) const;

	/**
	 * Get the cached resource DataTable.
	 * @return The resource definitions DataTable, or nullptr if not configured
	 */
	UDataTable* GetResourceDataTable() const { return CachedResourceDataTable.Get(); }

	// ============================================================================
	// TAG COLLECTION
	// ============================================================================

	/**
	 * Collect all tags from an ISM component and its owner actor.
	 * @param ISMComponent The component to collect tags from
	 * @return Array of all applicable tags
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	TArray<FName> CollectTargetTags(UInstancedStaticMeshComponent* ISMComponent) const;

	/**
	 * Get the item ID for a "GivesX" tag.
	 * @param Tag The tag to look up (e.g., "GivesStick")
	 * @return The item ID, or NAME_None if not found
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	FName GetItemIdForGivesTag(FName Tag) const;

	/**
	 * Parse "GivesX" tags from a tag array and return the corresponding item IDs.
	 * @param Tags Array of tags to parse
	 * @return Array of item IDs extracted from GivesX tags
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	TArray<FName> GetItemIdsFromGivesTags(const TArray<FName>& Tags) const;

	// ============================================================================
	// SMART INSPECT
	// ============================================================================

	/**
	 * Get the best item to inspect from a set of target tags.
	 * Picks the first item that still has learning potential (knowledge not maxed).
	 * If all items are maxed, returns the first item anyway.
	 *
	 * @param TargetTags Tags from the target object (parsed for GivesX)
	 * @param Knowledge Player's knowledge component
	 * @param Skills Player's skills component
	 * @return The item definition ID to inspect, or NAME_None if none found
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	FName GetSmartInspectItemId(
		const TArray<FName>& TargetTags,
		UMOKnowledgeComponent* Knowledge,
		UMOSkillsComponent* Skills
	) const;

	// ============================================================================
	// HARVEST RECIPE LOOKUP
	// ============================================================================

	/**
	 * Get all harvest recipes valid for the given target tags.
	 * Filters by RequiredTargetTag matching and validates skill/knowledge requirements.
	 *
	 * @param TargetTags Tags from the target object
	 * @param Knowledge Player's knowledge component
	 * @param Skills Player's skills component
	 * @param Inventory Player's inventory (for tool check)
	 * @param OutRecipeIds Array to fill with valid recipe IDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	void GetHarvestRecipesForTags(
		const TArray<FName>& TargetTags,
		UMOKnowledgeComponent* Knowledge,
		UMOSkillsComponent* Skills,
		UMOInventoryComponent* Inventory,
		TArray<FName>& OutRecipeIds
	) const;

	/**
	 * Get the "destroy" recipe for a target (bDestroysTarget = true).
	 * Typically "Chop Down" or similar.
	 * Returns recipe only if all requirements (including tools) are met.
	 *
	 * @param TargetTags Tags from the target object
	 * @param Knowledge Player's knowledge component
	 * @param Skills Player's skills component
	 * @param Inventory Player's inventory
	 * @return Recipe ID for the destroy operation, or NAME_None if none available
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	FName GetDestroyRecipeForTags(
		const TArray<FName>& TargetTags,
		UMOKnowledgeComponent* Knowledge,
		UMOSkillsComponent* Skills,
		UMOInventoryComponent* Inventory
	) const;

	/**
	 * Find the potential "destroy" recipe for a target (bDestroysTarget = true).
	 * Does NOT check tool requirements - just finds matching recipe by tag.
	 * Use CanExecuteDestroyRecipe to check if it can actually be used.
	 *
	 * @param TargetTags Tags from the target object
	 * @return Recipe ID for the destroy operation, or NAME_None if none found
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	FName FindDestroyRecipeForTags(const TArray<FName>& TargetTags) const;

	/**
	 * Check if a destroy recipe can be executed (has required tools).
	 *
	 * @param RecipeId The destroy recipe ID
	 * @param Inventory Player's inventory (for tool check)
	 * @return True if the recipe can be executed
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	bool CanExecuteDestroyRecipe(FName RecipeId, UMOInventoryComponent* Inventory) const;

	// ============================================================================
	// HARVEST EXECUTION
	// ============================================================================

	/**
	 * Begin a harvest operation using an ActionId from resource definition.
	 * @param ISMComponent The ISM component to harvest from
	 * @param InstanceIndex The instance index
	 * @param ActionId The harvest action ID (from FMOResourceNodeDefinitionRow::HarvestActions)
	 * @param Inventory Player's inventory (for time calculation and tool penalty check)
	 * @return True if harvest was started successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	bool BeginHarvest(
		UInstancedStaticMeshComponent* ISMComponent,
		int32 InstanceIndex,
		FName ActionId,
		UMOInventoryComponent* Inventory
	);

	/**
	 * Update the current harvest operation.
	 * Call this every frame during harvest.
	 * @param DeltaTime Time since last update
	 * @return True if harvest is still in progress, false if complete or invalid
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	bool UpdateHarvest(float DeltaTime);

	/**
	 * Cancel the current harvest operation.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	void CancelHarvest();

	/**
	 * Complete the current harvest operation.
	 * Removes instance if bDestroysTarget, adds items to inventory, grants XP.
	 * @param Inventory Player's inventory
	 * @param Skills Player's skills
	 * @return Craft result with outputs and XP
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	FMOCraftResult CompleteHarvest(
		UMOInventoryComponent* Inventory,
		UMOSkillsComponent* Skills
	);

	/**
	 * Check if a harvest operation is currently in progress.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	bool IsHarvestInProgress() const { return CurrentContext.IsValid() && !CurrentContext.ActiveActionId.IsNone(); }

	/**
	 * Get the current harvest context (read-only).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	const FMOHarvestContext& GetCurrentContext() const { return CurrentContext; }

	/**
	 * Get the progress of the current harvest (0-1).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	float GetHarvestProgress() const;

private:
	/** Current harvest operation context. */
	FMOHarvestContext CurrentContext;

	/** Cache of all harvest recipes (built on initialize). */
	TArray<const FMORecipeDefinitionRow*> HarvestRecipeCache;

	/** Cached reference to the resource definitions DataTable. */
	TWeakObjectPtr<UDataTable> CachedResourceDataTable;

	/**
	 * Character we registered with for interrupt notifications. Set in
	 * BeginHarvest, cleared in CompleteHarvest/CancelHarvest. Kept separately
	 * from CurrentContext so we can unregister even if the harvest succeeds
	 * (which Resets the context).
	 */
	TWeakObjectPtr<AMOCharacter> RegisteredHarvesterCharacter;

	/** Subscribe to the harvester's interrupt broadcast. */
	void RegisterWithHarvesterForInterrupts(UMOInventoryComponent* HarvesterInventory);

	/** Drop the harvester interrupt registration (if any). */
	void UnregisterFromHarvesterInterrupts();

	/** Build the harvest recipe cache from the recipe DataTable. */
	void BuildHarvestRecipeCache();

	/** Cache the resource DataTable reference. */
	void CacheResourceDataTable();
};
