#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MORecipeDefinitionRow.h"
#include "MOHarvestSubsystem.generated.h"

class UMOKnowledgeComponent;
class UMOSkillsComponent;
class UMOInventoryComponent;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
enum class EMOLearningPotential : uint8;
struct FMOCraftResult;

/**
 * Context for an in-progress harvest operation.
 * Stores the target ISM/HISM reference and operation state.
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

	/** The harvest recipe being executed. */
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
class MOFRAMEWORK_API UMOHarvestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

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
	 * Begin a harvest operation.
	 * @param ISMComponent The ISM component to harvest from
	 * @param InstanceIndex The instance index
	 * @param RecipeId The harvest recipe to execute
	 * @param Inventory Player's inventory (for time calculation)
	 * @return True if harvest was started successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	bool BeginHarvest(
		UInstancedStaticMeshComponent* ISMComponent,
		int32 InstanceIndex,
		FName RecipeId,
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
	bool IsHarvestInProgress() const { return CurrentContext.IsValid() && !CurrentContext.ActiveRecipeId.IsNone(); }

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

	/** Build the harvest recipe cache from the recipe DataTable. */
	void BuildHarvestRecipeCache();
};
