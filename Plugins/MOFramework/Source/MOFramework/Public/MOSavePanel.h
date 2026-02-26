/**
 * =============================================================================
 * MOSavePanel.h - Save Game Slot Selection Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Panel for displaying save slots and creating new saves. Shows all saves
 * for the current world in a scrollable list with entry widgets.
 *
 * FEATURES:
 * - Lists existing saves for current world
 * - Create new save button
 * - Overwrite confirmation for existing slots
 * - Broadcasts OnSaveRequested when slot is selected
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WORLD FILTERING: Only shows saves from current world. Don't show
 *   saves from other worlds in the in-game save panel.
 *
 * [2024-02] OVERWRITE FLOW: SaveToSlot should trigger confirmation dialog
 *   before actually overwriting. Panel broadcasts request, caller confirms.
 *
 * [2024-02] ENTRY WIDGETS: Uses UMOSaveSlotEntry for each save. Set
 *   SaveSlotEntryClass in Blueprint defaults.
 *
 * =============================================================================
 * RELATED FILES: MOLoadPanel.h, MOSaveSlotEntry.h, MOPersistenceSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOSaveGameTypes.h"
#include "MOSavePanel.generated.h"

class UMOCommonButton;
class UScrollBox;
class UMOSaveSlotEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOSavePanelRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSavePanelSaveRequestedSignature, const FString&, SlotName);

/**
 * Panel that displays available save slots and allows creating new saves.
 * Shows all saves for the current world in a ScrollBox.
 */
UCLASS()
class MOFRAMEWORK_API UMOSavePanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Refresh the list of saves from disk. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SavePanel")
	void RefreshSaveList();

	/** Get all save metadata for the current world. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SavePanel")
	TArray<FMOSaveMetadata> GetCurrentWorldSaves() const;

	/** Create a new save in the next available slot. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SavePanel")
	void CreateNewSave();

	/** Save to a specific slot (will show overwrite confirmation). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SavePanel")
	void SaveToSlot(const FString& SlotName);

	/** Called when panel requests to close. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|SavePanel")
	FMOSavePanelRequestCloseSignature OnRequestClose;

	/** Called when a save is requested. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|SavePanel")
	FMOSavePanelSaveRequestedSignature OnSaveRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Called when the save list is updated. Override in BP to update custom UI. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|SavePanel")
	void OnSaveListUpdated(const TArray<FMOSaveMetadata>& Saves);

private:
	void PopulateSaveList();
	void ClearSaveList();

	UFUNCTION() void HandleNewSaveClicked();
	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleSlotSelected(const FString& SlotName);

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_SavePanel
	// ============================================================

	/** ScrollBox containing save slot entries. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UScrollBox> SaveSlotsScrollBox;

	/** Button to create a new save. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> NewSaveButton;

	/** Button to go back / close panel. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> BackButton;

	// ============================================================
	// Config
	// ============================================================

	/** Widget class for save slot entries. */
	UPROPERTY(EditDefaultsOnly, Category="MO|UI|SavePanel")
	TSubclassOf<UMOSaveSlotEntry> SaveSlotEntryClass;

	// ============================================================
	// State
	// ============================================================

	UPROPERTY()
	TArray<FMOSaveMetadata> CachedSaves;

	UPROPERTY()
	TArray<TObjectPtr<UMOSaveSlotEntry>> SlotEntryWidgets;
};
