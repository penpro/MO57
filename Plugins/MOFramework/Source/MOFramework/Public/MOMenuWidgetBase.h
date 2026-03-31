/**
 * =============================================================================
 * MOMenuWidgetBase.h - Base class for all menu widgets
 * =============================================================================
 *
 * PURPOSE:
 * Provides standardized CommonUI integration for menu widgets including:
 * - Proper input configuration (GetDesiredInputConfig)
 * - Back action handling (Escape/Tab to close)
 * - Focus management
 * - Standard close delegate
 *
 * USAGE:
 * All menu widgets (Inventory, Crafting, Building, etc.) should inherit from
 * this class instead of directly from UCommonActivatableWidget.
 *
 * =============================================================================
 * KEY FEATURES
 * =============================================================================
 *
 * INPUT HANDLING:
 * - Uses CommonUI's GetDesiredInputConfig() instead of SetInputMode()
 * - bIsBackHandler = true: Escape key triggers close
 * - Tab key also triggers close via back action
 * - Toggle keys (I, C, B) handled at PlayerController level
 *
 * FOCUS:
 * - Override GetDesiredFocusTarget() to specify initial focus
 * - bAutoRestoreFocus = true: Restores previous focus on reactivation
 *
 * CLOSE BEHAVIOR:
 * - OnRequestClose delegate broadcast when menu wants to close
 * - Back action (Escape/Tab) triggers OnRequestClose
 * - Controllers listen to OnRequestClose to handle actual closure
 *
 * =============================================================================
 * KNOWN PITFALLS
 * =============================================================================
 *
 * [2026-03] FOCUS LOST TO CHILDREN: If you click a button, focus moves to that
 *   button. This is expected. Key events (Tab/Escape) still work because we use
 *   bIsBackHandler which routes through CommonUI's action system, not NativeOnKeyDown.
 *
 * [2026-03] DON'T USE SetInputMode: The base class handles input mode via
 *   GetDesiredInputConfig(). Using SetInputMode() will conflict.
 *
 * [2026-03] TOGGLE KEYS: Don't handle toggle keys (I, C, B, etc.) in widgets.
 *   Let the PlayerController handle these via Enhanced Input.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "MOUIDelegates.h"
#include "MOMenuWidgetBase.generated.h"

/**
 * Base class for all menu widgets providing standardized CommonUI integration.
 *
 * Handles input mode, focus, and close behavior automatically.
 * Subclasses should NOT override NativeOnKeyDown for close handling.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOMenuWidgetBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOMenuWidgetBase(const FObjectInitializer& ObjectInitializer);

	// =========================================================================
	// DELEGATES
	// =========================================================================

	/** Broadcast when the menu wants to close (Escape, Tab, or explicit close). */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Menu")
	FMOUIRequestClose OnRequestClose;

	// =========================================================================
	// PUBLIC API
	// =========================================================================

	/** Request this menu to close. Broadcasts OnRequestClose. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Menu")
	virtual void RequestClose();

	/** Check if this menu blocks game input (default: true for menus). */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Menu")
	bool DoesBlockGameInput() const { return bBlocksGameInput; }

protected:
	// =========================================================================
	// COMMONUI OVERRIDES
	// =========================================================================

	/** Configure input mode for this widget. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** Handle back action (Escape key). Returns true if handled. */
	virtual bool NativeOnHandleBackAction() override;

	/** Get the widget that should receive initial focus. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Called when widget is activated. */
	virtual void NativeOnActivated() override;

	/** Called when widget is deactivated. */
	virtual void NativeOnDeactivated() override;

	// =========================================================================
	// CONFIGURATION
	// =========================================================================

	/** If true, this menu uses Menu input mode (game doesn't receive input). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MO|UI|Menu")
	bool bBlocksGameInput = true;

	/** If true, the mouse cursor is shown when this menu is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MO|UI|Menu")
	bool bShowMouseCursor = true;

	/** If true, movement input is ignored while this menu is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MO|UI|Menu")
	bool bIgnoreMoveInput = true;

	/** If true, look input is ignored while this menu is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MO|UI|Menu")
	bool bIgnoreLookInput = true;

	/**
	 * Widget to focus when this menu opens.
	 * If null, uses default CommonUI focus behavior.
	 * Set this in NativeConstruct or Blueprint to control initial focus.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "MO|UI|Menu")
	TObjectPtr<UWidget> DefaultFocusWidget;

private:
	/** Apply input state changes when menu activates. */
	void ApplyMenuInputState(bool bActivating);
};
