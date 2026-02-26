/**
 * =============================================================================
 * MOBuildingRecipeEntryWidget.h - Building Recipe List Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget representing a single building recipe entry in the recipe list.
 * Mirrors UMORecipeEntryWidget for crafting. Shows recipe name, icon,
 * selection state, and buildability status with appropriate coloring.
 *
 * VISUAL STATES:
 * - Selected: SelectedColor background
 * - Buildable (not selected): BuildableColor background
 * - Unbuildable: UnbuildableColor background + dimmed text
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] BUTTON BINDING: EntryButton click triggers HandleButtonClicked().
 *   Fires OnEntryClicked delegate with RecipeId.
 *
 * [2024-02] TEXT COLORS: TextColorBuildable and TextColorUnbuildable control
 *   recipe name color. UpdateVisuals() applies based on bCanBuild.
 *
 * [2024-02] ABSTRACT CLASS: Must create Blueprint subclass with bound widgets.
 *   EntryButton is optional but required for click functionality.
 *
 * =============================================================================
 * RELATED FILES: MOBuildingRecipeListWidget.h, MOBuildingDetailPanel.h,
 *                MOBuildingUIController.h, MORecipeEntryWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBuildingRecipeListWidget.h"
#include "MOBuildingRecipeEntryWidget.generated.h"

class UMOCommonButton;
class UTextBlock;
class UImage;
class UBorder;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOBuildRecipeEntryClickedSignature, FName, RecipeId);

/**
 * Widget representing a single building recipe entry in the recipe list.
 * Mirrors UMORecipeEntryWidget for crafting.
 *
 * Requires a Blueprint implementation with bound widgets.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingRecipeEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOBuildingRecipeEntryWidget(const FObjectInitializer& ObjectInitializer);

	// --- Setup ---

	/**
	 * Configure this entry with recipe data.
	 * @param InData Visual data for the building recipe
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetupEntry(const FMOBuildRecipeListEntryData& InData);

	/** Update just the selection state. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetSelected(bool bInSelected);

	/** Update just the buildable state. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetCanBuild(bool bInCanBuild);

	// --- Getters ---

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FName GetRecipeId() const { return EntryData.RecipeId; }

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool IsSelected() const { return EntryData.bIsSelected; }

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool CanBuild() const { return EntryData.bCanBuild; }

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	const FMOBuildRecipeListEntryData& GetEntryData() const { return EntryData; }

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildRecipeEntryClickedSignature OnEntryClicked;

	// --- Configuration ---

	/** Color when entry is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FLinearColor SelectedColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);

	/** Color when entry is not selected but buildable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FLinearColor BuildableColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Color when entry cannot be built. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FLinearColor UnbuildableColor = FLinearColor(0.3f, 0.1f, 0.1f, 0.5f);

	/** Color for text when buildable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FSlateColor TextColorBuildable = FSlateColor(FLinearColor::White);

	/** Color for text when not buildable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FSlateColor TextColorUnbuildable = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	/** Update visual appearance based on current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void UpdateVisuals();

	/** Called when the entry button is clicked. */
	UFUNCTION()
	void HandleButtonClicked();

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnVisualsUpdated(const FMOBuildRecipeListEntryData& Data);

	// --- Widget Bindings ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> EntryButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> BackgroundBorder;

private:
	FMOBuildRecipeListEntryData EntryData;
};
