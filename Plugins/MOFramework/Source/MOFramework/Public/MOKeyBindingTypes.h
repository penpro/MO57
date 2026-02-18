#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "MOKeyBindingTypes.generated.h"

/**
 * Storage structure for a persisted key binding override.
 * Saved to GameUserSettings.ini via MOGameSettings.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOKeyBindingEntry
{
	GENERATED_BODY()

	/** Unique identifier matching the InputAction asset name (e.g., "IA_Inventory"). */
	UPROPERTY(Config, BlueprintReadWrite, Category="KeyBinding")
	FName ActionId;

	/** The overridden key. */
	UPROPERTY(Config, BlueprintReadWrite, Category="KeyBinding")
	FKey OverrideKey;

	/** Slot index for actions with multiple keys (primary=0, secondary=1). */
	UPROPERTY(Config, BlueprintReadWrite, Category="KeyBinding")
	int32 SlotIndex = 0;

	FMOKeyBindingEntry()
		: ActionId(NAME_None)
		, OverrideKey(EKeys::Invalid)
		, SlotIndex(0)
	{
	}

	FMOKeyBindingEntry(FName InActionId, FKey InKey, int32 InSlotIndex = 0)
		: ActionId(InActionId)
		, OverrideKey(InKey)
		, SlotIndex(InSlotIndex)
	{
	}

	bool IsValid() const
	{
		return !ActionId.IsNone() && OverrideKey.IsValid();
	}

	bool operator==(const FMOKeyBindingEntry& Other) const
	{
		return ActionId == Other.ActionId && SlotIndex == Other.SlotIndex;
	}
};

/**
 * Visual data for key binding UI entries.
 * Used to populate key binding entry widgets.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOKeyBindingVisualData
{
	GENERATED_BODY()

	/** Display name shown to user (e.g., "Open Inventory"). */
	UPROPERTY(BlueprintReadOnly, Category="KeyBinding")
	FText DisplayName;

	/** Action identifier matching the InputAction name. */
	UPROPERTY(BlueprintReadOnly, Category="KeyBinding")
	FName ActionId;

	/** Currently bound key. */
	UPROPERTY(BlueprintReadOnly, Category="KeyBinding")
	FKey CurrentKey;

	/** Original default key (for reset functionality). */
	UPROPERTY(BlueprintReadOnly, Category="KeyBinding")
	FKey DefaultKey;

	/** Category for grouping (e.g., "Movement", "UI", "Building"). */
	UPROPERTY(BlueprintReadOnly, Category="KeyBinding")
	FName Category;

	FMOKeyBindingVisualData()
		: DisplayName(FText::GetEmpty())
		, ActionId(NAME_None)
		, CurrentKey(EKeys::Invalid)
		, DefaultKey(EKeys::Invalid)
		, Category(NAME_None)
	{
	}
};

class UInputAction;
class UInputMappingContext;

/**
 * Configuration for a rebindable action in the options panel.
 * Uses soft references so it can be configured in Blueprint and works without a player controller.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOKeyBindingActionConfig
{
	GENERATED_BODY()

	/** Display name shown to user. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="KeyBinding")
	FText DisplayName;

	/** The input action asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="KeyBinding")
	TSoftObjectPtr<UInputAction> Action;

	/** Category for grouping in UI (e.g., "Movement", "UI", "Building"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="KeyBinding")
	FName Category;

	/** If true, uses BuildingContext instead of PawnControlContext. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="KeyBinding")
	bool bUseBuildingContext = false;

	FMOKeyBindingActionConfig()
		: DisplayName(FText::GetEmpty())
		, Category(NAME_None)
		, bUseBuildingContext(false)
	{
	}
};
