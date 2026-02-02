#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UObject/SoftObjectPath.h"
#include "MOCraftingTypes.h"
#include "MOBuildingTypes.h"
#include "MOworldSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FMOInventoryItemSaveEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid ItemGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FName ItemDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 Quantity = 0;

    /** Current durability for tools (-1 = infinite/not applicable). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 CurrentDurability = -1;
};

USTRUCT(BlueprintType)
struct FMOInventorySaveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 SlotCount = 0;

    // Size should be SlotCount. Invalid GUID means empty slot.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FGuid> SlotItemGuids;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOInventoryItemSaveEntry> Items;
};

USTRUCT(BlueprintType)
struct FMOPersistedPawnRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid PawnGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FTransform Transform;

    // Saved pawn class (soft) so we can respawn the same pawn type.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftClassPath PawnClassPath;

    // Character identity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString Gender;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 AgeInDays = 0;

    // Status
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    bool bIsDeceased = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    float HealthPercent = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString StatusText;

    // Tracking
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FDateTime LastPlayedTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString LocationName;

    // Portrait (asset path for now)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftObjectPath PortraitPath;
};

USTRUCT(BlueprintType)
struct FMOPersistedWorldItemRecord
{
    GENERATED_BODY()

    // This MUST be the Identity GUID. Do not store GUIDs on UMOItemComponent.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid ItemGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FTransform Transform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftClassPath ItemClassPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FName ItemDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 Quantity = 1;
};

/**
 * Save data for a persisted building in the world.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPersistedBuildingRecord
{
    GENERATED_BODY()

    /** Unique identifier for this building. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid BuildingGuid;

    /** World transform of the building. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FTransform Transform;

    /** Class path of the buildable actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftClassPath ActorClassPath;

    /** Recipe ID used to create this building. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FName RecipeId;

    /** Current build progress state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FMOBuildProgress Progress;

    /** Inventory data for containers/crafting stations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOInventoryItemSaveEntry> InventoryItems;

    /** Slot count for inventory (to restore proper slot layout). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 InventorySlotCount = 0;

    /** Slot GUIDs for inventory order. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FGuid> InventorySlotGuids;

    /** Current fuel level (for crafting stations). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    float CurrentFuel = 0.0f;

    /** Whether the station is currently active (for crafting stations). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    bool bIsActive = false;
};

UCLASS()
class MOFRAMEWORK_API UMOWorldSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FGuid> DestroyedGuids;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOPersistedPawnRecord> PersistedPawns;

    // PawnGuid -> InventorySaveData
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TMap<FGuid, FMOInventorySaveData> PawnInventoriesByGuid;

    // Runtime spawned item actors that still exist in the world at save time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOPersistedWorldItemRecord> WorldItems;

    // --- Crafting System Save Data ---

    /** Crafting queue state per pawn (PawnGuid -> QueueData). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Crafting")
    TMap<FGuid, FMOCraftingQueueSaveData> PawnCraftingQueuesByGuid;

    /** Discovered recipes per pawn (PawnGuid -> DiscoveryData). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Crafting")
    TMap<FGuid, FMORecipeDiscoverySaveData> PawnDiscoveredRecipesByGuid;

    // --- Building System Save Data ---

    /** All placed buildings in the world (including ghosts and under-construction). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Building")
    TArray<FMOPersistedBuildingRecord> Buildings;
};
