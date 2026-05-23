/**
 * =============================================================================
 * MOCharacterUIController.h - Skills, Status, and Inspection UI
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Specialized UI controller for character-related panels. Manages skills panel,
 * status panel (vitals/metabolism), and item inspection progress. Part of the
 * UIManager split architecture.
 *
 * KEY RESPONSIBILITIES:
 * 1. Manage Skills panel (open/close/toggle)
 * 2. Manage Status panel with medical component bindings
 * 3. Handle item inspection flow with progress widget
 * 4. Rebind UI to current pawn on possession changes
 *
 * UI WIDGETS MANAGED:
 * - SkillsPanel: Shows skill levels and XP progress
 * - StatusPanel: Shows vitals, metabolism, mental state bars
 * - InspectionProgressWidget: Circular progress during item inspection
 *
 * INSPECTION FLOW:
 * StartItemInspection(ItemId, Guid) -> Create progress widget
 * -> Player holds inspect -> Progress fills -> HandleInspectionCompleted()
 * -> KnowledgeComponent.InspectItem() -> Show XP grants
 *
 * CRITICAL PATTERNS:
 * 1. Status Panel Binding:
 *    RebindStatusPanelToCurrentPawn() -> Get medical components
 *    -> StatusPanel.BindToComponents(Vitals, Metabolism, Mental)
 *    Called by OnPawnChanged() from UIManager
 *
 * 2. Skills Panel:
 *    ToggleSkillsPanel() -> Open or close based on current state
 *    Uses modal background + UI input mode when open
 *
 * 3. Inspection Cancellation:
 *    Player moves or releases key -> CancelItemInspection()
 *    Clears progress widget, no XP granted
 *
 * KNOWN PITFALLS:
 * 1. STATUS PANEL PERSISTENCE: StatusPanel may persist across pawn changes.
 *    Always call RebindStatusPanelToCurrentPawn() after possession.
 *
 * 2. INSPECTION GUID: InspectingItemGuid tracks which item is being inspected.
 *    If item is moved/dropped during inspection, cancel inspection.
 *
 * 3. MEDICAL COMPONENT NULL: GetCurrentPawnMedicalComponents() can return nulls.
 *    StatusPanel must handle null bindings gracefully.
 *
 * RELATED FILES:
 * - MOUIControllerBase.h - Base class with shared utilities
 * - MOSkillsPanel.h - Skills display widget
 * - MOStatusPanel.h - Medical status display widget
 * - MOInspectionProgressWidget.h - Circular progress widget
 * - MOKnowledgeComponent.h - Receives inspection results
 *
 * TESTING CHECKLIST:
 * [ ] Skills panel toggles correctly
 * [ ] Status panel shows current pawn's vitals
 * [ ] Status panel rebinds on possession change
 * [ ] Inspection progress completes and awards XP
 * [ ] Inspection cancellation clears progress
 * [ ] Multiple inspections don't stack
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOInterruptibleInterface.h"
#include "MOCharacterUIController.generated.h"

class AMOCharacter;
class UMOSkillsPanel;
class UMOStatusPanel;
class UMOInspectionProgressWidget;
class UMOProgressWidgetBase;
class UMOTerraformingComponent;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;
enum class EMOTerraformMode : uint8;
struct FMOInspectionResult;

/**
 * Specialized UI controller for character-related UI.
 *
 * Handles:
 * - Skills panel (toggle, open, close)
 * - Status panel (vitals, metabolism, mental state)
 * - Item inspection (progress, completion, knowledge grants)
 *
 * This controller is extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of the character UI subsystem.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCharacterUIController : public UMOUIControllerBase,
	public IMOInterruptibleInterface
{
	GENERATED_BODY()

public:
	UMOCharacterUIController();

	/**
	 * Interrupt policy for an active item inspection:
	 *   Movement / Damage / Knockdown / Unconscious / Death / EnteredCombat
	 *     / LostControl / External  -> CANCEL (no progress preserved)
	 *   UserCancel  -> ignored (UI calls CancelItemInspection directly)
	 *
	 * Inspection is a "stand still and concentrate" action — any meaningful
	 * disturbance discards progress. Severity is not consulted today; a tap
	 * on the shoulder breaks concentration just like a stab wound.
	 */
	virtual void NotifyInterrupt_Implementation(const FMOInterruptContext& Context) override;

	// ==========================================================================
	// SKILLS PANEL
	// ==========================================================================

	/** Toggle skills panel visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Skills")
	void ToggleSkillsPanel();

	/** Open the skills panel. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Skills")
	void OpenSkillsPanel();

	/** Close the skills panel. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Skills")
	void CloseSkillsPanel();

	/** Check if skills panel is open. */
	UFUNCTION(BlueprintPure, Category="MO|Character|Skills")
	bool IsSkillsPanelOpen() const;

	/** Get the skills panel widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|Character|Skills")
	UMOSkillsPanel* GetSkillsPanel() const;

	// ==========================================================================
	// STATUS PANEL
	// ==========================================================================

	/** Toggle player status panel visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Status")
	void TogglePlayerStatus();

	/** Show or hide the player status panel. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Status")
	void SetPlayerStatusVisible(bool bVisible);

	/** Check if player status panel is visible. */
	UFUNCTION(BlueprintPure, Category="MO|Character|Status")
	bool IsPlayerStatusVisible() const;

	/** Get the status panel widget (may be null if not created yet). */
	UFUNCTION(BlueprintPure, Category="MO|Character|Status")
	UMOStatusPanel* GetStatusPanel() const;

	/** Rebind the status panel to current pawn's medical components. Call after pawn changes. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Status")
	void RebindStatusPanelToCurrentPawn();

	// ==========================================================================
	// ITEM INSPECTION
	// ==========================================================================

	/** Start inspecting an item. Shows progress widget and grants knowledge on completion. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Inspection")
	void StartItemInspection(FName ItemDefinitionId, const FGuid& ItemGuid);

	/** Cancel any active inspection. */
	UFUNCTION(BlueprintCallable, Category="MO|Character|Inspection")
	void CancelItemInspection();

	/** Check if an inspection is currently in progress. */
	UFUNCTION(BlueprintPure, Category="MO|Character|Inspection")
	bool IsInspectionInProgress() const;

	// ==========================================================================
	// INTERNAL (Called by UIManager during pawn changes)
	// ==========================================================================

	/** Create status panel widget. Called by UIManager during BeginPlay if configured. */
	void CreateStatusPanel();

	/** Called when pawn changes to rebind widgets. Wired via base class virtual. */
	void OnPawnChanged();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Base class hook: fires when the owning PC takes/releases a pawn.
	 * Routes through OnPawnChanged() so terraform delegate bindings,
	 * status-panel rebinds, etc. all re-run with the live pawn.
	 */
	virtual void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn) override;

private:
	/** Frame-based debounce to prevent double-toggle from ECommonInputMode::All */
	uint64 LastToggleFrame = 0;

	// --- Skills Panel ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Skills", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOSkillsPanel> SkillsPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Skills", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 SkillsPanelZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOSkillsPanel> SkillsPanelWidget;

	UFUNCTION()
	void HandleSkillsPanelRequestClose();

	// --- Status Panel ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Status", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOStatusPanel> StatusPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Status", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 StatusPanelZOrder = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Status", meta=(AllowPrivateAccess="true"))
	bool bCreateStatusPanelOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Status", meta=(AllowPrivateAccess="true"))
	bool bHideStatusPanelWhenMenuOpen = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOStatusPanel> StatusPanelWidget;

	/** Tracks whether status panel is currently visible (avoids querying widget visibility). */
	bool bStatusPanelVisible = false;

	UFUNCTION()
	void HandleStatusPanelRequestClose();

	/** Get medical components from current pawn (null-safe). */
	void GetCurrentPawnMedicalComponents(UMOVitalsComponent*& OutVitals, UMOMetabolismComponent*& OutMetabolism, UMOMentalStateComponent*& OutMental) const;

	// --- Inspection ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Inspection", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInspectionProgressWidget> InspectionProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Inspection", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InspectionProgressZOrder = 200;

	/** Duration of item inspection in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Inspection", meta=(ClampMin="1.0", AllowPrivateAccess="true"))
	float InspectionDuration = 15.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInspectionProgressWidget> InspectionProgressWidget;

	/** The item GUID currently being inspected. */
	FGuid InspectingItemGuid;

	/**
	 * Character we've registered with for interrupt notifications. Cached so
	 * we can unregister later even if the controlled pawn changes
	 * mid-inspection.
	 */
	TWeakObjectPtr<AMOCharacter> RegisteredInspectionCharacter;

	/** Subscribe to the inspecting pawn's interrupt broadcast. */
	void RegisterForInspectionInterrupts();

	/** Drop the inspection interrupt registration (if any). */
	void UnregisterFromInspectionInterrupts();

	// --- Terraforming Progress ---

	/**
	 * Widget class to spawn for terraform progress display. Accepts ANY
	 * UMOProgressWidgetBase subclass — the existing harvest progress widget
	 * Blueprint works fine here, since the only thing the controller needs
	 * is the base class's progress / time / label API. No terraform-specific
	 * widget class is required.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Terraforming", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOProgressWidgetBase> TerraformProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|Terraforming", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 TerraformProgressZOrder = 200;

	/** Active progress widget for the current pending terraform (null when none). */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOProgressWidgetBase> TerraformProgressWidget;

	/** The component we're currently bound to (so we can unbind on pawn change). */
	TWeakObjectPtr<UMOTerraformingComponent> BoundTerraformingComponent;

	/** Bind to the controlled pawn's TerraformingComponent OnTerraformStarted. */
	void BindToPawnTerraformingComponent();

	/** Drop the OnTerraformStarted binding. */
	void UnbindFromPawnTerraformingComponent();

	/** Component said "I just started a terraform action" — spawn the widget. */
	UFUNCTION()
	void HandleTerraformStarted(EMOTerraformMode Mode, float DurationSeconds);

	/** Component ticked — push progress into the widget for display. */
	UFUNCTION()
	void HandleTerraformProgress(float Progress01, float TimeRemainingSeconds);

	/** Component finished applying the sculpt — tear down the widget. */
	UFUNCTION()
	void HandleTerraformCompleted(bool bSuccess);

	/** Component aborted the action (movement / damage / explicit cancel) — tear down. */
	UFUNCTION()
	void HandleTerraformCancelled();

	/** Player clicked the widget's Cancel button — tell the component to cancel. */
	UFUNCTION()
	void HandleTerraformWidgetCancelled();

	/** Remove the progress widget from the layer stack and unbind its delegates. */
	void TearDownTerraformProgressWidget();

	UFUNCTION()
	void HandleInspectionCompleted(bool bCompleted, const FMOInspectionResult& Result);

	UFUNCTION()
	void HandleInspectionCancelled();
};
