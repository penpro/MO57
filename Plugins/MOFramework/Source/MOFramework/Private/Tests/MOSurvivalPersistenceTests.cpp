#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MOSurvivalStatsComponent.h"
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
#endif
