// The save slot's list-panel info file (FMOSaveSlotInfo, N-197): what it holds comes back, a damaged or foreign file is refused, and it stops
// counting the moment the save beside it changes.

#include "Misc/AutomationTest.h"
#include "MOSaveSlotInfo.h"
#include "MOWorldSaveGame.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOSaveSlotInfoTests
{
	FMOSaveMetadata Sample()
	{
		FMOSaveMetadata Meta;
		Meta.DisplayName = FText::FromString(TEXT("Riverside camp"));
		Meta.Timestamp = FDateTime(2026, 10, 29, 3, 14, 15);
		Meta.PlayTime = FTimespan::FromSeconds(12345.5);
		Meta.WorldName = TEXT("MOPCGScattering");
		Meta.WorldId = FGuid(1, 2, 3, 4);
		Meta.bIsAutosave = true;
		for (int32 i = 0; i < 300; ++i)
		{
			Meta.ScreenshotData.Add(static_cast<uint8>(i));
		}
		return Meta;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSaveSlotInfoEncode, "MOFramework.Persistence.SlotInfo.Encode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSaveSlotInfoEncode::RunTest(const FString& Parameters)
{
	const FMOSaveMetadata Meta = MOSaveSlotInfoTests::Sample();
	const FDateTime SavTime(2026, 10, 29, 3, 14, 16);
	const TArray<uint8> Bytes = FMOSaveSlotInfo::Encode(Meta, 654321, SavTime);

	FMOSaveMetadata Back;
	int64 Size = -1;
	FDateTime Time;
	TestTrue(TEXT("an info file decodes"), FMOSaveSlotInfo::Decode(Bytes, Back, Size, Time));
	TestEqual(TEXT("the save's size it describes"), Size, static_cast<int64>(654321));
	TestTrue(TEXT("the save's time it describes"), Time == SavTime);
	TestEqual(TEXT("display name"), Back.DisplayName.ToString(), Meta.DisplayName.ToString());
	TestTrue(TEXT("save time"), Back.Timestamp == Meta.Timestamp);
	TestEqual(TEXT("play time"), Back.PlayTime.GetTotalSeconds(), Meta.PlayTime.GetTotalSeconds());
	TestEqual(TEXT("world name"), Back.WorldName, Meta.WorldName);
	TestTrue(TEXT("world id"), Back.WorldId == Meta.WorldId);
	TestTrue(TEXT("autosave"), Back.bIsAutosave);
	TestTrue(TEXT("thumbnail bytes"), Back.ScreenshotData == Meta.ScreenshotData);

	// Damaged or foreign: refused, never half-read (a cut-off file, a file of another layout, a thumbnail length past the end).
	TArray<uint8> Cut = Bytes;
	Cut.SetNum(Bytes.Num() - 10);
	TestFalse(TEXT("a cut-off file is refused"), FMOSaveSlotInfo::Decode(Cut, Back, Size, Time));
	TArray<uint8> Foreign = Bytes;
	Foreign[0] ^= 0xFF;
	TestFalse(TEXT("a file without the magic is refused"), FMOSaveSlotInfo::Decode(Foreign, Back, Size, Time));
	TArray<uint8> Empty;
	TestFalse(TEXT("an empty file is refused"), FMOSaveSlotInfo::Decode(Empty, Back, Size, Time));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSaveSlotInfoOnDisk, "MOFramework.Persistence.SlotInfo.OnDisk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSaveSlotInfoOnDisk::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("MOTest_SlotInfo_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UMOWorldSaveGame* Save = NewObject<UMOWorldSaveGame>();
	Save->DisplayName = TEXT("Test world");
	Save->SaveTimestamp = FDateTime(2026, 10, 29, 4, 0, 0);
	Save->TotalPlayTimeSeconds = 600.0f;
	Save->WorldName = TEXT("TestMap");
	Save->WorldId = FGuid::NewGuid();
	Save->ScreenshotData = { 1, 2, 3, 4, 5 };

	TestTrue(TEXT("the save is written"), UGameplayStatics::SaveGameToSlot(Save, Slot, 0));
	FMOSaveMetadata Meta;
	FMOSaveSlotInfo::FillFromSave(*Save, Slot, Meta);
	TestTrue(TEXT("the info file is written beside it"), FMOSaveSlotInfo::Write(Meta, Slot));

	FMOSaveMetadata Read;
	TestTrue(TEXT("the info file describes the save on disk"), FMOSaveSlotInfo::Read(Slot, Read));
	TestEqual(TEXT("... its slot"), Read.SlotName, Slot);
	TestEqual(TEXT("... its display name"), Read.DisplayName.ToString(), FString(TEXT("Test world")));
	TestTrue(TEXT("... its world id"), Read.WorldId == Save->WorldId);
	TestEqual(TEXT("... its thumbnail"), Read.ScreenshotData.Num(), 5);

	// The save rewritten behind the info file's back (a longer name: another size): the info no longer counts.
	Save->DisplayName = TEXT("Test world, renamed by hand");
	TestTrue(TEXT("the save is rewritten"), UGameplayStatics::SaveGameToSlot(Save, Slot, 0));
	TestFalse(TEXT("a changed save makes its info file stale"), FMOSaveSlotInfo::Read(Slot, Read));

	FMOSaveSlotInfo::Delete(Slot);
	TestFalse(TEXT("deleted: no info"), IFileManager::Get().FileExists(*FMOSaveSlotInfo::InfoPath(Slot)));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	TestFalse(TEXT("no info without its save"), FMOSaveSlotInfo::Read(Slot, Read));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
