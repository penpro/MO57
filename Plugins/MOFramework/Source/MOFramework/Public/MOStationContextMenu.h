/**
 * =============================================================================
 * MOStationContextMenu.h - Crafting Station Interaction Menu
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Context menu for interacting with completed crafting stations (forge, kiln,
 * campfire, etc.). Provides quick actions without opening full inventory.
 *
 * OPTIONS:
 * - Open: Opens unified inventory with station inventory as secondary
 * - Craft: Opens crafting menu filtered to this station's recipes
 * - Light: Activates/ignites the station (fuel required)
 *
 * FUEL SYSTEM:
 * - Stations can require fuel (wood, coal) to operate
 * - Light button disabled if no fuel
 * - FuelTimeText shows remaining burn time
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] STATION FILTERING: Craft button should filter recipes to ones
 *   requiring this station type. Pass StationType to CraftingMenu.
 *
 * [2024-02] FUEL TIME UPDATE: NativeTick updates fuel display. Only tick
 *   when station is active to save performance.
 *
 * [2024-02] LIGHT STATE TOGGLE: Light button may need to toggle between
 *   "Light" and "Extinguish" based on IsStationActive().
 *
 * =============================================================================
 * RELATED FILES: MOCraftingStationActor.h, MOContextMenuBase.h, MOCraftingMenu.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOContextMenuBase.h"
#include "MOStationContextMenu.generated.h"

class AMOCraftingStationActor;
class UMOCommonButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOStationMenuRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOStationMenuOpenSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOStationMenuCraftSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOStationMenuLightSignature);
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOStationContextMenu : public UMOContextMenuBase
{
	GENERATED_BODY()

public:
	UMOStationContextMenu(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu for a specific crafting station.
	 * @param Station - The crafting station to configure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|UI")
	void InitializeForStation(AMOCraftingStationActor* Station);

	/**
	 * Refresh the display (fuel time, button states).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|UI")
	void RefreshDisplay();

	// ============================================================================
	// ACTIONS
	// ============================================================================

	/**
	 * Open the station inventory (unified inventory view).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|UI")
	void OpenInventory();

	/**
	 * Open the crafting menu filtered to this station type.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|UI")
	void OpenCraftingMenu();

	/**
	 * Activate/light the station.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|UI")
	void LightStation();

	// ============================================================================
	// STATE
	// ============================================================================

	/**
	 * Check if the station can be activated (has fuel or doesn't require it).
	 */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation|UI")
	bool CanLightStation() const;

	/**
	 * Check if the station is currently active.
	 */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation|UI")
	bool IsStationActive() const;

	/**
	 * Get fuel time remaining formatted as MM:SS.
	 */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation|UI")
	FText GetFuelTimeText() const;

	/**
	 * Get the station name for display.
	 */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation|UI")
	FText GetStationName() const;

	// SetPopupPosition is inherited from UMOContextMenuBase

	// Override to broadcast legacy OnRequestClose delegate
	virtual void RequestClose() override;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	UPROPERTY(BlueprintAssignable, Category="MO|CraftingStation|UI")
	FMOStationMenuRequestCloseSignature OnRequestClose;

	UPROPERTY(BlueprintAssignable, Category="MO|CraftingStation|UI")
	FMOStationMenuOpenSignature OnOpenClicked;

	UPROPERTY(BlueprintAssignable, Category="MO|CraftingStation|UI")
	FMOStationMenuCraftSignature OnCraftClicked;

	UPROPERTY(BlueprintAssignable, Category="MO|CraftingStation|UI")
	FMOStationMenuLightSignature OnLightClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// NativeOnKeyDown is inherited from UMOContextMenuBase

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	/** Station name display */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StationNameText;

	/** Fuel time remaining display */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FuelTimeText;

	/** Open inventory button */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> OpenButton;

	/** Craft button */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CraftButton;

	/** Light/Activate button */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> LightButton;

	// ============================================================================
	// BLUEPRINT EVENTS
	// ============================================================================

	/** Called when fuel time updates */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|CraftingStation|UI")
	void OnFuelTimeUpdated(float TimeRemaining, bool bIsActive);

	/** Called when button states change */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|CraftingStation|UI")
	void OnButtonStatesUpdated(bool bCanLight, bool bIsActive);

private:
	// ============================================================================
	// HANDLERS
	// ============================================================================

	UFUNCTION()
	void HandleOpenClicked();

	UFUNCTION()
	void HandleCraftClicked();

	UFUNCTION()
	void HandleLightClicked();

	// ============================================================================
	// STATE
	// ============================================================================

	UPROPERTY()
	TWeakObjectPtr<AMOCraftingStationActor> TargetStation;

	/** Update button enabled/disabled states */
	void UpdateButtonStates();
};
