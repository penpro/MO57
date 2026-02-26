/**
 * =============================================================================
 * MOContextMenuBase.h - Base Class for Popup Context Menus
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base class for all popup context menus. Provides standardized
 * positioning, keyboard handling, and close behavior. Subclass this for
 * any right-click or interaction popup menus.
 *
 * SUBCLASSES:
 * - UMOGhostContextMenu - Building ghost interactions
 * - UMOStationContextMenu - Crafting station interactions
 * - UMOKeepOnHarvestContextMenu - ISM/HISM resource harvesting
 * - UMOGroundContextMenu - Ground foraging interactions
 * - UMOSurvivorContextMenu - Survivor/NPC interactions
 *
 * FEATURES PROVIDED:
 * 1. SetPopupPosition() - Screen-space positioning
 * 2. Escape/Tab key closes menu
 * 3. OnCloseRequested delegate for cleanup
 * 4. Optional mouse-leave closing with grace period
 * 5. BindButtonClick() helper for safe button binding
 *
 * =============================================================================
 * BEST PRACTICES
 * =============================================================================
 *
 * 1. Use BindButtonClick() helper instead of manual binding:
 *    BindButtonClick(MyButton, this, &UMyMenu::HandleClick);
 *    This automatically removes existing bindings first.
 *
 * 2. Always broadcast OnCloseRequested when menu should close.
 *    Don't call RemoveFromParent() directly.
 *
 * 3. Context menus are NOT activatable widgets (no input mode change).
 *    They overlay the game world and disappear on interaction.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] POSITION CLAMPING: SetPopupPosition should clamp to viewport
 *   bounds. If menu appears off-screen, check the implementation.
 *
 * [2024-02] MOUSE LEAVE TIMING: MouseLeaveGraceTime prevents accidental
 *   closes. If menu closes too quickly, increase this value (default 0.3s).
 *
 * [2024-02] KEY HANDLING: NativeOnKeyDown returns FReply::Handled() for
 *   Escape/Tab. This prevents key from propagating to game. If you need
 *   different keys, override NativeOnKeyDown.
 *
 * [2024-02] DELEGATE CLEANUP: OnCloseRequested bindings are NOT auto-cleared.
 *   If UI manager has weak refs, this is fine. Otherwise, unbind on destroy.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOUIControllerBase.h - May spawn context menus
 * - MOGhostContextMenu.h - Building ghost subclass
 * - MOStationContextMenu.h - Crafting station subclass
 * - MOCommonButton.h - Button class used in menus
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOCommonButton.h"
#include "MOContextMenuBase.generated.h"

/**
 * Standard close request delegate used by all context menus.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOContextMenuCloseRequested);

/**
 * Base class for all context menu widgets.
 * See file header for usage guide and pitfalls.
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
