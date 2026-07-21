/**
 * =============================================================================
 * MOBuildWidget.h - Main Building Configuration Menu Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * DEPRECATED:
 * This parallel building workspace was never assigned to a live Widget
 * Blueprint. Use UMOBuildingMenu for recipe selection and
 * UMOGhostContextMenu for placed-building configuration/progress.
 *
 * WIDGET BINDINGS (required in Blueprint):
 * - RecipeList (UMOBuildingRecipeListWidget) - Scrollable recipe list
 * - DetailPanel (UMOBuildingDetailPanel) - Selected recipe details
 * - QueueWidget (UMOBuildingQueueWidget) - Optional construction queue
 * - CloseButton (UMOCommonButton) - Optional close button
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DUAL MODES: InitializeMenu() for recipe browsing, InitializeForBuilding()
 *   for ghost configuration. Don't mix - reinitialize when switching modes.
 *
 * [2024-02] AUTO REFRESH: If bAutoRefreshOnInventoryChange=true, binds to inventory
 *   changes. Remember to unbind in NativeDestruct.
 *
 * [2024-02] ABSTRACT CLASS: Must create Blueprint subclass with bound widgets.
 *   C++ class provides logic, Blueprint provides layout.
 *
 * =============================================================================
 * RELATED FILES: MOBuildingMenu.h, MOBuildingDetailPanel.h, MOBuildingUIController.h
 * LAST UPDATED: 2026-07-13 - Deprecated after live referencer verification
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MORecipeDefinitionRow.h"
#include "MOBuildingTypes.h"
#include "MOBuildWidget.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMORecipeDiscoveryComponent;
class UMOCraftingSubsystem;
class UMOBuildingRecipeListWidget;
class UMOBuildingDetailPanel;
class UMOBuildingQueueWidget;
class UWidgetSwitcher;
class UMOCommonButton;
class AMOBuildableActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBuildWidgetRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOBuildingSelectedSignature, FName, RecipeId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnStartBuildSignature);

/**
 * Main building menu widget that combines recipe list, detail panel, and queue.
 * Mirrors UMOCraftingMenu for crafting.
 *
 * Requires a Blueprint implementation with the following bound widgets:
 * - RecipeList (UMOBuildingRecipeListWidget)
 * - DetailPanel (UMOBuildingDetailPanel)
 * - QueueWidget (UMOBuildingQueueWidget) [optional]
 */
UCLASS(Abstract, Blueprintable, meta=(DeprecationMessage="Use UMOBuildingMenu and UMOGhostContextMenu"))
class UE_DEPRECATED(5.8, "Use UMOBuildingMenu and UMOGhostContextMenu") MOFRAMEWORK_API UMOBuildWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOBuildWidget(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	/**
	 * Initialize the building menu with required components.
	 * @param InInventory Inventory component for material checking
	 * @param InSkills Skills component for level requirements
	 * @param InKnowledge Knowledge component for recipe visibility
	 * @param InDiscovery Recipe discovery component (optional)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void InitializeMenu(
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills,
		UMOKnowledgeComponent* InKnowledge,
		UMORecipeDiscoveryComponent* InDiscovery = nullptr
	);

	/**
	 * Initialize the widget for a specific ghost building (after placement).
	 * This is for the build configuration popup when interacting with a placed ghost.
	 * @param Target - The buildable actor to configure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void InitializeForBuilding(AMOBuildableActor* Target);

	/**
	 * Get the current build options based on checkbox states.
	 * Used when starting construction on a ghost building.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FMOBuildProgress GetBuildOptions() const;

	// --- Recipe Management ---

	/** Refresh the recipe list from the crafting subsystem. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void RefreshRecipeList();

	/** Select a recipe to show in the detail panel. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SelectRecipe(FName RecipeId);

	/** Get the currently selected recipe ID. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FName GetSelectedRecipeId() const;

	// --- Building ---

	/**
	 * Attempt to start building the selected recipe.
	 * @param Count Number of times to build (default 1)
	 * @return True if build was started successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	bool BuildSelectedRecipe(int32 Count = 1);

	// --- Filtering ---

	/** Set the category filter for recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetCategoryFilter(FName Category);

	/** Clear the category filter to show all recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void ClearCategoryFilter();

	/** Set whether to show only buildable recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetShowOnlyBuildable(bool bOnlyBuildable);

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildWidgetRequestCloseSignature OnRequestClose;

	/** Broadcast when a building is selected for placement. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildingSelectedSignature OnBuildingSelected;

	/** Broadcast when the start build button is clicked (for ghost configuration). */
	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOOnStartBuildSignature OnStartBuild;

	// --- Configuration ---

	/** If true, automatically refresh when inventory changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	bool bAutoRefreshOnInventoryChange = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Called when a recipe is selected in the list. */
	UFUNCTION()
	void HandleRecipeSelected(FName RecipeId);

	/** Called when a refresh removes the selected recipe. */
	UFUNCTION()
	void HandleRecipeSelectionCleared();

	/** Called when the build button is pressed. */
	UFUNCTION()
	void HandleBuildRequested(FName RecipeId, int32 Count);

	/** Called when inventory changes. */
	UFUNCTION()
	void HandleInventoryChanged();

	/** Called when close button is clicked. */
	UFUNCTION()
	void HandleCloseClicked();

	// --- Widget Bindings (match MOCraftingMenu) ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOBuildingRecipeListWidget> RecipeList;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOBuildingDetailPanel> DetailPanel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOBuildingQueueWidget> QueueWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

private:
	// Cached component references
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> DiscoveryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOCraftingSubsystem> CraftingSubsystem;

	/** Target building when used for ghost configuration. */
	UPROPERTY()
	TWeakObjectPtr<AMOBuildableActor> TargetBuilding;

	// Current state
	FName CategoryFilter = NAME_None;
	bool bShowOnlyBuildable = false;
};
