/**
 * =============================================================================
 * MOBuildingQueueEntryWidget.h - Single Build Queue Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget representing a single entry in the building construction queue.
 * Mirrors UMOCraftingQueueEntryWidget for crafting. Shows recipe name, icon,
 * progress bar, time remaining, and cancel button.
 *
 * DISPLAY DATA:
 * - FMOBuildQueueEntryDisplayData: Visual data for queue entries
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DUAL CANCEL BUTTONS: Both CancelButton (UMOCommonButton) and
 *   CancelButtonSimple (UButton) are supported. Bind only one in Blueprint.
 *
 * [2024-02] STATE COLORS: ActiveColor, QueuedColor, PausedColor control
 *   background based on EMOBuildState. UpdateVisuals() applies them.
 *
 * [2024-02] PROGRESS UPDATES: Call UpdateProgress() frequently for smooth
 *   progress bar animation. SetupEntry() for full refresh.
 *
 * =============================================================================
 * RELATED FILES: MOBuildingQueueWidget.h, MOBuildProgressComponent.h,
 *                MOBuildingTypes.h, MOCraftingQueueEntryWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBuildingTypes.h"
#include "MOBuildingQueueEntryWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UButton;
class UMOCommonButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOBuildQueueEntryCancelRequestedSignature, const FGuid&, EntryId);

/**
 * Visual data for a build queue entry.
 * Mirrors FMOQueueEntryDisplayData for crafting.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildQueueEntryDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FGuid EntryId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FName RecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText RecipeName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Current part / total parts (e.g., "2/5") */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText CountText;

	/** Progress 0.0 - 1.0 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	float Progress = 0.0f;

	/** Time remaining for this entry. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText TimeRemainingText;

	/** True if this is the currently active build. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	bool bIsActive = false;

	/** Current build state. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	EMOBuildState State = EMOBuildState::Ghost;
};

/**
 * Widget representing a single entry in the build queue.
 * Mirrors UMOCraftingQueueEntryWidget for crafting.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingQueueEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOBuildingQueueEntryWidget(const FObjectInitializer& ObjectInitializer);

	// --- Setup ---

	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetupEntry(const FMOBuildQueueEntryDisplayData& InData);

	/** Update just the progress. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void UpdateProgress(float NewProgress, const FText& NewTimeRemaining);

	// --- Getters ---

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FGuid GetEntryId() const { return EntryData.EntryId; }

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	const FMOBuildQueueEntryDisplayData& GetEntryData() const { return EntryData; }

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildQueueEntryCancelRequestedSignature OnCancelRequested;

	// --- Configuration ---

	/** Color for active entry background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FLinearColor ActiveColor = FLinearColor(0.2f, 0.4f, 0.2f, 1.0f);

	/** Color for queued entry background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FLinearColor QueuedColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Color for paused entry background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	FLinearColor PausedColor = FLinearColor(0.4f, 0.3f, 0.1f, 1.0f);

protected:
	virtual void NativeConstruct() override;

	/** Called when cancel button is clicked. */
	UFUNCTION()
	void HandleCancelClicked();

	/** Update visuals based on current data. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void UpdateVisuals();

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnVisualsUpdated(const FMOBuildQueueEntryDisplayData& Data);

	// --- Widget Bindings (match MOCraftingQueueEntryWidget) ---

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
	FMOBuildQueueEntryDisplayData EntryData;
};
