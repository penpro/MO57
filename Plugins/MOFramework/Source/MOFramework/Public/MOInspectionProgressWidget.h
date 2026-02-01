#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOInspectionProgressWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UCommonButtonBase;
class UMOKnowledgeComponent;
class UMOSkillsComponent;
struct FMOInspectionResult;

/**
 * Widget displaying inspection progress with a countdown timer.
 * Shows item name, progress bar, and time remaining.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOInspectionProgressWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Called when inspection is completed (either success or cancelled). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInspectionCompletedSignature, bool, bCompleted, const FMOInspectionResult&, Result);
	UPROPERTY(BlueprintAssignable, Category="MO|Inspection")
	FMOInspectionCompletedSignature OnInspectionCompleted;

	/** Called when player requests to close/cancel the inspection. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInspectionCancelledSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|Inspection")
	FMOInspectionCancelledSignature OnInspectionCancelled;

	/** Initialize the widget for inspecting a specific item. */
	UFUNCTION(BlueprintCallable, Category="MO|Inspection")
	void StartInspection(
		FName InItemDefinitionId,
		const FText& InItemDisplayName,
		UMOKnowledgeComponent* InKnowledgeComponent,
		UMOSkillsComponent* InSkillsComponent,
		float InInspectionDuration = 15.0f);

	/** Cancel the inspection in progress. */
	UFUNCTION(BlueprintCallable, Category="MO|Inspection")
	void CancelInspection();

	/** Check if inspection is currently in progress. */
	UFUNCTION(BlueprintPure, Category="MO|Inspection")
	bool IsInspectionInProgress() const { return bIsInspecting; }

	/** Get current progress (0-1). */
	UFUNCTION(BlueprintPure, Category="MO|Inspection")
	float GetProgress() const { return CurrentProgress; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Update the visual display. Override in BP for custom visuals. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Inspection")
	void UpdateDisplay(float Progress, float TimeRemaining, const FText& ItemName);

	/** Called when inspection completes successfully. Override in BP for custom behavior. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Inspection")
	void OnInspectionSuccess(const FMOInspectionResult& Result);

	// --- Widget Bindings ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="MO|Inspection")
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="MO|Inspection")
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="MO|Inspection")
	TObjectPtr<UTextBlock> TimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MO|Inspection")
	TObjectPtr<UTextBlock> InstructionText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MO|Inspection")
	TObjectPtr<UCommonButtonBase> CancelButton;

private:
	UFUNCTION()
	void HandleCancelClicked();

	void CompleteInspection();

	// Current inspection state
	FName ItemDefinitionId;
	FText ItemDisplayName;
	TWeakObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	float InspectionDuration = 15.0f;
	float ElapsedTime = 0.0f;
	float CurrentProgress = 0.0f;
	bool bIsInspecting = false;
};
