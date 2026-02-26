/**
 * =============================================================================
 * MOInspectionProgressWidget.h - Item Inspection Progress Display
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget displaying inspection progress with countdown timer. Shows item name,
 * progress bar, and time remaining. Grants knowledge/XP on completion.
 *
 * FEATURES:
 * - Real-time countdown (uses wall clock, continues when tabbed out)
 * - Progress bar visualization
 * - Cancel button for early termination
 * - Debug override for inspection duration
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] REAL-TIME TRACKING: Uses wall clock (InspectionStartTime) not
 *   game delta time. Progress continues even when game is paused/minimized.
 *
 * [2024-02] DEBUG DURATION: DebugInspectionDuration > 0 overrides the
 *   duration passed to StartInspection(). Use 0 for normal behavior.
 *
 * [2024-02] COMPONENT REFS: KnowledgeComponent and SkillsComponent are weak
 *   pointers. Must be valid when CompleteInspection() is called.
 *
 * =============================================================================
 * RELATED FILES: MOCharacterUIController.h, MOKnowledgeComponent.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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

	/**
	 * Debug override for inspection duration.
	 * If > 0, this value is used instead of the duration passed to StartInspection.
	 * Set to 0 to use the normal duration from the item definition.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inspection|Debug")
	float DebugInspectionDuration = 0.0f;

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

	/** Get the item definition ID being inspected. */
	UFUNCTION(BlueprintPure, Category="MO|Inspection")
	FName GetInspectingItemId() const { return ItemDefinitionId; }

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

	/** Timer callback for real-time inspection progress (continues when tabbed out). */
	UFUNCTION()
	void TickInspection();

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

	/** Wall-clock time when inspection started (for real-time tracking). */
	double InspectionStartTime = 0.0;

	/** Timer handle for display updates (progress still uses wall-clock time). */
	FTimerHandle InspectionTimerHandle;
};
