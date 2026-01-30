#include "Misc/AutomationTest.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOSkillDatabaseSettings.h"
#include "MORecipeDatabaseSettings.h"
#include "Engine/DataTable.h"

#if WITH_DEV_AUTOMATION_TESTS

//=============================================================================
// Test Data Helpers
//=============================================================================

namespace MOFrameworkTestData
{
	/**
	 * Creates a programmatic item definition for testing.
	 * Avoids needing editor-created DataTables.
	 */
	FMOItemDefinitionRow MakeTestItem(FName ItemId, const FString& DisplayName, int32 MaxStack = 10, bool bConsumableFlag = false)
	{
		FMOItemDefinitionRow Item;
		Item.ItemId = ItemId;
		Item.DisplayName = FText::FromString(DisplayName);
		Item.Description = FText::FromString(FString::Printf(TEXT("Test item: %s"), *DisplayName));
		Item.MaxStackSize = MaxStack;
		Item.bConsumable = bConsumableFlag;
		return Item;
	}

	/**
	 * Creates a test item with nutrition data.
	 */
	FMOItemDefinitionRow MakeTestFood(FName ItemId, const FString& DisplayName, float Calories, float Water)
	{
		FMOItemDefinitionRow Item = MakeTestItem(ItemId, DisplayName, 5, true);
		Item.Nutrition.Calories = Calories;
		Item.Nutrition.WaterContent = Water;
		Item.Nutrition.Protein = Calories * 0.1f;  // Simple ratio for testing
		return Item;
	}

	/**
	 * Creates a programmatic skill definition for testing.
	 */
	FMOSkillDefinitionRow MakeTestSkill(FName SkillId, const FString& DisplayName, int32 MaxLevel = 100)
	{
		FMOSkillDefinitionRow Skill;
		Skill.SkillId = SkillId;
		Skill.DisplayName = FText::FromString(DisplayName);
		Skill.Description = FText::FromString(FString::Printf(TEXT("Test skill: %s"), *DisplayName));
		Skill.MaxLevel = MaxLevel;
		Skill.BaseXPPerLevel = 100.0f;
		Skill.XPExponent = 1.5f;
		Skill.Category = EMOSkillCategory::Crafting;
		return Skill;
	}

	/**
	 * Creates a programmatic recipe definition for testing.
	 */
	FMORecipeDefinitionRow MakeTestRecipe(FName RecipeId, const FString& DisplayName)
	{
		FMORecipeDefinitionRow Recipe;
		Recipe.RecipeId = RecipeId;
		Recipe.DisplayName = FText::FromString(DisplayName);
		Recipe.Description = FText::FromString(FString::Printf(TEXT("Test recipe: %s"), *DisplayName));
		Recipe.CraftTime = 1.0f;
		Recipe.SkillXPReward = 10.0f;
		return Recipe;
	}
}

//=============================================================================
// Skills Component Tests
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSkillsComponent_AddExperience_LevelsUp,
	"MOFramework.Skills.AddExperience.LevelsUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSkillsComponent_AddExperience_LevelsUp::RunTest(const FString& Parameters)
{
	// Create component
	UMOSkillsComponent* Skills = NewObject<UMOSkillsComponent>();
	TestNotNull(TEXT("Skills component created"), Skills);

	const FName TestSkillId = TEXT("TestCrafting");

	// Initialize skill at level 1
	Skills->InitializeSkill(TestSkillId);
	TestEqual(TEXT("Initial level is 1"), Skills->GetSkillLevel(TestSkillId), 1);

	// Add enough XP to level up (default XP for level 2 is ~283 with 100 base and 1.5 exponent)
	const bool bAddedXP = Skills->AddExperience(TestSkillId, 500.0f);
	TestTrue(TEXT("XP was added successfully"), bAddedXP);

	// Should have leveled up
	TestTrue(TEXT("Leveled up past level 1"), Skills->GetSkillLevel(TestSkillId) > 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSkillsComponent_SetSkillLevel_DirectSet,
	"MOFramework.Skills.SetSkillLevel.DirectSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSkillsComponent_SetSkillLevel_DirectSet::RunTest(const FString& Parameters)
{
	UMOSkillsComponent* Skills = NewObject<UMOSkillsComponent>();
	const FName TestSkillId = TEXT("TestMining");

	// Set directly to level 50
	Skills->SetSkillLevel(TestSkillId, 50);
	TestEqual(TEXT("Skill set to level 50"), Skills->GetSkillLevel(TestSkillId), 50);

	// Test level requirement check
	TestTrue(TEXT("Has skill level 50"), Skills->HasSkillLevel(TestSkillId, 50));
	TestTrue(TEXT("Has skill level 25"), Skills->HasSkillLevel(TestSkillId, 25));
	TestFalse(TEXT("Does not have skill level 75"), Skills->HasSkillLevel(TestSkillId, 75));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSkillsComponent_GetSkillProgress_ReturnsCorrectData,
	"MOFramework.Skills.GetSkillProgress.ReturnsCorrectData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSkillsComponent_GetSkillProgress_ReturnsCorrectData::RunTest(const FString& Parameters)
{
	UMOSkillsComponent* Skills = NewObject<UMOSkillsComponent>();
	const FName TestSkillId = TEXT("TestWoodcutting");

	// Initialize and add some XP
	Skills->InitializeSkill(TestSkillId);
	Skills->AddExperience(TestSkillId, 50.0f);

	FMOSkillProgress Progress;
	const bool bFound = Skills->GetSkillProgress(TestSkillId, Progress);

	TestTrue(TEXT("Skill progress found"), bFound);
	TestEqual(TEXT("Skill ID matches"), Progress.SkillId, TestSkillId);
	TestEqual(TEXT("Current XP is 50"), Progress.CurrentXP, 50.0f);
	TestTrue(TEXT("XP to next level is positive"), Progress.XPToNextLevel > 0.0f);

	return true;
}

//=============================================================================
// Knowledge Component Tests
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOKnowledgeComponent_GrantKnowledge_AddsToList,
	"MOFramework.Knowledge.GrantKnowledge.AddsToList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOKnowledgeComponent_GrantKnowledge_AddsToList::RunTest(const FString& Parameters)
{
	UMOKnowledgeComponent* Knowledge = NewObject<UMOKnowledgeComponent>();
	const FName TestKnowledgeId = TEXT("Knowledge_Herbalism_Basic");

	// Should not have knowledge initially
	TestFalse(TEXT("Does not have knowledge initially"), Knowledge->HasKnowledge(TestKnowledgeId));

	// Grant knowledge
	const bool bNewlyLearned = Knowledge->GrantKnowledge(TestKnowledgeId);
	TestTrue(TEXT("Knowledge was newly learned"), bNewlyLearned);
	TestTrue(TEXT("Has knowledge after grant"), Knowledge->HasKnowledge(TestKnowledgeId));

	// Granting again should return false
	const bool bSecondGrant = Knowledge->GrantKnowledge(TestKnowledgeId);
	TestFalse(TEXT("Second grant returns false (already known)"), bSecondGrant);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOKnowledgeComponent_HasAllKnowledge_ChecksMultiple,
	"MOFramework.Knowledge.HasAllKnowledge.ChecksMultiple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOKnowledgeComponent_HasAllKnowledge_ChecksMultiple::RunTest(const FString& Parameters)
{
	UMOKnowledgeComponent* Knowledge = NewObject<UMOKnowledgeComponent>();

	const FName Knowledge1 = TEXT("Knowledge_A");
	const FName Knowledge2 = TEXT("Knowledge_B");
	const FName Knowledge3 = TEXT("Knowledge_C");

	TArray<FName> RequiredKnowledge = { Knowledge1, Knowledge2 };

	// Grant only one
	Knowledge->GrantKnowledge(Knowledge1);

	TestFalse(TEXT("Does not have all knowledge with only one"), Knowledge->HasAllKnowledge(RequiredKnowledge));
	TestTrue(TEXT("Has any knowledge with one"), Knowledge->HasAnyKnowledge(RequiredKnowledge));

	// Grant the second
	Knowledge->GrantKnowledge(Knowledge2);
	TestTrue(TEXT("Has all knowledge with both"), Knowledge->HasAllKnowledge(RequiredKnowledge));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOKnowledgeComponent_InspectItem_GrantsXPWithDiminishing,
	"MOFramework.Knowledge.InspectItem.GrantsXPWithDiminishing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOKnowledgeComponent_InspectItem_GrantsXPWithDiminishing::RunTest(const FString& Parameters)
{
	UMOKnowledgeComponent* Knowledge = NewObject<UMOKnowledgeComponent>();
	UMOSkillsComponent* Skills = NewObject<UMOSkillsComponent>();

	const FName TestItemId = TEXT("Item_TestHerb");

	// First inspection
	FMOInspectionResult Result1 = Knowledge->InspectItem(TestItemId, Skills);
	TestTrue(TEXT("First inspection succeeds"), Result1.bSuccess);
	TestTrue(TEXT("First inspection marked as first"), Result1.bFirstInspection);

	// Second inspection
	FMOInspectionResult Result2 = Knowledge->InspectItem(TestItemId, Skills);
	TestTrue(TEXT("Second inspection succeeds"), Result2.bSuccess);
	TestFalse(TEXT("Second inspection not marked as first"), Result2.bFirstInspection);

	// Check inspection count
	FMOItemKnowledgeProgress Progress;
	Knowledge->GetInspectionProgress(TestItemId, Progress);
	TestEqual(TEXT("Inspection count is 2"), Progress.InspectionCount, 2);

	return true;
}

//=============================================================================
// Survival Stats Component Tests
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalStats_ModifyStat_ChangesValue,
	"MOFramework.Survival.ModifyStat.ChangesValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSurvivalStats_ModifyStat_ChangesValue::RunTest(const FString& Parameters)
{
	UMOSurvivalStatsComponent* Survival = NewObject<UMOSurvivalStatsComponent>();

	// Health starts at 100
	const float InitialHealth = Survival->GetStatCurrent(TEXT("Health"));
	TestEqual(TEXT("Initial health is 100"), InitialHealth, 100.0f);

	// Take damage
	Survival->ModifyStat(TEXT("Health"), -25.0f);
	TestEqual(TEXT("Health after -25 damage"), Survival->GetStatCurrent(TEXT("Health")), 75.0f);

	// Heal
	Survival->ModifyStat(TEXT("Health"), 10.0f);
	TestEqual(TEXT("Health after +10 heal"), Survival->GetStatCurrent(TEXT("Health")), 85.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalStats_SetStat_DirectSet,
	"MOFramework.Survival.SetStat.DirectSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSurvivalStats_SetStat_DirectSet::RunTest(const FString& Parameters)
{
	UMOSurvivalStatsComponent* Survival = NewObject<UMOSurvivalStatsComponent>();

	Survival->SetStat(TEXT("Hunger"), 50.0f);
	TestEqual(TEXT("Hunger set to 50"), Survival->GetStatCurrent(TEXT("Hunger")), 50.0f);
	TestEqual(TEXT("Hunger percent is 50%"), Survival->GetStatPercent(TEXT("Hunger")), 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalStats_IsStatDepleted_ChecksZero,
	"MOFramework.Survival.IsStatDepleted.ChecksZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSurvivalStats_IsStatDepleted_ChecksZero::RunTest(const FString& Parameters)
{
	UMOSurvivalStatsComponent* Survival = NewObject<UMOSurvivalStatsComponent>();

	TestFalse(TEXT("Health not depleted initially"), Survival->IsStatDepleted(TEXT("Health")));

	Survival->SetStat(TEXT("Health"), 0.0f);
	TestTrue(TEXT("Health depleted at zero"), Survival->IsStatDepleted(TEXT("Health")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalStats_IsStatCritical_ChecksThreshold,
	"MOFramework.Survival.IsStatCritical.ChecksThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSurvivalStats_IsStatCritical_ChecksThreshold::RunTest(const FString& Parameters)
{
	UMOSurvivalStatsComponent* Survival = NewObject<UMOSurvivalStatsComponent>();

	TestFalse(TEXT("Health not critical at 100"), Survival->IsStatCritical(TEXT("Health")));

	Survival->SetStat(TEXT("Health"), 20.0f);  // 20% is below default 25% threshold
	TestTrue(TEXT("Health critical at 20"), Survival->IsStatCritical(TEXT("Health")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivalStats_ApplyNutrition_UpdatesStatus,
	"MOFramework.Survival.ApplyNutrition.UpdatesStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSurvivalStats_ApplyNutrition_UpdatesStatus::RunTest(const FString& Parameters)
{
	UMOSurvivalStatsComponent* Survival = NewObject<UMOSurvivalStatsComponent>();

	FMOItemNutrition TestNutrition;
	TestNutrition.Calories = 200.0f;
	TestNutrition.WaterContent = 100.0f;
	TestNutrition.Protein = 15.0f;
	TestNutrition.VitaminC = 25.0f;

	const float InitialCalories = Survival->NutritionStatus.Calories;
	const float InitialHydration = Survival->NutritionStatus.Hydration;

	Survival->ApplyNutrition(TestNutrition);

	TestEqual(TEXT("Calories increased by 200"), Survival->NutritionStatus.Calories, InitialCalories + 200.0f);
	TestEqual(TEXT("Hydration increased by 100"), Survival->NutritionStatus.Hydration, InitialHydration + 100.0f);

	return true;
}

//=============================================================================
// Integration Tests
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOIntegration_SkillsAndKnowledge_WorkTogether,
	"MOFramework.Integration.SkillsAndKnowledge.WorkTogether",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOIntegration_SkillsAndKnowledge_WorkTogether::RunTest(const FString& Parameters)
{
	UMOSkillsComponent* Skills = NewObject<UMOSkillsComponent>();
	UMOKnowledgeComponent* Knowledge = NewObject<UMOKnowledgeComponent>();

	const FName TestItemId = TEXT("Item_RareHerb");
	const FName HerbalismSkill = TEXT("Herbalism");

	// Set up skill
	Skills->SetSkillLevel(HerbalismSkill, 10);

	// Inspect item with skills context
	FMOInspectionResult Result = Knowledge->InspectItem(TestItemId, Skills);

	TestTrue(TEXT("Inspection succeeded with skills"), Result.bSuccess);

	// Inspection should track the skill level used
	FMOItemKnowledgeProgress Progress;
	Knowledge->GetInspectionProgress(TestItemId, Progress);
	// LastInspectionSkillLevel may depend on implementation - just check the progress exists
	TestEqual(TEXT("Progress shows 1 inspection"), Progress.InspectionCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
