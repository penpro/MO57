#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOCraftingTypes.h"
#include "MOCraftingQueueEntryWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UButton;
class UMOCommonButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOQueueEntryCancelRequestedSignature, const FGuid&, EntryId);

/**
 * Visual data for a queue entry.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQueueEntryDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FGuid EntryId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FName RecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText RecipeName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Current count / total count (e.g., "2/5") */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText CountText;

	/** Progress 0.0 - 1.0 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	float Progress = 0.0f;

	/** Time remaining for this entry. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText TimeRemainingText;

	/** True if this is the currently active craft. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	bool bIsActive = false;
};

/**
 * Widget representing a single entry in the crafting queue.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCraftingQueueEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOCraftingQueueEntryWidget(const FObjectInitializer& ObjectInitializer);

	// --- Setup ---

	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetupEntry(const FMOQueueEntryDisplayData& InData);

	/** Update just the progress. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void UpdateProgress(float NewProgress, const FText& NewTimeRemaining);

	// --- Getters ---

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FGuid GetEntryId() const { return EntryData.EntryId; }

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	const FMOQueueEntryDisplayData& GetEntryData() const { return EntryData; }

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMOQueueEntryCancelRequestedSignature OnCancelRequested;

	// --- Configuration ---

	/** Color for active entry background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	FLinearColor ActiveColor = FLinearColor(0.2f, 0.4f, 0.2f, 1.0f);

	/** Color for queued entry background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	FLinearColor QueuedColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

protected:
	virtual void NativeConstruct() override;

	/** Called when cancel button is clicked. */
	UFUNCTION()
	void HandleCancelClicked();

	/** Update visuals based on current data. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void UpdateVisuals();

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnVisualsUpdated(const FMOQueueEntryDisplayData& Data);

	// --- Widget Bindings ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> CancelButtonSimple;

private:
	FMOQueueEntryDisplayData EntryData;
};
