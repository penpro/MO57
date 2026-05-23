/**
 * =============================================================================
 * MOLoadPanel.h - Load Game Slot Panel (concrete subclass)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Concrete load panel — inherits all list/filter/rename/delete behavior from
 * UMOSaveSlotListPanel and adds the Load-specific bits: OnLoadRequested when
 * a slot is picked, plus a runtime setter for the current-world filter
 * (main menu wants this off so all worlds are visible).
 *
 * SUBCLASS RESPONSIBILITIES (this file):
 *   - HandleSlotSelected → broadcast OnLoadRequested for the chosen slot
 *   - SetFilterToCurrentWorld for main-menu vs. in-game contexts
 *
 * BASE RESPONSIBILITIES (UMOSaveSlotListPanel):
 *   - ScrollBox + entry widgets + RefreshSaveList
 *   - Rename/Delete delegates + ConfirmRename/ConfirmDelete
 *   - Back button + OnRequestClose
 *   - World filtering (bFilterToCurrentWorld field)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] IN-GAME VS MAIN MENU: bFilterToCurrentWorld defaults true (good
 *   for in-game). Main menu must call SetFilterToCurrentWorld(false) before
 *   opening so all worlds appear.
 *
 * [2024-02] LEVEL TRANSITION: Loading triggers level change. Caller must
 *   handle cleanup + show loading screen — the panel only broadcasts.
 *
 * =============================================================================
 * RELATED FILES: MOSaveSlotListPanel.h, MOSavePanel.h, MOSaveSlotEntry.h
 * LAST UPDATED: 2026-05-23
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOSaveSlotListPanel.h"
#include "MOLoadPanel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOLoadPanelLoadRequestedSignature, const FString&, SlotName);

/**
 * Load-flavored slot panel. Adds OnLoadRequested + runtime world-filter
 * toggle on top of the shared base.
 */
UCLASS()
class MOFRAMEWORK_API UMOLoadPanel : public UMOSaveSlotListPanel
{
	GENERATED_BODY()

public:
	/** Toggle current-world filtering at runtime. Refreshes the list. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|LoadPanel")
	void SetFilterToCurrentWorld(bool bFilter);

	/** Load from a specific slot (broadcasts OnLoadRequested; caller handles transition). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|LoadPanel")
	void LoadFromSlot(const FString& SlotName);

	/** Fired when a load is requested. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|LoadPanel")
	FMOLoadPanelLoadRequestedSignature OnLoadRequested;

protected:
	virtual void HandleSlotSelected(const FString& SlotName) override;
};
