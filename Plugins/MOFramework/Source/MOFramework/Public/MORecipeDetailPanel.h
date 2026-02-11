#pragma once

#include "CoreMinimal.h"
#include "MORecipeDetailPanelBase.h"
#include "MORecipeDefinitionRow.h"
#include "MOUIDelegates.h"
#include "MORecipeDetailPanel.generated.h"

class UTextBlock;

// Legacy delegate - prefer FMOUICraftRequest from MOUIDelegates.h for new code
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
 * Panel that displays detailed information about a selected crafting recipe.
 *
 * Shows recipe name, description, ingredients with have/need counts,
 * outputs, skill requirements, station requirements, and craft button.
 *
 * Inherits common functionality from UMORecipeDetailPanelBase.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMORecipeDetailPanel : public UMORecipeDetailPanelBase
{
	GENERATED_BODY()

public:
	UMORecipeDetailPanel(const FObjectInitializer& ObjectInitializer);

	// --- Delegates ---

	/** Broadcast when the craft button is clicked. Uses standard delegate signature. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMOUICraftRequest OnCraftAction;

	/** @deprecated Use OnCraftAction instead. Broadcast for backward compatibility. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMOCraftRequestedSignature OnCraftRequested;

	// --- Legacy API (wrapper functions for backward compatibility) ---

	/** Set the craft amount. Wraps SetActionAmount. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetCraftAmount(int32 Amount) { SetActionAmount(Amount); }

	/** Get the current craft amount. Wraps GetActionAmount. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	int32 GetCraftAmount() const { return GetActionAmount(); }

	/** Get the maximum amount that can be crafted with current resources. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	int32 GetMaxCraftableAmount() const { return GetMaxPerformableAmount(); }

	/** Check if the current recipe can be crafted. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	bool CanCraftCurrentRecipe() const { return CanPerformAction(); }

	// --- Data Access for Blueprints ---

	/** Get ingredient display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void GetIngredients(TArray<FMOIngredientDisplayData>& OutIngredients) const;

	/** Get output display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void GetOutputs(TArray<FMOOutputDisplayData>& OutOutputs) const;

	// --- Base Class Overrides ---

	virtual void RefreshDisplay() override;
	virtual int32 GetMaxPerformableAmount() const override;
	virtual bool CanPerformAction() const override;

protected:
	virtual void OnDisplayRecipe(const FMORecipeDefinitionRow* Recipe) override;
	virtual void PopulateRequirementsContainer() override;
	virtual void PopulateOutputsContainer() override;
	virtual void HandleActionButtonClicked() override;
	virtual float GetRecipeTime(const FMORecipeDefinitionRow* Recipe) const override;

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

	// --- Crafting-Specific Widget Bindings ---

	/** Shows required crafting station (e.g., "Requires: Campfire"). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RequiredStationText;

private:
	// Cached recipe data for Blueprint access
	TArray<FMOIngredientDisplayData> CachedIngredients;
	TArray<FMOOutputDisplayData> CachedOutputs;
};
