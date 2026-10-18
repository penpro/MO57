/**
 * =============================================================================
 * MOCraftingQueueComponent.h - Per-Pawn Crafting Queue with Timed Progression
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Manages a per-pawn crafting queue with real-time progression tracking. Supports
 * long craft durations (hours/days), offline progress calculation, background
 * crafting, and queue management with cancel/refund support.
 *
 * KEY RESPONSIBILITIES:
 * 1. Maintain ordered queue of crafting entries (FMOCraftingQueueList)
 * 2. Track craft progress using real timestamps (not delta time)
 * 3. Reserve original material lots on enqueue, consume one repeat on completion
 * 4. Persist products and undelivered refunds until inventory can receive them
 * 5. Support offline progress calculation on save load
 * 6. Track active station for station-specific recipes
 *
 * OWNERSHIP:
 * - Owner: AMOCharacter pawn (crafting-capable actors)
 * - Lifespan: Exists for pawn lifetime
 *
 * QUEUE ENTRY LIFECYCLE:
 * EnqueueCraft(RecipeId, Count, Station)
 * -> ReserveIngredientsForCraft()
 * -> Add FMOCraftingQueueEntry to Queue
 * -> OnQueueChanged broadcast
 * -> Tick processes active entry
 * -> CompletCurrentCraft() -> consume captured inputs, install products, then reward/notify
 * -> Entry removed, next started
 *
 * TIMESTAMP-BASED PROGRESS:
 * - CurrentCraftStartTime stores DateTime when craft began
 * - Progress = (Now - StartTime) / CraftDuration
 * - Allows accurate offline progress calculation
 * - AdvanceQueueByTime() handles save/load catch-up
 *
 * STATION TRACKING:
 * - SetActiveStation(Actor) stores reference and GUID
 * - GetActiveStationGuid() used for save/load
 * - ClearActiveStation() when leaving station range
 *
 * CRITICAL PATTERNS:
 * 1. INGREDIENTS: Reserved on enqueue; captured per-craft quantities consume owned lots on completion.
 * 2. REFUND: Cancellation retains exact state; full-inventory remainders remain persisted and visible.
 * 3. LEGACY: Old preconsumed saves have no provenance; unresolved cancellation records are preserved.
 * 4. OFFLINE: ApplySaveData with bCalculateOfflineProgress=true
 * 5. FASTARRAY: Queue uses FFastArraySerializer for replication
 *
 * KNOWN PITFALLS:
 * 1. STATION REFERENCE: ActiveStation may become invalid if destroyed
 * 2. CRAFT DURATION: Uses GetEffectiveCraftDuration() for tool bonuses
 * 3. QUEUE SIZE: MaxQueueSize limits entries (default 20)
 *
 * RELATED FILES:
 * - MOCraftingTypes.h - FMOCraftingQueueEntry, FMOCraftingQueueList
 * - MOCraftingSubsystem.h - Validation and output production
 * - MOCraftingCapableInterface.h - Interface for pawns
 * - MOCraftingUIController.h - UI integration
 *
 * TESTING CHECKLIST:
 * [ ] EnqueueCraft consumes ingredients
 * [ ] Progress updates during tick
 * [ ] Craft completion produces outputs
 * [ ] Cancel refunds ingredients
 * [ ] Save/load preserves queue state
 * [ ] Offline progress calculates correctly
 *
 * LAST UPDATED: 2026-10-17 - Authoritative context and persisted material ownership
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOCraftingTypes.h"
#include "MOInterruptibleInterface.h"
#include "MOCraftingQueueComponent.generated.h"

class AMOCharacter;
class UMOCraftingSubsystem;
class UMOInventoryComponent;
class UMORecipeDiscoveryComponent;
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCraftingQueueComponent : public UActorComponent,
	public IMOInterruptibleInterface
{
	GENERATED_BODY()

public:
	/**
	 * Interrupt policy for an active crafting queue:
	 *   Movement              -> ignored (player can walk around mid-craft)
	 *   Damage (sev >= 0.5)   -> Pause (real injury disrupts fine work)
	 *   Knockdown / Unconscious / EnteredCombat / LostControl
	 *                         -> Pause (preserve elapsed time via timestamp model)
	 *   Death                 -> Cancel current craft with refund
	 *   UserCancel            -> ignored (UI calls CancelCraft directly)
	 *
	 * Pausing captures elapsed progress. Resume back-dates the timestamp by that
	 * saved progress. Context suspension retries the same station identity and
	 * never accrues progress while the station is unusable.
	 */
	virtual void NotifyInterrupt_Implementation(const FMOInterruptContext& Context) override;
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
	 * Add a craft to the queue. Ingredients transfer into a persisted reservation immediately.
	 * @param RecipeId Recipe to craft
	 * @param Count Number of times to repeat (default 1)
	 * @param Station Legacy display hint; execution resolves the active station actor instead.
	 * @return True if successfully enqueued
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool EnqueueCraft(FName RecipeId, int32 Count = 1, EMOCraftingStation Station = EMOCraftingStation::None);

	/** Submit a real station actor. The server resolves its type, identity, reach, active state and heat capability. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	bool EnqueueCraftAtStation(FName RecipeId, int32 Count, AActor* StationActor);

	/** Native authoritative receipt for an assigning job. EntryId is assigned before queue/inventory observers run. */
	bool EnqueueCraftAtStationWithId(FName RecipeId, int32 Count, AActor* StationActor, FGuid& OutEntryId,
		TFunction<void(const FGuid&)> OnInstalled = {});

	/** Opt-in native assignments retain one result (Count must be one). OnInstalled installs the assignment's
	 * saved ownership before any observers; it must not call delegates or mutate the crafting queue. */
	bool GetCompletionReceipt(const FGuid& EntryId, FMOCraftingCompletionReceipt& OutReceipt) const;
	void AcknowledgeCompletionReceipt(const FGuid& EntryId);

	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	bool IsWaitingForCraftingContext() const { return bAwaitingContext; }

	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	FText GetCraftingBlockReason() const { return CraftingBlockReason; }

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

	/** Owned material lots awaiting room/a compatible vessel. They survive save/load and retry delivery. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	int32 GetPendingMaterialLotCount() const { return PendingMaterialLotCount; }

	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	int32 GetUnresolvedLegacyRefundCount() const { return UnresolvedLegacyRefundCount; }

	/** Retry retained materials, e.g. after making inventory room. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void DeliverPendingMaterials();

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

	/** Get overall queue progress (0.0 - 1.0), accounting for all entries and repeats. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	float GetOverallQueueProgress() const;

	// --- Station Tracking ---

	/**
	 * Set the active crafting station.
	 * Called when player opens a crafting station to track which station they're using.
	 * @param Station The crafting station actor (must implement IMOIdentifiableInterface)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void SetActiveStation(AActor* Station);

	/** Clear the active station reference. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Queue")
	void ClearActiveStation();

	/** Get the GUID of the active station (invalid GUID if none). */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	FGuid GetActiveStationGuid() const;

	/** Get the active station actor (may be null). */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Queue")
	AActor* GetActiveStation() const;

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

	/**
	 * Client->host transport for co-op: a remote client that locally controls this pawn
	 * forwards its craft request here instead of no-oping. Runs EnqueueCraft on the host
	 * (authority); the replicated Queue then updates the requesting client's UI (#132/H20).
	 */
	UFUNCTION(Server, Reliable)
	void ServerRequestEnqueueCraft(FName RecipeId, int32 Count, AActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestCancelCraft(FGuid EntryId);

	UFUNCTION(Server, Reliable)
	void ServerRequestCancelAllCrafts();

private:
	// --- Internal Methods ---
	AActor* ResolveEntryStation(const FMOCraftingQueueEntry& Entry) const;
	bool ValidateEntryContext(const FMOCraftingQueueEntry& Entry, FText& OutReason) const;
	void WaitForContext(const FText& Reason);
	TMap<FGuid, TWeakObjectPtr<AActor>> KnownStations;
	float ContextRetryElapsed = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_MaterialStatus)
	bool bAwaitingContext = false;

	UPROPERTY(ReplicatedUsing=OnRep_MaterialStatus)
	FText CraftingBlockReason;

	/** Process crafting progress. Called by tick or timer. */
	void ProcessCraftingTick(float DeltaTime);

	/** Complete the current craft and start the next one. */
	bool CompletCurrentCraft();

	bool ReserveIngredientsForCraft(FMOCraftingQueueEntry& Entry);
	bool RetainCancelledIngredients(const FMOCraftingQueueEntry& Entry);
	void RefreshTickEnabled();

	UPROPERTY()
	FMOIngredientReservation PendingReturns;
	UPROPERTY()
	FMOIngredientReservation PendingProducts;
	UPROPERTY()
	TArray<FMOCraftingCompletionReceipt> CompletionReceipts;
	UPROPERTY()
	TArray<FMOCraftingQueueEntry> UnresolvedLegacyRefunds;
	float MaterialRetryElapsed = 0.0f;
	bool bDeliveringMaterials = false;
	bool bProcessingCompletion = false;
	UPROPERTY(ReplicatedUsing=OnRep_MaterialStatus)
	int32 PendingMaterialLotCount = 0;
	UPROPERTY(ReplicatedUsing=OnRep_MaterialStatus)
	int32 UnresolvedLegacyRefundCount = 0;
	UFUNCTION()
	void OnRep_MaterialStatus();

	/** Calculate progress based on real time elapsed. */
	float CalculateProgressFromTime(const FMOCraftingQueueEntry& Entry, float CraftDuration) const;

	/** Get the craft duration for a recipe (with tool quality modifiers). */
	float GetEffectiveCraftDuration(FName RecipeId) const;

	/** Advance queue by elapsed time (for offline progress). */
	void AdvanceQueueByTime(float ElapsedSeconds);

	/** Cache component references. */
	void CacheComponents();

	/** Hook into the owning character's interrupt broadcast. Idempotent. */
	void RegisterWithOwnerForInterrupts();

	/** Drop the owner interrupt registration. */
	void UnregisterFromOwnerInterrupts();

	/**
	 * Character we registered with for interrupt notifications. Cached so
	 * we can unregister on EndPlay even if GetOwner() has already torn down.
	 * Resolved during the first StartCrafting() that actually activates a craft.
	 */
	TWeakObjectPtr<AMOCharacter> RegisteredCrafterCharacter;

private:
	/** The crafting queue (replicated via FastArray). */
	UPROPERTY(Replicated)
	FMOCraftingQueueList Queue;

	/** Whether crafting is currently active. */
	UPROPERTY(ReplicatedUsing=OnRep_MaterialStatus)
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

	/** The crafting station the player is currently using (for save/load). */
	UPROPERTY()
	TWeakObjectPtr<AActor> ActiveStation;
};
