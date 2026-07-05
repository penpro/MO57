/**
 * =============================================================================
 * MOPersistenceSubsystem.h - World Save/Load System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * GameInstanceSubsystem managing world save/load operations. Persists pawns,
 * inventories, world items, buildings, voxel sculpts, quests, and weather.
 * Maintains GUID-based identity across save sessions.
 *
 * KEY RESPONSIBILITIES:
 * 1. Save/load full world state to USaveGame objects
 * 2. Track pawn records (living, deceased, spawned status)
 * 3. Track destroyed GUIDs to prevent respawning deleted items
 * 4. Manage current save slot and metadata (playtime, screenshots)
 * 5. Spawn pawns from saved records for possession system
 *
 * ARCHITECTURE NOTES:
 * - GameInstanceSubsystem: Survives level transitions, one per game instance
 * - Binds to UMOIdentityRegistrySubsystem to track destroyed actors
 * - LoadedWorldSave holds in-memory copy of save state
 * - Suppression system prevents recording false destroys during load
 *
 * CRITICAL PATTERNS:
 * 1. Save/Load Flow:
 *    Save: CaptureX() methods -> SaveObject -> USaveGame::SaveGameToSlot()
 *    Load: LoadGameFromSlot() -> RespawnX() methods -> ApplyX() methods
 *
 * 2. Destroyed GUID Tracking:
 *    - SessionDestroyedGuids: GUIDs destroyed this session
 *    - IsGuidDestroyed(): Check before spawning items
 *    - ClearDestroyedGuid(): When item re-enters world
 *
 * 3. Load Suppression:
 *    - During load, many actors get destroyed/respawned
 *    - bSuppressDestroyedGuidRecording prevents false positives
 *    - Clears after LoadSuppressionDuration (0.25s)
 *
 * KNOWN PITFALLS:
 * 1. CIRCULAR DEPENDENCY: MOInventoryComponent.DropItemByGuid() calls
 *    IsGuidDestroyed() creating runtime dependency. If decoupling needed,
 *    create IMOPersistenceProvider interface.
 *
 * 2. PAWN CLASS MISMATCH: If saved PawnClass changed or deleted, spawn
 *    fails. Check FMOPersistedPawnRecord::PawnClass before spawn.
 *
 * 3. WORLD CONTEXT: This is GameInstance subsystem but binds to World.
 *    Must handle world transitions - Deinitialize unbinds, new world
 *    triggers HandlePostWorldInitialization.
 *
 * 4. VOXEL SCULPT DATA: Large sculpts can bloat save files. Consider
 *    chunking or compression for large worlds.
 *
 * 5. SCREENSHOT CAPTURE: CaptureScreenshotForSave() may fail silently
 *    if no viewport is rendering. Check for null/empty data.
 *
 * RELATED FILES:
 * - MOWorldSaveGame.h - USaveGame class holding all persisted data
 * - MOSaveGameTypes.h - Struct definitions for persisted records
 * - MOIdentityComponent.h - Per-actor GUID identity
 * - MOIdentityRegistrySubsystem.h - GUID-to-Actor mapping
 * - MOInventoryComponent.h - Inventory serialization (circular dep)
 *
 * TESTING CHECKLIST:
 * [ ] New game -> save -> load maintains state
 * [ ] Deleted item stays deleted after save/load
 * [ ] Pawn possession restored correctly
 * [ ] Level transition doesn't lose data
 * [ ] Failed pawn loads reported in LastLoadResult
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h" // UWorld::InitializationValues
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOworldSaveGame.h"
#include "MOSaveGameTypes.h"
#include "MOPersistenceSubsystem.generated.h"

class AActor;
class APawn;
class UMOIdentityComponent;
class UMOIdentityRegistrySubsystem;
class UMOInventoryComponent;
class UMOItemComponent;

// Result of a load operation with detailed failure info
USTRUCT(BlueprintType)
struct FMOLoadResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly)
    int32 PawnsLoaded = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 PawnsFailed = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ItemsLoaded = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ItemsFailed = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 BuildingsLoaded = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 BuildingsFailed = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FGuid> FailedPawnGuids;

    UPROPERTY(BlueprintReadOnly)
    FString ErrorMessage;

    /** World seed from the loaded save (for voxel terrain regeneration). */
    UPROPERTY(BlueprintReadOnly)
    int32 WorldSeed = 0;

    /**
     * GUID of the pawn the player controller was possessing at save time.
     * Game mode uses this after voxel terrain is ready to re-possess the
     * correct pawn, so the player resumes IN their character at the saved
     * location rather than being left as a sky-cam spectator.
     */
    UPROPERTY(BlueprintReadOnly)
    FGuid LastPossessedPawnGuid;
};

UCLASS()
class MOFRAMEWORK_API UMOPersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool SaveWorldToSlot(const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool LoadWorldFromSlot(const FString& SlotName);

    // Load with detailed result - use this to detect and handle pawn loss
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    FMOLoadResult LoadWorldFromSlotWithResult(const FString& SlotName);

    // Get the last load result (useful if you used LoadWorldFromSlot)
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    const FMOLoadResult& GetLastLoadResult() const { return LastLoadResult; }

    // Check if any pawns failed to load in the last load operation
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    bool HasFailedPawns() const { return LastLoadResult.PawnsFailed > 0; }

    /** Get all save slot names (without extension). */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    TArray<FString> GetAllSaveSlots() const;

    /** Get save slots filtered by world identifier. */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    TArray<FString> GetSaveSlotsForWorld(const FString& WorldIdentifier) const;

    /** Get the current world's identifier (used for filtering saves). */
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    FString GetCurrentWorldIdentifier() const;

    /** Delete a save slot. Returns true if deleted successfully. */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool DeleteSaveSlot(const FString& SlotName);

    /**
     * Rename a save slot's display name. Does NOT rename the underlying file
     * (the slot name on disk is stable; only the player-visible DisplayName
     * changes). The metadata in the saved game is updated in place by loading
     * the save, mutating DisplayName, and writing it back to the same slot.
     *
     * @param SlotName The save slot file (stable identifier on disk).
     * @param NewDisplayName Player-facing label shown in the save UI.
     * @return True if the rename succeeded.
     */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool RenameSaveSlot(const FString& SlotName, const FText& NewDisplayName);

    /** Check if a save slot exists. */
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    bool DoesSaveSlotExist(const FString& SlotName) const;

    /**
     * Get metadata for a save slot without loading full save data.
     * @param SlotName The save slot to query
     * @param OutMetadata Filled with metadata if successful
     * @return True if metadata was retrieved successfully
     */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool GetSaveSlotMetadata(const FString& SlotName, FMOSaveMetadata& OutMetadata) const;

    /**
     * Set whether the next save should be marked as autosave.
     * Resets after save completes.
     */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    void SetNextSaveIsAutosave(bool bIsAutosave) { bNextSaveIsAutosave = bIsAutosave; }

    /**
     * Add session play time (call this periodically or before save).
     * @param DeltaSeconds Time to add in seconds
     */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    void AddSessionPlayTime(float DeltaSeconds) { SessionPlayTimeSeconds += DeltaSeconds; }

    /** Get current session play time. */
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    float GetSessionPlayTime() const { return SessionPlayTimeSeconds; }

    /**
     * Reset all GameInstance-scoped session state for a NEW world. Without
     * this, "play save A -> main menu -> New Game" leaks save A's
     * LoadedWorldSave / destroyed-GUID ledger / playtime into the new world
     * (cross-save contamination), and a careless save can overwrite slot A.
     * The new-game flow calls this with the slot the new world will use.
     */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    void ResetForNewWorld(const FString& NewSlotName);

    // --- Pawn Record Access (for Possession Menu) ---

    /** Get all pawn records from the current save data. */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    TArray<FMOPersistedPawnRecord> GetAllPawnRecords() const;

    /** Get a specific pawn record by GUID. */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool GetPawnRecord(const FGuid& PawnGuid, FMOPersistedPawnRecord& OutRecord) const;

    /** Update a pawn record (e.g., when renaming). Optionally auto-saves to current slot. */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool UpdatePawnRecord(const FMOPersistedPawnRecord& Record, bool bAutoSave = true);

    /** Get the current save slot name (empty if not loaded/saved yet). */
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    FString GetCurrentSlotName() const { return CurrentSlotName; }

    /** Save to the current slot (must have loaded or saved previously). */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool SaveToCurrentSlot();

    /** Spawn a pawn from a saved record (for possession). */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    APawn* SpawnPawnFromRecord(const FGuid& PawnGuid);

    /** Register a new pawn record (when creating new character). */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    void RegisterPawnRecord(const FMOPersistedPawnRecord& Record);

    /** Mark a pawn as deceased. */
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    void MarkPawnDeceased(const FGuid& PawnGuid);

    /** Check if any living (non-deceased) pawns exist. */
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    bool HasLivingPawns() const;

    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    bool IsGuidDestroyed(const FGuid& Guid) const;

    // Useful for drop logic later: if an item is re-instantiated into the world, remove it from DestroyedGuids.
    UFUNCTION(BlueprintCallable, Category="MO|Persistence")
    void ClearDestroyedGuid(const FGuid& Guid);

private:
    void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);
    void BindToWorld(UWorld* World);
    void UnbindFromWorld();

    void ApplyDestroyedGuidsToWorld(UWorld* World);

    UFUNCTION()
    void HandleIdentityRegistered(const FGuid& StableGuid, AActor* Actor);

    UFUNCTION()
    void HandleIdentityDestroyed(const FGuid& StableGuid);

private:
    // Save helpers
    bool IsPersistedPawn(const APawn* Pawn) const;
    void CapturePersistedPawnsAndInventories(UWorld* World, UMOWorldSaveGame* SaveObject) const;

    bool IsPersistedWorldItemActor(const AActor* Actor) const;
    void CaptureWorldItems(UWorld* World, UMOWorldSaveGame* SaveObject) const;

    bool IsPersistedBuildingActor(const AActor* Actor) const;
    void CaptureBuildings(UWorld* World, UMOWorldSaveGame* SaveObject) const;

    // Load helpers
    void UnpossessAllControllers(UWorld* World) const;

    void DestroyAllPersistedPawns(UWorld* World);
    void RespawnPersistedPawns(UWorld* World, const TArray<FMOPersistedPawnRecord>& PersistedPawns, FMOLoadResult& OutResult);
    void ApplyInventoriesToSpawnedPawns(UWorld* World, const TMap<FGuid, FMOInventorySaveData>& PawnInventoriesByGuid);

    void DestroyAllPersistedWorldItems(UWorld* World);
    void RespawnWorldItems(UWorld* World, const TArray<FMOPersistedWorldItemRecord>& WorldItems, FMOLoadResult& OutResult);

    void DestroyAllPersistedBuildings(UWorld* World);
    void RespawnBuildings(UWorld* World, const TArray<FMOPersistedBuildingRecord>& Buildings, FMOLoadResult& OutResult);

    // Voxel sculpt persistence
    void CaptureVoxelSculptData(UWorld* World, UMOWorldSaveGame* SaveObject) const;
    void RestoreVoxelSculptData(UWorld* World, const TArray<FMOVoxelSculptSaveRecord>& SculptData);

    // Quest persistence
    void CaptureQuestData(UMOWorldSaveGame* SaveObject) const;
    void RestoreQuestData(const FMOQuestSaveData& QuestData);

    // Weather/time persistence (UDS/UDW integration)
    void CaptureWeatherData(UWorld* World, UMOWorldSaveGame* SaveObject) const;
    void RestoreWeatherData(UWorld* World, const FMOWeatherSaveData& WeatherData);

    // Terrain-modification persistence (modified-zone tracker for foliage cleanup)
    void CaptureTerrainModificationData(UWorld* World, UMOWorldSaveGame* SaveObject) const;
    void RestoreTerrainModificationData(UWorld* World, const FMOTerrainModificationSaveData& TerrainModData);

    // (H34) Game clock persistence (in-game date/time, TimeScale, accumulators)
    void CaptureGameClockData(UWorld* World, UMOWorldSaveGame* SaveObject) const;
    void RestoreGameClockData(UWorld* World, const FMOGameClockSaveData& ClockData);
    void CaptureColonyData(UWorld* World, UMOWorldSaveGame* SaveObject) const;   // (V1)
    void RestoreColonyData(UWorld* World, const FMOColonySaveData& ColonyData);  // (V1)

    // (H37) Resource depletion persistence (per-node yield counts + respawn timers)
    void CaptureResourceDepletionData(UWorld* World, UMOWorldSaveGame* SaveObject) const;
    void RestoreResourceDepletionData(UWorld* World, const FMOResourceDepletionSaveData& DepletionData);

    /**
     * (H30) Shared per-pawn component-state restoration. Applies the 7 component
     * save blocks (vitals/anatomy/metabolism/mental/skills/equipment/recruitment)
     * plus combat (H38) and job queue (H39) from a record onto a spawned pawn.
     * Called by BOTH the full-load path (ApplyInventoriesToSpawnedPawns) and the
     * possession-menu path (SpawnPawnFromRecord) so the two never diverge.
     */
    void ApplyPawnComponentState(APawn* Pawn, const FMOPersistedPawnRecord& Record) const;

    // Setup spectator camera above last possessed pawn on load
    void SetupSpectatorCameraForLoad(UWorld* World, const UMOWorldSaveGame* SaveData);

    void ClearLoadSuppression();

    // Generate a unique slot name for auto-save when no slot has been set
    FString GenerateAutoSaveSlotName() const;

    // Capture a screenshot and store it in the save object
    void CaptureScreenshotForSave(UMOWorldSaveGame* SaveObject) const;

private:
    UPROPERTY()
    TSet<FGuid> SessionDestroyedGuids;

    UPROPERTY()
    TObjectPtr<UMOWorldSaveGame> LoadedWorldSave;

    UPROPERTY()
    TSet<FGuid> PawnInventoryGuidsAppliedThisLoad;

    FDelegateHandle PostWorldInitHandle;

    UPROPERTY()
    TWeakObjectPtr<UWorld> BoundWorld;

    UPROPERTY()
    TWeakObjectPtr<UMOIdentityRegistrySubsystem> BoundRegistry;

    UPROPERTY()
    bool bSuppressDestroyedGuidRecording = false;

    // Guids of actors being replaced in the current load pass (pawns and items).
    UPROPERTY()
    TSet<FGuid> ReplacedGuidsThisLoad;

    FTimerHandle ClearSuppressionTimerHandle;

    // Result of the last load operation
    UPROPERTY()
    FMOLoadResult LastLoadResult;

    // Current save slot name (set on load or save)
    UPROPERTY()
    FString CurrentSlotName;

    // Time to suppress destroyed GUID recording after load (seconds)
    static constexpr float LoadSuppressionDuration = 0.25f;

    // Metadata tracking
    bool bNextSaveIsAutosave = false;
    float SessionPlayTimeSeconds = 0.0f;
};
