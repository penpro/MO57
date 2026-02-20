#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOQuestUIController.generated.h"

class UMOQuestLogPanel;
class UMOQuestHUDWidget;
class UMOQuestSubsystem;

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
};
