/**
 * =============================================================================
 * MOSaveGameTypes.h - Save System Type Definitions
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines types used by the save/load system UI. Extracted to separate header
 * to avoid circular dependencies between UI widgets and MOPersistenceSubsystem.
 *
 * KEY TYPES:
 * - FMOSaveMetadata: Displayed in save/load UI (slot name, timestamp, etc.)
 *
 * WHY SEPARATE FILE:
 * Save UI widgets need FMOSaveMetadata but shouldn't include the full
 * MOPersistenceSubsystem.h (heavy dependencies). This file provides
 * lightweight access to save-related types.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SCREENSHOT DATA: ScreenshotData is PNG compressed, 80x80 pixels.
 *   Use UMOUIUtils::LoadTextureFromData() to convert to UTexture2D for display.
 *
 * [2024-02] PLAYTIME: PlayTime is FTimespan, NOT float seconds. Use
 *   PlayTime.GetTotalSeconds() or format methods for display.
 *
 * [2024-02] DEPRECATED FIELD: ScreenshotPath is deprecated. New saves use
 *   ScreenshotData (embedded). Keep ScreenshotPath for legacy save compat.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOPersistenceSubsystem.h - Generates and uses this metadata
 * - MOSavePanel.h / MOLoadPanel.h - UI that displays this metadata
 * - MOSaveSlotEntry.h - Widget for individual save slot display
 * - MOworldSaveGame.h - Full save game data structures
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOSaveGameTypes.generated.h"

/**
 * Metadata for a save file displayed in the save/load UI.
 * See file header for usage notes.
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

	/** Path to screenshot thumbnail (if any). DEPRECATED - use ScreenshotData instead. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	FString ScreenshotPath;

	/** Screenshot thumbnail data (PNG compressed, 80x80). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Save")
	TArray<uint8> ScreenshotData;
};
