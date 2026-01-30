#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOCraftingTypes.h"
#include "MOCraftingQueueComponent.generated.h"

class UMOCraftingSubsystem;
class UMOInventoryComponent;
class UMORecipeDiscoveryComponent;

/**
 * Component that manages a per-pawn crafting queue with timed progression.
 *
 * Supports long craft times (hours/days) with:
 * - Real timestamp tracking for accurate progress across save/load
 * - Offline progress calculation when loading a save
 * - Background crafting (continues when UI is closed)
 * - Queue management with cancel/refund support
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCraftingQueueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOCraftingQueueComponent();

	// --- Delegates ---

	/** Broadcast when craft progress updates (called periodically for active craft). */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|Queue")
	FMOOnCraftProgressSignature OnCraftProgress;

	/** Broadcast when a craft completes successfully. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|Queue")
	FMOOnCraftCompletedSignature OnCraftCompleted;

	/** Broadcast when a craft is cancelled. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|Queue")
	FMOOnCraftCancelledSignature OnCraftCancelled;

	/** Broadcast when the queue changes (add, remove, reorder). */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|Queue")
	FMOOnCraftQueueChangedSignature OnQueueChanged;

	// --- Queue Management ---

	/**
	 * Add a craft to the queue. Ingredients are consumed immediately.
	 * @param RecipeId Recipe to craft
	 * @param Count Number of times to repeat (default 1)
	 * @param Station Station type being used (for validation)
	 * @return True if successfully enqueued
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool EnqueueCraft(FName RecipeId, int32 Count = 1, EMOCraftingStation Station = EMOCraftingStation::None);

	/**
	 * Cancel a specific craft in the queue.
	 * @param EntryId The queue entry to cancel
	 * @param bRefundIngredients If true, refund consumed ingredients
	 * @return True if found and cancelled
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool CancelCraft(const FGuid& EntryId, bool bRefundIngredients = true);

	/**
	 * Cancel all crafts in the queue.
	 * @param bRefundIngredients If true, refund all consumed ingredients
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void CancelAllCrafts(bool bRefundIngredients = true);

	/**
	 * Move a queue entry to a new position.
	 * @param EntryId The entry to move
	 * @param NewIndex New position in queue (0 = front, but cannot move ahead of active craft)
	 * @return True if moved successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool ReorderQueueEntry(const FGuid& EntryId, int32 NewIndex);

	// --- Progress Control ---

	/**
	 * Start or resume crafting.
	 * @return True if crafting started/resumed
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool StartCrafting();

	/** Pause crafting (e.g., when leaving station range). */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void PauseCrafting();

	/** Check if crafting is currently active. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	bool IsCraftingActive() const { return bIsCraftingActive; }

	// --- Query ---

	/** Get the number of entries in the queue. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	int32 GetQueueLength() const { return Queue.Entries.Num(); }

	/** Check if the queue is empty. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	bool IsQueueEmpty() const { return Queue.Entries.Num() == 0; }

	/** Get the currently active craft (first in queue). Returns false if queue empty. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool GetCurrentCraft(FMOCraftingQueueEntry& OutEntry) const;

	/** Get all queue entries. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void GetAllQueueEntries(TArray<FMOCraftingQueueEntry>& OutEntries) const;

	/** Get a specific queue entry by ID. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool GetQueueEntry(const FGuid& EntryId, FMOCraftingQueueEntry& OutEntry) const;

	/** Get total time remaining for all queued crafts (in seconds). */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	float GetTotalTimeRemaining() const;

	/** Get time remaining for the current craft (in seconds). */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	float GetCurrentCraftTimeRemaining() const;

	/** Get the current craft progress (0.0 - 1.0). */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	float GetCurrentCraftProgress() const;

	// --- Save/Load ---

	/** Build save data from current queue state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void BuildSaveData(FMOCraftingQueueSaveData& OutSaveData) const;

	/**
	 * Apply save data and optionally advance time for offline progress.
	 * @param InSaveData The save data to apply
	 * @param bCalculateOfflineProgress If true, calculate how much time passed and advance crafts
	 * @return True if applied successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool ApplySaveData(const FMOCraftingQueueSaveData& InSaveData, bool bCalculateOfflineProgress = true);

	/** Clear the entire queue without refunds. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void ClearQueue();

	// --- Configuration ---

	/** How often to update progress (in seconds). Lower = smoother progress bars but more overhead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Queue", meta=(ClampMin="0.1", ClampMax="5.0"))
	float ProgressUpdateInterval = 0.5f;

	/** Maximum number of entries allowed in queue (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Queue", meta=(ClampMin="0"))
	int32 MaxQueueSize = 20;

	/** If true, crafting continues in background when UI is closed (requires station proximity check). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Queue")
	bool bAllowBackgroundCrafting = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// --- Internal Methods ---

	/** Process crafting progress. Called by tick or timer. */
	void ProcessCraftingTick(float DeltaTime);

	/** Complete the current craft and start the next one. */
	void CompletCurrentCraft();

	/** Consume ingredients for a craft. Returns true if successful. */
	bool ConsumeIngredientsForCraft(FName RecipeId, int32 Count);

	/** Refund ingredients for a cancelled craft. */
	void RefundIngredientsForCraft(FName RecipeId, int32 Count);

	/** Calculate progress based on real time elapsed. */
	float CalculateProgressFromTime(const FMOCraftingQueueEntry& Entry, float CraftDuration) const;

	/** Get the craft duration for a recipe (with tool quality modifiers). */
	float GetEffectiveCraftDuration(FName RecipeId) const;

	/** Advance queue by elapsed time (for offline progress). */
	void AdvanceQueueByTime(float ElapsedSeconds);

	/** Cache component references. */
	void CacheComponents();

private:
	/** The crafting queue (replicated via FastArray). */
	UPROPERTY(Replicated)
	FMOCraftingQueueList Queue;

	/** Whether crafting is currently active. */
	UPROPERTY(Replicated)
	bool bIsCraftingActive = false;

	/** Real-world timestamp when current craft started (for accurate time tracking). */
	UPROPERTY()
	FDateTime CurrentCraftStartTime;

	/** Accumulated time since last progress update. */
	float AccumulatedDeltaTime = 0.0f;

	// Cached component references
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> CachedInventory;

	UPROPERTY()
	TWeakObjectPtr<UMOCraftingSubsystem> CachedCraftingSubsystem;

	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> CachedDiscovery;
};
