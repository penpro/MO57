/**
 * =============================================================================
 * MOQuestUIController.h - Quest UI Controller Component
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Specialized UI controller for quest-related UI. Manages quest log panel
 * (full quest list/details) and quest HUD widget (tracked objectives).
 * Sibling component on player controller alongside other UI controllers.
 *
 * MANAGED WIDGETS:
 * - MOQuestLogPanel: Full quest log (toggle via ToggleQuestLog)
 * - MOQuestHUDWidget: HUD tracker (auto-created on BeginPlay if configured)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] HUD AUTO-CREATE: bCreateQuestHUDOnBeginPlay controls whether
 *   quest HUD is created automatically. Set false for main menu.
 *
 * [2024-02] Z-ORDER: QuestLogPanelZOrder (50) and QuestHUDZOrder (5) set
 *   widget layer ordering. HUD should be lower to appear behind menus.
 *
 * [2024-02] CONTROLLER BASE: Inherits from MOUIControllerBase, not
 *   UActorComponent. Has access to GetPawn, GetPlayerController, etc.
 *
 * =============================================================================
 * RELATED FILES: MOUIControllerBase.h, MOQuestLogPanel.h, MOQuestHUDWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOQuestUIController.generated.h"

class UMOQuestLogPanel;
class UMOQuestHUDWidget;
class UMOQuestSubsystem;
class UMOTutorialHintWidget;

/**
 * Specialized UI controller for quest-related UI.
 *
 * Handles:
 * - Quest log panel (full quest list and details)
 * - Quest HUD widget (tracked objectives on screen)
 *
 * This controller is a sibling component on the player controller,
 * alongside MOCharacterUIController, MOCraftingUIController, etc.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOQuestUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOQuestUIController();

	// ==========================================================================
	// QUEST LOG PANEL
	// ==========================================================================

	/** Toggle quest log visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void ToggleQuestLog();

	/** Open the quest log. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void OpenQuestLog();

	/** Close the quest log. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void CloseQuestLog();

	/** Check if quest log is open. */
	UFUNCTION(BlueprintPure, Category="MO|Quest|UI")
	bool IsQuestLogOpen() const;

	/** Get the quest log widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|Quest|UI")
	UMOQuestLogPanel* GetQuestLog() const;

	// ==========================================================================
	// QUEST HUD WIDGET
	// ==========================================================================

	/** Show the quest HUD tracker. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void ShowQuestHUD();

	/** Hide the quest HUD tracker. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void HideQuestHUD();

	/** Check if quest HUD is visible. */
	UFUNCTION(BlueprintPure, Category="MO|Quest|UI")
	bool IsQuestHUDVisible() const;

	/** Get the quest HUD widget (may be null if not created). */
	UFUNCTION(BlueprintPure, Category="MO|Quest|UI")
	UMOQuestHUDWidget* GetQuestHUD() const;

	/** Create and show the quest HUD. Called during BeginPlay if configured. */
	void CreateQuestHUD();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Frame-based debounce to prevent double-toggle from ECommonInputMode::All */
	uint64 LastToggleFrame = 0;

	// --- Quest Log Panel ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOQuestLogPanel> QuestLogPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 QuestLogPanelZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOQuestLogPanel> QuestLogPanelWidget;

	UFUNCTION()
	void HandleQuestLogRequestClose();

	// --- Quest HUD Widget ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOQuestHUDWidget> QuestHUDWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 QuestHUDZOrder = 5;

	/** Whether to create quest HUD on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI", meta=(AllowPrivateAccess="true"))
	bool bCreateQuestHUDOnBeginPlay = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOQuestHUDWidget> QuestHUDWidget;

	// --- Tutorial Hint Widget ---

	/**
	 * Blueprint class for the tutorial hint banner shown top-center of the
	 * HUD whenever an active tutorial objective has bShowAsTutorialPopup=true.
	 * Assign WBP_TutorialHint here in the controller defaults.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI|Tutorial", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOTutorialHintWidget> TutorialHintWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI|Tutorial", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 TutorialHintZOrder = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI|Tutorial", meta=(AllowPrivateAccess="true"))
	bool bCreateTutorialHintOnBeginPlay = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOTutorialHintWidget> TutorialHintWidget;

	/** Create and add the tutorial hint widget to the viewport. */
	void CreateTutorialHintWidget();
};
