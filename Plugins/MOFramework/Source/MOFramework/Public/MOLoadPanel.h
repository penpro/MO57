#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOSavePanel.h"
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
