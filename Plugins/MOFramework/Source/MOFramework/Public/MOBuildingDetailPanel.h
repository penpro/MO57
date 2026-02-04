#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MORecipeDefinitionRow.h"
#include "MOBuildingTypes.h"
#include "MOBuildingDetailPanel.generated.h"

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
class UCheckBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOBuildRequestedSignature, FName, RecipeId, int32, Count);

/**
 * Visual data for a build part requirement.
 * Mirrors FMOIngredientDisplayData for crafting.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildPartDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FName ItemDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FName ActionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	int32 RequiredQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	int32 AvailableQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	bool bHasEnough = false;

	/** True if this is an action (e.g., Dig, Hammer) rather than an item. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	bool bIsAction = false;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * Visual data for building output (the completed building).
 * Mirrors FMOOutputDisplayData for crafting.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildOutputDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FName BuildableActorClass = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * Panel that displays detailed information about a selected building recipe.
 * Mirrors UMORecipeDetailPanel for crafting.
 *
 * Shows building name, description, build parts with have/need counts,
 * output (the building), skill requirements, and build button.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingDetailPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOBuildingDetailPanel(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void InitializePanel(
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills,
		UMORecipeDiscoveryComponent* InDiscovery
	);

	// --- Recipe Display ---

	/**
	 * Display details for a building recipe.
	 * @param RecipeId Recipe to display (NAME_None to clear)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void DisplayRecipe(FName RecipeId);

	/** Clear the panel (show empty state). */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void ClearDisplay();

	/** Refresh the display with current inventory state. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void RefreshDisplay();

	/** Get the currently displayed recipe ID. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FName GetDisplayedRecipeId() const { return DisplayedRecipeId; }

	// --- Build Amount ---

	/** Set the build amount. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetBuildAmount(int32 Amount);

	/** Get the current build amount. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	int32 GetBuildAmount() const { return BuildAmount; }

	/** Get the maximum amount that can be built with current resources. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	int32 GetMaxBuildableAmount() const;

	// --- Build Options ---

	/** Get the current material source options based on checkbox states. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FMOBuildProgress GetBuildOptions() const;

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildRequestedSignature OnBuildRequested;

	// --- Data Access for Blueprints ---

	/** Get build part display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void GetBuildParts(TArray<FMOBuildPartDisplayData>& OutBuildParts) const;

	/** Get output display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void GetOutputs(TArray<FMOBuildOutputDisplayData>& OutOutputs) const;

	/** Check if the current recipe can be built. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool CanBuildCurrentRecipe() const;

protected:
	virtual void NativeConstruct() override;

	/** Called when the build button is clicked. */
	UFUNCTION()
	void HandleBuildButtonClicked();

	/** Called when the build max button is clicked. */
	UFUNCTION()
	void HandleBuildMaxButtonClicked();

	/** Called when build amount changes. */
	UFUNCTION()
	void HandleBuildAmountChanged(float Value);

	/** Update the build button enabled state. */
	void UpdateBuildButtonState();

	/** Build part display data. */
	FMOBuildPartDisplayData BuildPartData(const FMOBuildPart& Part) const;

	/** Build output display data. */
	FMOBuildOutputDisplayData BuildOutputData(const FMORecipeDefinitionRow* Recipe) const;

	/** Blueprint event for custom display updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnRecipeDisplayed(FName RecipeId, const FText& DisplayName, const FText& Description);

	/** Blueprint event for build parts list update. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnBuildPartsUpdated(const TArray<FMOBuildPartDisplayData>& BuildParts);

	/** Blueprint event for outputs list update. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnOutputsUpdated(const TArray<FMOBuildOutputDisplayData>& Outputs);

	// --- Widget Bindings (match MORecipeDetailPanel) ---

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

	// --- Additional Build-Specific Bindings ---

	/** Checkbox for drawing materials from inventory. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> InventoryCheckbox;

	/** Checkbox for drawing materials from nearby containers. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> ContainersCheckbox;

	/** Checkbox for drawing materials from surrounding area. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> SurroundingCheckbox;

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
	int32 BuildAmount = 1;
	float GatherRange = 150.0f;

	// Cached recipe data
	TArray<FMOBuildPartDisplayData> CachedBuildParts;
	TArray<FMOBuildOutputDisplayData> CachedOutputs;

	// Dynamically created text widgets
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> IngredientTextWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> OutputTextWidgets;

	/** Populate ingredients container with text widgets. */
	void PopulateIngredientsContainer();

	/** Populate output container with text widgets. */
	void PopulateOutputsContainer();

	/** Create a simple text widget for ingredient/output display. */
	UTextBlock* CreateSimpleTextWidget(const FText& Text, bool bHasEnough = true);
};
