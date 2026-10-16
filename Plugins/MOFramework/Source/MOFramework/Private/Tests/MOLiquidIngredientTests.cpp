/**
 * Water as a recipe ingredient (UMOInventoryComponent::CountIngredient / ConsumeIngredient / AddIngredient): Water01 and CleanWater01 are MEASURES of
 * liquid (FMOItemDefinitionRow::LiquidMeasureMl) drawn from the containers a pawn carries, not items. Run:
 *   python Tools/ue.py auto --filter "MOFramework.Inventory.LiquidIngredient"
 * Reads the real item table: Water01 / CleanWater01 = 250 ml, the bark container and the clay pot hold 1000 ml.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOLiquidIngredientTest
{
	const FName Water(TEXT("Water01"));
	const FName CleanWater(TEXT("CleanWater01"));
	const FName BarkContainer(TEXT("BarkContainer01"));
	const FName Pot(TEXT("CrudePottery01"));

	struct FWorld
	{
		UWorld* World = nullptr;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("MOLiquidIngredientTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		UMOInventoryComponent* Spawn()
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UMOInventoryComponent* Inv = NewObject<UMOInventoryComponent>(Actor);
			Inv->SetSlotCountAuthority(12);
			Actor->AddInstanceComponent(Inv);
			Inv->RegisterComponent();
			Actor->DispatchBeginPlay();
			return Inv;
		}
	};

	FMOLiquidContents Liquid(float Ml, bool bBoiled, float Salinity, float Illness)
	{
		FMOLiquidContents L;
		L.Millilitres = Ml;
		L.bBoiled = bBoiled;
		L.SalinityGPerL = Salinity;
		L.IllnessChancePerLitre = Illness;
		return L;
	}

	float MlIn(const UMOInventoryComponent& Inv, const FGuid& Guid)
	{
		FMOInventoryEntry Entry;
		return Inv.TryGetEntryByGuid(Guid, Entry) ? Entry.Liquid.Millilitres : -1.0f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOLiquidIngredientTest, "MOFramework.Inventory.LiquidIngredient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOLiquidIngredientTest::RunTest(const FString& Parameters)
{
	using namespace MOLiquidIngredientTest;

	FMOItemDefinitionRow WaterDef, CleanDef;
	if (!TestTrue(TEXT("Water01 and CleanWater01 are in the item table"), UMOItemDatabaseSettings::GetItemDefinition(Water, WaterDef)
		&& UMOItemDatabaseSettings::GetItemDefinition(CleanWater, CleanDef)))
	{
		return false;
	}
	TestEqual(TEXT("Water01 is a 250 ml measure"), WaterDef.LiquidMeasureMl, 250.0f);
	TestTrue(TEXT("CleanWater01 is a 250 ml measure of SAFE water"), CleanDef.LiquidMeasureMl == 250.0f && CleanDef.bMeasureNeedsSafeWater);

	FWorld W;
	UMOInventoryComponent* Inv = W.Spawn();
	const FGuid Bark = FGuid::NewGuid();
	const FGuid Pottery = FGuid::NewGuid();
	const FGuid SeaPot = FGuid::NewGuid();
	Inv->AddItemByGuid(Bark, BarkContainer, 1);
	Inv->AddItemByGuid(Pottery, Pot, 1);
	Inv->AddItemByGuid(SeaPot, Pot, 1);

	// 600 ml of untreated river water in the bark container: two whole measures of water, none of clean water.
	Inv->SetLiquid(Bark, Liquid(600.0f, false, 0.1f, 0.005f));
	TestEqual(TEXT("600 ml of river water = 2 measures of water"), Inv->CountIngredient(Water), 2);
	TestEqual(TEXT("... and none of clean water (not boiled)"), Inv->CountIngredient(CleanWater), 0);

	// Boiled sea water is not clean water: boiling does not take the salt out.
	Inv->SetLiquid(SeaPot, Liquid(1000.0f, true, 35.0f, 0.0f));
	TestEqual(TEXT("boiled sea water is not clean water"), Inv->CountIngredient(CleanWater), 0);
	TestEqual(TEXT("... though it is water (6 measures with the river water)"), Inv->CountIngredient(Water), 6);
	Inv->SetLiquid(SeaPot, FMOLiquidContents());

	// Taking: all or nothing, from the liquid.
	TestTrue(TEXT("take 2 measures of water"), Inv->ConsumeIngredient(Water, 2));
	TestEqual(TEXT("100 ml left in the bark container"), MlIn(*Inv, Bark), 100.0f, 0.5f);
	TestFalse(TEXT("a third measure is not there"), Inv->ConsumeIngredient(Water, 1));
	TestEqual(TEXT("... and nothing was taken"), MlIn(*Inv, Bark), 100.0f, 0.5f);

	// Boiled fresh water in the pot is clean water (and water).
	Inv->SetLiquid(Pottery, Liquid(500.0f, true, 0.1f, 0.0f));
	TestEqual(TEXT("500 ml boiled fresh water = 2 measures of clean water"), Inv->CountIngredient(CleanWater), 2);
	TestEqual(TEXT("water counts every suitable container (100 + 500 ml)"), Inv->CountIngredient(Water), 2);

	// Pouring: clean water goes to the pot already holding clean water, never into the untreated bark container.
	TestEqual(TEXT("pour 1 measure of clean water"), Inv->AddIngredient(CleanWater, 1), 1);
	TestEqual(TEXT("into the pot of clean water"), MlIn(*Inv, Pottery), 750.0f, 0.5f);
	TestEqual(TEXT("the bark container is untouched"), MlIn(*Inv, Bark), 100.0f, 0.5f);
	// Untreated water is not poured into clean water (it would spoil it): it goes to the bark container.
	TestEqual(TEXT("pour 1 measure of water"), Inv->AddIngredient(Water, 1), 1);
	TestEqual(TEXT("into the bark container"), MlIn(*Inv, Bark), 350.0f, 0.5f);
	TestEqual(TEXT("the clean pot stays clean"), Inv->CountIngredient(CleanWater), 3);

	// More than there is room for: the pot of clean water fills first, then the empty pot; the rest is spilled, and the count says so.
	TestEqual(TEXT("8 more measures of clean water: 250 ml + 1000 ml of room = 5 placed"), Inv->AddIngredient(CleanWater, 8), 5);
	TestEqual(TEXT("the clean pot is full"), MlIn(*Inv, Pottery), 1000.0f, 0.5f);
	TestEqual(TEXT("the empty pot took 1000 ml"), MlIn(*Inv, SeaPot), 1000.0f, 0.5f);
	TestEqual(TEXT("the untreated bark container still untouched"), MlIn(*Inv, Bark), 350.0f, 0.5f);

	// An ordinary ingredient is still the items carried.
	TestEqual(TEXT("an ordinary item counts as items (two pots carried)"), Inv->CountIngredient(Pot), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
