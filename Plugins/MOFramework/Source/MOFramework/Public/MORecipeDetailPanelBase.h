/**
 * =============================================================================
 * MORecipeDetailPanelBase.h - Base Recipe Detail Panel Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base class for recipe detail panels (crafting and building).
 * Provides common UI elements and logic for displaying recipe name, description,
 * icon, skill requirements, time, ingredients, and outputs with amount controls.
 *
 * SUBCLASS INTERFACE (must override):
 * - GetDisplayedRecipe() - Look up recipe from DisplayedRecipeId
 * - CanPerformAction() - Check if action is valid
 * - GetMaxPerformableAmount() - Calculate max craftable/buildable
 * - PopulateRequirementsContainer() - Add ingredient/part widgets
 * - PopulateOutputsContainer() - Add output widgets
 * - HandleActionButtonClicked() - Execute craft/build action
 *
 * WIDGET BINDINGS:
 * RecipeNameText, RecipeDescriptionText, RecipeIcon, IngredientsContainer,
 * OutputsContainer, SkillRequirementText, CraftTimeText, CraftButton,
 * CraftMaxButton, CraftAmountSpinBox, CraftAmountSlider, CraftAmountText
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WIDGET OPTIONAL: All widget bindings are BindWidgetOptional.
 *   Always null-check before use.
 *
 * [2024-02] WEAK POINTERS: InventoryComponent, SkillsComponent, DiscoveryComponent
 *   are weak pointers. Validate before use in handlers.
 *
 * [2024-02] DYNAMIC WIDGETS: IngredientTextWidgets and OutputTextWidgets are
 *   dynamically created. ClearTextWidgets() removes them from container too.
 *
 * =============================================================================
 * RELATED FILES: MORecipeDetailPanel.h (crafting), MOBuildDetailPanel.h (building)
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MORecipeDetailPanelBase.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;
class UMORecipeDiscoveryComponent;
class UTextBlock;
class UImage;
class UPanelWidget;
class UMOCommonButton;
class USpinBox;
class USlider;
struct FMORecipeDefinitionRow;

/**
 * Base class for recipe detail panels (crafting and building).
 * Provides common functionality for displaying recipe information.
 *
 * Subclasses should override:
 * - GetRecipe() - Return the displayed recipe definition
 * - CanPerformAction() - Check if action can be performed
 * - GetMaxPerformableAmount() - Get max amount based on resources
 * - OnActionButtonClicked() - Handle the main action button
 * - PopulateRequirementsContainer() - Fill requirements list (ingredients/build parts)
 * - PopulateOutputsContainer() - Fill outputs list
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMORecipeDetailPanelBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMORecipeDetailPanelBase(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the panel with player components.
	 * @param InInventory Inventory component for resource checking
	 * @param InSkills Skills component for level requirements
	 * @param InDiscovery Recipe discovery component
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Recipe")
	void InitializePanel(
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills,
		UMORecipeDiscoveryComponent* InDiscovery
	);

	// ============================================================================
	// DISPLAY
	// ============================================================================

	/**
	 * Display details for a recipe.
	 * @param RecipeId Recipe to display (NAME_None to clear)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Recipe")
	virtual void DisplayRecipe(FName RecipeId);

	/**
	 * Clear the panel (show empty state).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Recipe")
	virtual void ClearDisplay();

	/**
	 * Refresh the display with current inventory state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Recipe")
	virtual void RefreshDisplay();

	/**
	 * Get the currently displayed recipe ID.
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Recipe")
	FName GetDisplayedRecipeId() const { return DisplayedRecipeId; }

	// ============================================================================
	// AMOUNT CONTROLS
	// ============================================================================

	/**
	 * Set the action amount (craft count, build count, etc).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Recipe")
	void SetActionAmount(int32 Amount);

	/**
	 * Get the current action amount.
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Recipe")
	int32 GetActionAmount() const { return ActionAmount; }

	/**
	 * Get the maximum amount that can be performed with current resources.
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Recipe")
	virtual int32 GetMaxPerformableAmount() const { return 0; }

	/**
	 * Check if the current recipe's action can be performed.
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Recipe")
	virtual bool CanPerformAction() const { return false; }

	/** Player-facing reason the current action is unavailable. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Recipe")
	virtual FText GetActionUnavailableReason() const;

protected:
	virtual void NativeConstruct() override;

	// ============================================================================
	// SUBCLASS INTERFACE
	// ============================================================================

	/**
	 * Get the recipe being displayed.
	 * Subclasses should look up and return the recipe based on DisplayedRecipeId.
	 */
	virtual const FMORecipeDefinitionRow* GetDisplayedRecipe() const;

	/**
	 * Called when displaying a recipe. Subclasses populate requirements/outputs.
	 * Base implementation handles common fields (name, description, icon, skill, time).
	 */
	virtual void OnDisplayRecipe(const FMORecipeDefinitionRow* Recipe);

	/**
	 * Populate the requirements/ingredients container.
	 * Override in subclasses to add ingredient or build part widgets.
	 */
	virtual void PopulateRequirementsContainer();

	/**
	 * Populate the outputs container.
	 * Override in subclasses to add output widgets.
	 */
	virtual void PopulateOutputsContainer();

	/**
	 * Called when the action button is clicked.
	 * Override to implement craft/build logic.
	 */
	UFUNCTION()
	virtual void HandleActionButtonClicked();

	/**
	 * Called when the max button is clicked.
	 */
	UFUNCTION()
	virtual void HandleMaxButtonClicked();

	/**
	 * Called when amount controls change.
	 */
	UFUNCTION()
	void HandleAmountChanged(float Value);

	/**
	 * Update the action button enabled state.
	 */
	virtual void UpdateActionButtonState();

	/**
	 * Get the time value for this recipe (CraftTime or TotalBuildTime).
	 * Override if needed.
	 */
	virtual float GetRecipeTime(const FMORecipeDefinitionRow* Recipe) const;

	// ============================================================================
	// HELPER FUNCTIONS
	// ============================================================================

	/**
	 * Clear dynamically created text widgets from a container.
	 */
	void ClearTextWidgets(TArray<TObjectPtr<UTextBlock>>& WidgetArray);

	/**
	 * Add a text widget to a container and tracking array.
	 */
	void AddTextWidget(UPanelWidget* Container, TArray<TObjectPtr<UTextBlock>>& WidgetArray, const FText& Text, bool bHasEnough = true);

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	/** Recipe name display. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	/** Recipe description display. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeDescriptionText;

	/** Recipe icon display. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

	/** Container for ingredients/requirements list. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> IngredientsContainer;

	/** Container for outputs list. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> OutputsContainer;

	/** Skill requirement text display. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillRequirementText;

	/** Craft/build time display. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftTimeText;

	/** Main action button (Craft/Build). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CraftButton;

	/** Optional explanation shown while the primary action is unavailable. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionUnavailableText;

	/** Action max button (Craft Max/Build Max). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CraftMaxButton;

	/** Amount spin box control. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<USpinBox> CraftAmountSpinBox;

	/** Amount slider control. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<USlider> CraftAmountSlider;

	/** Amount text display. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftAmountText;

	// ============================================================================
	// COMPONENT REFERENCES
	// ============================================================================

	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> DiscoveryComponent;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Currently displayed recipe ID. */
	FName DisplayedRecipeId = NAME_None;

	/** Current action amount. */
	int32 ActionAmount = 1;

	/** Dynamically created text widgets for ingredients. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> IngredientTextWidgets;

	/** Dynamically created text widgets for outputs. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> OutputTextWidgets;
};
