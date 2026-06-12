/**
 * =============================================================================
 * MOModalWidget.h - Base class for modal dialog widgets
 * =============================================================================
 *
 * PURPOSE:
 * Provides standardized CommonUI integration for modal dialogs including:
 * - Input mode: ECommonInputMode::Menu (blocks ALL game input)
 * - bIsModal = true (consumes all input, prevents cascading)
 * - Back action handling (Escape to close)
 *
 * USAGE:
 * Modal dialogs (InGameMenu, ConfirmationDialog) should inherit from this class.
 *
 * For gameplay menus (inventory, crafting, etc.), use UMOMenuWidget instead.
 *
 * =============================================================================
 * INPUT HANDLING
 * =============================================================================
 *
 * Uses ECommonInputMode::Menu which sends input ONLY to UI.
 * Combined with bIsModal = true, this completely blocks all game input
 * including toggle keys (I, C, B).
 *
 * This is intentional for modals - you don't want to accidentally open
 * inventory while a confirmation dialog is showing.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOUIDelegates.h"
#include "MOModalWidget.generated.h"

/**
 * Base class for modal dialog widgets (in-game menu, confirmation dialogs).
 *
 * Uses ECommonInputMode::Menu + bIsModal=true to completely block all
 * game input while the modal is active.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOModalWidget : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOModalWidget(const FObjectInitializer& ObjectInitializer);

	// =========================================================================
	// DELEGATES
	// =========================================================================

	/** Broadcast when the modal wants to close. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Modal")
	FMOUIRequestClose OnRequestClose;

	// =========================================================================
	// PUBLIC API
	// =========================================================================

	/** Request this modal to close. Broadcasts OnRequestClose. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Modal")
	virtual void RequestClose();

protected:
	// =========================================================================
	// COMMONUI OVERRIDES
	// =========================================================================

	/**
	 * Configure input mode.
	 * Uses ECommonInputMode::Menu to completely block game input.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/**
	 * Close keys route through RequestClose so the controller's cleanup path
	 * runs (modal background, reticle, etc). The base default is a bare
	 * DeactivateWidget that skips cleanup.
	 */
	virtual bool NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent) override;

	/** Called when widget is activated. */
	virtual void NativeOnActivated() override;

	/** Called when widget is deactivated. */
	virtual void NativeOnDeactivated() override;

	// NOTE: Input mode (cursor, move/look blocking) is handled by CommonUI
	// via GetDesiredInputConfig() + bIsModal - no manual management needed.
};
