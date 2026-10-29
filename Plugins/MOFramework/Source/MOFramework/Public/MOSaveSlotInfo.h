// A save's list-panel facts in a small file beside it (N-197): <Slot>.slotinfo next to <Slot>.sav in Saved/SaveGames holds the display name,
// time, play time, world, autosave flag and thumbnail, and the size and time of the .sav it was written for. The save format has no separate
// header, so without it the Load panel's first open deserialized every world save whole (0.6-0.9 MB each) for a name and a picture.
//
// One rule keeps it honest: the info counts only while the .sav on disk still has the size and time it recorded. Anything that rewrites,
// replaces or copies the save (a save, a rename, a thumbnail, a file copied from another machine) makes it stale, and the caller reads the
// save whole and writes it afresh. UMOPersistenceSubsystem writes it at its one save chokepoint (WriteSaveSlot) and on every whole read.

#pragma once

#include "CoreMinimal.h"
#include "MOSaveGameTypes.h"

class UMOWorldSaveGame;

struct MOFRAMEWORK_API FMOSaveSlotInfo
{
	/** Saved/SaveGames/<Slot>.sav, where the engine's save system writes the slot. */
	static FString SavePath(const FString& SlotName);

	/** Saved/SaveGames/<Slot>.slotinfo. */
	static FString InfoPath(const FString& SlotName);

	/** The list-panel facts of a loaded save (the one place a save becomes metadata). */
	static void FillFromSave(const UMOWorldSaveGame& Save, const FString& SlotName, FMOSaveMetadata& OutMetadata);

	/** Writes the info file for the slot's .sav as it is on disk now (call right after the .sav is written). False if there is no .sav or the write failed. */
	static bool Write(const FMOSaveMetadata& Metadata, const FString& SlotName);

	/** The slot's info, if its file exists, parses and still describes the .sav on disk (same size and time). */
	static bool Read(const FString& SlotName, FMOSaveMetadata& OutMetadata);

	/** Removes the slot's info file (with the save). */
	static void Delete(const FString& SlotName);

	/** The bytes of an info file and back (pure, for the tests): SavSize / SavTime are the .sav's size and time it describes. */
	static TArray<uint8> Encode(const FMOSaveMetadata& Metadata, int64 SavSize, const FDateTime& SavTime);
	static bool Decode(const TArray<uint8>& Bytes, FMOSaveMetadata& OutMetadata, int64& OutSavSize, FDateTime& OutSavTime);
};
