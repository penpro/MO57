/**
 * =============================================================================
 * MOSaveSlotListPanel.h - Shared Base for Save / Load Slot List Panels
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base for any panel that lists save slots and supports per-slot
 * actions (select + rename + delete). Both UMOSavePanel and UMOLoadPanel
 * derive from this — they share ~90% of their logic and would otherwise be
 * copy-paste twins.
 *
 * WHAT THE BASE OWNS:
 * - Scroll box + entry widget creation/binding
 * - Save metadata query (with optional current-world filter) + sort
 * - Refresh flow
 * - Entry-level rename/delete delegate wiring + ConfirmRename/ConfirmDelete
 * - Back button + OnRequestClose
 * - Subclass hook for "what does selecting a slot do"
 *
 * WHAT SUBCLASSES OWN:
 * - The semantic of slot selection (Save = trigger save; Load = trigger load)
 * - Any panel-specific extras (Save: NewSaveButton + OnSaveRequested)
 *
 * BLUEPRINT (WBP_SavePanel / WBP_LoadPanel):
 *   Parent class:   UMOSavePanel or UMOLoadPanel (concrete subclasses)
 *   Required BindWidgets (inherited from this base):
 *     - SaveSlotsScrollBox (UScrollBox)
 *     - BackButton         (UMOCommonButton)
 *   Subclass-specific widgets:
 *     - UMOSavePanel adds NewSaveButton
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] DON'T DUPLICATE BASE BINDINGS: Subclasses must NOT re-declare
 *   SaveSlotsScrollBox or BackButton with their own meta=(BindWidget). The
 *   base owns those. Re-declaring creates two FProperties pointing at the
 *   same widget, which UE compiles but produces undefined runtime behavior.
 *
 * [2026-05] PURE VIRTUAL HOOK: HandleSlotSelected is pure virtual — abstract
 *   on this base, must be overridden in any concrete subclass. The base's
 *   entry-binding loop calls it directly via the entry's OnSlotSelected
 *   delegate.
 *
 * [2026-05] FILTER FIELD: bFilterToCurrentWorld is on the base; subclasses
 *   set it differently in their constructor (Save = true / locked, Load =
 *   true but exposed via SetFilterToCurrentWorld for main-menu use).
 *
 * =============================================================================
 * RELATED FILES: MOSavePanel.h, MOLoadPanel.h, MOSaveSlotEntry.h,
 *                MOPersistenceSubsystem.h
 * LAST UPDATED: 2026-05-23
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOSaveGameTypes.h"
#include "MOSaveSlotEntry.h"  // reuse entry-level delegate signatures
#include "MOSaveSlotListPanel.generated.h"

class UMOCommonButton;
class UScrollBox;
class UMOConfirmationDialog;
class UMOTextInputDialog;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOSaveSlotListRequestCloseSignature);

/**
 * Abstract base for save-slot list panels. See file header for split of
 * responsibilities between base and concrete subclasses.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOSaveSlotListPanel : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	/** Refresh the list of saves from disk and rebuild the entry widgets. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SaveSlotList")
	void RefreshSaveList();

	/**
	 * Apply a rename to a slot's player-visible display name. Called by the
	 * hosting widget after a text-input modal collected NewDisplayName.
	 * Auto-refreshes the list on success.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SaveSlotList")
	bool ConfirmRename(const FString& SlotName, const FText& NewDisplayName);

	/**
	 * Delete a slot. Called by the hosting widget after a confirm modal.
	 * Auto-refreshes the list on success.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SaveSlotList")
	bool ConfirmDelete(const FString& SlotName);

	/** Fired when the back button is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|SaveSlotList")
	FMOSaveSlotListRequestCloseSignature OnRequestClose;

	/**
	 * Fired when the player clicks rename on a slot. The hosting widget
	 * collects the new name via a text-input modal and calls ConfirmRename.
	 * Signature reused from UMOSaveSlotEntry — same payload semantics.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|SaveSlotList")
	FMOSaveSlotRenameRequestedSignature OnSlotRenameRequested;

	/**
	 * Fired when the player clicks delete on a slot. The hosting widget
	 * confirms via modal and calls ConfirmDelete.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|SaveSlotList")
	FMOSaveSlotDeleteRequestedSignature OnSlotDeleteRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * Subclass hook — what happens when the player selects a slot. Save panel
	 * fires OnSaveRequested; Load panel fires OnLoadRequested.
	 */
	virtual void HandleSlotSelected(const FString& SlotName) PURE_VIRTUAL(UMOSaveSlotListPanel::HandleSlotSelected, );

	/**
	 * Called after RefreshSaveList rebuilds the entry widgets. Override in BP
	 * to update any subclass-specific UI (e.g., enabling/disabling other
	 * buttons based on slot count).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|SaveSlotList")
	void OnSaveListUpdated(const TArray<FMOSaveMetadata>& Saves);

	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleEntrySelected(const FString& SlotName);
	UFUNCTION() void HandleEntryRenameRequested(const FString& SlotName);
	UFUNCTION() void HandleEntryDeleteRequested(const FString& SlotName);

	// ========================================================================
	// REQUIRED BIND WIDGETS (subclasses must NOT redeclare these)
	// ========================================================================

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UScrollBox> SaveSlotsScrollBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> BackButton;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Widget class for save slot entries. Set in subclass BP defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|UI|SaveSlotList")
	TSubclassOf<UMOSaveSlotEntry> SaveSlotEntryClass;

	/**
	 * Whether to show only saves matching the current world identifier.
	 * Defaults true (in-game save / load is world-scoped). Main menu load
	 * should set this to false to list all worlds.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|UI|SaveSlotList")
	bool bFilterToCurrentWorld = true;

	/**
	 * Confirmation dialog WBP used for the delete prompt. If set, the panel
	 * auto-spawns this on delete-button click, calls ConfirmDelete on
	 * confirm, and refreshes. If null, the panel just broadcasts
	 * OnSlotDeleteRequested for the consumer to handle.
	 *
	 * Recommended: set to WBP_ConfirmationDialog (or any subclass of
	 * UMOConfirmationBase) in this panel's BP defaults so delete works
	 * out-of-the-box without per-host BP plumbing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|UI|SaveSlotList|Modals")
	TSubclassOf<UMOConfirmationDialog> ConfirmationDialogClass;

	/**
	 * Text-input dialog WBP used for the rename prompt. If set, the panel
	 * auto-spawns it pre-filled with the slot's current display name, calls
	 * ConfirmRename on confirm, and refreshes. If null, the panel just
	 * broadcasts OnSlotRenameRequested.
	 *
	 * Recommended: set to WBP_TextInputDialog (subclass of UMOTextInputDialog)
	 * in this panel's BP defaults.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|UI|SaveSlotList|Modals")
	TSubclassOf<UMOTextInputDialog> TextInputDialogClass;

	// ========================================================================
	// STATE
	// ========================================================================

	UPROPERTY()
	TArray<FMOSaveMetadata> CachedSaves;

	UPROPERTY()
	TArray<TObjectPtr<UMOSaveSlotEntry>> SlotEntryWidgets;

private:
	void PopulateSaveList();
	void ClearSaveList();

	/** Query metadata for all saves, applying the current-world filter. */
	TArray<FMOSaveMetadata> QuerySaveMetadata() const;

	/** Pre-fill text for the rename modal — current display name for SlotName. */
	FText GetDisplayNameForSlot(const FString& SlotName) const;

	/** Bound to the auto-spawned confirmation dialog's OnConfirmed for delete. */
	UFUNCTION()
	void HandleDeleteConfirmed();

	/** Bound to the auto-spawned text-input dialog's OnTextConfirmed for rename. */
	UFUNCTION()
	void HandleRenameConfirmed(const FText& NewName);

	/** Slot the player is currently confirming on. Set when the modal opens, cleared on close. */
	UPROPERTY(Transient)
	FString PendingActionSlotName;
};
