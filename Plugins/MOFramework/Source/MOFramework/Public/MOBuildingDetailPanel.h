/**
 * =============================================================================
 * MOBuildingDetailPanel.h - Building Recipe Detail UI Panel
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Panel that displays detailed information about a selected building recipe.
 * Mirrors UMORecipeDetailPanel for crafting. Shows building name, description,
 * required parts with have/need counts, material source checkboxes, and
 * build button.
 *
 * DISPLAY DATA STRUCTS:
 * - FMOBuildPartDisplayData: Part requirements with availability
 * - FMOBuildOutputDisplayData: The completed building info
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] CHECKBOX STATE: InventoryCheckbox, ContainersCheckbox, and
 *   SurroundingCheckbox control material sources. GetBuildOptions() reads
 *   current state - call after user interaction.
 *
 * [2024-02] DUAL DELEGATES: OnBuildAction (preferred) and OnBuildRequested
 *   (legacy) both fire. Use OnBuildAction for new code.
 *
 * [2024-02] GATHER RANGE: GatherRange (150cm default) determines nearby
 *   material search radius. Not exposed to Blueprint.
 *
 * =============================================================================
 * RELATED FILES: MORecipeDetailPanelBase.h, MOBuildingRecipeListWidget.h,
 *                MOBuildingUIController.h, MOBuildingTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MORecipeDetailPanelBase.h"
#include "MORecipeDefinitionRow.h"
#include "MOBuildingTypes.h"
#include "MOUIDelegates.h"
#include "MOBuildingDetailPanel.generated.h"

class UCheckBox;

// Legacy delegate - prefer FMOUICraftRequest from MOUIDelegates.h for new code
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
 *
 * Inherits common functionality from UMORecipeDetailPanelBase.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingDetailPanel : public UMORecipeDetailPanelBase
{
	GENERATED_BODY()

public:
	UMOBuildingDetailPanel(const FObjectInitializer& ObjectInitializer);

	// --- Delegates ---

	/** Broadcast when the build button is clicked. Uses standard delegate signature. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOUICraftRequest OnBuildAction;

	/** @deprecated Use OnBuildAction instead. Broadcast for backward compatibility. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildRequestedSignature OnBuildRequested;

	// --- Legacy API (wrapper functions for backward compatibility) ---

	/** Set the build amount. Wraps SetActionAmount. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetBuildAmount(int32 Amount) { SetActionAmount(Amount); }

	/** Get the current build amount. Wraps GetActionAmount. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	int32 GetBuildAmount() const { return GetActionAmount(); }

	/** Get the maximum amount that can be built with current resources. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	int32 GetMaxBuildableAmount() const { return GetMaxPerformableAmount(); }

	/** Check if the current recipe can be built. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool CanBuildCurrentRecipe() const { return CanPerformAction(); }

	// --- Build Options ---

	/** Get the current material source options based on checkbox states. */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FMOBuildProgress GetBuildOptions() const;

	// --- Data Access for Blueprints ---

	/** Get build part display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void GetBuildParts(TArray<FMOBuildPartDisplayData>& OutBuildParts) const;

	/** Get output display data for the current recipe. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void GetOutputs(TArray<FMOBuildOutputDisplayData>& OutOutputs) const;

	// --- Base Class Overrides ---

	virtual void RefreshDisplay() override;
	virtual int32 GetMaxPerformableAmount() const override;
	virtual bool CanPerformAction() const override;

protected:
	virtual void NativeConstruct() override;
	virtual void OnDisplayRecipe(const FMORecipeDefinitionRow* Recipe) override;
	virtual void PopulateRequirementsContainer() override;
	virtual void PopulateOutputsContainer() override;
	virtual void HandleActionButtonClicked() override;

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

	// --- Building-Specific Widget Bindings ---

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
	// Building-specific state
	float GatherRange = 150.0f;

	// Cached recipe data for Blueprint access
	TArray<FMOBuildPartDisplayData> CachedBuildParts;
	TArray<FMOBuildOutputDisplayData> CachedOutputs;
};
