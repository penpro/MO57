#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOItemContextMenu.generated.h"

class UMOCommonButton;
class UMOInventoryComponent;
class UPanelWidget;

/**
 * Context menu that appears when right-clicking an inventory slot.
 * Displays available actions based on the item's properties.
 *
 * Auto-hides when mouse leaves the menu area.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOContextMenuClosedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOContextMenuActionSignature, FName, ActionId, const FGuid&, ItemGuid);

UCLASS()
class MOFRAMEWORK_API UMOItemContextMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the context menu for a specific item.
	 * Call this after creating the widget and before adding to viewport.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void InitializeForItem(UMOInventoryComponent* InInventoryComponent, const FGuid& InItemGuid, int32 InSlotIndex);

	/**
	 * Position the menu at the given screen location (typically slot position).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void SetMenuPosition(FVector2D ScreenPosition);

	/** Called when the menu is closed for any reason. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|ContextMenu")
	FMOContextMenuClosedSignature OnMenuClosed;

	/** Called when an action button is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|ContextMenu")
	FMOContextMenuActionSignature OnActionSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Called to update button visibility based on item properties. Override in BP for custom logic. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|ContextMenu")
	void RefreshButtonVisibility();

private:
	void BindButtonEvents();
	void CloseMenu();
	bool IsMouseOverMenu() const;

	// Button click handlers
	UFUNCTION() void HandleUseClicked();
	UFUNCTION() void HandleDrop1Clicked();
	UFUNCTION() void HandleDropAllClicked();
	UFUNCTION() void HandleInspectClicked();
	UFUNCTION() void HandleSplitStackClicked();
	UFUNCTION() void HandleCraftClicked();

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_ItemContextMenu
	// ============================================================

	/** Container panel that holds all the buttons. Used for mouse-over detection. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> ButtonContainer;

	/** Use/Consume button - hidden if item is not consumable. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> UseButton;

	/** Drop single item button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> Drop1Button;

	/** Drop entire stack button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> DropAllButton;

	/** Inspect item button - grants knowledge/XP. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> InspectButton;

	/** Split stack button - hidden if quantity <= 1. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> SplitStackButton;

	/** Craft button - opens crafting menu filtered to this item. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> CraftButton;

	// ============================================================
	// State
	// ============================================================

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	FGuid ItemGuid;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	/** Time since mouse left the menu (for delayed auto-close). */
	float MouseOutsideTimer = 0.0f;

	/** Delay before auto-closing when mouse leaves (seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|UI|ContextMenu")
	float AutoCloseDelay = 0.15f;

	/** Whether the menu has been initialized. */
	bool bInitialized = false;

	/** Timer handle for auto-close check. */
	FTimerHandle MouseCheckTimerHandle;

	/** Start the mouse check timer. */
	void StartMouseCheckTimer();

	/** Stop the mouse check timer. */
	void StopMouseCheckTimer();

	/** Called by timer to check mouse position. */
	void CheckMousePosition();
};
