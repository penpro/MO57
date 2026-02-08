#pragma once

#include "CoreMinimal.h"
#include "MOSaveGameTypes.generated.h"

/**
 * Metadata for a save file displayed in the save/load UI.
 * Extracted to separate header to avoid UI<->Subsystem coupling.
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
