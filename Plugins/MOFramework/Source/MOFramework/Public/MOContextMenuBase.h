#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOCommonButton.h"
#include "MOContextMenuBase.generated.h"

/**
 * Standard close request delegate used by all context menus.
 * Consolidates FMOGhostMenuRequestCloseSignature, FMOStationMenuRequestCloseSignature, etc.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOContextMenuCloseRequested);

/**
 * Base class for all context menu widgets.
 * Provides common functionality:
 * - SetPopupPosition() for screen positioning
 * - Escape/Tab key handling to close the menu
 * - OnCloseRequested delegate
 * - Mouse leave behavior with grace period
 *
 * Subclasses:
 * - UMOGhostContextMenu (building ghosts)
 * - UMOStationContextMenu (crafting stations)
 * - UMOKeepOnHarvestContextMenu (ISM/HISM harvest targets)
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOContextMenuBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOContextMenuBase(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// POSITIONING
	// ============================================================================

	/**
	 * Set the screen position for this popup menu.
	 * @param ScreenPosition - Position in screen space
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	virtual void SetPopupPosition(FVector2D ScreenPosition);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/**
	 * Called when the menu requests to be closed.
	 * UI Manager should listen to this and remove the widget.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|ContextMenu")
	FMOContextMenuCloseRequested OnCloseRequested;

	// ============================================================================
	// CLOSE BEHAVIOR
	// ============================================================================

	/**
	 * Request the menu to close.
	 * Broadcasts OnCloseRequested delegate.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	virtual void RequestClose();

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ============================================================================
	// MOUSE LEAVE CONFIGURATION
	// ============================================================================

	/** Whether to close menu when mouse leaves (disabled by default, uses click-outside). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|UI|ContextMenu")
	bool bCloseOnMouseLeave = false;

	/** Time to wait before closing on mouse leave (grace period). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|UI|ContextMenu")
	float MouseLeaveGraceTime = 0.3f;

	/** Whether mouse is currently over the widget. */
	bool bIsMouseOver = false;

	/** Timer for mouse leave grace period. */
	float MouseLeaveTimer = 0.0f;

	// ============================================================================
	// HELPER METHODS
	// ============================================================================

	/**
	 * Helper to safely bind a button click handler.
	 * Removes any existing bindings first to prevent duplicates.
	 * @param Button - The button to bind
	 * @param Object - The object containing the handler
	 * @param MethodPtr - Pointer to the handler method
	 */
	template<typename ObjectType>
	void BindButtonClick(UMOCommonButton* Button, ObjectType* Object, void (ObjectType::*MethodPtr)())
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(Object);
			Button->OnClicked().AddUObject(Object, MethodPtr);
		}
	}

	/**
	 * Override this in subclasses to determine if the menu should close on mouse leave.
	 * Base implementation returns bCloseOnMouseLeave.
	 */
	virtual bool ShouldCloseOnMouseLeave() const;
};
