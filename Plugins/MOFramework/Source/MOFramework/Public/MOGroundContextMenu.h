/**
 * =============================================================================
 * MOGroundContextMenu.h - Ground Right-Click Context Menu
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Context menu shown when right-clicking on ground (no interactable target).
 * Provides options for searching nearby items and digging for supplies.
 * Uses foraging skill level to determine search radius.
 *
 * BUTTONS:
 * - SearchNearbyButton: Search for nearby items
 * - DigForSuppliesButton: Dig for buried supplies
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SCREEN POSITION: SetMenuPosition() ignores parameter and uses
 *   actual mouse position for accuracy.
 *
 * [2024-02] FORAGING PAWN: ForagingPawn weak pointer must be valid when
 *   button handlers execute. Check .IsValid() before use.
 *
 * [2024-02] MOUSE TRACKING: Uses timer-based mouse check like MOItemContextMenu.
 *   AutoCloseDelay determines hover-out-to-close delay.
 *
 * [2026-06] (H22) TIMED ACTION: Clicking Search/Dig no longer executes instantly.
 *   It starts a timed, interruptible foraging action (ForagingActionDuration);
 *   items + XP are only granted on completion in FinishForagingAction(). Movement
 *   or other interrupts (IMOInterruptibleInterface) cancel it with no reward.
 *   ShouldCloseOnMouseLeave() is suppressed while the action runs (mirrors
 *   UMOGhostContextMenu's build-timer guard) so cursor drift doesn't cancel it.
 *
 * =============================================================================
 * RELATED FILES: MOContextMenuBase.h, MOForagingSubsystem.h, MOGhostContextMenu.h
 * LAST UPDATED: 2026-06-30 (H22 timed/interruptible foraging)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOContextMenuBase.h"
#include "MOInterruptibleInterface.h"
#include "MOGroundContextMenu.generated.h"

class UMOCommonButton;
class UTextBlock;
class UPanelWidget;
class AMOWorldItem;
class AMOCharacter;
class APawn;

/**
 * (H22) Which foraging action is currently being performed as a timed action.
 */
UENUM()
enum class EMOGroundForageAction : uint8
{
	None,
	SearchNearby,
	DigForSupplies
};

/**
 * Delegate fired when a foraging action completes.
 * @param RevealedItems - Array of world items that were revealed/spawned
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOForagingCompleteSignature, const TArray<AMOWorldItem*>&, RevealedItems);

/**
 * Context menu shown when right-clicking on ground (no interactable target).
 * Provides options for searching nearby items and digging for supplies.
 *
 * Blueprint Setup:
 * - Create WBP_GroundContextMenu with parent UMOGroundContextMenu
 * - Add SearchNearbyButton (UMOCommonButton) - "Search Nearby"
 * - Add DigForSuppliesButton (UMOCommonButton) - "Dig for Supplies"
 * - Add RadiusText (UTextBlock, optional) - Shows search radius
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOGroundContextMenu : public UMOContextMenuBase,
	public IMOInterruptibleInterface
{
	GENERATED_BODY()

public:
	UMOGroundContextMenu(const FObjectInitializer& ObjectInitializer);

	/**
	 * (H22) IMOInterruptibleInterface: cancels an in-progress timed foraging action
	 * on any meaningful disturbance (movement, damage, death, etc.). Foraging is a
	 * stay-put activity with no resumable state, so the action is discarded — no
	 * items spawned, no XP granted.
	 */
	virtual void NotifyInterrupt_Implementation(const FMOInterruptContext& Context) override;

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu for a specific world location.
	 * @param WorldLocation - Location in world space where player clicked
	 * @param ForagingPawn - The pawn performing the foraging action
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	void InitializeForLocation(FVector WorldLocation, APawn* ForagingPawn);

	/**
	 * Position the menu at the current mouse cursor location.
	 * @param ScreenPosition - Ignored, uses actual mouse position for accuracy
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	void SetMenuPosition(FVector2D ScreenPosition);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Called when Search Nearby completes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Foraging")
	FMOForagingCompleteSignature OnSearchComplete;

	/** Called when Dig for Supplies completes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Foraging")
	FMOForagingCompleteSignature OnDigComplete;

	// ============================================================================
	// GETTERS
	// ============================================================================

	/** Get the world location this menu was opened for. */
	UFUNCTION(BlueprintPure, Category="MO|Foraging")
	FVector GetTargetLocation() const { return TargetLocation; }

	/** Get the calculated search radius. */
	UFUNCTION(BlueprintPure, Category="MO|Foraging")
	float GetSearchRadius() const { return SearchRadius; }

	/** Get the foraging pawn. */
	UFUNCTION(BlueprintPure, Category="MO|Foraging")
	APawn* GetForagingPawn() const { return ForagingPawn.Get(); }

	/** Get the foraging skill level. */
	UFUNCTION(BlueprintPure, Category="MO|Foraging")
	int32 GetForagingLevel() const { return ForagingLevel; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * (H22) Keep the menu open while a timed foraging action is running, so a tiny
	 * cursor drift doesn't cancel a multi-second dig. Mirrors UMOGhostContextMenu's
	 * build-timer guard.
	 */
	virtual bool ShouldCloseOnMouseLeave() const override;

	/**
	 * (H22) Seconds a foraging action takes to complete. Foraging is no longer an
	 * instant, free-XP world action — the action runs for this duration and can be
	 * interrupted (movement, damage) before any items/XP are granted.
	 */
	UPROPERTY(EditDefaultsOnly, Category="MO|Foraging", meta=(ClampMin="0.1"))
	float ForagingActionDuration = 3.0f;

	// ============================================================================
	// BOUND WIDGETS
	// ============================================================================

	/** Container panel that holds all the buttons. Used for mouse-over detection. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UPanelWidget> ButtonContainer;

	/** Button to search for nearby items. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> SearchNearbyButton;

	/** Button to dig for supplies. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> DigForSuppliesButton;

	/** Optional text showing the search radius. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RadiusText;

	/** Optional text showing the foraging skill level. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillLevelText;

private:
	// ============================================================================
	// STATE
	// ============================================================================

	/** World location where the player clicked. */
	FVector TargetLocation;

	/** The pawn performing the foraging action. */
	TWeakObjectPtr<APawn> ForagingPawn;

	/** Calculated search radius based on skill. */
	float SearchRadius = 0.0f;

	/** Foraging skill level of the pawn. */
	int32 ForagingLevel = 0;

	// ============================================================================
	// (H22) TIMED FORAGING ACTION STATE
	// ============================================================================

	/** Which action is running (None when idle). */
	EMOGroundForageAction PendingForageAction = EMOGroundForageAction::None;

	/** Elapsed time into the current foraging action. */
	float ForageActionElapsed = 0.0f;

	/** Timer handle driving the foraging action progress. */
	FTimerHandle ForageActionTimerHandle;

	/** Character we registered with for interrupt notifications (for movement-cancel). */
	TWeakObjectPtr<AMOCharacter> RegisteredForageCharacter;

	// ============================================================================
	// BUTTON HANDLERS
	// ============================================================================

	/** Handle Search Nearby button click. */
	UFUNCTION()
	void HandleSearchNearbyClicked();

	/** Handle Dig for Supplies button click. */
	UFUNCTION()
	void HandleDigForSuppliesClicked();

	/** Update the UI text displays. */
	void UpdateDisplayText();

	// ============================================================================
	// (H22) TIMED FORAGING ACTION
	// ============================================================================

	/**
	 * Begin a timed, interruptible foraging action. Disables buttons, suppresses
	 * mouse-leave close, registers for pawn interrupts, and starts the action timer.
	 * The foraging primitive (which spawns items + grants XP) only runs on
	 * completion in FinishForagingAction().
	 */
	void StartForagingAction(EMOGroundForageAction Action);

	/** Timer callback: advances elapsed time and completes when the duration is met. */
	void TickForagingAction();

	/** Completion: invokes the foraging primitive for the pending action, then closes. */
	void FinishForagingAction();

	/** Cancel an in-progress action (interrupt / destruct). No items, no XP. */
	void CancelForagingAction();

	/** True while a timed foraging action is running. */
	bool IsForagingActionInProgress() const { return PendingForageAction != EMOGroundForageAction::None; }

	/** Register/unregister this menu as an interrupt listener on the foraging character. */
	void RegisterForagingInterrupts();
	void UnregisterForagingInterrupts();

	// ============================================================================
	// MOUSE TRACKING
	// ============================================================================

	/** Check if mouse is currently over the menu. */
	bool IsMouseOverMenu() const;

	/** Start the mouse check timer. */
	void StartMouseCheckTimer();

	/** Stop the mouse check timer. */
	void StopMouseCheckTimer();

	/** Called by timer to check mouse position. */
	void CheckMousePosition();

	/** Timer handle for mouse position check. */
	FTimerHandle MouseCheckTimerHandle;

	/** Time since mouse left the menu. */
	float MouseOutsideTimer = 0.0f;

	/** Delay before auto-closing when mouse leaves (seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Foraging")
	float AutoCloseDelay = 0.15f;
};
