/**
 * =============================================================================
 * MORecipeEntryWidget.h - Crafting Recipe List Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Individual recipe entry in a recipe list. Shows recipe name, icon, and
 * visual state (selected, craftable, unavailable). Used by MORecipeListWidget.
 *
 * INHERITS FROM: UMOListEntryBase (provides button handling, selection state)
 *
 * VISUAL STATES:
 * - Selected: SelectedColor background
 * - Craftable: CraftableColor background, white text
 * - Unavailable: UncraftableColor background, gray text
 *
 * DELEGATES:
 * - OnRecipeSelected (FMOUIRecipeSelected): Domain-specific delegate
 * - OnEntryClicked (legacy): Deprecated, use OnRecipeSelected
 * - OnEntrySelected (inherited): Generic list entry delegate
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] BUTTON TYPE: Uses UMOCommonButton (CommonUI) from base class.
 *   Button handling is in base, but we override to broadcast recipe-specific delegates.
 *
 * [2024-02] ENTRY DATA: SetupEntry() caches FMORecipeListEntryData. Partial
 *   updates (SetSelected, SetCanCraft) modify cache and call UpdateVisuals().
 *
 * [2024-02] THREE COLOR STATES: Unlike base's 2-state, recipe has 3:
 *   selected, craftable, uncraftable. UpdateVisuals() is overridden.
 *
 * =============================================================================
 * RELATED FILES: MOListEntryBase.h, MORecipeListWidget.h, MOCraftingMenu.h
 * LAST UPDATED: 2026-03-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOListEntryBase.h"
#include "MORecipeListWidget.h"
#include "MOUIDelegates.h"
#include "MORecipeEntryWidget.generated.h"

class UTextBlock;
class UImage;

// Legacy delegate - prefer FMOUIRecipeSelected from MOUIDelegates.h for new code
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMORecipeEntryClickedSignature, FName, RecipeId);

/**
 * Recipe list entry widget.
 * Inherits button handling and selection from UMOListEntryBase.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMORecipeEntryWidget : public UMOListEntryBase
{
	GENERATED_BODY()

public:
	UMORecipeEntryWidget(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// SETUP
	// ============================================================================

	/**
	 * Configure this entry with recipe data.
	 * @param InData Visual data for the recipe
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Crafting|UI")
	void SetupEntry(const FMORecipeListEntryData& InData);

	/** Update just the selection state. */
	virtual void SetSelected(bool bInSelected) override;

	/** Update just the craftable state. */
	UFUNCTION(BlueprintCallable, Category = "MO|Crafting|UI")
	void SetCanCraft(bool bInCanCraft);

	// ============================================================================
	// GETTERS
	// ============================================================================

	UFUNCTION(BlueprintPure, Category = "MO|Crafting|UI")
	FName GetRecipeId() const { return EntryData.RecipeId; }

	UFUNCTION(BlueprintPure, Category = "MO|Crafting|UI")
	bool CanCraft() const { return EntryData.bCanCraft; }

	UFUNCTION(BlueprintPure, Category = "MO|Crafting|UI")
	const FMORecipeListEntryData& GetEntryData() const { return EntryData; }

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when this entry is clicked. Uses standard delegate signature. */
	UPROPERTY(BlueprintAssignable, Category = "MO|Crafting|UI")
	FMOUIRecipeSelected OnRecipeSelected;

	/** @deprecated Use OnRecipeSelected instead. Broadcast for backward compatibility. */
	UPROPERTY(BlueprintAssignable, Category = "MO|Crafting|UI")
	FMORecipeEntryClickedSignature OnEntryClicked;

	// ============================================================================
	// CONFIGURATION (additional to base class)
	// ============================================================================

	/** Color when entry is not selected but craftable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Crafting|UI|Style")
	FLinearColor CraftableColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Color when entry cannot be crafted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Crafting|UI|Style")
	FLinearColor UncraftableColor = FLinearColor(0.3f, 0.1f, 0.1f, 0.5f);

	/** Color for text when craftable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Crafting|UI|Style")
	FSlateColor TextColorCraftable = FSlateColor(FLinearColor::White);

	/** Color for text when not craftable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Crafting|UI|Style")
	FSlateColor TextColorUncraftable = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));

protected:
	virtual void NativePreConstruct() override;

	/** Override to handle recipe-specific 3-state visuals. */
	virtual void UpdateVisuals_Implementation() override;

	/** Override to broadcast recipe-specific delegates. */
	virtual void HandleButtonClicked() override;

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category = "MO|Crafting|UI")
	void OnVisualsUpdated(const FMORecipeListEntryData& Data);

	// ============================================================================
	// WIDGET BINDINGS (recipe-specific)
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

private:
	FMORecipeListEntryData EntryData;
};
