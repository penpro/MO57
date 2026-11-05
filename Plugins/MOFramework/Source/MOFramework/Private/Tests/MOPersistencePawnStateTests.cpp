// A pawn's state through the save's own code (UMOPersistenceSubsystem::CapturePersistedPawnsAndInventories, ApplyPawnComponentState), and a fence round it: until Nov 2026 a pawn's knowledge
// and character were in no save, so each load forgot what it had studied and gave a colonist new traits. Run: python Tools/ue.py auto --filter "MOFramework.Persistence."

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "MOCharacter.h"
#include "MOPersistenceSubsystem.h"
#include "MOPersistenceTestPawn.h"
#include "MOworldSaveGame.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPersistence_PawnStateSurvivesASave,
	"MOFramework.Persistence.PawnStateSurvivesASave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPersistence_PawnStateSurvivesASave::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOPersistencePawnStateWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	UMOPersistenceSubsystem* Persistence = NewObject<UMOPersistenceSubsystem>(Instance);

	AMOPersistenceTestPawn* Source = World->SpawnActor<AMOPersistenceTestPawn>();
	if (!TestNotNull(TEXT("the test pawn spawns"), Source))
	{
		return false;
	}
	FMOItemKnowledgeProgress Studied;
	Studied.ItemDefinitionId = TEXT("Stick01");
	Studied.InspectionCount = 3;
	Studied.UnlockedKnowledge = { TEXT("WoodProperties") };
	Studied.LastInspectionSkillLevel = 2.0f;
	Source->Knowledge->ItemKnowledge.Add(Studied);
	Source->Knowledge->AllLearnedKnowledge = { TEXT("WoodProperties"), TEXT("BarkFiber") };
	FMOPersonalityTraits Traits;
	Traits.Conscientiousness = -0.4f;
	Traits.Sociability = 0.7f;
	Traits.Stability = -0.9f;
	Source->Personality->SetTraits(Traits);

	UMOWorldSaveGame* Save = NewObject<UMOWorldSaveGame>();
	Persistence->CapturePersistedPawnsAndInventories(World, Save);
	if (!TestEqual(TEXT("the save captures the pawn"), Save->PersistedPawns.Num(), 1))
	{
		return false;
	}
	const FMOPersistedPawnRecord& Record = Save->PersistedPawns[0];
	TestTrue(TEXT("the record says it has component data"), Record.bHasComponentData);

	AMOPersistenceTestPawn* Target = World->SpawnActor<AMOPersistenceTestPawn>();
	Persistence->ApplyPawnComponentState(Target, Record);

	TestEqual(TEXT("what it learned comes back"), Target->Knowledge->AllLearnedKnowledge.Num(), 2);
	TestTrue(TEXT("by name"), Target->Knowledge->AllLearnedKnowledge.Contains(TEXT("BarkFiber")));
	if (TestEqual(TEXT("what it studied comes back"), Target->Knowledge->ItemKnowledge.Num(), 1))
	{
		TestEqual(TEXT("the item"), Target->Knowledge->ItemKnowledge[0].ItemDefinitionId, FName(TEXT("Stick01")));
		TestEqual(TEXT("how many times"), Target->Knowledge->ItemKnowledge[0].InspectionCount, 3);
	}
	TestEqual(TEXT("its character comes back: sociability"), Target->Personality->GetTraits().Sociability, 0.7f);
	TestEqual(TEXT("its character comes back: stability"), Target->Personality->GetTraits().Stability, -0.9f);

	// A record from before these fields leaves a live pawn as it is.
	FMOPersistedPawnRecord Old;
	Old.bHasComponentData = true;
	AMOPersistenceTestPawn* Live = World->SpawnActor<AMOPersistenceTestPawn>();
	Live->Knowledge->AllLearnedKnowledge = { TEXT("BarkFiber") };
	Live->Personality->SetTraits(Traits);
	Persistence->ApplyPawnComponentState(Live, Old);
	TestEqual(TEXT("an old record leaves its knowledge alone"), Live->Knowledge->AllLearnedKnowledge.Num(), 1);
	TestEqual(TEXT("an old record leaves its character alone"), Live->Personality->GetTraits().Sociability, 0.7f);
	return true;
}

// The fence: a component on the survivor with a BuildSaveData whose struct no save holds is state that a load forgets. Expected: all in a save. By its logic it would have failed on UMOPersonalityComponent before N-289 (not run against that code).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPersistence_EveryPawnComponentIsSaved,
	"MOFramework.Persistence.EveryPawnComponentIsSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPersistence_EveryPawnComponentIsSaved::RunTest(const FString& Parameters)
{
	TSet<const UScriptStruct*> Saved;
	for (TFieldIterator<FStructProperty> Field(FMOPersistedPawnRecord::StaticStruct()); Field; ++Field)
	{
		Saved.Add(Field->Struct);
	}
	const auto AddMapValues = [&Saved](const UStruct* Owner)
	{
		for (TFieldIterator<FMapProperty> Field(Owner); Field; ++Field)
		{
			if (const FStructProperty* Value = CastField<FStructProperty>(Field->ValueProp))
			{
				Saved.Add(Value->Struct);
			}
		}
	};
	AddMapValues(UMOWorldSaveGame::StaticClass());      // inventories, crafting queues, discoveries by pawn
	AddMapValues(FMOColonySaveData::StaticStruct());    // a colonist's history

	TArray<UObject*> Subobjects;
	GetMutableDefault<AMOCharacter>()->GetDefaultSubobjects(Subobjects);
	int32 Checked = 0;
	for (const UObject* Subobject : Subobjects)
	{
		const UActorComponent* Component = Cast<UActorComponent>(Subobject);
		const UFunction* Build = Component ? Component->GetClass()->FindFunctionByName(TEXT("BuildSaveData")) : nullptr;
		if (!Build)
		{
			continue;
		}
		const FStructProperty* Param = nullptr;
		for (TFieldIterator<FStructProperty> Field(Build); Field && !Param; ++Field)
		{
			Param = *Field;
		}
		if (!Param)
		{
			continue;
		}
		++Checked;
		if (Component->GetClass()->GetName() == TEXT("MOAdrenalineComponent"))
		{
			continue;   // adrenaline does not outlast a fight: it starts at its baseline after a load, by design
		}
		TestTrue(*FString::Printf(TEXT("%s's %s is in a save"), *Component->GetClass()->GetName(), *Param->Struct->GetName()), Saved.Contains(Param->Struct));
	}
	TestTrue(*FString::Printf(TEXT("the fence saw the survivor's saving components (%d)"), Checked), Checked >= 8);
	return true;
}

#endif
