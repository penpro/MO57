// Work at the ground kneels (UMOTimedActionComponent::BeginKneeling, N-181, Oct 20 2026): the pawn stood upright through a 3-minute hand drill,
// butchering and drinking at a lake. The pawn crouches for the action and stands after -- unless it was crouching already -- and a completion that
// begins the next kneeling action (the next cut) keeps it down. Run: python Tools/ue.py auto --filter "MOFramework.TimedAction.Posture"

#include "Misc/AutomationTest.h"
#include "MOTimedActionComponent.h"
#include "MOMedicalActionTestClock.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace MOTimedActionPostureTests
{
	struct FWorld
	{
		UGameInstance* GI;
		UWorld* World;
		ACharacter* Pawn;
		UMOTimedActionComponent* Timed;
		AMOMedicalActionTestClock* Clock;
		FWorld()
		{
			GI = NewObject<UGameInstance>(GEngine);
			GI->InitializeStandalone();
			World = GI->GetWorld();
			World->InitializeActorsForPlay(FURL());
			Clock = World->SpawnActor<AMOMedicalActionTestClock>();
			World->SetGameState(Clock);
			Pawn = World->SpawnActor<ACharacter>();
			Pawn->GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
			// A controller on this machine: the server moves its own and AI pawns (a joiner's is crouched by the joiner, from OnRep_State)
			World->SpawnActor<AAIController>()->Possess(Pawn);
			Timed = NewObject<UMOTimedActionComponent>(Pawn);
			Pawn->AddInstanceComponent(Timed);
			Timed->RegisterComponent();
		}
		~FWorld()
		{
			Timed->Cancel(TEXT("test ended"));
			GI->Shutdown();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		bool Down() const { return Pawn->GetCharacterMovement()->bWantsToCrouch; }
		void Advance(double Seconds)
		{
			Clock->Now += Seconds;
			Timed->TickComponent(float(Seconds), LEVELTICK_All, nullptr);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTimedActionPosture_KneelsForGroundWork, "MOFramework.TimedAction.Posture.KneelsForGroundWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMOTimedActionPosture_KneelsForGroundWork::RunTest(const FString& Parameters)
{
	using namespace MOTimedActionPostureTests;
	FWorld F;
	TestTrue(TEXT("CONTROL: the pawn is locally controlled (this machine moves it)"), F.Pawn->IsLocallyControlled());

	F.Timed->Begin(TEXT("Inspect"), FText::GetEmpty(), 5, [](){});
	TestFalse(TEXT("a standing action keeps the pawn up"), F.Down());
	F.Timed->Cancel(TEXT("next"));

	F.Timed->BeginKneeling(TEXT("Light"), FText::GetEmpty(), 5, [](){});
	TestTrue(TEXT("work at the ground kneels"), F.Down());
	TestEqual(TEXT("the replicated state carries the posture (the joiner's own machine crouches from it)"), F.Timed->GetState().Posture, EMOActionPosture::Kneel);
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	TestFalse(TEXT("... and stands up when it is done"), F.Down());

	F.Timed->BeginKneeling(TEXT("Drink"), FText::GetEmpty(), 5, [](){});
	F.Timed->Cancel(TEXT("moved"));
	TestFalse(TEXT("a cancelled kneeling action stands up too"), F.Down());

	int32 Cuts = 0;
	F.Timed->BeginKneeling(TEXT("Butcher"), FText::GetEmpty(), 5, [&]()
	{
		++Cuts;
		F.Timed->BeginKneeling(TEXT("Butcher"), FText::GetEmpty(), 5, [&]() { ++Cuts; });
	});
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	TestTrue(TEXT("CONTROL: the first cut finished and the next began"), Cuts == 1 && F.Timed->IsActive());
	TestTrue(TEXT("butchering the next part keeps the pawn down (no stand-up between cuts)"), F.Down());
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	TestFalse(TEXT("... and it stands after the last"), F.Down());

	F.Pawn->Crouch();
	F.Timed->BeginKneeling(TEXT("Light"), FText::GetEmpty(), 5, [](){});
	F.Timed->Cancel(TEXT("done"));
	TestTrue(TEXT("a pawn that was crouching already stays down after the action (the player's own crouch)"), F.Down());

	F.Pawn->UnCrouch();
	F.Timed->Begin(TEXT("Light"), FText::GetEmpty(), 5, [](){});
	F.Timed->SetPosture(EMOActionPosture::Kneel);
	F.Timed->SetPosture(EMOActionPosture::Stand);
	TestFalse(TEXT("SetPosture(Stand) mid-action stands it up"), F.Down());
	return true;
}

#endif
