/**
 * =============================================================================
 * MOLoadPanel.h - Load Game Slot Selection Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Panel for displaying and selecting saves to load. Can filter to current
 * world only (in-game) or show all saves (main menu).
 *
 * FEATURES:
 * - Lists saves with metadata (date, playtime, etc.)
 * - Optional world filtering for in-game use
 * - Load confirmation before switching
 * - Broadcasts OnLoadRequested when slot selected
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] IN-GAME VS MAIN MENU: bFilterToCurrentWorld is true by default.
 *   Main menu load panel should set this to false to show all worlds.
 *
 * [2024-02] LEVEL TRANSITION: Loading triggers level change. Caller must
 *   handle cleanup and show loading screen.
 *
 * [2024-02] ENTRY WIDGETS: Uses same UMOSaveSlotEntry as save panel.
 *   Ensure SaveSlotEntryClass is set in Blueprint.
 *
 * =============================================================================
 * RELATED FILES: MOSavePanel.h, MOSaveSlotEntry.h, MOPersistenceSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOSaveGameTypes.h"
#include "MOLoadPanel.generated.h"

class UMOCommonButton;
class UScrollBox;
class UMOSaveSlotEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOLoadPanelRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOLoadPanelLoadRequestedSignature, const FString&, SlotName);

/**
 * Panel that displays available saves and allows loading.
 * When opened in-game, shows only saves from the current world.
 */
UCLASS()
class MOFRAMEWORK_API UMOLoadPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Refresh the list of saves from disk. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|LoadPanel")
	void RefreshSaveList();

	/** Set whether to filter to current world only. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|LoadPanel")
	void SetFilterToCurrentWorld(bool bFilter);

	/** Load from a specific slot (will show confirmation first). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|LoadPanel")
	void LoadFromSlot(const FString& SlotName);

	/** Called when panel requests to close. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|LoadPanel")
	FMOLoadPanelRequestCloseSignature OnRequestClose;

	/** Called when a load is requested. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|LoadPanel")
	FMOLoadPanelLoadRequestedSignature OnLoadRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Called when the save list is updated. Override in BP to update custom UI. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|LoadPanel")
	void OnSaveListUpdated(const TArray<FMOSaveMetadata>& Saves);

private:
	void PopulateSaveList();
	void ClearSaveList();

	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleSlotSelected(const FString& SlotName);

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_LoadPanel
	// ============================================================

	/** ScrollBox containing save slot entries. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UScrollBox> SaveSlotsScrollBox;

	/** Button to go back / close panel. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> BackButton;

	// ============================================================
	// Config
	// ============================================================

	/** Widget class for save slot entries. */
	UPROPERTY(EditDefaultsOnly, Category="MO|UI|LoadPanel")
	TSubclassOf<UMOSaveSlotEntry> SaveSlotEntryClass;

	/** Whether to filter saves to current world only (default true for in-game). */
	UPROPERTY(EditDefaultsOnly, Category="MO|UI|LoadPanel")
	bool bFilterToCurrentWorld = true;

	// ============================================================
	// State
	// ============================================================

	UPROPERTY()
	TArray<FMOSaveMetadata> CachedSaves;

	UPROPERTY()
	TArray<TObjectPtr<UMOSaveSlotEntry>> SlotEntryWidgets;
};
