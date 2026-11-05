// A destroyed organ's clock (UMOAnatomyComponent::CheckDeathConditions, BuildSaveData / ApplySaveDataAuthority). Until Nov 2026 losing ONE lung gave three minutes to
// live (the both-lungs rule was only in the branch for a part with no row, and every lung has one), and saving during the countdown cancelled it (the June audit's M13).
// Expected: one lung, no clock; both, a clock that a save and a load carry on from where it stood. Run: python Tools/ue.py auto --filter "MOFramework.Medical.Anatomy.FatalOrgans"

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "MOAnatomyComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOFatalOrganTests
{
	struct FWorld
	{
		UWorld* World;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOFatalOrganWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		UMOAnatomyComponent* Spawn()
		{
			APawn* Pawn = World->SpawnActor<APawn>();
			UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>(Pawn);
			Pawn->AddInstanceComponent(Anatomy);
			Anatomy->RegisterComponent();
			Pawn->DispatchBeginPlay();
			return Anatomy;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_FatalOrgans,
	"MOFramework.Medical.Anatomy.FatalOrgans",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_FatalOrgans::RunTest(const FString& Parameters)
{
	MOFatalOrganTests::FWorld W;
	UMOAnatomyComponent* Body = W.Spawn();
	const float Ruin = 100000.0f;

	// One lung: a person lives on the other.
	TestTrue(TEXT("the left lung takes the blow"), Body->InflictDamage(EMOBodyPartType::LungLeft, Ruin, EMOWoundType::None));
	TestFalse(TEXT("the left lung is destroyed"), Body->IsBodyPartFunctional(EMOBodyPartType::LungLeft));
	TestTrue(TEXT("the right lung still works"), Body->IsBodyPartFunctional(EMOBodyPartType::LungRight));
	TestEqual(TEXT("one lung gone: no clock (failing before: 180 s)"), Body->GetDeathClockRemaining(), 0.0f);

	// Both: the clock runs, and it is in the save.
	TestTrue(TEXT("the right lung takes the blow"), Body->InflictDamage(EMOBodyPartType::LungRight, Ruin, EMOWoundType::None));
	TestTrue(TEXT("both lungs gone: the clock runs"), Body->GetDeathClockRemaining() > 0.0f);
	TestEqual(TEXT("the right lung started it"), Body->GetDeathClockPart(), EMOBodyPartType::LungRight);
	const float Full = Body->GetDeathClockRemaining();

	FMOAnatomySaveData Saved;
	Body->BuildSaveData(Saved);
	TestEqual(TEXT("the save holds the organ"), Saved.DeathClockPart, EMOBodyPartType::LungRight);
	TestEqual(TEXT("the save holds the time left"), Saved.DeathClockRemaining, Full);

	// Loaded: it goes on from where it stood (failing before: no clock, and no death).
	Saved.DeathClockRemaining = 42.0f;
	UMOAnatomyComponent* Loaded = W.Spawn();
	TestTrue(TEXT("the save applies"), Loaded->ApplySaveDataAuthority(Saved));
	TestEqual(TEXT("loaded: the time left is what was saved"), Loaded->GetDeathClockRemaining(), 42.0f);
	TestEqual(TEXT("loaded: the organ is what was saved"), Loaded->GetDeathClockPart(), EMOBodyPartType::LungRight);
	// A timer set before the manager's first tick waits for that tick, and the manager ticks once a frame: two ticks, a frame apart, run it.
	FTimerManager& Timers = W.World->GetTimerManager();
	Timers.Tick(0.1f);
	++GFrameCounter;
	Timers.Tick(1.1f);
	TestTrue(TEXT("loaded: the clock is running, not only remembered"), Loaded->GetDeathClockRemaining() < 42.0f);

	// A save from before the clock was saved: a body with no lungs starts one.
	FMOAnatomySaveData OldSave = Saved;
	OldSave.DeathClockPart = EMOBodyPartType::None;
	OldSave.DeathClockRemaining = 0.0f;
	UMOAnatomyComponent* FromOld = W.Spawn();
	TestTrue(TEXT("the old save applies"), FromOld->ApplySaveDataAuthority(OldSave));
	TestTrue(TEXT("an old save with both lungs gone starts the clock"), FromOld->GetDeathClockRemaining() > 0.0f);

	// A load over a running clock replaces it: a healthy save clears it, and one lung gone stays no clock.
	FMOAnatomySaveData OneLung;
	UMOAnatomyComponent* Other = W.Spawn();
	Other->InflictDamage(EMOBodyPartType::LungLeft, Ruin, EMOWoundType::None);
	Other->BuildSaveData(OneLung);
	TestEqual(TEXT("the save of one lung gone holds no clock"), OneLung.DeathClockRemaining, 0.0f);
	TestTrue(TEXT("that save applies over a running clock"), FromOld->ApplySaveDataAuthority(OneLung));
	TestEqual(TEXT("the running clock is gone"), FromOld->GetDeathClockRemaining(), 0.0f);

	// Survivable alone: a kidney; a limb starts nothing.
	UMOAnatomyComponent* Limb = W.Spawn();
	Limb->InflictDamage(EMOBodyPartType::KidneyLeft, Ruin, EMOWoundType::None);
	TestEqual(TEXT("one kidney gone: no clock"), Limb->GetDeathClockRemaining(), 0.0f);
	Limb->InflictDamage(EMOBodyPartType::HandLeft, Ruin, EMOWoundType::None);
	TestEqual(TEXT("a hand gone: no clock"), Limb->GetDeathClockRemaining(), 0.0f);
	return true;
}

#endif
