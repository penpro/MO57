// The reticle and the survivors (Wes, Oct 29 2026): "change the target reticle out to a small circle ... green to interact or red to attack or blue
// when it wants a right click", "a 'closing circle' method for aiming", "always one within 200m with the campfire going", and the survivor's
// dialogue tied to the campfire ask ("If you can do better than me I'll join your village" / "Ohh you already built one!?").

#include "Misc/AutomationTest.h"
#include "MOInteractableComponent.h"
#include "MORecruitmentComponent.h"
#include "MOReticleWidget.h"
#include "MOSpawnManagerSubsystem.h"
#include "MOSpawnSettings.h"
#include "MOSurvivorDialogueWidget.h"
#include "MOUIManagerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_ReticleIntent,
	"MOFramework.UI.ReticleIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_ReticleIntent::RunTest(const FString& Parameters)
{
	using I = EMOReticleIntent;
	using In = EMOInteractionInput;
	const auto For = [](bool bThreat, bool bHasTarget, bool bForage, bool bActor, In Input, bool bWater)
	{
		return UMOUIManagerComponent::ReticleIntentFor(bThreat, bHasTarget, bForage, bActor, Input, bWater);
	};
	TestEqual(TEXT("nothing under it: white"), static_cast<int32>(For(false, false, false, false, In::Primary, false)), static_cast<int32>(I::None));
	TestEqual(TEXT("a tree / a stick (an instance): green"), static_cast<int32>(For(false, true, false, false, In::Primary, false)), static_cast<int32>(I::Interact));
	TestEqual(TEXT("a stranger / a finished building: green"), static_cast<int32>(For(false, true, false, true, In::Primary, false)), static_cast<int32>(I::Interact));
	TestEqual(TEXT("an unfinished building / a colonist: blue"), static_cast<int32>(For(false, true, false, true, In::Secondary, false)), static_cast<int32>(I::Secondary));
	TestEqual(TEXT("another player's character: white"), static_cast<int32>(For(false, true, false, true, In::None, false)), static_cast<int32>(I::None));
	TestEqual(TEXT("grass (forages anywhere): white"), static_cast<int32>(For(false, true, true, false, In::Primary, false)), static_cast<int32>(I::None));
	TestEqual(TEXT("water in reach: green"), static_cast<int32>(For(false, false, false, false, In::Primary, true)), static_cast<int32>(I::Interact));
	TestEqual(TEXT("water in reach over a lake bed's grass: green"), static_cast<int32>(For(false, true, true, false, In::Primary, true)), static_cast<int32>(I::Interact));
	TestEqual(TEXT("a threat wins over anything"), static_cast<int32>(For(true, true, false, true, In::Secondary, true)), static_cast<int32>(I::Threat));
	TestEqual(TEXT("a threat with nothing else under it"), static_cast<int32>(For(true, false, false, false, In::Primary, false)), static_cast<int32>(I::Threat));

	// The colours are four different tokens.
	const FLinearColor White = UMOReticleWidget::ColorForIntent(I::None);
	const FLinearColor Green = UMOReticleWidget::ColorForIntent(I::Interact);
	const FLinearColor Blue = UMOReticleWidget::ColorForIntent(I::Secondary);
	const FLinearColor Red = UMOReticleWidget::ColorForIntent(I::Threat);
	TestTrue(TEXT("the four intents have four colours"), White != Green && Green != Blue && Blue != Red && Red != White && Green != Red);
	TestTrue(FString::Printf(TEXT("green reads green (%s)"), *Green.ToString()), Green.G > Green.R && Green.G > Green.B);
	TestTrue(FString::Printf(TEXT("blue reads blue (%s)"), *Blue.ToString()), Blue.B > Blue.R && Blue.B > Blue.G);
	TestTrue(FString::Printf(TEXT("red reads red (%s)"), *Red.ToString()), Red.R > Red.G && Red.R > Red.B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_ReticleClosingCircle,
	"MOFramework.UI.ReticleClosingCircle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_ReticleClosingCircle::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("opens wide"), UMOReticleWidget::ClosingSpread(0.0f, 1.5f), 1.0f, 1e-4f);
	TestEqual(TEXT("closed at the end"), UMOReticleWidget::ClosingSpread(1.5f, 1.5f), 0.0f, 1e-4f);
	TestEqual(TEXT("stays closed after"), UMOReticleWidget::ClosingSpread(9.0f, 1.5f), 0.0f, 1e-4f);
	TestEqual(TEXT("no duration: closed"), UMOReticleWidget::ClosingSpread(0.0f, 0.0f), 0.0f, 1e-4f);
	float Last = 2.0f;
	bool bShrinks = true;
	for (float T = 0.0f; T <= 1.5f; T += 0.05f)
	{
		const float S = UMOReticleWidget::ClosingSpread(T, 1.5f);
		bShrinks &= S <= Last + 1e-5f;
		Last = S;
	}
	TestTrue(TEXT("never widens while closing"), bShrinks);
	TestTrue(TEXT("closes fast, then settles: past half-closed by a third of the way"), UMOReticleWidget::ClosingSpread(0.5f, 1.5f) < 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSpawn_SurvivorNearPlayers,
	"MOFramework.Spawn.SurvivorNearPlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSpawn_SurvivorNearPlayers::RunTest(const FString& Parameters)
{
	// Out of view: a new survivor is placed behind the player's view where it can be.
	const FVector Eye(0.0, 0.0, 0.0);
	TestFalse(TEXT("straight ahead is in view"), UMOSpawnManagerSubsystem::IsBehindView(Eye, 0.0f, FVector(1000.0, 0.0, 0.0)));
	TestTrue(TEXT("straight behind is out of view"), UMOSpawnManagerSubsystem::IsBehindView(Eye, 0.0f, FVector(-1000.0, 0.0, 0.0)));
	TestFalse(TEXT("90 deg to the side is not behind (100 deg needed)"), UMOSpawnManagerSubsystem::IsBehindView(Eye, 0.0f, FVector(0.0, 1000.0, 0.0)));
	TestTrue(TEXT("90 deg to the side is out of a 70 deg cone"), UMOSpawnManagerSubsystem::IsBehindView(Eye, 0.0f, FVector(0.0, 1000.0, 0.0), 70.0f));
	TestTrue(TEXT("the view's yaw counts: looking west, east is behind"), UMOSpawnManagerSubsystem::IsBehindView(Eye, 180.0f, FVector(1000.0, 0.0, 0.0)));
	TestFalse(TEXT("on top of the viewer is not a direction"), UMOSpawnManagerSubsystem::IsBehindView(Eye, 0.0f, FVector(0.0, 0.0, 500.0)));

	// The defaults are Wes's rule: within 200 m, a new one closer than that so she counts.
	const UMOSpawnSettings* Settings = GetDefault<UMOSpawnSettings>();
	TestTrue(TEXT("on by default"), Settings->bKeepSurvivorNearPlayers);
	TestEqual(TEXT("within 200 m"), Settings->NearbySurvivorRadiusM, 200.0f);
	TestTrue(TEXT("a new one lands inside the radius"), Settings->NearbySurvivorSpawnMaxM < Settings->NearbySurvivorRadiusM
		&& Settings->NearbySurvivorSpawnMinM < Settings->NearbySurvivorSpawnMaxM);

	// A record with no pawn is no survivor to count.
	FMOSpawnedEntityRecord Empty;
	Empty.Category = EMOSpawnCategory::Survivor;
	TestFalse(TEXT("a record without a pawn is not a wandering survivor"), UMOSpawnManagerSubsystem::IsWanderingSurvivor(Empty));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMORecruitment_SurvivorDialogue,
	"MOFramework.Colony.SurvivorDialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMORecruitment_SurvivorDialogue::RunTest(const FString& Parameters)
{
	UMORecruitmentComponent* Survivor = NewObject<UMORecruitmentComponent>();
	TestTrue(TEXT("every survivor asks for something (QuestChance 1)"), Survivor->QuestChance >= 1.0f);
	if (!TestTrue(TEXT("the default ask exists"), Survivor->PossibleQuests.Num() > 0))
	{
		return false;
	}
	const FMORecruitmentQuest& Ask = Survivor->PossibleQuests[0];
	TestEqual(TEXT("the ask is a building"), static_cast<int32>(Ask.QuestType), static_cast<int32>(EMORecruitmentQuestType::NeedsBuilding));
	TestEqual(TEXT("... a campfire"), Ask.RequiredRecipeId.ToString(), FString(TEXT("BuildCampfire")));
	TestEqual(TEXT("... which starts the campfire quest line"), Ask.PlayerQuestId.ToString(), FString(TEXT("Tutorial_BuildCampfire")));
	TestTrue(TEXT("greetings vary"), Survivor->GreetingLines.Num() >= 3);
	TestTrue(TEXT("camp lines vary"), Survivor->CampLines.Num() >= 2);
	TestTrue(TEXT("Wes's greeting is one of them"), Survivor->GreetingLines.ContainsByPredicate([](const FText& T) { return T.ToString().Contains(TEXT("alone forever")); }));
	TestTrue(TEXT("Wes's camp line is one of them"), Survivor->CampLines.ContainsByPredicate([](const FText& T) { return T.ToString().Contains(TEXT("as far as I could get")); }));

	Survivor->ActiveQuest = Ask;
	const FString First = Survivor->GetDialogueFor(EMORecruitmentOutcome::QuestStarted).ToString();
	TArray<FString> Lines;
	First.ParseIntoArrayLines(Lines);
	TestEqual(FString::Printf(TEXT("the first meeting: greeting, camp, the ask (%s)"), *First), Lines.Num(), 3);
	TestTrue(TEXT("... the ask last"), First.EndsWith(Ask.QuestDialogue.ToString()));
	TestTrue(TEXT("... 'I'll join your village'"), First.Contains(TEXT("join your village")));
	const FString Met = Survivor->GetDialogueFor(EMORecruitmentOutcome::QuestAlreadyMet).ToString();
	TestTrue(FString::Printf(TEXT("already built: the same meeting, then 'you already built one' (%s)"), *Met), Met.StartsWith(First) && Met.Contains(TEXT("already built one")));
	TestEqual(TEXT("talking twice keeps the same story"), Survivor->GetDialogueFor(EMORecruitmentOutcome::QuestStarted).ToString(), First);
	TestTrue(TEXT("coming back early: the reminder"), Survivor->GetDialogueFor(EMORecruitmentOutcome::QuestPending).EqualTo(Ask.ReminderDialogue));
	TestTrue(TEXT("coming back with it built: the thanks"), Survivor->GetDialogueFor(EMORecruitmentOutcome::Recruited).EqualTo(Ask.SuccessDialogue));
	TestTrue(TEXT("a stranger who does not trust the village yet says so"), Survivor->GetDialogueFor(EMORecruitmentOutcome::DeclinedStanding).ToString().EndsWith(Survivor->WaryLine.ToString()));
	TestTrue(TEXT("the dead and the rest say nothing"), Survivor->GetDialogueFor(EMORecruitmentOutcome::Unavailable).IsEmpty());

	FMORecruitmentQuest NoAsk;
	Survivor->ActiveQuest = NoAsk;
	const FString Joins = Survivor->GetDialogueFor(EMORecruitmentOutcome::Recruited).ToString();
	TestTrue(FString::Printf(TEXT("no ask: a greeting and the join line (%s)"), *Joins), Joins.EndsWith(Survivor->JoinLine.ToString()) && Joins.Contains(TEXT("\n")));

	TestFalse(TEXT("no world: no building stands"), UMORecruitmentComponent::IsBuildingStanding(nullptr, TEXT("BuildCampfire")));

	FMORecruitmentResult Result;
	Result.Outcome = EMORecruitmentOutcome::QuestAlreadyMet;
	TestTrue(TEXT("already met = joined"), Result.Joined() && Result.Succeeded());
	Result.Outcome = EMORecruitmentOutcome::QuestStarted;
	TestTrue(TEXT("a quest given moved it on but did not join"), !Result.Joined() && Result.Succeeded());

	// The popup stays long enough to read and never forever.
	TestEqual(TEXT("a short line: at least 4 s"), UMOSurvivorDialogueWidget::ReadSeconds(FText::FromString(TEXT("Hi."))), 4.0f, 1e-3f);
	TestTrue(TEXT("the first meeting reads in 6-16 s"), UMOSurvivorDialogueWidget::ReadSeconds(FText::FromString(First)) >= 6.0f
		&& UMOSurvivorDialogueWidget::ReadSeconds(FText::FromString(First)) <= 16.0f);
	TestEqual(TEXT("capped at 16 s"), UMOSurvivorDialogueWidget::ReadSeconds(FText::FromString(FString::ChrN(2000, TEXT('a')))), 16.0f, 1e-3f);
	return true;
}

#endif
