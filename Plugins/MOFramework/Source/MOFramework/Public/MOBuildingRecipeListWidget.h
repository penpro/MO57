/**
 * =============================================================================
 * MOBuildingRecipeListWidget.h - Building Recipe List Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Scrollable list widget displaying available building recipes. Mirrors
 * UMORecipeListWidget for crafting. Supports filtering by category and
 * buildability, selection, and visual state updates.
 *
 * INHERITS FROM: UMOScrollListBase (provides container management, selection)
 *
 * DISPLAY DATA:
 * - FMOBuildRecipeListEntryData: Visual data per recipe entry
 *
 * COMPONENTS USED:
 * - UMOInventoryComponent: For material availability check
 * - UMOSkillsComponent: For skill requirements (future)
 * - UMORecipeDiscoveryComponent: For discovered recipe filtering
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ENTRY WIDGET CLASS: RecipeEntryWidgetClass must be set or no
 *   entries will spawn. Set in Blueprint details panel.
 *
 * [2024-02] WEAK POINTERS: InventoryComponent, SkillsComponent, DiscoveryComponent
 *   are weak pointers. Verify validity before use in CanBuildRecipe().
 *
 * [2024-02] FILTERING: CategoryFilter and bShowOnlyBuildable work independently.
 *   Both filters must pass for recipe to be shown.
 *
 * [2026-03] WIDGET BINDINGS: Uses RecipeScrollBox/RecipeContainer (not base's
 *   ContentScrollBox/ContentContainer) for backward compatibility. GetContainer()
 *   is overridden to use these.
 *
 * =============================================================================
 * RELATED FILES: MOScrollListBase.h, MOBuildingRecipeEntryWidget.h,
 *                MOBuildingDetailPanel.h, MOBuildingUIController.h
 * LAST UPDATED: 2026-03-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOScrollListBase.h"
#include "MORecipeDefinitionRow.h"
#include "MOBuildingRecipeListWidget.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;
class UMORecipeDiscoveryComponent;
class UMOBuildingRecipeEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOBuildRecipeSelectedSignature, FName, RecipeId);

/**
 * Visual data for a building recipe entry in the list.
 * Mirrors FMORecipeListEntryData for crafting.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildRecipeListEntryData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FName RecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FName Category = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** True if the player has all materials to build this. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	bool bCanBuild = false;

	/** True if this building recipe has been discovered. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	bool bIsDiscovered = true;

	/** True if currently selected. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	bool bIsSelected = false;
};

/**
 * Widget that displays a scrollable list of building recipes.
 * Inherits container/selection management from UMOScrollListBase.
 *
 * Requires a Blueprint implementation with:
 * - RecipeScrollBox (UScrollBox) or RecipeContainer (UVerticalBox)
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingRecipeListWidget : public UMOScrollListBase
{
	GENERATED_BODY()

public:
	UMOBuildingRecipeListWidget(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	/**
	 * Initialize the list with data sources.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void InitializeList(
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills,
		UMORecipeDiscoveryComponent* InDiscovery
	);

	// --- Recipe Population ---

	/**
	 * Populate the list with the given building recipe IDs.
	 * @param RecipeIds Array of recipe IDs to display
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void PopulateRecipes(const TArray<FName>& RecipeIds);

	/** Clear all recipes from the list. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void ClearRecipes();

	/** Refresh the visual state of all entries (buildability, selection). */
	virtual void RefreshEntryStates() override;

	// --- Selection ---

	/** Select a recipe by ID. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SelectRecipe(FName RecipeId);

	/** Get the currently selected recipe ID. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FName GetSelectedRecipeId() const { return SelectedRecipeId; }

	// --- Filtering ---

	/** Set the category filter. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetCategoryFilter(FName Category);

	/** Set whether to only show buildable recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetShowOnlyBuildable(bool bOnlyBuildable);

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildRecipeSelectedSignature OnRecipeSelected;

	// --- Configuration ---

	/** Widget class to use for recipe entries. Must be a subclass of UMOBuildingRecipeEntryWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	TSubclassOf<UMOBuildingRecipeEntryWidget> RecipeEntryWidgetClass;

protected:
	virtual void NativeConstruct() override;

	/** Override to use our specific widget bindings (RecipeScrollBox/RecipeContainer). */
	virtual UPanelWidget* GetContainer() const;

	/** Called when a building recipe entry is clicked. */
	UFUNCTION()
	void HandleBuildingEntryClicked(FName RecipeId);

	/** Build visual data for a recipe. */
	FMOBuildRecipeListEntryData BuildEntryData(FName RecipeId) const;

	/** Check if a building can be built with current resources. */
	bool CanBuildRecipe(FName RecipeId) const;

	// --- Widget Bindings (domain-specific, override base) ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UScrollBox> RecipeScrollBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> RecipeContainer;

private:
	// Cached component references
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> DiscoveryComponent;

	// Current building entries (domain-specific type, different from base's EntryWidgets)
	UPROPERTY()
	TArray<TObjectPtr<UMOBuildingRecipeEntryWidget>> BuildingEntryWidgets;

	// Current state
	TArray<FName> CurrentRecipeIds;
	FName SelectedRecipeId = NAME_None;
	FName CategoryFilter = NAME_None;
	bool bShowOnlyBuildable = false;
};
