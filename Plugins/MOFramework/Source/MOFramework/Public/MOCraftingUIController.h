/**
 * =============================================================================
 * MOCraftingUIController.h - Crafting Menu and Harvest Operations UI
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Specialized UI controller for crafting system interfaces. Manages crafting
 * menu, station context menus, harvest context menus for PCG foliage, and
 * harvest progress widgets. Part of the UIManager split architecture.
 *
 * KEY RESPONSIBILITIES:
 * 1. Manage Crafting menu (open/close/toggle)
 * 2. Show station context menus for crafting stations
 * 3. Show harvest context menus for ISM/HISM targets (trees, rocks)
 * 4. Handle harvest operations with progress tracking
 *
 * UI WIDGETS MANAGED:
 * - CraftingMenu: Full crafting interface with recipe selection
 * - StationContextMenu: Quick actions for crafting stations
 * - KeepOnHarvestContextMenu: Actions for harvestable PCG instances
 * - HarvestProgressWidget: Circular progress during harvesting
 *
 * CONTEXT MENU TARGETS:
 * - StationContextMenu: AMOCraftingStationActor (campfire, forge, etc.)
 * - KeepOnHarvestContextMenu: FMOInteractionTarget (ISM/HISM instances)
 *
 * CRITICAL PATTERNS:
 * 1. Crafting Menu:
 *    ToggleCraftingMenu() -> Check if open -> Open or Close
 *    Uses modal background + UI input mode
 *
 * 2. Station Context:
 *    ShowStationContextMenu(Actor, WorldPos) -> Create widget at screen pos
 *    -> User clicks Open/Craft/Light -> Handler methods
 *
 * 3. Harvest Flow:
 *    ShowKeepOnHarvestContextMenu(Target) -> User selects recipe
 *    -> StartHarvestOperation(RecipeId) -> Show progress widget
 *    -> HandleHarvestCompleted() -> Award items, remove instance
 *
 * KNOWN PITFALLS:
 * 1. TARGET VALIDITY: FMOInteractionTarget can become invalid if ISM changes.
 *    Validate target before starting harvest operation.
 *
 * 2. HARVEST INTERRUPTION: Moving or being attacked cancels harvest.
 *    HandleHarvestCancelled() must clean up UI properly.
 *
 * 3. CONTEXT MENU POSITIONING: WorldPosition -> ScreenPosition conversion.
 *    Menu may appear offscreen if position is behind camera.
 *
 * 4. STATION REFERENCE: CurrentStationTarget is weak pointer.
 *    Station can be destroyed while context menu is open.
 *
 * RELATED FILES:
 * - MOUIControllerBase.h - Base class with shared utilities
 * - MOCraftingMenu.h - Full crafting interface widget
 * - MOStationContextMenu.h - Station quick-action widget
 * - MOKeepOnHarvestContextMenu.h - Harvest quick-action widget
 * - MOPCGInteractionSubsystem.h - Handles ISM/HISM harvest logic
 *
 * TESTING CHECKLIST:
 * [ ] Crafting menu toggles correctly
 * [ ] Station context menu appears at correct position
 * [ ] Harvest context menu shows available recipes
 * [ ] Harvest progress completes and awards items
 * [ ] Harvest cancellation cleans up UI
 * [ ] Station destruction hides context menu
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOInteractorComponent.h"
#include "MOCraftingUIController.generated.h"

class UMOCraftingMenu;
class UMOStationContextMenu;
class UMOKeepOnHarvestContextMenu;
class UMOHarvestProgressWidget;
class AMOCraftingStationActor;
struct FMOCraftResult;

/**
 * Specialized UI controller for crafting-related UI.
 *
 * Handles:
 * - Crafting menu (toggle, open, close)
 * - Station context menus (open, craft, light actions)
 * - Keep-on-harvest context menus (ISM/HISM targets)
 * - Harvest operations and progress
 *
 * This controller is extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of the crafting UI subsystem.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCraftingUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOCraftingUIController();

	// ==========================================================================
	// CRAFTING MENU
	// ==========================================================================

	/** Toggle crafting menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void ToggleCraftingMenu();

	/** Open the crafting menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void OpenCraftingMenu();

	/** Close the crafting menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void CloseCraftingMenu();

	/** Check if crafting menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting")
	bool IsCraftingMenuOpen() const;

	/** Get the crafting menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|Crafting")
	UMOCraftingMenu* GetCraftingMenu() const;

	// ==========================================================================
	// STATION CONTEXT MENU
	// ==========================================================================

	/** Show the station context menu for a crafting station. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Station")
	void ShowStationContextMenu(AActor* StationActor, FVector WorldPosition);

	/** Hide the station context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Station")
	void HideStationContextMenu();

	/** Check if station context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Station")
	bool IsStationContextMenuOpen() const;

	// ==========================================================================
	// KEEPONHARVEST CONTEXT MENU
	// ==========================================================================

	/** Show the keep-on-harvest context menu for an ISM/HISM target. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void ShowKeepOnHarvestContextMenu(const FMOInteractionTarget& Target);

	/** Hide the keep-on-harvest context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void HideKeepOnHarvestContextMenu();

	/** Check if keep-on-harvest context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Harvest")
	bool IsKeepOnHarvestContextMenuOpen() const;

	// ==========================================================================
	// HARVEST OPERATIONS
	// ==========================================================================

	/** Start a harvest operation on the current target. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void StartHarvestOperation(FName RecipeId);

	/** Cancel any active harvest operation. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void CancelHarvestOperation();

	/** Check if a harvest operation is in progress. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Harvest")
	bool IsHarvestInProgress() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- Crafting Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOCraftingMenu> CraftingMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 CraftingMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOCraftingMenu> CraftingMenuWidget;

	UFUNCTION()
	void HandleCraftingMenuRequestClose();

	// --- Station Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Station", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOStationContextMenu> StationContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Station", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 StationContextMenuZOrder = 60;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOStationContextMenu> StationContextMenuWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AMOCraftingStationActor> CurrentStationTarget;

	UFUNCTION()
	void HandleStationContextMenuRequestClose();

	UFUNCTION()
	void HandleStationContextMenuOpen();

	UFUNCTION()
	void HandleStationContextMenuCraft();

	UFUNCTION()
	void HandleStationContextMenuLight();

	// --- KeepOnHarvest Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOKeepOnHarvestContextMenu> KeepOnHarvestContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOHarvestProgressWidget> HarvestProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 KeepOnHarvestContextMenuZOrder = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 HarvestProgressZOrder = 200;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOKeepOnHarvestContextMenu> KeepOnHarvestContextMenuWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOHarvestProgressWidget> HarvestProgressWidget;

	/** The current interaction target for harvest operations. */
	FMOInteractionTarget CurrentHarvestTarget;

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuRequestClose();

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuInspectClicked();

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuHarvestClicked(FName RecipeId);

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuChopDownClicked();

	UFUNCTION()
	void HandleHarvestCompleted(bool bCompleted, const FMOCraftResult& Result);

	UFUNCTION()
	void HandleHarvestCancelled();
};
