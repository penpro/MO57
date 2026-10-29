#include "MOSaveSlotInfo.h"

#include "MOWorldSaveGame.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace MOSaveSlotInfoPrivate
{
	/** "MOSI" and a version: a file from an older layout reads as stale (the save is read whole once and the file rewritten). */
	constexpr uint32 Magic = 0x49534F4D;
	constexpr int32 Version = 1;
	/** A thumbnail is 128 x 128 PNG (a few tens of KB); anything near this is not an info file. */
	constexpr int64 MaxFileBytes = 4 * 1024 * 1024;
}

FString FMOSaveSlotInfo::SavePath(const FString& SlotName)
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".sav"));
}

FString FMOSaveSlotInfo::InfoPath(const FString& SlotName)
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".slotinfo"));
}

void FMOSaveSlotInfo::FillFromSave(const UMOWorldSaveGame& Save, const FString& SlotName, FMOSaveMetadata& OutMetadata)
{
	OutMetadata.SlotName = SlotName;
	OutMetadata.DisplayName = FText::FromString(Save.DisplayName.IsEmpty() ? SlotName : Save.DisplayName);
	OutMetadata.Timestamp = Save.SaveTimestamp;
	OutMetadata.PlayTime = FTimespan::FromSeconds(Save.TotalPlayTimeSeconds);
	OutMetadata.WorldName = Save.WorldName;
	OutMetadata.WorldId = Save.WorldId;
	OutMetadata.bIsAutosave = Save.bIsAutosave;
	OutMetadata.ScreenshotData = Save.ScreenshotData;
	OutMetadata.ScreenshotPath = FString();   // deprecated
}

TArray<uint8> FMOSaveSlotInfo::Encode(const FMOSaveMetadata& Metadata, int64 SavSize, const FDateTime& SavTime)
{
	TArray<uint8> Bytes;
	FMemoryWriter Ar(Bytes);
	uint32 Magic = MOSaveSlotInfoPrivate::Magic;
	int32 Version = MOSaveSlotInfoPrivate::Version;
	int64 Ticks = SavTime.GetTicks();
	FString DisplayName = Metadata.DisplayName.ToString();
	double PlayTimeSeconds = Metadata.PlayTime.GetTotalSeconds();
	int64 SaveTicks = Metadata.Timestamp.GetTicks();
	FString WorldName = Metadata.WorldName;
	FGuid WorldId = Metadata.WorldId;
	bool bAutosave = Metadata.bIsAutosave;
	TArray<uint8> Screenshot = Metadata.ScreenshotData;
	Ar << Magic << Version << SavSize << Ticks << DisplayName << SaveTicks << PlayTimeSeconds << WorldName << WorldId << bAutosave << Screenshot;
	return Bytes;
}

bool FMOSaveSlotInfo::Decode(const TArray<uint8>& Bytes, FMOSaveMetadata& OutMetadata, int64& OutSavSize, FDateTime& OutSavTime)
{
	FMemoryReader Ar(Bytes);
	uint32 Magic = 0;
	int32 Version = 0;
	Ar << Magic << Version;
	if (Ar.IsError() || Magic != MOSaveSlotInfoPrivate::Magic || Version != MOSaveSlotInfoPrivate::Version)
	{
		return false;
	}
	int64 Ticks = 0;
	FString DisplayName;
	int64 SaveTicks = 0;
	double PlayTimeSeconds = 0.0;
	FString WorldName;
	FGuid WorldId;
	bool bAutosave = false;
	TArray<uint8> Screenshot;
	Ar << OutSavSize << Ticks << DisplayName << SaveTicks << PlayTimeSeconds << WorldName << WorldId << bAutosave;
	// The thumbnail's length is checked before the array is sized from it: a damaged file must not ask for gigabytes.
	int32 ScreenshotBytes = 0;
	Ar << ScreenshotBytes;
	if (Ar.IsError() || ScreenshotBytes < 0 || ScreenshotBytes > Ar.TotalSize() - Ar.Tell())
	{
		return false;
	}
	Screenshot.SetNumUninitialized(ScreenshotBytes);
	Ar.Serialize(Screenshot.GetData(), ScreenshotBytes);
	if (Ar.IsError())
	{
		return false;
	}
	OutSavTime = FDateTime(Ticks);
	OutMetadata.DisplayName = FText::FromString(DisplayName);
	OutMetadata.Timestamp = FDateTime(SaveTicks);
	OutMetadata.PlayTime = FTimespan::FromSeconds(PlayTimeSeconds);
	OutMetadata.WorldName = WorldName;
	OutMetadata.WorldId = WorldId;
	OutMetadata.bIsAutosave = bAutosave;
	OutMetadata.ScreenshotData = MoveTemp(Screenshot);
	OutMetadata.ScreenshotPath = FString();
	return true;
}

bool FMOSaveSlotInfo::Write(const FMOSaveMetadata& Metadata, const FString& SlotName)
{
	IFileManager& FileManager = IFileManager::Get();
	const FString Sav = SavePath(SlotName);
	const int64 SavSize = FileManager.FileSize(*Sav);
	if (SavSize < 0)
	{
		return false;
	}
	return FFileHelper::SaveArrayToFile(Encode(Metadata, SavSize, FileManager.GetTimeStamp(*Sav)), *InfoPath(SlotName));
}

bool FMOSaveSlotInfo::Read(const FString& SlotName, FMOSaveMetadata& OutMetadata)
{
	IFileManager& FileManager = IFileManager::Get();
	const FString Sav = SavePath(SlotName);
	const FString Info = InfoPath(SlotName);
	const int64 SavSize = FileManager.FileSize(*Sav);
	const int64 InfoSize = FileManager.FileSize(*Info);
	if (SavSize < 0 || InfoSize <= 0 || InfoSize > MOSaveSlotInfoPrivate::MaxFileBytes)
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Info, FILEREAD_Silent))
	{
		return false;
	}
	FMOSaveMetadata Decoded;
	int64 RecordedSize = -1;
	FDateTime RecordedTime;
	if (!Decode(Bytes, Decoded, RecordedSize, RecordedTime) || RecordedSize != SavSize || RecordedTime != FileManager.GetTimeStamp(*Sav))
	{
		return false;   // another layout, damaged, or the save changed since: read it whole
	}
	OutMetadata = MoveTemp(Decoded);
	OutMetadata.SlotName = SlotName;
	if (OutMetadata.DisplayName.IsEmpty())
	{
		OutMetadata.DisplayName = FText::FromString(SlotName);
	}
	return true;
}

void FMOSaveSlotInfo::Delete(const FString& SlotName)
{
	IFileManager::Get().Delete(*InfoPath(SlotName), /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
}
