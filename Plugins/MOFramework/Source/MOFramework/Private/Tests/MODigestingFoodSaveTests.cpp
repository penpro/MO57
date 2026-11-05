// A meal in the stomach keeps its vitamins and minerals through a save (the June audit's L9): the save entry listed the macronutrients only, so what was still digesting lost them at a
// load. Compared field by field, so a new nutrient on FMODigestingFood fails here until the save carries it. Expected: 1.5 back; failing before: 0.
// Run: python Tools/ue.py auto --filter "MOFramework.Medical.Metabolism.DigestingFoodSaved"

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "MOMetabolismComponent.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_DigestingFoodSaved, "MOFramework.Medical.Metabolism.DigestingFoodSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_DigestingFoodSaved::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* From = NewObject<UMOMetabolismComponent>(NewObject<AActor>());
	FMODigestingFood Meal;
	for (TFieldIterator<FFloatProperty> Field(FMODigestingFood::StaticStruct()); Field; ++Field)
	{
		Field->SetPropertyValue_InContainer(&Meal, 1.5f);
	}
	From->DigestingFood.Items.Add(Meal);

	FMOMetabolismSaveData Saved;
	From->BuildSaveData(Saved);
	UMOMetabolismComponent* To = NewObject<UMOMetabolismComponent>(NewObject<AActor>());
	TestTrue(TEXT("the save applies"), To->ApplySaveDataAuthority(Saved));
	if (!TestEqual(TEXT("the meal is still in the stomach"), To->DigestingFood.Items.Num(), 1))
	{
		return false;
	}
	for (TFieldIterator<FFloatProperty> Field(FMODigestingFood::StaticStruct()); Field; ++Field)
	{
		TestEqual(*FString::Printf(TEXT("%s survives the save"), *Field->GetName()), Field->GetPropertyValue_InContainer(&To->DigestingFood.Items[0]), 1.5f);
	}
	return true;
}

#endif
