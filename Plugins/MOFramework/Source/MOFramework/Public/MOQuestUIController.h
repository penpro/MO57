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
 * [2026-10] TUTORIAL TEXT YIELDS TO MENUS: the hint banner and the quest tracker
 *   hide while any menu is open (SetHUDYieldedToMenus, driven by
 *   UMOUIManagerComponent::UpdateReticleVisibility). Two traps:
 *   (1) a controller's UpdateReticleVisibility() right after PushWidgetToLayer
 *   reads the menu count BEFORE CommonUI lists the widget as active (count 0), so
 *   the only call that sees the menu is the one in UMOActivatableWidget::
 *   NativeOnActivated -- do not remove it. (2) Opening one menu closes the others,
 *   so Inventory -> Crafting is a switch; the flip counter (GetHUDYieldFlipCount)
 *   is how HUD.TutorialTextYieldsToMenus proves it never flashes back in between.
 *   The hint widget keeps its own "suppressed" flag because a tutorial objective
 *   completing under an open menu re-pulls the hint by itself.
 *
 * =============================================================================
 * RELATED FILES: MOUIControllerBase.h, MOQuestLogPanel.h, MOQuestHUDWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "Components/SlateWrapperTypes.h"
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

	/** Get the tutorial hint banner widget (may be null if not created). */
	UFUNCTION(BlueprintPure, Category="MO|Quest|UI")
	UMOTutorialHintWidget* GetTutorialHint() const { return TutorialHintWidget.Get(); }

	/** Create and show the quest HUD. Called during BeginPlay if configured. */
	void CreateQuestHUD();

	// ==========================================================================
	// HUD YIELDS TO MENUS
	// ==========================================================================

	/**
	 * The tutorial text on the HUD (the hint banner and the quest tracker) goes away while any menu is open and comes back when the last one closes. Called by
	 * UMOUIManagerComponent::UpdateReticleVisibility -- the one chokepoint every menu open/close path already runs through -- so a new menu needs no code here.
	 */
	void SetHUDYieldedToMenus(bool bMenuOpen);

	/** How many times the tutorial text has switched between hidden and shown because of menus (diagnostics + tests: switching menus must not flash it back). */
	int32 GetHUDYieldFlipCount() const { return HUDYieldFlipCount; }

	/** The quest tracker's visibility rule in one pure function (unit-tested): hidden when nobody wants it shown, or when a menu is open and yielding is enabled. */
	static ESlateVisibility ComputeQuestHUDVisibility(bool bWantedVisible, bool bMenuOpen, bool bYieldEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Frame-based debounce to prevent double-toggle from ECommonInputMode::All */
	uint64 LastToggleFrame = 0;

	/**
	 * Subscribed to UMOQuestSubsystem::OnQuestSystemReady. Spawns the quest
	 * HUD + tutorial hint widgets once the subsystem has loaded its DataTable.
	 * Called immediately if the subsystem is already ready when BeginPlay runs.
	 */
	UFUNCTION()
	void HandleQuestSystemReady();

	/**
	 * Actually spawn the BeginPlay-time widgets (Quest HUD + tutorial hint).
	 * Called from HandleQuestSystemReady. Idempotent — re-entry is a no-op if
	 * the widgets already exist.
	 */
	void SpawnReadyTimeWidgets();

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

	/** Hide the quest tracker while any menu is open (it returns when the last menu closes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI", meta=(AllowPrivateAccess="true"))
	bool bHideQuestHUDWhileMenuOpen = true;

	/** Hide the tutorial hint banner while any menu is open (it returns when the last menu closes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI|Tutorial", meta=(AllowPrivateAccess="true"))
	bool bHideTutorialHintWhileMenuOpen = true;

	/** Whether the quest tracker is meant to be on screen (ShowQuestHUD / HideQuestHUD), independent of a menu hiding it for a while. */
	bool bQuestHUDWantedVisible = true;

	/** Last state passed to SetHUDYieldedToMenus; applied to widgets created while a menu is already open. */
	bool bMenuOpenForHUD = false;

	int32 HUDYieldFlipCount = 0;

	/** Push the current wanted/yielded state onto the quest tracker widget. */
	void ApplyQuestHUDVisibility();

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
