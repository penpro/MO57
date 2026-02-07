#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MORecipeDefinitionRow.h"
#include "MORecipeDetailPanel.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;
class UMORecipeDiscoveryComponent;
class UMOCraftingSubsystem;
class UTextBlock;
class UImage;
class UVerticalBox;
class UPanelWidget;
class UButton;
class UMOCommonButton;
class USpinBox;
class USlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOCraftRequestedSignature, FName, RecipeId, int32, Count);

/**
 * Visual data for an ingredient requirement.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOIngredientDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FName ItemDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	int32 RequiredQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	int32 AvailableQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	bool bHasEnough = false;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * Visual data for a recipe output.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOOutputDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FName ItemDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	float Chance = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * Panel that displays detailed information about a selected recipe.
 *
 * Shows recipe name, description, ingredients with have/need counts,
 * outputs, skill requirements, and craft button.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMORecipeDetailPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UMORecipeDetailPanel(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void InitializePanel(
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills,
		UMORecipeDiscoveryComponent* InDiscovery
	);

	// --- Recipe Display ---

	/**
	 * Display details for a recipe.
	 * @param RecipeId Recipe to display (NAME_None to clear)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void DisplayRecipe(FName RecipeId);

	/** Clear the panel (show empty state). */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void ClearDisplay();

	/** Refresh the display with current inventory state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void RefreshDisplay();

	/** Get the currently displayed recipe ID. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FName GetDisplayedRecipeId() const { return DisplayedRecipeId; }

	// --- Craft Amount ---

	/** Set the craft amount. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetCraftAmount(int32 Amount);

	/** Get the current craft amount. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	int32 GetCraftAmount() const { return CraftAmount; }

	/** Get the maximum amount that can be crafted with current resources. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	int32 GetMaxCraftableAmount() const;

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMOCraftRequestedSignature OnCraftRequested;

	// --- Data Access for Blueprints ---

	/** Get ingredient display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void GetIngredients(TArray<FMOIngredientDisplayData>& OutIngredients) const;

	/** Get output display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void GetOutputs(TArray<FMOOutputDisplayData>& OutOutputs) const;

	/** Check if the current recipe can be crafted. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	bool CanCraftCurrentRecipe() const;

protected:
	virtual void NativeConstruct() override;

	/** Called when the craft button is clicked. */
	UFUNCTION()
	void HandleCraftButtonClicked();

	/** Called when the craft max button is clicked. */
	UFUNCTION()
	void HandleCraftMaxButtonClicked();

	/** Called when craft amount changes. */
	UFUNCTION()
	void HandleCraftAmountChanged(float Value);

	/** Update the craft button enabled state. */
	void UpdateCraftButtonState();

	/** Build ingredient display data. */
	FMOIngredientDisplayData BuildIngredientData(const FMORecipeIngredient& Ingredient) const;

	/** Build output display data. */
	FMOOutputDisplayData BuildOutputData(const FMORecipeOutput& Output) const;

	/** Blueprint event for custom display updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnRecipeDisplayed(FName RecipeId, const FText& DisplayName, const FText& Description);

	/** Blueprint event for ingredient list update. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnIngredientsUpdated(const TArray<FMOIngredientDisplayData>& Ingredients);

	/** Blueprint event for outputs list update. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnOutputsUpdated(const TArray<FMOOutputDisplayData>& Outputs);

	// --- Widget Bindings ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> IngredientsContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> OutputsContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillRequirementText;

	/** Shows required crafting station (e.g., "Requires: Campfire"). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RequiredStationText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftTimeText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CraftButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CraftMaxButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<USpinBox> CraftAmountSpinBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<USlider> CraftAmountSlider;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftAmountText;

private:
	// Cached component references
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> DiscoveryComponent;

	// Current state
	FName DisplayedRecipeId = NAME_None;
	int32 CraftAmount = 1;

	// Cached recipe data
	TArray<FMOIngredientDisplayData> CachedIngredients;
	TArray<FMOOutputDisplayData> CachedOutputs;

	// Dynamically created text widgets
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> IngredientTextWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> OutputTextWidgets;

	/** Populate ingredient container with text widgets. */
	void PopulateIngredientsContainer();

	/** Populate output container with text widgets. */
	void PopulateOutputsContainer();

	/** Create a simple text widget for ingredient/output display. */
	UTextBlock* CreateSimpleTextWidget(const FText& Text, bool bHasEnough = true);
};
