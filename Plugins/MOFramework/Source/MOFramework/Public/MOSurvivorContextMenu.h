/**
 * =============================================================================
 * MOSurvivorContextMenu.h - Survivor Right-Click Context Menu
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Context menu shown when right-clicking a recruited survivor. Provides
 * quick access to command survivors (Follow, Stay, Go Home, Open Tasks).
 *
 * BUTTONS:
 * - FollowMeButton: Survivor follows the player
 * - StayHereButton: Survivor stays at current location
 * - GoHomeButton: Survivor returns to assigned home (disabled if no home)
 * - OpenTasksButton: Opens full task assignment menu
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] GO HOME BUTTON: UpdateButtonStates() disables GoHomeButton if
 *   survivor has no home assigned.
 *
 * [2024-02] WEAK POINTERS: TargetSurvivor, CachedController, CachedJobQueue
 *   are weak pointers. Check .IsValid() before use in handlers.
 *
 * [2024-02] DELEGATE: OnOpenTasksRequested fires BEFORE menu closes.
 *   Caller should close menu after handling if needed.
 *
 * =============================================================================
 * RELATED FILES: MOContextMenuBase.h, MOSurvivorController.h, MOSurvivorTaskMenu.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOContextMenuBase.h"
#include "MOSurvivorContextMenu.generated.h"

class UTextBlock;
class UMOCommonButton;
class UMOSurvivorJobQueueComponent;
class AMOSurvivorController;

/**
 * Delegate fired when the "Open Tasks" button is clicked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSurvivorContextMenuOpenTasksSignature, APawn*, Survivor);

/**
 * Context menu shown when right-clicking a recruited survivor.
 *
 * Provides quick access to:
 * - Follow Me: Survivor follows the player
 * - Stay Here: Survivor stays at current location
 * - Go Home: Survivor returns to assigned home
 * - Assign Tasks: Opens the full task assignment menu
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOSurvivorContextMenu : public UMOContextMenuBase
{
	GENERATED_BODY()

public:
	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu for a specific survivor.
	 * @param Survivor - The pawn to control
	 * @param ScreenPosition - Position to display the menu
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Survivor")
	void InitializeForSurvivor(APawn* Survivor, FVector2D ScreenPosition);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when the "Open Tasks" button is clicked. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Survivor")
	FMOSurvivorContextMenuOpenTasksSignature OnOpenTasksRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	/** Survivor's display name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> SurvivorNameText;

	/** Current command/task status. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> CurrentTaskText;

	/** "Follow Me" button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Survivor")
	TObjectPtr<UMOCommonButton> FollowMeButton;

	/** "Stay Here" button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Survivor")
	TObjectPtr<UMOCommonButton> StayHereButton;

	/** "Go Home" button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Survivor")
	TObjectPtr<UMOCommonButton> GoHomeButton;

	/** "Assign Tasks" button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Survivor")
	TObjectPtr<UMOCommonButton> OpenTasksButton;

private:
	/** The survivor pawn this menu is controlling. */
	TWeakObjectPtr<APawn> TargetSurvivor;

	/** Cached survivor controller. */
	TWeakObjectPtr<AMOSurvivorController> CachedController;

	/** Cached job queue component. */
	TWeakObjectPtr<UMOSurvivorJobQueueComponent> CachedJobQueue;

	// Button handlers
	void HandleFollowMeClicked();
	void HandleStayHereClicked();
	void HandleGoHomeClicked();
	void HandleOpenTasksClicked();

	/**
	 * Update button states based on survivor status.
	 * Disables "Go Home" if no home is assigned, etc.
	 */
	void UpdateButtonStates();

	/**
	 * Update the display texts.
	 */
	void UpdateDisplayTexts();

	/**
	 * Get the survivor controller for the target pawn.
	 */
	AMOSurvivorController* GetSurvivorController() const;

	/**
	 * Get the job queue for the target pawn.
	 */
	UMOSurvivorJobQueueComponent* GetJobQueue() const;
};
