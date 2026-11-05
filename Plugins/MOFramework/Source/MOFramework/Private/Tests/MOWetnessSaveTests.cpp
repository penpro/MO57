// A pawn's wet clothing survives a save (FMOVitalsSaveData::WetnessLevel). Until Nov 2026 it was not saved: soaked when saved was dry when loaded, so quitting was a way to
// dry off (the June audit's L9). Expected: 0.73 back after the round trip; failing before: 0. Run: python Tools/ue.py auto --filter "MOFramework.Medical.Vitals.WetnessSaved"

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "MOVitalsComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_WetnessSaved,
	"MOFramework.Medical.Vitals.WetnessSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_WetnessSaved::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Soaked = NewObject<UMOVitalsComponent>(NewObject<AActor>());
	Soaked->SetWetnessLevel(0.73f);

	FMOVitalsSaveData Saved;
	Soaked->BuildSaveData(Saved);
	TestEqual(TEXT("the save holds the wetness"), Saved.WetnessLevel, 0.73f);

	// Through the bytes, as a save file does it.
	TArray<uint8> Bytes;
	{
		FMemoryWriter Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, false);
		FMOVitalsSaveData::StaticStruct()->SerializeItem(Archive, &Saved, nullptr);
	}
	FMOVitalsSaveData Read;
	{
		FMemoryReader Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOVitalsSaveData::StaticStruct()->SerializeItem(Archive, &Read, nullptr);
	}

	UMOVitalsComponent* Loaded = NewObject<UMOVitalsComponent>(NewObject<AActor>());
	TestTrue(TEXT("the save applies"), Loaded->ApplySaveDataAuthority(Read));
	TestEqual(TEXT("soaked when saved is soaked when loaded"), Loaded->GetWetnessLevel(), 0.73f);

	// A save from before the field: dry, as it always loaded.
	UMOVitalsComponent* Old = NewObject<UMOVitalsComponent>(NewObject<AActor>());
	Old->SetWetnessLevel(0.4f);
	TestTrue(TEXT("an old save applies"), Old->ApplySaveDataAuthority(FMOVitalsSaveData()));
	TestEqual(TEXT("an old save loads dry"), Old->GetWetnessLevel(), 0.0f);
	return true;
}

#endif
