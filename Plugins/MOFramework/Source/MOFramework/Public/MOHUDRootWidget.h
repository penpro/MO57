/**
 * =============================================================================
 * MOHUDRootWidget.h - Composite HUD root container
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Single always-visible widget that owns the layout for ALL gameplay HUD
 * elements (thermal comfort, quest tracker, reticle, mode indicator, tool
 * hint, FPS counter, future compass/hunger/etc).
 *
 * Pushed once to MOPrimaryGameLayout::Layer_HUD at PlayerController BeginPlay
 * and never popped — passive container, no input claim, no focus. Designer
 * lays out children inside WBP_HUDRoot's canvas; sub-widgets bind their own
 * data sources (vitals component, quest subsystem, etc.) so the root stays
 * dumb.
 *
 * RELATIONSHIP TO PROCEDURAL HUD WIDGETS (legacy):
 * Existing widgets (Reticle, ModeIndicator, ToolHint, QuestHUD, TutorialHint,
 * FPSCounter) currently AddToViewport individually from UMOUIManagerComponent
 * and UMOQuestUIController. They continue to work alongside this HUD root
 * during the migration. Phase 2 (separate task) moves them into this root.
 *
 * USAGE (Blueprint):
 *   1. Create WBP_HUDRoot based on this class.
 *   2. Lay out anchor containers on the canvas (UCanvasPanel + UVerticalBox
 *      stacks for TopLeft, TopRight, BottomLeft, BottomRight, etc.)
 *   3. Drop child widgets (WBP_ThermalComfortIndicator, etc.) into the
 *      appropriate containers.
 *   4. Assign WBP_HUDRoot to HUDRootClass on AMOPlayerController (or
 *      configure default in BP_PlayerController class defaults).
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] PASSIVE WIDGET: GetDesiredInputConfig returns empty TOptional so
 *   CommonUI defers to widgets on layers ABOVE this one. Without this, the
 *   HUD would force Game-mode input even when menus are open.
 *
 * [2026-05] DO NOT POP: This widget is meant to live for the entire gameplay
 *   session. Don't call DeactivateWidget on it. If you need to hide it,
 *   toggle Visibility on the root.
 *
 * =============================================================================
 * RELATED FILES: MOPrimaryGameLayout.h, MOUIManagerComponent.h,
 *                MOStatusEffectStripWidget.h, MOWindDirectionWidget.h
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOHUDRootWidget.generated.h"

class UMOStatusEffectStripWidget;
class UMOWindDirectionWidget;

/**
 * Composite HUD root. Lives on Layer_HUD for the entire gameplay session.
 * Children laid out in the WBP designer canvas.
 */
UCLASS()
class MOFRAMEWORK_API UMOHUDRootWidget : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOHUDRootWidget(const FObjectInitializer& ObjectInitializer);

	// =========================================================================
	// PASSIVE HUD — no input claim
	// =========================================================================

	/**
	 * Return empty TOptional so this widget doesn't force a Game-mode input
	 * config. CommonUI then deference to whatever widget sits on a higher
	 * layer (menu, modal). Without this, opening a menu while the HUD is
	 * active gets fighting input modes.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	/**
	 * Diagnostic: log which BindWidgetOptional slots resolved. Lets us tell
	 * "WBP_HUDRoot is on screen but child widget isn't placed" apart from
	 * "child widget is placed but its own NativeConstruct silently failed."
	 */
	virtual void NativeOnInitialized() override;

	// =========================================================================
	// CHILD SLOTS (BindWidgetOptional — fill what your WBP supports)
	// =========================================================================
	//
	// Each is optional so the designer can include or omit any element in the
	// WBP without breaking compile-time binding. C++ uses these to address
	// children by name if needed (show/hide, dynamic data injection).
	//
	// Adding more slots: declare a new BindWidgetOptional below, drag the
	// matching widget into the WBP designer with the SAME name + "Is Variable"
	// checked. No further C++ wiring.

	// NOTE: Removed dedicated UMOThermalComfortWidget — thermal is now a
	// moodle in the StatusStrip (see UMOCharacterUIController::RefreshThermalMoodle).
	// Comfortable removes the moodle; Cold/Hot/VeryCold/VeryHot push it
	// with severity ratcheting. Designers can remove the old
	// WBP_ThermalComfortComponent placement from WBP_HUDRoot when ready.

	/**
	 * Generic moodle strip (wet, bleeding, hungry, well-fed, buffs, etc.).
	 * Sources push moodles via AddOrUpdateMoodle; strip auto-collapses when
	 * empty. Diagnostic accessor for cheats / debug overlays.
	 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOStatusEffectStripWidget> StatusStrip;

	/**
	 * Wind direction indicator (relative to player facing). Auto-hides when
	 * WindIntensity is below the widget's HideBelowIntensity threshold.
	 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOWindDirectionWidget> WindIndicator;

public:
	/** Accessor for sources/cheats that need to push moodles. May be null if WBP doesn't include the strip. */
	UFUNCTION(BlueprintPure, Category="MO|HUD")
	UMOStatusEffectStripWidget* GetStatusStrip() const { return StatusStrip; }

	/** Accessor for the wind indicator. May be null if WBP doesn't include it. */
	UFUNCTION(BlueprintPure, Category="MO|HUD")
	UMOWindDirectionWidget* GetWindIndicator() const { return WindIndicator; }

	// Future:
	// UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	// TObjectPtr<UMOHungerWidget> HungerIndicator;
};
