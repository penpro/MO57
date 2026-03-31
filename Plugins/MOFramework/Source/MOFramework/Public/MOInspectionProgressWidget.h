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
 * INHERITS FROM: UMOProgressWidgetBase (provides BindWidget, progress tracking)
 *
 * WIDGET BINDINGS (inherited from base):
 * - ProgressBar (UProgressBar) - Visual progress
 * - ActionNameText (UTextBlock) - "Inspecting: Item Name"
 * - TimeRemainingText (UTextBlock) - "3.2s remaining"
 * - InstructionText (optional) - Instruction hint
 * - CancelButton (optional) - Abort inspection
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] REAL-TIME TRACKING: Uses wall clock via base class. Progress
 *   continues even when game is paused/minimized.
 *
 * [2024-02] DEBUG DURATION: DebugInspectionDuration > 0 overrides the
 *   duration passed to StartInspection(). Use 0 for normal behavior.
 *
 * [2024-02] COMPONENT REFS: KnowledgeComponent and SkillsComponent are weak
 *   pointers. Must be valid when completion occurs.
 *
 * =============================================================================
 * RELATED FILES: MOProgressWidgetBase.h, MOCharacterUIController.h, MOKnowledgeComponent.h
 * LAST UPDATED: 2026-03-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOProgressWidgetBase.h"
#include "MOInspectionProgressWidget.generated.h"

class UMOKnowledgeComponent;
class UMOSkillsComponent;
struct FMOInspectionResult;

/**
 * Widget displaying inspection progress with a countdown timer.
 * Inherits core progress functionality from UMOProgressWidgetBase.
 * Shows item name, progress bar, and time remaining.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOInspectionProgressWidget : public UMOProgressWidgetBase
{
	GENERATED_BODY()

public:
	// ============================================================================
	// DELEGATES (domain-specific)
	// ============================================================================

	/** Called when inspection is completed (either success or cancelled). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInspectionCompletedSignature, bool, bCompleted, const FMOInspectionResult&, Result);
	UPROPERTY(BlueprintAssignable, Category = "MO|Inspection")
	FMOInspectionCompletedSignature OnInspectionCompleted;

	/** Called when player requests to close/cancel the inspection. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInspectionCancelledSignature);
	UPROPERTY(BlueprintAssignable, Category = "MO|Inspection")
	FMOInspectionCancelledSignature OnInspectionCancelled;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/**
	 * Debug override for inspection duration.
	 * If > 0, this value is used instead of the duration passed to StartInspection.
	 * Set to 0 to use the normal duration from the item definition.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Inspection|Debug")
	float DebugInspectionDuration = 0.0f;

	// ============================================================================
	// INSPECTION CONTROL
	// ============================================================================

	/** Initialize the widget for inspecting a specific item. */
	UFUNCTION(BlueprintCallable, Category = "MO|Inspection")
	void StartInspection(
		FName InItemDefinitionId,
		const FText& InItemDisplayName,
		UMOKnowledgeComponent* InKnowledgeComponent,
		UMOSkillsComponent* InSkillsComponent,
		float InInspectionDuration = 15.0f);

	/** Cancel the inspection in progress. */
	UFUNCTION(BlueprintCallable, Category = "MO|Inspection")
	void CancelInspection();

	/** Check if inspection is currently in progress. */
	UFUNCTION(BlueprintPure, Category = "MO|Inspection")
	bool IsInspectionInProgress() const { return IsProgressActive(); }

	/** Get the item definition ID being inspected. */
	UFUNCTION(BlueprintPure, Category = "MO|Inspection")
	FName GetInspectingItemId() const { return ItemDefinitionId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Override base UpdateDisplay to use inspection-specific formatting. */
	virtual void UpdateDisplay_Implementation(float Progress, float TimeRemaining, const FText& InActionName) override;

	/** Override base OnProgressSuccess to complete inspection via knowledge component. */
	virtual void OnProgressSuccess_Implementation() override;

	/** Called when inspection completes successfully. Override in BP for custom behavior. */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|Inspection")
	void OnInspectionSuccess(const FMOInspectionResult& Result);

private:
	// ============================================================================
	// INSPECTION-SPECIFIC STATE
	// ============================================================================

	/** The item being inspected. */
	FName ItemDefinitionId;

	/** Display name for the item. */
	FText ItemDisplayName;

	/** Cached knowledge component (weak ref). */
	TWeakObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	/** Cached skills component (weak ref). */
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;
};
