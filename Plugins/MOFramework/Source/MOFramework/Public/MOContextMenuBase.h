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
 * ARCHITECTURE DECISION (2026-03)
 * =============================================================================
 *
 * Context menus inherit from UCommonUserWidget, NOT UCommonActivatableWidget.
 * This is intentional because context menus have different semantics:
 *
 * 1. Ephemeral popups that dismiss on click-outside
 * 2. NOT managed by widget stacks (no push/pop lifecycle)
 * 3. Added directly to viewport or overlay panel
 * 4. Handle their own Escape key via NativeOnKeyDown
 *
 * See commonui_migration_audit.md CORRECTION 3 for full rationale.
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
 * 3. Context menus are added to GameOverlay layer via AddToViewport or
 *    as children of an overlay panel, NOT pushed to widget stacks.
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
 * [2026-03] NOT ACTIVATABLE: Context menus are NOT UCommonActivatableWidget.
 *   Don't try to push them to stacks. Use AddToViewport() instead.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOUIControllerBase.h - May spawn context menus
 * - MOGhostContextMenu.h - Building ghost subclass
 * - MOStationContextMenu.h - Crafting station subclass
 * - MOCommonButton.h - Button class used in menus
 *
 * LAST UPDATED: 2026-03-31
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Framework/Application/IInputProcessor.h"
#include "MOCommonButton.h"
#include "MOContextMenuBase.generated.h"

class UMOContextMenuBase;

/**
 * Slate input pre-processor that closes a context menu when the user clicks
 * outside its bounds. Registered while the menu is constructed, unregistered
 * when it's destroyed. We do NOT consume the click — letting it flow through
 * means the player can right-click another resource and immediately get its
 * menu instead of needing a "wake-up" click after the old one closes.
 */
class FMOContextMenuClickOutsideHandler : public IInputProcessor
{
public:
	explicit FMOContextMenuClickOutsideHandler(UMOContextMenuBase* InMenu);

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

private:
	TWeakObjectPtr<UMOContextMenuBase> MenuWeak;
};

/**
 * Standard close request delegate used by all context menus.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOContextMenuCloseRequested);

/**
 * Base class for all context menu widgets.
 * See file header for usage guide and pitfalls.
 *
 * Inherits from UCommonUserWidget (NOT activatable) because context menus
 * are ephemeral popups managed separately from widget stacks.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOContextMenuBase : public UCommonUserWidget
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Whether cursor was visible before this context menu appeared */
	bool bWasCursorVisible = false;

	// ============================================================================
	// MOUSE LEAVE CONFIGURATION
	// ============================================================================

	/**
	 * Whether to close menu when mouse leaves its bounds (after grace period).
	 * Enabled by default so popup menus behave like a typical context menu.
	 * Subclasses with conditional close logic (e.g. ghost menus that need to
	 * stay open during a build timer) can still override ShouldCloseOnMouseLeave().
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|UI|ContextMenu")
	bool bCloseOnMouseLeave = true;

	/** Time to wait before closing on mouse leave (grace period). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|UI|ContextMenu")
	float MouseLeaveGraceTime = 0.3f;

	/**
	 * Whether to close the menu when the user clicks anywhere outside it.
	 * Implemented via a Slate input pre-processor registered for this menu's
	 * lifetime. The click itself is NOT consumed — clicks fall through to the
	 * world so the player can right-click a new target without an extra step.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|UI|ContextMenu")
	bool bCloseOnClickOutside = true;

	/**
	 * Warp the mouse cursor just inside the menu's top-left when the menu
	 * opens. This guarantees a clean mouse-enter event so the mouse-leave
	 * grace timer behaves predictably — without it, the cursor stays where
	 * the player right-clicked (often outside or only partially over the
	 * menu) and the "leaving the menu" close path can never fire because
	 * the menu was never "entered" from Slate's point of view.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|UI|ContextMenu")
	bool bWarpCursorOnOpen = true;

	/**
	 * How many pixels to inset the warped cursor from the menu's top-left
	 * corner. A small positive value (5px default) keeps us solidly inside
	 * the widget so float rounding can't put us back outside the bounds.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|UI|ContextMenu", meta=(ClampMin="0"))
	float CursorWarpInsetPx = 5.0f;

	/** Whether mouse is currently over the widget. */
	bool bIsMouseOver = false;

	/** Timer for mouse leave grace period. */
	float MouseLeaveTimer = 0.0f;

	/** Time (seconds) the menu has been open. Used to ignore the opening click. */
	float TimeSinceConstruct = 0.0f;

	/** Active click-outside handler (only valid while the menu is constructed). */
	TSharedPtr<FMOContextMenuClickOutsideHandler> ClickOutsideHandler;

public:
	// ============================================================================
	// CLICK-OUTSIDE INTERNALS (called by FMOContextMenuClickOutsideHandler)
	// ============================================================================
	//
	// These are public because the Slate input pre-processor lives outside this
	// class and needs to call them on every click. Conceptually internal — UI
	// code shouldn't invoke them directly.

	/** Test whether a screen-space click position is inside this menu's geometry. */
	bool IsScreenPositionInsideMenu(const FVector2D& ScreenPosition) const;

	/** True once the menu has been displayed long enough that the next click is intentional. */
	bool IsReadyForClickOutsideClose() const;

	/** Called by the click-outside handler when a click lands outside the menu. */
	void HandleClickOutside();

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
