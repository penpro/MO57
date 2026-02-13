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

/**
 * Widget representing a single building entry in the building menu.
 */
UCLASS()
class MOFRAMEWORK_API UMOBuildingEntryWidget : public UUserWidget
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
