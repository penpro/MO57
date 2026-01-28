#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h" // UWorld::InitializationValues
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOworldSaveGame.h"
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
    TArray<FGuid> FailedPawnGuids;

    UPROPERTY(BlueprintReadOnly)
    FString ErrorMessage;
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

    /** Check if a save slot exists. */
    UFUNCTION(BlueprintPure, Category="MO|Persistence")
    bool DoesSaveSlotExist(const FString& SlotName) const;

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

    // Load helpers
    void UnpossessAllControllers(UWorld* World) const;

    void DestroyAllPersistedPawns(UWorld* World);
    void RespawnPersistedPawns(UWorld* World, const TArray<FMOPersistedPawnRecord>& PersistedPawns, FMOLoadResult& OutResult);
    void ApplyInventoriesToSpawnedPawns(UWorld* World, const TMap<FGuid, FMOInventorySaveData>& PawnInventoriesByGuid);

    void DestroyAllPersistedWorldItems(UWorld* World);
    void RespawnWorldItems(UWorld* World, const TArray<FMOPersistedWorldItemRecord>& WorldItems, FMOLoadResult& OutResult);

    void ClearLoadSuppression();

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

    // Time to suppress destroyed GUID recording after load (seconds)
    static constexpr float LoadSuppressionDuration = 0.25f;
};
