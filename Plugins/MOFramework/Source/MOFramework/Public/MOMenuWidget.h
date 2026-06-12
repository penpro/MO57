/**
 * =============================================================================
 * MOMenuWidget.h - Base class for gameplay menu widgets
 * =============================================================================
 *
 * PURPOSE:
 * Provides standardized CommonUI integration for gameplay menus including:
 * - Input mode: ECommonInputMode::Menu (blocks all game input)
 * - Back action handling (Escape/Tab to close)
 * - Toggle key handling (I, C, B, K, O to switch menus)
 * - Focus management
 *
 * USAGE:
 * Gameplay menus (Inventory, Crafting, Building, Skills, Status) should
 * inherit from this class.
 *
 * For modal dialogs (confirmation, in-game menu), use UMOModalWidget instead.
 *
 * =============================================================================
 * INPUT HANDLING
 * =============================================================================
 *
 * Uses ECommonInputMode::Menu which blocks ALL game input (movement, look, etc.)
 * and shows the mouse cursor. This is the proper CommonUI approach.
 *
 * Toggle keys (I, C, B, K, O) are intercepted in NativeOnPreviewKeyDown and
 * forwarded to the UIManager for menu switching.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOUIDelegates.h"
#include "MOMenuWidget.generated.h"

/**
 * Base class for gameplay menu widgets (inventory, crafting, building, etc.).
 *
 * Uses ECommonInputMode::Menu to block game input while menu is open.
 * Toggle keys are handled in NativeOnPreviewKeyDown.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOMenuWidget : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOMenuWidget(const FObjectInitializer& ObjectInitializer);

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

protected:
	// =========================================================================
	// COMMONUI OVERRIDES
	// =========================================================================

	/**
	 * Configure input mode.
	 * Uses ECommonInputMode::Menu to block all game input and show cursor.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/**
	 * Close keys route through RequestClose so controller cleanup runs
	 * (modal background, reticle). The base default is a bare DeactivateWidget.
	 */
	virtual bool NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent) override;

	/** Called when widget is activated. */
	virtual void NativeOnActivated() override;

	/** Called when widget is deactivated. */
	virtual void NativeOnDeactivated() override;
};
