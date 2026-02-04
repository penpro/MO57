#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOSkillDefinitionRow.h"
#include "MOSkillEntryWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UButton;

/**
 * Display data for a single skill entry.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSkillDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	FName SkillId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	FText Description;

	/** Describes how to gain XP for this skill. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	FText HowToIncrease;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	EMOSkillCategory Category = EMOSkillCategory::None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	int32 MaxLevel = 100;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	float CurrentXP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	float XPToNextLevel = 100.0f;

	/** Progress within current level (0.0 - 1.0). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	float LevelProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSkillSelectedSignature, FName, SkillId);

/**
 * Widget representing a single skill entry in the skills panel.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOSkillEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOSkillEntryWidget(const FObjectInitializer& ObjectInitializer);

	/** Set up the entry with skill data. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void SetupEntry(const FMOSkillDisplayData& InData);

	/** Update XP progress without rebuilding everything. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void UpdateProgress(float NewProgress, float NewCurrentXP, float NewXPToNext);

	/** Play a flash animation to highlight this entry (e.g., when skill/knowledge changes). */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void PlayFlashAnimation(float Duration = 2.0f);

	/** Get the skill ID for this entry. */
	UFUNCTION(BlueprintPure, Category="MO|Skills|UI")
	FName GetSkillId() const { return SkillData.SkillId; }

	/** Get the full skill data. */
	UFUNCTION(BlueprintPure, Category="MO|Skills|UI")
	const FMOSkillDisplayData& GetSkillData() const { return SkillData; }

	/** Fired when this skill entry is clicked/selected. */
	UPROPERTY(BlueprintAssignable, Category="MO|Skills|UI")
	FMOSkillSelectedSignature OnSkillSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Update all visual elements from current data. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void UpdateVisuals();

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Skills|UI")
	void OnVisualsUpdated(const FMOSkillDisplayData& Data);

	/** Handle click on this entry. */
	UFUNCTION()
	void HandleClicked();

	// --- Widget Bindings ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> XPText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> XPProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> SkillIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CategoryText;

	/** Button to select this skill entry. If not bound, the widget won't be clickable from C++. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> SelectButton;

private:
	FMOSkillDisplayData SkillData;

	/** Timer for flash animation. */
	FTimerHandle FlashTimerHandle;

	/** Flash animation state. */
	float FlashElapsedTime = 0.0f;
	float FlashDuration = 2.0f;
	bool bIsFlashing = false;

	/** Original progress bar fill color (to restore after flash). */
	FLinearColor OriginalFillColor;

	/** Tick the flash animation. */
	void TickFlashAnimation();
};
