#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOCharacterUIController.generated.h"

class UMOSkillsPanel;
class UMOStatusPanel;
class UMOInspectionProgressWidget;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;
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

	/** Called when pawn changes to rebind widgets. */
	void OnPawnChanged();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
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

	UFUNCTION()
	void HandleInspectionCompleted(bool bCompleted, const FMOInspectionResult& Result);

	UFUNCTION()
	void HandleInspectionCancelled();
};
