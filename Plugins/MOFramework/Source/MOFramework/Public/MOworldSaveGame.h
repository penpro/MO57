#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UObject/SoftObjectPath.h"
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
};
