#include "Misc/AutomationTest.h"
#include "MORecruitmentComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

// A night away lost the survivor who had asked (N-311): the ask's 10 hours ran on the wall clock, so a save reloaded the next day failed it at the first tick and she vanished.
// Expected: an ask 100 s old in its save has 100 s gone however long the file sat, counts on only with the world running, and comes back through the save; failing before: the saved start time, read against today.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColonyAskClockRunsInTheWorld, "MOFramework.Colony.AskClockRunsInTheWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOColonyAskClockRunsInTheWorld::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOAskClockWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	AActor* Stranger = World->SpawnActor<AActor>();
	UMORecruitmentComponent* Recruit = NewObject<UMORecruitmentComponent>(Stranger);
	Stranger->AddInstanceComponent(Recruit);
	Recruit->RegisterComponent();

	FMORecruitmentSaveData Saved;
	Saved.bHasValidData = true;
	Saved.RecruitmentState = EMORecruitmentState::QuestActive;
	Saved.ActiveQuest.QuestType = EMORecruitmentQuestType::NeedsBuilding;
	Saved.ActiveQuest.TimeoutSeconds = 36000.0f;
	Saved.QuestElapsedSeconds = 100.0f;
	Recruit->ApplySaveDataAuthority(Saved);
	TestEqual(TEXT("the time away did not count"), Recruit->GetQuestTimeRemaining(), 35900.0f, 0.5f);

	UActorComponent* Tickable = Recruit;   // the override is protected
	Tickable->TickComponent(10.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("it counts with the world running"), Recruit->GetQuestTimeRemaining(), 35890.0f, 0.5f);
	TestTrue(TEXT("she still waits"), Recruit->HasActiveQuest());

	FMORecruitmentSaveData Again;
	Recruit->BuildSaveData(Again);
	TestEqual(TEXT("it goes back into the save"), Again.QuestElapsedSeconds, 110.0f, 0.5f);

	// CONTROL: the ten hours still end, so the clock above is the one that decides.
	Tickable->TickComponent(36000.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("CONTROL: run out, she is gone"), Recruit->HasActiveQuest());

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
