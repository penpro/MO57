#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOQuestTypes.h"
#include "MOQuestLogEntry.generated.h"

class UTextBlock;
class UMOCommonButton;

/** Delegate for when a quest entry is selected. */
DECLARE_DELEGATE_OneParam(FMOOnQuestEntrySelected, FName /*QuestId*/);

/**
 * Display data for a quest in the quest log list.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQuestLogDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FName Category;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	bool bIsComplete = false;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	bool bIsTracked = false;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	bool bIsTutorial = false;

	/** Number of completed objectives. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	int32 CompletedObjectives = 0;

	/** Total number of objectives. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	int32 TotalObjectives = 0;
};

/**
 * Widget representing a single quest entry in the quest log list.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOQuestLogEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOQuestLogEntry(const FObjectInitializer& ObjectInitializer);

	/** Set up the entry with quest data. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void SetupEntry(const FMOQuestLogDisplayData& InData);

	/** Set selection state. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void SetSelected(bool bSelected);

	/** Get the quest ID this entry represents. */
	UFUNCTION(BlueprintPure, Category="MO|Quest|UI")
	FName GetQuestId() const { return CachedData.QuestId; }

	/** Native delegate for selection. */
	FMOOnQuestEntrySelected OnQuestSelected;

protected:
	virtual void NativeConstruct() override;

	/** Called when select button is clicked. */
	UFUNCTION()
	void HandleSelectClicked();

	/** Update display from cached data. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void RefreshDisplay();

	/** Blueprint-callable selection event. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Quest|UI")
	void OnSelectionChanged(bool bSelected);

protected:
	/** Button to select this quest. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> SelectButton;

	/** Quest name text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> QuestNameText;

	/** Status text (e.g., "Complete", "Tracking", "Active"). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Progress text (e.g., "3/5 objectives"). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	/** Cached display data. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FMOQuestLogDisplayData CachedData;

	/** Whether this entry is currently selected. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	bool bIsSelected = false;
};
