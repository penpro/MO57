#pragma once

#include "CoreMinimal.h"
#include "MOUIDelegates.generated.h"

/**
 * Dummy struct to force UHT to process this file.
 * Required for delegate declarations to be visible to other headers.
 */
USTRUCT()
struct FMOUIDelegatesModule
{
	GENERATED_BODY()
};

/**
 * Standard UI delegate library for MOFramework.
 *
 * This file consolidates common delegate signatures used across UI widgets.
 * Using these standard delegates instead of per-widget declarations:
 * - Reduces duplicate code
 * - Makes binding patterns consistent
 * - Simplifies UI manager orchestration
 *
 * MIGRATION GUIDE:
 * Old: DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOGhostMenuRequestCloseSignature);
 * New: Use FMOUIRequestClose from this file
 *
 * Widgets should gradually migrate to these standard delegates.
 * Legacy delegates are kept for backward compatibility.
 */

// ============================================================================
// STANDARD UI DELEGATES
// ============================================================================

/**
 * Request to close a UI element (menu, panel, popup).
 * Replaces: FMOGhostMenuRequestCloseSignature, FMOStationMenuRequestCloseSignature,
 *           FMOKeepOnHarvestMenuRequestClose, FMOContextMenuClosedSignature, etc.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOUIRequestClose);

/**
 * Generic action triggered by a UI element.
 * @param ActionId - Identifier for the action (e.g., "Craft", "Build", "Cancel")
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUIActionTriggered, FName, ActionId);

/**
 * Action with item/target identifier.
 * @param ActionId - Identifier for the action
 * @param TargetGuid - GUID of the target item/object
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOUIActionWithTarget, FName, ActionId, const FGuid&, TargetGuid);

/**
 * Action with recipe identifier.
 * @param ActionId - Identifier for the action
 * @param RecipeId - Recipe row name
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOUIActionWithRecipe, FName, ActionId, FName, RecipeId);

/**
 * Selection changed in a UI element.
 * @param SelectedId - Identifier of the selected item (NAME_None if deselected)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUISelectionChanged, FName, SelectedId);

/**
 * Selection changed with GUID.
 * @param SelectedGuid - GUID of the selected item (invalid GUID if deselected)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUISelectionChangedGuid, const FGuid&, SelectedGuid);

/**
 * Quantity changed (for crafting amounts, split stacks, etc.).
 * @param NewQuantity - The new quantity value
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUIQuantityChanged, int32, NewQuantity);

/**
 * Confirmation dialog result.
 * @param bConfirmed - True if user confirmed, false if cancelled
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUIConfirmationResult, bool, bConfirmed);

/**
 * Progress update (for build timers, crafting progress, etc.).
 * @param Progress - Progress value (0.0 to 1.0)
 * @param TimeRemaining - Time remaining in seconds (optional, -1 if not applicable)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOUIProgressUpdate, float, Progress, float, TimeRemaining);

/**
 * Visibility change request.
 * @param bShouldBeVisible - Whether the UI element should be visible
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUIVisibilityChanged, bool, bShouldBeVisible);

// ============================================================================
// INVENTORY-SPECIFIC DELEGATES
// ============================================================================

/**
 * Slot clicked in an inventory grid.
 * @param SlotIndex - Index of the clicked slot
 * @param ItemGuid - GUID of item in slot (invalid if empty)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOUISlotClicked, int32, SlotIndex, const FGuid&, ItemGuid);

/**
 * Item drag started.
 * @param SourceSlotIndex - Index of the source slot
 * @param ItemGuid - GUID of the item being dragged
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOUIDragStarted, int32, SourceSlotIndex, const FGuid&, ItemGuid);

/**
 * Item dropped onto a slot.
 * @param SourceSlotIndex - Original slot index
 * @param TargetSlotIndex - Destination slot index
 * @param ItemGuid - GUID of the dropped item
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOUIDropCompleted, int32, SourceSlotIndex, int32, TargetSlotIndex, const FGuid&, ItemGuid);

// ============================================================================
// CRAFTING-SPECIFIC DELEGATES
// ============================================================================

/**
 * Craft request.
 * @param RecipeId - Recipe to craft
 * @param Quantity - Number to craft
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOUICraftRequest, FName, RecipeId, int32, Quantity);

/**
 * Recipe selected in crafting menu.
 * @param RecipeId - Selected recipe ID (NAME_None if deselected)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUIRecipeSelected, FName, RecipeId);

// ============================================================================
// BUILDING-SPECIFIC DELEGATES
// ============================================================================

/**
 * Build action requested.
 * @param RecipeId - Building recipe to construct
 * @param Location - World location for placement
 * @param Rotation - Rotation for placement
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOUIBuildRequest, FName, RecipeId, FVector, Location, FRotator, Rotation);

// ============================================================================
// NOTIFICATION DELEGATES
// ============================================================================

/**
 * Notification dismissed.
 * @param NotificationId - ID of the dismissed notification
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUINotificationDismissed, int32, NotificationId);
