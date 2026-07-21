/**
 * =============================================================================
 * MOBuildingEntryWidget.h - Building Recipe List Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * DEPRECATED:
 * This pre-CommonUI entry has no live Blueprint child or callsite. Use
 * UMOBuildingRecipeEntryWidget, which participates in the authoritative
 * UMOScrollListBase selection lifecycle.
 *
 * DISPLAYS:
 * - NameText: Building name from recipe
 * - PreviewImage: Building thumbnail/icon
 * - BuildTimeText: Construction duration
 * - MaterialsText: Summary of required materials
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] BUTTON TYPE: Uses standard UButton (not CommonUI). Consider
 *   migrating to UMOCommonButton for consistency.
 *
 * [2024-02] RECIPE DATA: InitializeEntry receives full FMORecipeDefinitionRow.
 *   Extract display info but don't cache the entire struct.
 *
 * [2024-02] PREVIEW IMAGE: May need async loading for thumbnails. Use
 *   TSoftObjectPtr and StreamableManager for large icon sets.
 *
 * =============================================================================
 * RELATED FILES: MOBuildingMenu.h, MOBuildingRecipeListWidget.h, MORecipeDefinitionRow.h
 * LAST UPDATED: 2026-07-13 - Deprecated after live referencer verification
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBuildingEntryWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
struct FMORecipeDefinitionRow;

/**
 * Delegate for when a building entry is clicked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnBuildingEntryClickedSignature, FName, RecipeId);
UCLASS(meta=(DeprecationMessage="Use UMOBuildingRecipeEntryWidget"))
class UE_DEPRECATED(5.8, "Use UMOBuildingRecipeEntryWidget") MOFRAMEWORK_API UMOBuildingEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when this entry is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnBuildingEntryClickedSignature OnEntryClicked;

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize this entry with recipe data.
	 * @param InRecipeId - The recipe ID
	 * @param Recipe - The recipe definition
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void InitializeEntry(FName InRecipeId, const FMORecipeDefinitionRow& Recipe);

	/**
	 * Get the recipe ID for this entry.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	FName GetRecipeId() const { return RecipeId; }

protected:
	// ============================================================================
	// WIDGETS (Set in Blueprint)
	// ============================================================================

	/** Building name text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Building preview image. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> PreviewImage;

	/** Build time text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BuildTimeText;

	/** Material summary text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MaterialsText;

	/** Main button for clicking. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> EntryButton;

	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** Recipe ID for this entry. */
	FName RecipeId = NAME_None;

	UFUNCTION()
	void HandleButtonClicked();
};
