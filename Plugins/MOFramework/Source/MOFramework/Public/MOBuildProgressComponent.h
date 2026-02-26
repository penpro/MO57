/**
 * =============================================================================
 * MOBuildProgressComponent.h - Construction Progress Tracker
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WEIGHTED PARTS: Parts have Weight field. TimePerWeight distributes
 *   total build time. Don't assume parts take equal time.
 *
 * [2024-02] MATERIAL GATHERING: Uses MOMaterialSourceInterface. Sources checked
 *   by priority. If materials unavailable, construction pauses (not fails).
 *
 * [2024-02] STATE PERSISTENCE: FMOBuildProgress is saved. On load, construction
 *   resumes from ElapsedTime. Ensure CurrentPartIndex is correct.
 *
 * [2024-02] AUTHORITY: Construction tick only runs on server. Clients receive
 *   replicated progress values.
 *
 * =============================================================================
 * RELATED FILES: MOBuildableActor.h, MOBuildingComponent.h, MOBuildingTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOBuildingTypes.h"

#include "MOBuildProgressComponent.generated.h"

class UMOInventoryComponent;

/**
 * =============================================================================
 * UMOBuildProgressComponent - Construction Progress Tracker
 * =============================================================================
 *
 * PURPOSE:
 * Tracks and manages construction progress for buildable actors. Handles
 * timed construction with weighted parts, material consumption, and
 * state transitions.
 *
 * OWNERSHIP:
 * - Owner: AMOBuildableActor (created in constructor as default subobject)
 * - Lifespan: Exists for duration of owning actor
 *
 * -----------------------------------------------------------------------------
 * CONSTRUCTION ALGORITHM
 * -----------------------------------------------------------------------------
 *
 * INITIALIZATION:
 *   InitializeFromRecipe(Recipe) sets up:
 *   - BuildParts array from Recipe.BuildParts
 *   - TotalBuildTime from Recipe.TotalBuildTime
 *   - GatherRange from Recipe.BuildRange
 *   - ConsumedPerPart tracking array (initialized to 0)
 *   - TotalWeight calculated from sum(part.Quantity * part.Weight)
 *   - TimePerWeight = TotalBuildTime / TotalWeight
 *
 * TIME DISTRIBUTION:
 *   Each part contributes: PartTime = Quantity * Weight * TimePerWeight
 *   Example with TotalBuildTime=60s:
 *     Part 1: Stone x20, Weight=1 -> 20 weight units -> 20s
 *     Part 2: Dig x1, Weight=5 -> 5 weight units -> 5s
 *     Part 3: Wood x10, Weight=2 -> 20 weight units -> 20s
 *     Part 4: Hammer x1, Weight=15 -> 15 weight units -> 15s
 *     Total: 60 weight units -> 60s
 *
 * TICK PROCESSING:
 *   ProcessConstruction(DeltaTime):
 *   1. Advance ElapsedTime
 *   2. Calculate which part we're in based on accumulated time
 *   3. Calculate progress within current part
 *   4. Check material consumption needs
 *   5. If materials needed and unavailable, pause
 *   6. Broadcast progress update
 *   7. If ElapsedTime >= TotalBuildTime, finalize
 *
 * MATERIAL CONSUMPTION:
 *   CheckMaterialConsumption():
 *   1. Calculate RequiredConsumed = floor(CurrentPartProgress * Quantity)
 *   2. If RequiredConsumed > ConsumedPerPart[i]:
 *      a. Try to consume (RequiredConsumed - Consumed) items
 *      b. If success: increment ConsumedPerPart[i]
 *      c. If failure: broadcast OnMaterialNeeded, pause construction
 *
 * -----------------------------------------------------------------------------
 * BUILD PART TYPES
 * -----------------------------------------------------------------------------
 *
 * ITEM PARTS (FMOBuildPart with ItemDefinitionId set):
 *   - Consumes items from inventory/containers/world
 *   - Example: "Stone01" x20, Weight=1
 *   - Each unit consumed progressively during part time
 *
 * ACTION PARTS (FMOBuildPart with ActionId set):
 *   - Represents labor/work actions
 *   - No item consumption, just time
 *   - Example: "Dig" x1, Weight=5 (5 weight units of digging)
 *   - Could trigger animations/sounds in future
 *
 * -----------------------------------------------------------------------------
 * STATE TRANSITIONS
 * -----------------------------------------------------------------------------
 *
 *   Ghost -> StartConstruction() -> Constructing
 *   Constructing -> PauseConstruction() -> Paused
 *   Paused -> ResumeConstruction() -> Constructing
 *   Constructing -> (time complete) -> Complete
 *   Any (except Complete) -> CancelConstruction() -> Ghost
 *
 * -----------------------------------------------------------------------------
 * DELEGATE BROADCASTS
 * -----------------------------------------------------------------------------
 *
 * OnConstructionStarted
 *   -> When: StartConstruction() called successfully
 *   -> Listeners: Audio system, visual effects
 *
 * OnConstructionProgress(float Progress)
 *   -> When: Every tick during construction
 *   -> Progress: 0.0 to 1.0 overall completion
 *   -> Listeners: UI progress bars, visual interpolation
 *
 * OnPartCompleted(int32 PartIndex, FMOBuildPart Part)
 *   -> When: A build part finishes (all quantity consumed)
 *   -> Listeners: Audio (material placement sounds)
 *
 * OnConstructionCompleted
 *   -> When: FinalizeConstruction() called (time complete)
 *   -> Listeners: AMOBuildableActor::OnConstructionCompleted
 *
 * OnConstructionCancelled
 *   -> When: CancelConstruction() called
 *   -> Listeners: Refund logic, UI updates
 *
 * OnMaterialNeeded(FName ItemId, int32 Quantity)
 *   -> When: Construction pauses due to missing material
 *   -> Listeners: UI notification system
 *
 * -----------------------------------------------------------------------------
 * PERSISTENCE
 * -----------------------------------------------------------------------------
 *
 * BuildSaveData(FMOBuildProgress& OutData):
 *   Copies current Progress struct (includes all state)
 *
 * ApplySaveData(const FMOBuildProgress& InData):
 *   Restores Progress struct
 *   Resumes tick if state was Constructing
 *
 * =============================================================================
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOBuildProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOBuildProgressComponent();

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when construction starts. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnConstructionStartedSignature OnConstructionStarted;

	/** Broadcast when construction progress updates. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnConstructionProgressSignature OnConstructionProgress;

	/** Broadcast when a build part completes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnPartCompletedSignature OnPartCompleted;

	/** Broadcast when construction completes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnConstructionCompletedSignature OnConstructionCompleted;

	/** Broadcast when construction is cancelled. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnConstructionCancelledSignature OnConstructionCancelled;

	/** Broadcast when a material is needed for construction. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnMaterialNeededSignature OnMaterialNeeded;

	/** Broadcast when construction state changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnConstructionStateChangedSignature OnConstructionStateChanged;

	// ============================================================================
	// STATE ACCESS
	// ============================================================================

	/**
	 * Get the current build state.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	EMOBuildState GetState() const { return Progress.State; }

	/**
	 * Get overall construction progress (0.0 - 1.0).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	float GetProgress() const { return Progress.GetOverallProgress(); }

	/**
	 * Get remaining construction time in seconds.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	float GetTimeRemaining() const { return Progress.GetRemainingTime(); }

	/**
	 * Get the current build progress data.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	const FMOBuildProgress& GetProgressData() const { return Progress; }

	/**
	 * Get the recipe ID for this building.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	FName GetRecipeId() const { return RecipeId; }

	/**
	 * Get the current build part index.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	int32 GetCurrentPartIndex() const { return Progress.CurrentPartIndex; }

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize this component from a recipe definition.
	 * @param Recipe - The recipe definition
	 */
	void InitializeFromRecipe(const FMORecipeDefinitionRow& Recipe);

	// ============================================================================
	// CONSTRUCTION CONTROL
	// ============================================================================

	/**
	 * Start construction timer (requires all materials to be deposited first).
	 * @param Options - Material sourcing options
	 * @param InBuilderInventory - Inventory to draw materials from (optional)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void StartConstruction(const FMOBuildProgress& Options, UMOInventoryComponent* InBuilderInventory = nullptr);

	/**
	 * Set the builder's inventory for material gathering.
	 * @param InInventory - Inventory component to draw from
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void SetBuilderInventory(UMOInventoryComponent* InInventory);

	// ============================================================================
	// MATERIAL DEPOSIT SYSTEM
	// ============================================================================

	/**
	 * Deposit one unit of a material into the building.
	 * @param ItemId - The item to deposit
	 * @return True if material was accepted (still needed)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	bool DepositMaterial(FName ItemId);

	/**
	 * Get how many of an item have been deposited.
	 * @param ItemId - The item to check
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	int32 GetDepositedCount(FName ItemId) const;

	/**
	 * Get how many of an item are required total.
	 * @param ItemId - The item to check
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	int32 GetRequiredCount(FName ItemId) const;

	/**
	 * Check if all required materials have been deposited.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool AreAllMaterialsDeposited() const;

	/**
	 * Get all unique material requirements.
	 * @param OutItemIds - Array of required item IDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void GetRequiredMaterials(TArray<FName>& OutItemIds) const;

	/**
	 * Interrupt the build timer (e.g., player moved).
	 * Applies the interruption penalty.
	 * @param PenaltyPercent - Percentage of total build time to lose (default 10%)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void InterruptBuild(float PenaltyPercent = 0.1f);

	/**
	 * Get all deposited materials for refund/drop.
	 * @param OutMaterials - Map of ItemId to quantity deposited
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void GetDepositedMaterials(TMap<FName, int32>& OutMaterials) const;

	/**
	 * Pause construction.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void PauseConstruction();

	/**
	 * Resume paused construction.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void ResumeConstruction();

	/**
	 * Cancel construction.
	 * @param bRefundMaterials - If true, attempt to refund consumed materials
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void CancelConstruction(bool bRefundMaterials = true);

	// ============================================================================
	// MATERIAL GATHERING
	// ============================================================================

	/**
	 * Attempt to gather the next required material.
	 * @return True if material was gathered successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	bool TryGatherNextMaterial();

	/**
	 * Find actors that could provide materials within range.
	 * @param Range - Gather range in Unreal Units
	 * @return Array of potential material source actors
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	TArray<AActor*> FindMaterialSources(float Range) const;

	// ============================================================================
	// SAVE/LOAD
	// ============================================================================

	/**
	 * Build save data from current state.
	 * @param OutData - The data to populate
	 */
	void BuildSaveData(FMOBuildProgress& OutData) const;

	/**
	 * Apply saved data to restore state.
	 * @param InData - The saved data to apply
	 */
	void ApplySaveData(const FMOBuildProgress& InData);

protected:
	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ============================================================================
	// STATE
	// ============================================================================

	/** Current construction progress. */
	UPROPERTY()
	FMOBuildProgress Progress;

	/** Recipe ID for this building. */
	UPROPERTY()
	FName RecipeId = NAME_None;

	/** Build parts from the recipe. */
	UPROPERTY()
	TArray<FMOBuildPart> BuildParts;

	/** Cached total weight for time distribution. */
	int32 TotalWeight = 0;

	/** Time per weight unit. */
	float TimePerWeight = 0.0f;

	/** Reference to builder's inventory (if drawing from inventory). */
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> BuilderInventory;

	/** Materials deposited into this building (ItemId -> Quantity). */
	UPROPERTY()
	TMap<FName, int32> DepositedMaterials;

	// ============================================================================
	// INTERNAL
	// ============================================================================

	/** Calculate total weight from build parts. */
	void CalculateTotalWeight();

	/** Process construction tick. */
	void ProcessConstruction(float DeltaTime);

	/** Check if current part needs material consumption. */
	void CheckMaterialConsumption();

	/** Try to consume material for a build part. */
	bool TryConsumeMaterial(const FMOBuildPart& Part);

	/** Complete the current build part. */
	void CompleteCurrentPart();

	/** Finalize construction. */
	void FinalizeConstruction();
};
