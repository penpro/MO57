/**
 * =============================================================================
 * MOSavePanel.h - Save Game Slot Panel (concrete subclass)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Concrete save panel — inherits all list/filter/rename/delete behavior from
 * UMOSaveSlotListPanel and adds the Save-specific bits: a "New Save" button
 * and an OnSaveRequested delegate fired when the player picks a slot to
 * overwrite. The rest lives on the base.
 *
 * SUBCLASS RESPONSIBILITIES (this file):
 *   - NewSaveButton (BindWidget) + CreateNewSave()
 *   - HandleSlotSelected → broadcast OnSaveRequested for the chosen slot
 *
 * BASE RESPONSIBILITIES (UMOSaveSlotListPanel):
 *   - ScrollBox + entry widgets + RefreshSaveList
 *   - Rename/Delete delegates + ConfirmRename/ConfirmDelete
 *   - Back button + OnRequestClose
 *   - World filtering
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] WBP COMPAT: WBP_SavePanel parents to this class. Inherited
 *   BindWidgets (SaveSlotsScrollBox, BackButton) bind transparently — keep
 *   widget names matching the base's property names.
 *
 * [2024-02] OVERWRITE FLOW: SaveToSlot broadcasts OnSaveRequested. The
 *   hosting widget (UIManager / main menu BP) is responsible for confirming
 *   overwrite via UMOConfirmationBase before calling the persistence save.
 *
 * =============================================================================
 * RELATED FILES: MOSaveSlotListPanel.h, MOLoadPanel.h, MOSaveSlotEntry.h
 * LAST UPDATED: 2026-05-23
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOSaveSlotListPanel.h"
#include "MOSavePanel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSavePanelSaveRequestedSignature, const FString&, SlotName);

/**
 * Save-flavored slot panel. Adds a "New Save" affordance and a save-request
 * delegate on top of the shared list/filter/rename/delete base.
 */
UCLASS()
class MOFRAMEWORK_API UMOSavePanel : public UMOSaveSlotListPanel
{
	GENERATED_BODY()

public:
	/** Create a new save in the next available slot. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SavePanel")
	void CreateNewSave();

	/** Save to a specific slot (broadcasts OnSaveRequested; caller confirms overwrite). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SavePanel")
	void SaveToSlot(const FString& SlotName);

	/** Fired when a save is requested. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|SavePanel")
	FMOSavePanelSaveRequestedSignature OnSaveRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	virtual void HandleSlotSelected(const FString& SlotName) override;

	UFUNCTION() void HandleNewSaveClicked();

	/** Button to create a new save. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<class UMOCommonButton> NewSaveButton;
};
