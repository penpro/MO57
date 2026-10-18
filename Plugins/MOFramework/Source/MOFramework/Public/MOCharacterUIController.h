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
 * - (an inspection's progress is the pawn's timed-action bar: the server times it, this controller draws it)
 *
 * INSPECTION FLOW:
 * StartItemInspection(ItemId, Guid) -> AMOPlayerController::ServerInspectItem (FMOInspectionActions: the server checks the item is at hand,
 * times the study on the pawn, inspects on its own copy) -> ClientInspectionResult -> ShowInspectionResult()
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
 *    The server's timed action ends it when the pawn moves or is hurt; CancelItemInspection() stops it on request.
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
 * - MOInspectionActions.h - the server's inspection
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
#include "MOVitalsTypes.h"  // EMOWetnessState (used in HandleWetnessStateChanged)
#include "MOInstanceRef.h"
#include "MOCharacterUIController.generated.h"

class AMOCharacter;
class UMOSkillsPanel;
class UMOStatusPanel;
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
class MOFRAMEWORK_API UMOCharacterUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOCharacterUIController();

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

	/**
	 * Inspect ItemDefinitionId: a carried item (ItemGuid), or one in the world the player is looking at -- Instance names it (a stick on the ground, the
	 * tree its bark comes from). The server checks it is at hand and runs the study (AMOPlayerController::ServerInspectItem).
	 */
	void StartItemInspectionAt(FName ItemDefinitionId, const FGuid& ItemGuid, const FMOInstanceRef& Instance);

	/** An inspection this player asked for finished (AMOPlayerController::ClientInspectionResult): the XP / knowledge popups and quest events. */
	void ShowInspectionResult(FName ItemDefinitionId, const FMOInspectionResult& Result);

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
	/** Frame-based debounce: one toggle per frame even if the key arrives twice (a guard from the ECommonInputMode::All era; menus use Menu mode now) */
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

	// =========================================================================
	// VITALS-DERIVED MOODLES (wet, future: bleeding, hot/cold thresholds, etc.)
	// =========================================================================
	//
	// Subscribes to UMOVitalsComponent::OnWetnessStateChanged on the live pawn
	// and pushes/removes a "Wet" moodle on the HUD status strip. Re-binds via
	// OnPossessedPawnChanged so the listener always tracks the current pawn.

	/** Vitals we're currently subscribed to — held weakly so possess-swap doesn't keep it alive. */
	UPROPERTY(Transient)
	TWeakObjectPtr<class UMOVitalsComponent> BoundVitalsComponent;

	/**
	 * Icons for the wet moodle, indexed by Level:
	 *   [0] Damp, [1] Wet, [2] Soaked
	 * Author one texture per level in BP defaults. If the array is shorter
	 * than the resolved level, the last entry is used (so a single-icon
	 * fallback works fine). If empty, the moodle renders label-only.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Character|Moodles", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<class UTexture2D>> WetMoodleIcons;

	/**
	 * Icons for the thermal moodle, indexed by EMOThermalComfort enum value:
	 *   [0] VeryCold, [1] Cold, [2] Comfortable (unused — no moodle when comfortable),
	 *   [3] Hot, [4] VeryHot
	 * Comfortable removes the moodle; the other four states push it with
	 * severity ratcheting (Warning at Cold/Hot, Critical at VeryCold/VeryHot).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Character|Moodles", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<class UTexture2D>> ThermalMoodleIcons;

	/** Bind to the live pawn's vitals OnWetnessStateChanged. */
	void BindToPawnVitalsForMoodles();

	/** Drop the wetness binding (called on pawn-swap and EndPlay). */
	void UnbindFromPawnVitalsForMoodles();

	/** Push/remove the wet moodle on the HUD strip based on wetness state. */
	void RefreshWetMoodle(EMOWetnessState State);

	UFUNCTION()
	void HandleWetnessStateChanged(EMOWetnessState OldState, EMOWetnessState NewState);

	UFUNCTION()
	void HandleThermalComfortChanged(EMOThermalComfort OldComfort, EMOThermalComfort NewComfort);

	/** Push/remove the thermal moodle on the HUD strip based on comfort state. */
	void RefreshThermalMoodle(EMOThermalComfort Comfort);

	// --- Survival-stat-derived moodles (hunger, thirst, energy) ---
	//
	// Same wiring pattern as wetness: subscribe on pawn change, push/remove
	// moodles based on the stat's percent. Each stat has two buckets:
	// "warning" (under WarningPercent) and "critical" (under CriticalPercent).

	/** Survival stats we're currently subscribed to. */
	UPROPERTY(Transient)
	TWeakObjectPtr<class UMOSurvivalStatsComponent> BoundSurvivalStats;

	/** Percent at which Hunger/Thirst/Energy show the warning moodle (default 0.50). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Character|Moodles",
		meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float SurvivalMoodleWarningPercent = 0.50f;

	/** Percent at which Hunger/Thirst/Energy escalate to critical (default 0.25). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Character|Moodles",
		meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float SurvivalMoodleCriticalPercent = 0.25f;

	/** Indexed by Level — [0] Hungry, [1] Starving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Character|Moodles", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<class UTexture2D>> HungerMoodleIcons;

	/** Indexed by Level — [0] Thirsty, [1] Dehydrated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Character|Moodles", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<class UTexture2D>> ThirstMoodleIcons;

	/** Indexed by Level — [0] Tired, [1] Exhausted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Character|Moodles", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<class UTexture2D>> EnergyMoodleIcons;

	/**
	 * Helper: returns the texture at Level (clamped to the array's last
	 * entry if Level overflows). Empty array → nullptr. Centralizes the
	 * "graceful fallback when designer hasn't authored every tier yet"
	 * behavior so every moodle source treats out-of-range identically.
	 */
	static class UTexture2D* PickIconForLevel(const TArray<TObjectPtr<class UTexture2D>>& Icons, int32 Level);

	/**
	 * The moodle icon arrays above are designer-authored in the controller blueprint and were empty, so the status strip showed label-only pills. An EMPTY array is filled with the shipped glyph set
	 * (Content/UI/Icons/Moodles/T_Moodle_*, generated by Tools/gen_moodle_icons.py); an array a designer authored is never touched. Idempotent; called before any moodle is built.
	 */
	void EnsureDefaultMoodleIcons();
	bool bDefaultMoodleIconsEnsured = false;

	void BindToPawnSurvivalStatsForMoodles();
	void UnbindFromPawnSurvivalStatsForMoodles();

	UFUNCTION()
	void HandleSurvivalStatChanged(FName StatName, float OldValue, float NewValue);

	/** Push/remove a moodle for the given stat based on its current percent. */
	void RefreshSurvivalStatMoodle(FName StatName);

	// =========================================================================
	// TIMED BODY ACTIONS (UMOTimedActionComponent): a progress bar while the pawn drinks, fills, boils ...
	// =========================================================================

	/** The bar's widget class: any UMOProgressWidgetBase (only its label / progress / time API is used). Unset = the terraform progress widget's class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Character|TimedAction", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOProgressWidgetBase> TimedActionProgressWidgetClass;

	TWeakObjectPtr<class UMOTimedActionComponent> BoundTimedAction;
	FDelegateHandle TimedActionChangedHandle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOProgressWidgetBase> TimedActionProgressWidget;

	FTimerHandle TimedActionProgressTimer;

	void BindToPawnTimedAction();
	void UnbindFromPawnTimedAction();
	void HandleTimedActionChanged(const struct FMOTimedActionState& State);
	void UpdateTimedActionProgress();
	void TearDownTimedActionProgressWidget();

	UFUNCTION()
	void HandleTimedActionWidgetCancelled();

	// =========================================================================
	// ILLNESS MOODLES: every symptomatic condition whose definition shows one (FMOConditionDefinitionRow::bShowsMoodle) -- generic, no per-illness code
	// =========================================================================

	TWeakObjectPtr<class UMOAnatomyComponent> BoundAnatomy;
	FDelegateHandle ConditionsChangedHandle;

	/** Moodle ids this controller pushed for conditions (so a recovered illness's moodle is removed). */
	TSet<FName> ShownConditionMoodles;

	void BindToPawnConditionsForMoodles();
	void UnbindFromPawnConditionsForMoodles();
	void RefreshConditionMoodles();
};
