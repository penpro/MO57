/**
 * =============================================================================
 * MOActivatableWidget.h - Base class for all CommonUI activatable widgets
 * =============================================================================
 *
 * PURPOSE:
 * Provides the foundation for all activatable widgets in the UI system.
 * This is the common base for both gameplay menus (UMOMenuWidget) and
 * modal dialogs (UMOModalWidget).
 *
 * FEATURES:
 * - FObjectInitializer constructor pattern (required for CommonUI)
 * - bAutoRestoreFocus enabled by default
 * - Lifecycle logging for debugging
 * - NativeGetDesiredFocusTarget hook for subclasses
 *
 * USAGE:
 * Don't inherit from this directly. Use:
 * - UMOMenuWidget for gameplay menus (inventory, crafting, building, etc.)
 * - UMOModalWidget for modal dialogs (confirmation, in-game menu)
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "Input/UIActionBindingHandle.h"
#include "MOActivatableWidget.generated.h"

class UInputAction;

/**
 * Abstract base class for all activatable widgets in MOFramework.
 *
 * Provides common functionality:
 * - FObjectInitializer constructor pattern
 * - Auto-restore focus when reactivated
 * - Lifecycle logging for debugging
 * - Focus target management
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// IMPORTANT: FObjectInitializer pattern required for CommonUI widgets
	UMOActivatableWidget(const FObjectInitializer& ObjectInitializer);

protected:
	// =========================================================================
	// COMMONUI OVERRIDES
	// =========================================================================

	/** Configure input mode for this widget. Subclasses should override. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** Get the widget that should receive initial focus. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * Universal Tab handler — closes the widget on Tab regardless of subclass.
	 * Used to live in UMOMenuWidget / UMOModalWidget, but that meant any activatable
	 * not extending those (e.g. UMOQuestLogPanel) silently couldn't be closed with
	 * Tab. Placing it on the base means every activatable shares the behavior.
	 *
	 * Tab → DeactivateWidget. Escape is still handled separately via
	 * NativeOnHandleBackAction (CommonUI's back action chain).
	 */
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Called when widget is activated. */
	virtual void NativeOnActivated() override;

	/** Called when widget is deactivated. */
	virtual void NativeOnDeactivated() override;

	/**
	 * Absorbs clicks that land on this widget's bounds but weren't consumed by any
	 * child. This stops the click from falling through to the viewport — which would
	 * otherwise trigger the focus listener's outside-click logic for a click that's
	 * actually inside this widget. This is purely about isolation; closing on
	 * outside click is handled in one place: UMOGameUIManagerSubsystem::HandleFocusChanging.
	 * Gated on input mode: UI surfaces absorb, Game-mode passive overlays let clicks pass.
	 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// =========================================================================
	// FOCUS CONFIGURATION
	// =========================================================================

	/**
	 * Widget to focus when this widget activates.
	 * If null, uses default CommonUI focus behavior.
	 * Subclasses should set this in NativeConstruct.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "MO|UI")
	TObjectPtr<UWidget> DefaultFocusWidget;

public:
	/**
	 * If true, the GameUIManagerSubsystem closes this widget when the user clicks
	 * outside its bounds (focus drifts to the viewport via mouse).
	 * Default: true (menus, panels). Modals/confirmations override to false.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MO|UI")
	bool bClosesOnOutsideClick = true;

	/**
	 * The InputAction that closes this widget while it is active. Set this in the
	 * widget Blueprint defaults to the same IA that opens it (e.g. IA_Inventory).
	 *
	 * On NativeOnActivated, the widget registers a CommonUI action binding via
	 * RegisterUIActionBinding. CommonUI's action router fires it whenever a key
	 * mapped to the action is pressed — regardless of Menu input mode blocking
	 * regular Enhanced Input. This is the supported CommonUI path; replaces the
	 * earlier hand-rolled TryToggleMenuByKey + PreviewKeyDown dispatch.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MO|UI")
	TObjectPtr<UInputAction> CloseAction;

public:
	/**
	 * Claim Slate keyboard focus for this widget's desired target, walking
	 * the chain down through any nested UMOActivatableWidget desired-focus
	 * targets until we find a leaf (a focusable concrete widget like a
	 * button or text input).
	 *
	 * Used when another widget deactivates above us and we should regain
	 * focus — e.g. a modal closing on top of a still-active menu. Without
	 * the chain walk, NativeGetDesiredFocusTarget might return another
	 * activatable widget (e.g. a sub-panel inside a switcher) that doesn't
	 * itself accept keyboard focus, leaving focus stranded.
	 *
	 * Safe to call any time after NativeConstruct.
	 */
	void ClaimFocusForReactivation();

private:
	/** CommonUI binding handle for CloseAction. Created in NativeOnActivated. */
	FUIActionBindingHandle CloseActionBinding;

	/** Bound to CloseActionBinding — calls DeactivateWidget. */
	void HandleCloseActionTriggered();
};
