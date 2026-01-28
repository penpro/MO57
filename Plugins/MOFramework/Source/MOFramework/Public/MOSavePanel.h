#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOSavePanel.generated.h"

class UMOCommonButton;
class UScrollBox;
class UMOSaveSlotEntry;

/**
 * Metadata for a save file displayed in the save/load UI.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSaveMetadata
{
	GENERATED_BODY()

	/** The save slot name/ID. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FString SlotName;

	/** Display name for the save. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FText DisplayName;

	/** Timestamp when the save was created. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FDateTime Timestamp;

	/** Total playtime at the time of save. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FTimespan PlayTime;

	/** World/level name. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FString WorldName;

	/** Player character name or description. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FString CharacterInfo;

	/** Whether this is an autosave. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	bool bIsAutosave = false;

	/** Path to screenshot thumbnail (if any). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FString ScreenshotPath;
};

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
