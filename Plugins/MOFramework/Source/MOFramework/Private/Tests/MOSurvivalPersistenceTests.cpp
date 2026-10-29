#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MOSurvivalStatsComponent.h"
#include "MOPersistenceSubsystem.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MOworldSaveGame.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace MOSurvivalPersistenceTests
{
	struct FWorld
	{
		UWorld* World;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOSurvivalPersistenceWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		UMOSurvivalStatsComponent* Spawn()
		{
			auto* Pawn = World->SpawnActor<APawn>();
			auto* Stats = NewObject<UMOSurvivalStatsComponent>(Pawn);
			Pawn->AddInstanceComponent(Stats);
			Stats->RegisterComponent();
			Pawn->DispatchBeginPlay();
			return Stats;
		}
	};
	FMOPersistedPawnRecord RoundTrip(FMOPersistedPawnRecord Record)
	{
		TArray<uint8> Bytes;
		{
			FMemoryWriter Memory(Bytes);
			FObjectAndNameAsStringProxyArchive Archive(Memory, false);
			FMOPersistedPawnRecord::StaticStruct()->SerializeItem(Archive, &Record, nullptr);
		}
		FMOPersistedPawnRecord Loaded;
		FMemoryReader Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOPersistedPawnRecord::StaticStruct()->SerializeItem(Archive, &Loaded, nullptr);
		return Loaded;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalSaveRoundTrip, "MOFramework.Survival.Persistence.RoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSurvivalSaveRoundTrip::RunTest(const FString&)
{
	using namespace MOSurvivalPersistenceTests;
	FWorld World;
	auto* Source = World.Spawn();
	auto* Target = World.Spawn();
	Source->SetStat(TEXT("Health"), 33);
	Source->SetStat(TEXT("Stamina"), 25);
	Source->SetStat(TEXT("Hunger"), 42);
	Source->SetStat(TEXT("Thirst"), 31);
	Source->SetStat(TEXT("Temperature"), 36.2f);
	Source->SetStat(TEXT("Energy"), 18);
	Source->NutritionStatus.Calories = 850;
	Source->NutritionStatus.Hydration = 125;
	Source->NutritionStatus.VitaminA = 88;
	Source->NutritionStatus.Sodium = 123;
	FMOPersistedPawnRecord Record;
	Record.bHasComponentData = true;
	Source->BuildSaveData(Record.SurvivalData);
	const auto Loaded = RoundTrip(Record);
	Target->Hunger.Max = 80;
	Target->Hunger.DecayRate = 0.125f;
	Target->Stamina.RegenRate = 4.25f;
	TestTrue(TEXT("Pawn-record serialization includes valid survival data"), Loaded.SurvivalData.bHasValidData);
	TestTrue(TEXT("An authoritative pawn restores all meters"), Target->ApplySaveDataAuthority(Loaded.SurvivalData));
	for (const auto& Pair : Loaded.SurvivalData.CurrentValues)
		TestEqual(*FString::Printf(TEXT("%s survives the pawn-record round trip"), *Pair.Key.ToString()), Target->GetStatCurrent(Pair.Key), Pair.Value);
	for (TFieldIterator<FFloatProperty> Field(FMONutritionStatus::StaticStruct()); Field; ++Field)
		TestEqual(*FString::Printf(TEXT("Nutrition %s survives"), *Field->GetName()), Field->GetPropertyValue_InContainer(&Target->NutritionStatus), Field->GetPropertyValue_InContainer(&Source->NutritionStatus));
	TestEqual(TEXT("Class-authored hunger limit is preserved"), Target->Hunger.Max, 80.0f);
	TestEqual(TEXT("Class-authored decay rate is preserved"), Target->Hunger.DecayRate, 0.125f);
	TestEqual(TEXT("Class-authored stamina recovery is preserved"), Target->Stamina.RegenRate, 4.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalSaveValidation, "MOFramework.Survival.Persistence.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSurvivalSaveValidation::RunTest(const FString&)
{
	using namespace MOSurvivalPersistenceTests;
	FWorld World;
	auto* Stats = World.Spawn();
	Stats->Hunger.Current = 41;
	Stats->Thirst.Current = 32;
	FMOSurvivalSaveData Save;
	TestFalse(TEXT("Old saves with no survival field do not overwrite live meters"), Stats->ApplySaveDataAuthority(Save));
	Stats->BuildSaveData(Save);
	Save.CurrentValues[TEXT("Hunger")] = 99;
	Save.CurrentValues[TEXT("Thirst")] = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("A corrupt meter refuses the whole snapshot"), Stats->ApplySaveDataAuthority(Save));
	TestEqual(TEXT("Refusal did not partly restore hunger"), Stats->Hunger.Current, 41.0f);
	TestEqual(TEXT("Refusal did not corrupt thirst"), Stats->Thirst.Current, 32.0f);
	Stats->BuildSaveData(Save);
	Save.Nutrition.Sodium = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Nonfinite nutrition is refused atomically"), Stats->ApplySaveDataAuthority(Save));
	Stats->BuildSaveData(Save);
	Save.Version = 999;
	TestFalse(TEXT("Unknown data versions are not guessed"), Stats->ApplySaveDataAuthority(Save));
	Stats->BuildSaveData(Save);
	Save.CurrentValues[TEXT("Hunger")] = 99;
	Stats->GetOwner()->SetRole(ROLE_AutonomousProxy);
	TestFalse(TEXT("A client cannot restore authoritative meters"), Stats->ApplySaveDataAuthority(Save));
	TestEqual(TEXT("Client refusal preserves the meter"), Stats->Hunger.Current, 41.0f);
	Stats->GetOwner()->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalSavePacing, "MOFramework.Survival.Persistence.Pacing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSurvivalSavePacing::RunTest(const FString&)
{
	using namespace MOSurvivalPersistenceTests;
	FWorld World;
	auto* Control = World.Spawn();
	auto* Loaded = World.Spawn();
	Control->AdvanceStats(600);
	FMOSurvivalSaveData Save;
	Control->BuildSaveData(Save);
	TestTrue(TEXT("Partly spent meters restore"), Loaded->ApplySaveDataAuthority(Save));
	Control->AdvanceStats(120);
	Loaded->AdvanceStats(120);
	for (const auto& Pair : Save.CurrentValues)
		TestEqual(*FString::Printf(TEXT("Restored %s advances at the unchanged game-clock rate"), *Pair.Key.ToString()), Loaded->GetStatCurrent(Pair.Key), Control->GetStatCurrent(Pair.Key));
	TestEqual(TEXT("Approved thirst rate remains one full meter per game hour"), Loaded->Thirst.DecayRate, 100.0f / 3600.0f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarryInstallFolderSaves, "MOFramework.Persistence.CarryInstallFolderSaves", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMOCarryInstallFolderSaves::RunTest(const FString& Parameters)
{
	// Oct 28: the Oct 17 Steam build was Development (worlds beside the game); the Shipping build after it saves in the user's folder.
	IFileManager& Files = IFileManager::Get();
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::AutomationTransientDir() / TEXT("CarrySaves"));
	Files.DeleteDirectory(*Root, false, true);
	const FString From = Root / TEXT("Install/SaveGames"), To = Root / TEXT("User/SaveGames");
	FFileHelper::SaveStringToFile(TEXT("install A"), *(From / TEXT("World_A.sav")));
	FFileHelper::SaveStringToFile(TEXT("install B"), *(From / TEXT("World_B.sav")));
	FFileHelper::SaveStringToFile(TEXT("not a world"), *(From / TEXT("notes.txt")));
	FFileHelper::SaveStringToFile(TEXT("user B"), *(To / TEXT("World_B.sav")));
	FFileHelper::SaveStringToFile(TEXT("user C"), *(To / TEXT("World_C.sav")));

	TestEqual(TEXT("CONTROL: the same folder (the editor, a Development build) carries nothing"), UMOPersistenceSubsystem::CarryInstallFolderSaves(From, From + TEXT("/")), 0);
	TestEqual(TEXT("one world the user folder lacks is carried"), UMOPersistenceSubsystem::CarryInstallFolderSaves(From, To), 1);
	FString Text;
	TestTrue(TEXT("... World_A copied byte for byte"), FFileHelper::LoadFileToString(Text, *(To / TEXT("World_A.sav"))) && Text == TEXT("install A"));
	TestTrue(TEXT("... a world in both keeps the user folder's copy"), FFileHelper::LoadFileToString(Text, *(To / TEXT("World_B.sav"))) && Text == TEXT("user B"));
	TestTrue(TEXT("... the user folder's own world stays"), Files.FileExists(*(To / TEXT("World_C.sav"))));
	TestFalse(TEXT("... only worlds (*.sav) are carried"), Files.FileExists(*(To / TEXT("notes.txt"))));
	TestTrue(TEXT("... the install folder keeps its worlds"), Files.FileExists(*(From / TEXT("World_A.sav"))));
	Files.Delete(*(To / TEXT("World_A.sav")));
	FFileHelper::SaveStringToFile(TEXT("install D"), *(From / TEXT("World_D.sav")));
	TestEqual(TEXT("once only: a world deleted later is not brought back, a new one is not carried"), UMOPersistenceSubsystem::CarryInstallFolderSaves(From, To), 0);
	TestFalse(TEXT("... World_A stays deleted"), Files.FileExists(*(To / TEXT("World_A.sav"))));
	Files.DeleteDirectory(*Root, false, true);
	return true;
}

#endif
