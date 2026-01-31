#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MORecipeListWidget.h"
#include "MORecipeEntryWidget.generated.h"

class UMOCommonButton;
class UTextBlock;
class UImage;
class UBorder;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMORecipeEntryClickedSignature, FName, RecipeId);

/**
 * Widget representing a single recipe entry in the recipe list.
 *
 * Requires a Blueprint implementation with bound widgets.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMORecipeEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMORecipeEntryWidget(const FObjectInitializer& ObjectInitializer);

	// --- Setup ---

	/**
	 * Configure this entry with recipe data.
	 * @param InData Visual data for the recipe
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetupEntry(const FMORecipeListEntryData& InData);

	/** Update just the selection state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetSelected(bool bInSelected);

	/** Update just the craftable state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetCanCraft(bool bInCanCraft);

	// --- Getters ---

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FName GetRecipeId() const { return EntryData.RecipeId; }

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	bool IsSelected() const { return EntryData.bIsSelected; }

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	bool CanCraft() const { return EntryData.bCanCraft; }

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	const FMORecipeListEntryData& GetEntryData() const { return EntryData; }

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMORecipeEntryClickedSignature OnEntryClicked;

	// --- Configuration ---

	/** Color when entry is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	FLinearColor SelectedColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);

	/** Color when entry is not selected but craftable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	FLinearColor CraftableColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Color when entry cannot be crafted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	FLinearColor UncraftableColor = FLinearColor(0.3f, 0.1f, 0.1f, 0.5f);

	/** Color for text when craftable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	FSlateColor TextColorCraftable = FSlateColor(FLinearColor::White);

	/** Color for text when not craftable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	FSlateColor TextColorUncraftable = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	/** Update visual appearance based on current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void UpdateVisuals();

	/** Called when the entry button is clicked. */
	UFUNCTION()
	void HandleButtonClicked();

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnVisualsUpdated(const FMORecipeListEntryData& Data);

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
	FMORecipeListEntryData EntryData;
};
