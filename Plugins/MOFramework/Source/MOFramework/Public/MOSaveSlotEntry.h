/**
 * =============================================================================
 * MOSaveSlotEntry.h - Save/Load Slot Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Individual save slot entry widget for display in save/load panel scroll box.
 * Shows save name, timestamp, playtime, world name, character info, and
 * optional screenshot thumbnail. Inherits CommonButtonBase for click handling.
 *
 * DISPLAY ELEMENTS:
 * - SaveNameText: Display name of save
 * - TimestampText: When saved (e.g., "Jan 27, 2026 3:05 PM")
 * - PlayTimeText: Total playtime (e.g., "2h 35m")
 * - WorldNameText: World/location name
 * - CharacterInfoText: Character summary
 * - ScreenshotImage: Thumbnail preview
 * - AutosaveIndicator: Visible only for autosaves
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SCREENSHOT TEXTURE: CachedScreenshotTexture prevents GC of
 *   dynamically created texture. Cleared when entry is reused.
 *
 * [2024-02] COMMON BUTTON: Inherits UCommonButtonBase. Use NativeOnClicked()
 *   override, not OnClicked delegate.
 *
 * [2024-02] METADATA SOURCE: InitializeFromMetadata() takes FMOSaveMetadata
 *   from MOSavePanel.h. Ensure panel populates metadata before passing.
 *
 * =============================================================================
 * RELATED FILES: MOSavePanel.h, MOPersistenceSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "MOSavePanel.h"
#include "MOSaveSlotEntry.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSaveSlotSelectedSignature, const FString&, SlotName);

/**
 * Individual save slot entry displayed in the save/load scroll box.
 * Inherits from CommonButtonBase to be clickable.
 */
UCLASS()
class MOFRAMEWORK_API UMOSaveSlotEntry : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Initialize this entry with save metadata. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|SaveSlot")
	void InitializeFromMetadata(const FMOSaveMetadata& InMetadata);

	/** Get the slot name for this entry. */
	UFUNCTION(BlueprintPure, Category="MO|UI|SaveSlot")
	FString GetSlotName() const { return Metadata.SlotName; }

	/** Get the full metadata. */
	UFUNCTION(BlueprintPure, Category="MO|UI|SaveSlot")
	const FMOSaveMetadata& GetMetadata() const { return Metadata; }

	/** Called when this slot is selected. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|SaveSlot")
	FMOSaveSlotSelectedSignature OnSlotSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnClicked() override;

	/** Called when metadata is set. Override in BP to customize display. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|SaveSlot")
	void OnMetadataUpdated(const FMOSaveMetadata& NewMetadata);

private:
	void RefreshDisplay();

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_SaveSlotEntry
	// ============================================================

	/** Display name of the save. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SaveNameText;

	/** Timestamp text (e.g., "Jan 27, 2026 3:05 PM"). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimestampText;

	/** Playtime text (e.g., "2h 35m"). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayTimeText;

	/** World/location name. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WorldNameText;

	/** Character info text. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CharacterInfoText;

	/** Screenshot thumbnail. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ScreenshotImage;

	/** Autosave indicator (visible only for autosaves). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> AutosaveIndicator;

	// ============================================================
	// State
	// ============================================================

	UPROPERTY()
	FMOSaveMetadata Metadata;

	/** Cached texture for screenshot (prevents GC). */
	UPROPERTY()
	TObjectPtr<UTexture2D> CachedScreenshotTexture;
};
