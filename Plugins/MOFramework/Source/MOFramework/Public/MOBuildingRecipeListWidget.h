#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MORecipeDefinitionRow.h"
#include "MOBuildingRecipeListWidget.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;
class UMORecipeDiscoveryComponent;
class UMOBuildingRecipeEntryWidget;
class UScrollBox;
class UVerticalBox;

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
 * Mirrors UMORecipeListWidget for crafting.
 *
 * Requires a Blueprint implementation with:
 * - RecipeScrollBox (UScrollBox) or RecipeContainer (UVerticalBox)
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingRecipeListWidget : public UUserWidget
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
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void RefreshEntryStates();

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

	/** Called when a recipe entry is clicked. */
	UFUNCTION()
	void HandleEntryClicked(FName RecipeId);

	/** Build visual data for a recipe. */
	FMOBuildRecipeListEntryData BuildEntryData(FName RecipeId) const;

	/** Check if a building can be built with current resources. */
	bool CanBuildRecipe(FName RecipeId) const;

	// --- Widget Bindings ---

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

	// Current recipe entries
	UPROPERTY()
	TArray<TObjectPtr<UMOBuildingRecipeEntryWidget>> EntryWidgets;

	// Current state
	TArray<FName> CurrentRecipeIds;
	FName SelectedRecipeId = NAME_None;
	FName CategoryFilter = NAME_None;
	bool bShowOnlyBuildable = false;
};
