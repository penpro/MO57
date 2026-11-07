// Lighting a fire is a real, chancy attempt (FMOFireActions::LightChance): practice helps, a better tool (a bow drill) beats an improvised hand drill, rain and wet
// hands hurt, nothing is certain.

#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MOCraftingStationActor.h"
#include "MOFireActions.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOSkillDatabaseSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

// N-358. A player on the Steam hub, Oct 30: "it keeps telling me to add tinder. I have dry grass and have stripped bark to make fibers. What am I missing?" Nothing: dried grass and bark fibre are tinder, and now the game agrees.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFireActions_TinderIsTheItems,
	"MOFramework.Fire.TinderIsTheItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOFireActions_TinderIsTheItems::RunTest(const FString& Parameters)
{
	const float Proper = FMOFireActions::GetTinderQuality(TEXT("Tinder01")), Grass = FMOFireActions::GetTinderQuality(TEXT("DriedGrass01")), Fibre = FMOFireActions::GetTinderQuality(TEXT("PlantFiber01"));
	TestEqual(TEXT("proper tinder takes an ember best"), Proper, 1.0f);
	TestTrue(TEXT("dried grass is tinder, not quite as good"), Grass > 0.0f && Grass < Proper);
	TestTrue(TEXT("bark fibre is tinder, worse than grass"), Fibre > 0.0f && Fibre < Grass);
	TestEqual(TEXT("a stone isn't"), FMOFireActions::GetTinderQuality(TEXT("Stone01")), 0.0f);
	TestEqual(TEXT("a sheet of bark isn't, until it's shredded"), FMOFireActions::GetTinderQuality(TEXT("Bark01")), 0.0f);
	const float Base = FMOFireActions::LightChance(0.4f, 0, false, 0.0f);
	TestEqual(TEXT("the best tinder changes nothing"), FMOFireActions::LightChance(0.4f, 0, false, 0.0f, 1.0f), Base);
	TestEqual(TEXT("poorer tinder catches less often"), FMOFireActions::LightChance(0.4f, 0, false, 0.0f, 0.5f), Base * 0.5f, 0.0001f);

	// The menu a player sees, on a real pawn and a laid fire.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOFireTinderWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	APawn* Pawn = World->SpawnActor<APawn>();
	USceneComponent* Root = NewObject<USceneComponent>(Pawn);
	Pawn->SetRootComponent(Root);
	Root->RegisterComponent();
	UMOInventoryComponent* Inventory = NewObject<UMOInventoryComponent>(Pawn);
	Pawn->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	Pawn->DispatchBeginPlay();
	Inventory->SetSlotCountAuthority(20);
	AMOCraftingStationActor* Fire = World->SpawnActor<AMOCraftingStationActor>();
	Fire->bRequiresFuel = true;
	Fire->MaxFuel = 100.0f;
	Fire->CurrentFuel = 20.0f;
	const auto LightOption = [&]()
	{
		TArray<FMOItemActionOption> Options;
		FMOFireActions::GetActions(Pawn, Fire, Options);
		const FMOItemActionOption* Found = Options.FindByPredicate([](const FMOItemActionOption& O) { return O.Id == FMOFireActions::Light; });
		return Found ? *Found : FMOItemActionOption();
	};
	const auto Add = [&](const TCHAR* Id, int32 Count) { return Inventory->AddItemByGuid(FGuid::NewGuid(), FName(Id), Count); };

	TestTrue(TEXT("two dry branches for a hand drill"), Add(TEXT("Branch01"), 2));
	FMOItemActionOption Nothing = LightOption();
	TestFalse(TEXT("with no tinder it can't be lit"), Nothing.bEnabled);
	for (const TCHAR* Id : { TEXT("Tinder01"), TEXT("DriedGrass01"), TEXT("PlantFiber01") })
	{
		TestTrue(FString::Printf(TEXT("the refusal names what to look for (%s): %s"), Id, *Nothing.DisabledReason.ToString()), Nothing.DisabledReason.ToString().Contains(UMOItemDatabaseSettings::GetItemDisplayName(FName(Id)).ToString()));
	}
	float Quality = 0.0f;
	TestTrue(TEXT("shredded bark fibre"), Add(TEXT("PlantFiber01"), 1));
	FMOItemActionOption WithFibre = LightOption();
	TestTrue(FString::Printf(TEXT("bark fibre lights it: %s"), *WithFibre.DisabledReason.ToString()), WithFibre.bEnabled);
	TestTrue(FString::Printf(TEXT("and the menu says with what: %s"), *WithFibre.Label.ToString()), WithFibre.Label.ToString().Contains(UMOItemDatabaseSettings::GetItemDisplayName(TEXT("PlantFiber01")).ToString()));
	TestEqual(TEXT("it would spend the fibre"), FMOFireActions::FindBestTinder(Inventory, Quality), FName(TEXT("PlantFiber01")));
	TestTrue(TEXT("dried grass"), Add(TEXT("DriedGrass01"), 1));
	TestEqual(TEXT("grass before fibre"), FMOFireActions::FindBestTinder(Inventory, Quality), FName(TEXT("DriedGrass01")));
	TestEqual(TEXT("... at its quality"), Quality, Grass);
	TestTrue(TEXT("proper tinder"), Add(TEXT("Tinder01"), 1));
	TestEqual(TEXT("the best first"), FMOFireActions::FindBestTinder(Inventory, Quality), FName(TEXT("Tinder01")));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFireActions_LightChance,
	"MOFramework.Fire.LightChance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOFireActions_LightChance::RunTest(const FString& Parameters)
{
	const float Improvised = FMOFireActions::ImprovisedDrillEffectiveness;
	constexpr float BowDrill = 0.7f;   // BowDrill01's Firestarter effectiveness
	const float Novice = FMOFireActions::LightChance(Improvised, 0, false, 0.0f);
	TestEqual(TEXT("a novice's improvised hand drill in the dry: 20 %"), Novice, 0.2f, 1e-5f);
	TestTrue(TEXT("practice helps"), FMOFireActions::LightChance(Improvised, 5, false, 0.0f) > Novice);
	TestTrue(TEXT("a bow drill beats an improvised drill"), FMOFireActions::LightChance(BowDrill, 0, false, 0.0f) > Novice);
	TestEqual(TEXT("rain cuts it to a third"), FMOFireActions::LightChance(Improvised, 0, true, 0.0f), Novice * 0.3f, 1e-5f);
	TestEqual(TEXT("wet hands cut it"), FMOFireActions::LightChance(Improvised, 0, false, 0.8f), Novice * 0.6f, 1e-5f);
	TestEqual(TEXT("a little damp does not"), FMOFireActions::LightChance(Improvised, 0, false, 0.3f), Novice, 1e-5f);
	TestEqual(TEXT("never certain: capped at 95 %"), FMOFireActions::LightChance(BowDrill, 50, false, 0.0f), 0.95f, 1e-5f);
	TestTrue(TEXT("a master in the rain still struggles"), FMOFireActions::LightChance(BowDrill, 10, true, 0.9f) < 0.3f);
	TestEqual(TEXT("no tool, no chance"), FMOFireActions::LightChance(0.0f, 0, false, 0.0f), 0.0f, 1e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFireActions_PracticeCounts,
	"MOFramework.Fire.PracticeCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Wes, F33 A (Oct 20 2026): the novice's 20 % stays, practice counts sooner -- three failed hand drills reach Firemaking level 1, so the 4th try is the better
// one (it was 8 XP a failure: eight failures, 24 minutes of drilling, before the first +6 %).
bool FMOFireActions_PracticeCounts::RunTest(const FString& Parameters)
{
	const FMOSkillDefinitionRow* Firemaking = UMOSkillDatabaseSettings::GetSkillDefinition(TEXT("Firemaking"));
	TestNotNull(TEXT("Firemaking is a skill"), Firemaking);
	if (!Firemaking)
	{
		return true;
	}
	TestTrue(FString::Printf(TEXT("three failed attempts (%.0f XP) reach level 1 (%.0f XP)"), 3.0f * FMOFireActions::FailedAttemptXP, Firemaking->BaseXPPerLevel),
		3.0f * FMOFireActions::FailedAttemptXP >= Firemaking->BaseXPPerLevel);
	TestTrue(TEXT("a fire that takes teaches more than one that dies"), FMOFireActions::LitAttemptXP > FMOFireActions::FailedAttemptXP);
	TestTrue(TEXT("... and the 4th try is better than the first"), FMOFireActions::LightChance(FMOFireActions::ImprovisedDrillEffectiveness, 1, false, 0.0f)
		> FMOFireActions::LightChance(FMOFireActions::ImprovisedDrillEffectiveness, 0, false, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
