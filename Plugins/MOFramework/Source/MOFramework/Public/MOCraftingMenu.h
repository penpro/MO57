/**
 * =============================================================================
 * MOCraftingMenu.h - Main Crafting Interface Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Main crafting interface combining recipe list, detail panel, and crafting
 * queue. Used for both hand-crafting and crafting station interactions.
 *
 * COMPOSITION:
 * - RecipeListWidget: Scrollable list of available recipes
 * - RecipeDetailPanel: Selected recipe info, ingredients, craft button
 * - CraftingQueueWidget: Active/queued crafts with progress
 *
 * FILTERING:
 * Recipes filtered by:
 * - Knowledge (player must know recipe)
 * - Skills (minimum skill level requirements)
 * - Station (recipe must match current station or be hand-craftable)
 * - Discovery state (experimental recipes hidden until discovered)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] STATION CONTEXT: Menu can be opened standalone (hand crafting) or
 *   at a station. Check CurrentStation != nullptr to determine mode.
 *
 * [2024-02] LEGACY DELEGATES: FMOCraftingMenuRequestCloseSignature is legacy.
 *   New code should use FMOUIRequestClose from MOUIDelegates.h.
 *
 * [2024-02] QUEUE BINDING: Queue widget must bind to same pawn's queue
 *   component. On possession change, rebind or close menu.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MORecipeListWidget.h - Recipe list subwidget
 * - MORecipeDetailPanel.h - Recipe details subwidget
 * - MOCraftingQueueWidget.h - Queue display subwidget
 * - MOCraftingSubsystem.h - Crafting validation and execution
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOMenuWidgetBase.h"
#include "MORecipeDefinitionRow.h"
#include "MOUIDelegates.h"
#include "MOCraftingMenu.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;
class UMOCraftingSubsystem;
class UMORecipeListWidget;
class UMORecipeDetailPanel;
class UMOCraftingQueueWidget;
class UWidgetSwitcher;
class UMOCommonButton;
class UTextBlock;
class UCheckBox;
class AMOCraftingStationActor;

// Legacy delegate - prefer FMOUIRequestClose from MOUIDelegates.h for new code
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOCraftingMenuRequestCloseSignature);

/**
 * Main crafting menu widget.
 * See file header for composition and pitfalls.
 * Inherits from UMOMenuWidgetBase for standardized CommonUI input handling.
 * - RecipeList (UMORecipeListWidget)
 * - DetailPanel (UMORecipeDetailPanel)
 * - QueueWidget (UMOCraftingQueueWidget) [optional]
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCraftingMenu : public UMOMenuWidgetBase
{
	GENERATED_BODY()

public:
	UMOCraftingMenu(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	/**
	 * Initialize the crafting menu with required components.
	 * @param InInventory Inventory component for ingredient checking
	 * @param InSkills Skills component for level requirements
	 * @param InKnowledge Knowledge component for recipe visibility
	 * @param InCraftingQueue Crafting queue component (optional)
	 * @param InDiscovery Recipe discovery component (optional)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void InitializeMenu(
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills,
		UMOKnowledgeComponent* InKnowledge,
		UMOCraftingQueueComponent* InCraftingQueue = nullptr,
		UMORecipeDiscoveryComponent* InDiscovery = nullptr
	);

	/**
	 * Set the crafting station type to filter recipes.
	 * @param InStation Station type (None = show all hand-craftable recipes)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetCraftingStation(EMOCraftingStation InStation);

	/**
	 * Set the active station actor for fuel time display.
	 * @param InStation The crafting station actor (can be null for hand crafting)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetActiveStationActor(AMOCraftingStationActor* InStation);

	/**
	 * Get the current station display name.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FText GetStationDisplayName() const;

	// --- Recipe Management ---

	/** Refresh the recipe list from the crafting subsystem. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void RefreshRecipeList();

	/** Select a recipe to show in the detail panel. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SelectRecipe(FName RecipeId);

	/** Get the currently selected recipe ID. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FName GetSelectedRecipeId() const;

	// --- Crafting ---

	/**
	 * Attempt to craft the selected recipe.
	 * @param Count Number of times to craft (default 1)
	 * @return True if craft was enqueued successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	bool CraftSelectedRecipe(int32 Count = 1);

	// --- Filtering ---

	/** Set the category filter for recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetCategoryFilter(FName Category);

	/** Clear the category filter to show all recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void ClearCategoryFilter();

	/** Set whether to show only craftable recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetShowOnlyCraftable(bool bOnlyCraftable);

	/** Set whether to show all known recipes (ignores station filter). */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetShowAllKnown(bool bShowAll);

	/** Get whether showing all known recipes. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	bool IsShowingAllKnown() const { return bShowAllKnownRecipes; }

	// --- Delegates ---

	/** @deprecated Use OnRequestClose (from base class) instead. Broadcast for legacy code. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMOCraftingMenuRequestCloseSignature OnLegacyRequestClose;

	// Override RequestClose to also broadcast legacy delegate
	virtual void RequestClose() override;

	// --- Configuration ---

	/** If true, automatically refresh when inventory changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	bool bAutoRefreshOnInventoryChange = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Called when a recipe is selected in the list. */
	UFUNCTION()
	void HandleRecipeSelected(FName RecipeId);

	/** Called when a refresh removes the selected recipe. */
	UFUNCTION()
	void HandleRecipeSelectionCleared();

	/** Called when the craft button is pressed. */
	UFUNCTION()
	void HandleCraftRequested(FName RecipeId, int32 Count);

	/** Called when inventory changes. */
	UFUNCTION()
	void HandleInventoryChanged();

	/** Called when close button is clicked. */
	UFUNCTION()
	void HandleCloseClicked();

	/** Called when "Show All Known" checkbox state changes. */
	UFUNCTION()
	void HandleShowAllKnownChanged(bool bIsChecked);

	/** Update the station display (name + fuel time). */
	void UpdateStationDisplay();

	// --- Widget Bindings ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMORecipeListWidget> RecipeList;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMORecipeDetailPanel> DetailPanel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCraftingQueueWidget> QueueWidget;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

	/** Station name display (e.g., "Campfire"). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StationNameText;

	/** Fuel time remaining display (e.g., "5:32 remaining"). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FuelTimeText;

	/** "Show All Known" checkbox to ignore station filter. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> ShowAllKnownCheckbox;

private:
	// Cached component references
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOCraftingQueueComponent> CraftingQueueComponent;

	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> DiscoveryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOCraftingSubsystem> CraftingSubsystem;

	// Current state
	EMOCraftingStation CurrentStation = EMOCraftingStation::None;
	FName CategoryFilter = NAME_None;
	bool bShowOnlyCraftable = false;
	bool bShowAllKnownRecipes = false;

	/** Active crafting station actor for fuel time display. */
	UPROPERTY()
	TWeakObjectPtr<AMOCraftingStationActor> ActiveStationActor;
};
