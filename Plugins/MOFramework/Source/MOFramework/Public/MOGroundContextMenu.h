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
 * =============================================================================
 * RELATED FILES: MOContextMenuBase.h, MOForagingSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOContextMenuBase.h"
#include "MOGroundContextMenu.generated.h"

class UMOCommonButton;
class UTextBlock;
class UPanelWidget;
class AMOWorldItem;
class APawn;

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
class MOFRAMEWORK_API UMOGroundContextMenu : public UMOContextMenuBase
{
	GENERATED_BODY()

public:
	UMOGroundContextMenu(const FObjectInitializer& ObjectInitializer);

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
