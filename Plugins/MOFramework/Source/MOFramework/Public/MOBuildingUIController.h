/**
 * =============================================================================
 * MOBuildingUIController.h - Building Menu and Construction UI
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Specialized UI controller for building system interfaces. Manages building
 * menu for recipe selection, ghost context menus for placed ghosts, and
 * build widgets showing construction progress. Part of UIManager split.
 *
 * KEY RESPONSIBILITIES:
 * 1. Manage Building menu (open/close/toggle)
 * 2. Show ghost context menus for placed building ghosts
 * 3. Display build widget during construction
 * 4. Track current build target actor
 *
 * UI WIDGETS MANAGED:
 * - BuildingMenu: Recipe browser and selection interface
 * - GhostContextMenu: Actions for building ghost (build, cancel)
 * - BuildWidget: Construction progress display
 *
 * BUILDING FLOW:
 * Player opens Building menu -> Selects recipe -> Ghost spawned
 * -> Player places ghost -> ShowBuildWidget(Ghost)
 * -> Player clicks build -> HandleGhostContextMenuBuildStarted()
 * -> Construction progress -> Complete or cancelled
 *
 * CRITICAL PATTERNS:
 * 1. Building Menu:
 *    ToggleBuildingMenu() -> HandleBuildingSelected(RecipeId)
 *    -> Spawn ghost via MOBuildingSubsystem
 *
 * 2. Build Widget:
 *    ShowBuildWidget(Target) -> Track CurrentBuildTarget
 *    -> Display construction requirements and progress
 *    -> HideBuildWidget() on cancel or complete
 *
 * 3. Ghost Lifecycle:
 *    HandleGhostContextMenuBuildStarted() -> Start construction
 *    HandleGhostContextMenuCancelled() -> Remove ghost
 *
 * KNOWN PITFALLS:
 * 1. GHOST WEAK REFERENCE: CurrentBuildTarget is weak pointer.
 *    Ghost can be destroyed (cancelled) while widget is open.
 *
 * 2. RECIPE VALIDATION: Recipe may require materials player doesn't have.
 *    Ghost spawns anyway, but build button should be disabled.
 *
 * 3. GHOST CONTEXT POSITIONING: Context menu appears at ghost location.
 *    May need world-to-screen conversion for proper placement.
 *
 * RELATED FILES:
 * - MOUIControllerBase.h - Base class with shared utilities
 * - MOBuildingMenu.h - Recipe selection widget
 * - MOBuildWidget.h - Construction progress widget
 * - MOGhostContextMenu.h - Ghost action menu widget
 * - MOBuildingSubsystem.h - Building logic subsystem
 * - MOBuildableActor.h - Building ghost and complete actors
 *
 * TESTING CHECKLIST:
 * [ ] Building menu opens and shows recipes
 * [ ] Recipe selection spawns correct ghost
 * [ ] Build widget appears when approaching ghost
 * [ ] Build button starts construction
 * [ ] Cancel removes ghost properly
 * [ ] Construction progress displays correctly
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOBuildingUIController.generated.h"

class UMOBuildingMenu;
class UMOBuildWidget;
class UMOGhostContextMenu;
class AMOBuildableActor;

/**
 * Specialized UI controller for building-related UI.
 *
 * Handles:
 * - Building menu (toggle, open, close)
 * - Ghost context menu (for building ghost interaction)
 * - Build widget (construction progress)
 *
 * This controller is extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of the building UI subsystem.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOBuildingUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOBuildingUIController();

	// ==========================================================================
	// BUILDING MENU
	// ==========================================================================

	/** Toggle building menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void ToggleBuildingMenu();

	/** Open the building menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void OpenBuildingMenu();

	/** Close the building menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void CloseBuildingMenu();

	/** Check if building menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsBuildingMenuOpen() const;

	/** Get the building menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	UMOBuildingMenu* GetBuildingMenu() const;

	// ==========================================================================
	// GHOST CONTEXT MENU (BUILD WIDGET)
	// ==========================================================================

	/** Show the build widget for a ghost building. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void ShowBuildWidget(AMOBuildableActor* Target);

	/** Hide the build widget. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void HideBuildWidget();

	/** Check if build widget is open. */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsBuildWidgetOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Frame-based debounce to prevent double-toggle from ECommonInputMode::All */
	uint64 LastToggleFrame = 0;

	// --- Building Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOBuildingMenu> BuildingMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 BuildingMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOBuildingMenu> BuildingMenuWidget;

	UFUNCTION()
	void HandleBuildingMenuRequestClose();

	UFUNCTION()
	void HandleBuildingSelected(FName RecipeId);

	// --- Ghost Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOGhostContextMenu> GhostContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 GhostContextMenuZOrder = 60;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOGhostContextMenu> GhostContextMenuWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AMOBuildableActor> CurrentBuildTarget;

	UFUNCTION()
	void HandleGhostContextMenuRequestClose();

	UFUNCTION()
	void HandleGhostContextMenuBuildStarted();

	UFUNCTION()
	void HandleGhostContextMenuCancelled();
};
