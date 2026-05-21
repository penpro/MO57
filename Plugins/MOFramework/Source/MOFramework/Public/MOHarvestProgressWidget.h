/**
 * =============================================================================
 * MOHarvestProgressWidget.h - Harvest Progress UI Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget displaying harvest operation progress. Shows action name, progress
 * bar, time remaining, and cancel option. Used when harvesting resources
 * like chopping trees or gathering materials.
 *
 * INHERITS FROM: UMOProgressWidgetBase (provides BindWidget, progress tracking)
 *
 * WIDGET BINDINGS (inherited from base):
 * - ProgressBar (UProgressBar) - Visual progress
 * - ActionNameText (UTextBlock) - "Chopping Tree"
 * - TimeRemainingText (UTextBlock) - "3.2s"
 * - InstructionText (optional) - Instruction hint
 * - CancelButton (optional) - Abort harvest
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] REAL-TIME TRACKING: Uses wall-clock time via base class for
 *   accurate progress even if frame rate drops.
 *
 * [2024-02] DELEGATE CLEANUP: OnHarvestCompleted/OnHarvestCancelled persist.
 *   Clear bindings after use if reusing widget instance.
 *
 * =============================================================================
 * RELATED FILES: MOProgressWidgetBase.h, MOHarvestSubsystem.h, MOCraftingUIController.h
 * LAST UPDATED: 2026-03-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOProgressWidgetBase.h"
#include "MOHarvestProgressWidget.generated.h"

class UMOKnowledgeComponent;
class UMOSkillsComponent;
class UMOInventoryComponent;
class UInstancedStaticMeshComponent;
struct FMOCraftResult;

/**
 * Widget displaying harvest progress with a countdown timer.
 * Inherits core progress functionality from UMOProgressWidgetBase.
 * See file header for widget bindings and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOHarvestProgressWidget : public UMOProgressWidgetBase
{
	GENERATED_BODY()

public:
	// ============================================================================
	// DELEGATES (domain-specific)
	// ============================================================================

	/** Called when harvest is completed (either success or cancelled). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOHarvestCompletedSignature, bool, bCompleted, const FMOCraftResult&, Result);
	UPROPERTY(BlueprintAssignable, Category = "MO|Harvest")
	FMOHarvestCompletedSignature OnHarvestCompleted;

	/** Called when player requests to close/cancel the harvest. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOHarvestCancelledSignature);
	UPROPERTY(BlueprintAssignable, Category = "MO|Harvest")
	FMOHarvestCancelledSignature OnHarvestCancelled;

	// ============================================================================
	// HARVEST CONTROL
	// ============================================================================

	/**
	 * Start a harvest operation with progress display.
	 * @param InISMComponent The ISM/HISM component to harvest from
	 * @param InInstanceIndex The instance index to harvest
	 * @param InActionId The harvest action ID (from resource definition's HarvestActions)
	 * @param InActionDisplayName Display name for the action (e.g., "Gathering Sticks")
	 * @param InInventory Player's inventory (for time calculation and output)
	 * @param InSkills Player's skills (for XP)
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Harvest")
	void StartHarvest(
		UInstancedStaticMeshComponent* InISMComponent,
		int32 InInstanceIndex,
		FName InActionId,
		const FText& InActionDisplayName,
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills
	);

	/** Cancel the harvest in progress. */
	UFUNCTION(BlueprintCallable, Category = "MO|Harvest")
	void CancelHarvest();

	/** Check if harvest is currently in progress. */
	UFUNCTION(BlueprintPure, Category = "MO|Harvest")
	bool IsHarvestInProgress() const { return IsProgressActive(); }

	/** Get the action ID being executed. */
	UFUNCTION(BlueprintPure, Category = "MO|Harvest")
	FName GetCurrentActionId() const { return HarvestActionId; }

	/** Update the visual display. Can be called externally for carcass butchering etc. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "MO|Harvest")
	void UpdateHarvestDisplay(float Progress, float TimeRemaining, const FText& InActionName);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Override base UpdateDisplay to use harvest-specific formatting. */
	virtual void UpdateDisplay_Implementation(float Progress, float TimeRemaining, const FText& InActionName) override;

	/** Override base OnProgressSuccess to complete harvest via subsystem. */
	virtual void OnProgressSuccess_Implementation() override;

	/** Called when harvest completes successfully. Override in BP for custom behavior. */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|Harvest")
	void OnHarvestSuccess(const FMOCraftResult& Result);

	/**
	 * Reactor for the subsystem's OnHarvestCancelled broadcast — fires when
	 * something OTHER than this widget cancelled the harvest (movement
	 * interrupt, network kick, console command, etc.). Tears down the widget
	 * so the visual progress doesn't outlive the underlying state.
	 *
	 * Previously the widget and subsystem each had independent OnHarvestCancelled
	 * delegates with no wiring between them — if the subsystem cancelled
	 * itself, the widget kept ticking. This handler closes that gap.
	 */
	UFUNCTION()
	void HandleSubsystemHarvestCancelled();

	/** Subscribe to the subsystem's cancellation broadcast (idempotent). */
	void BindSubsystemCancellation();

	/** Unsubscribe from the subsystem's cancellation broadcast. */
	void UnbindSubsystemCancellation();

	// ============================================================================
	// HARVEST-SPECIFIC STATE
	// ============================================================================

	/** The harvest action ID being executed. */
	FName HarvestActionId = NAME_None;

	/** Cached inventory component (weak ref). */
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	/** Cached skills component (weak ref). */
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	/**
	 * Cached pointer to the harvest subsystem we're bound to. Kept so we can
	 * unbind in NativeDestruct even if World->GetSubsystem<> returns a
	 * different/null pointer at teardown time.
	 */
	TWeakObjectPtr<class UMOHarvestSubsystem> BoundHarvestSubsystem;

	/**
	 * Re-entrancy guard. When HandleSubsystemHarvestCancelled fires it calls
	 * cancellation paths that themselves broadcast — without this guard we'd
	 * double-broadcast OnHarvestCancelled and double-deactivate.
	 */
	bool bSuppressSubsystemCancellationHandler = false;
};
