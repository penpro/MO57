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
 * INHERITS FROM: UMOListEntryBase (provides button handling, selection state)
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
 * [2024-02] BUTTON BINDING: Uses UMOCommonButton from base class.
 *   Override HandleButtonClicked to broadcast building-specific delegates.
 *
 * [2024-02] TEXT COLORS: TextColorBuildable and TextColorUnbuildable control
 *   recipe name color. UpdateVisuals() applies based on bCanBuild.
 *
 * [2024-02] ABSTRACT CLASS: Must create Blueprint subclass with bound widgets.
 *
 * =============================================================================
 * RELATED FILES: MOListEntryBase.h, MOBuildingRecipeListWidget.h, MOBuildingDetailPanel.h
 * LAST UPDATED: 2026-03-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOListEntryBase.h"
#include "MOBuildingRecipeListWidget.h"
#include "MOBuildingRecipeEntryWidget.generated.h"

class UTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOBuildRecipeEntryClickedSignature, FName, RecipeId);

/**
 * Widget representing a single building recipe entry in the recipe list.
 * Inherits button handling and selection from UMOListEntryBase.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingRecipeEntryWidget : public UMOListEntryBase
{
	GENERATED_BODY()

public:
	UMOBuildingRecipeEntryWidget(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// SETUP
	// ============================================================================

	/**
	 * Configure this entry with recipe data.
	 * @param InData Visual data for the building recipe
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Building|UI")
	void SetupEntry(const FMOBuildRecipeListEntryData& InData);

	/** Update just the selection state. */
	virtual void SetSelected(bool bInSelected) override;

	/** Update just the buildable state. */
	UFUNCTION(BlueprintCallable, Category = "MO|Building|UI")
	void SetCanBuild(bool bInCanBuild);

	// ============================================================================
	// GETTERS
	// ============================================================================

	UFUNCTION(BlueprintPure, Category = "MO|Building|UI")
	FName GetRecipeId() const { return EntryData.RecipeId; }

	UFUNCTION(BlueprintPure, Category = "MO|Building|UI")
	bool CanBuild() const { return EntryData.bCanBuild; }

	UFUNCTION(BlueprintPure, Category = "MO|Building|UI")
	const FMOBuildRecipeListEntryData& GetEntryData() const { return EntryData; }

	// ============================================================================
	// DELEGATES
	// ============================================================================

	UPROPERTY(BlueprintAssignable, Category = "MO|Building|UI")
	FMOBuildRecipeEntryClickedSignature OnEntryClicked;

	// ============================================================================
	// CONFIGURATION (additional to base class)
	// ============================================================================

	/** Color when entry is not selected but buildable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Building|UI|Style")
	FLinearColor BuildableColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Color when entry cannot be built. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Building|UI|Style")
	FLinearColor UnbuildableColor = FLinearColor(0.3f, 0.1f, 0.1f, 0.5f);

	/** Color for text when buildable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Building|UI|Style")
	FSlateColor TextColorBuildable = FSlateColor(FLinearColor::White);

	/** Color for text when not buildable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Building|UI|Style")
	FSlateColor TextColorUnbuildable = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));

protected:
	virtual void NativePreConstruct() override;

	/** Override to handle building-specific 3-state visuals. */
	virtual void UpdateVisuals_Implementation() override;

	/** Override to broadcast building-specific delegates. */
	virtual void HandleButtonClicked() override;

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category = "MO|Building|UI")
	void OnVisualsUpdated(const FMOBuildRecipeListEntryData& Data);

	// ============================================================================
	// WIDGET BINDINGS (building-specific)
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

private:
	FMOBuildRecipeListEntryData EntryData;
};
