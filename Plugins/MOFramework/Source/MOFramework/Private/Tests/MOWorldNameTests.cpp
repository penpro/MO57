#include "Misc/AutomationTest.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// A world's name went nowhere (N-309): every save wrote the slot's name, so the New Game panel's World name had no reader and the next autosave undid a rename.
// Expected: the name stands through a new world's first save and a rename; failing before: always the slot.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPersistenceWorldNameIsTheSavesName, "MOFramework.Persistence.WorldNameIsTheSavesName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPersistenceWorldNameIsTheSavesName::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	UMOPersistenceSubsystem* Persistence = NewObject<UMOPersistenceSubsystem>(Instance);

	const FString Slot = TEXT("zz_MOWorldNameTest");
	Persistence->ResetForNewWorld(Slot);
	TestEqual(TEXT("a world with no name shows its slot's"), Persistence->ResolveSaveDisplayName(Slot), Slot);

	Persistence->SetWorldDisplayName(TEXT("Alder Hollow"));
	TestEqual(TEXT("a named world's save carries the name"), Persistence->ResolveSaveDisplayName(Slot), FString(TEXT("Alder Hollow")));

	// A rename from the Load list, on disk and on the world being played: the next save keeps it.
	UMOWorldSaveGame* OnDisk = NewObject<UMOWorldSaveGame>(GetTransientPackage());
	OnDisk->DisplayName = Slot;
	if (TestTrue(TEXT("the test's save writes"), UGameplayStatics::SaveGameToSlot(OnDisk, Slot, 0)))
	{
		TestTrue(TEXT("the rename takes"), Persistence->RenameSaveSlot(Slot, FText::FromString(TEXT("Second Name"))));
		TestEqual(TEXT("the world being played has the new name"), Persistence->GetWorldDisplayName(), FString(TEXT("Second Name")));
		TestEqual(TEXT("so its next save keeps it"), Persistence->ResolveSaveDisplayName(Slot), FString(TEXT("Second Name")));

		// CONTROL: a rename of another world's slot leaves this world's name alone.
		Persistence->ResetForNewWorld(TEXT("zz_MOWorldNameOther"));
		Persistence->SetWorldDisplayName(TEXT("This World"));
		TestTrue(TEXT("CONTROL: the other rename takes"), Persistence->RenameSaveSlot(Slot, FText::FromString(TEXT("Third Name"))));
		TestEqual(TEXT("CONTROL: and this world keeps its own"), Persistence->GetWorldDisplayName(), FString(TEXT("This World")));

		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".slotinfo"))));
	}

	Persistence->ResetForNewWorld(TEXT("zz_MOWorldNameNext"));
	TestEqual(TEXT("a new world starts with no name of the last one's"), Persistence->GetWorldDisplayName(), FString());
	return true;
}

#endif
