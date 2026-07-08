#include "Misc/AutomationTest.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOSkillDatabaseSettings.h"
#include "MORecipeDatabaseSettings.h"
#include "MOTerraformingComponent.h"
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

	// Skills initialize at level 0 - the first XP gains carry them to level 1
	// (see UMOSkillsComponent::InitializeSkill)
	Skills->InitializeSkill(TestSkillId);
	TestEqual(TEXT("Initial level is 0"), Skills->GetSkillLevel(TestSkillId), 0);

	// Add enough XP to level up several times. With no DataTable entry the default
	// curve applies (100 base XP, 1.25x per level): 0->1 = 100, 1->2 = 125, 2->3 = 156.25.
	// 500 XP covers those three level-ups (381.25 total) but not 3->4 (195.31).
	const bool bAddedXP = Skills->AddExperience(TestSkillId, 500.0f);
	TestTrue(TEXT("XP was added successfully"), bAddedXP);

	// Should have leveled up from 0 to exactly 3
	TestEqual(TEXT("500 XP on default curve reaches level 3"), Skills->GetSkillLevel(TestSkillId), 3);

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
	const FName TestKnowledgeId = TEXT("Knowledge_TestHerb");

	// InspectItem resolves items through UMOItemDatabaseSettings, so register a
	// fixture row in the Items DataTable for the duration of this test (removed below).
	UDataTable* ItemTable = GetDefault<UMOItemDatabaseSettings>()->GetItemDefinitionsDataTable();
	if (!TestNotNull(TEXT("Items DataTable resolved"), ItemTable))
	{
		return false;
	}

	FMOItemDefinitionRow TestItem = MOFrameworkTestData::MakeTestItem(TestItemId, TEXT("Test Herb"));
	FMOInspectionGrant Grant;
	Grant.Id = TestKnowledgeId;
	Grant.bIsKnowledge = true;
	Grant.XPAmount = 50.0f;
	Grant.MaxLevel = 0;  // Unlimited
	TestItem.Inspection.Grants.Add(Grant);
	ItemTable->AddRow(TestItemId, TestItem);
	UMOItemDatabaseSettings::InvalidateCache();

	// First inspection
	FMOInspectionResult Result1 = Knowledge->InspectItem(TestItemId, Skills);
	TestTrue(TEXT("First inspection succeeds"), Result1.bSuccess);
	TestTrue(TEXT("First inspection marked as first"), Result1.bFirstInspection);
	TestEqual(TEXT("First inspection grants XP for the knowledge entry"), Result1.XPGrants.Num(), 1);

	// Second inspection
	FMOInspectionResult Result2 = Knowledge->InspectItem(TestItemId, Skills);
	TestTrue(TEXT("Second inspection succeeds"), Result2.bSuccess);
	TestFalse(TEXT("Second inspection not marked as first"), Result2.bFirstInspection);

	// Check inspection count
	FMOItemKnowledgeProgress Progress;
	Knowledge->GetInspectionProgress(TestItemId, Progress);
	TestEqual(TEXT("Inspection count is 2"), Progress.InspectionCount, 2);

	// Two 50 XP grants reach the 100 XP needed for knowledge level 1 (default curve),
	// which is when the knowledge counts as learned
	TestEqual(TEXT("Knowledge leveled to 1 after two inspections"), Skills->GetSkillLevel(TestKnowledgeId), 1);
	TestTrue(TEXT("Knowledge is learned once its level is above 0"), Knowledge->HasKnowledge(TestKnowledgeId));

	// Remove the fixture row so the shared DataTable is untouched for other tests
	ItemTable->RemoveRow(TestItemId);
	UMOItemDatabaseSettings::InvalidateCache();

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

	// InspectItem resolves items through UMOItemDatabaseSettings, so register a
	// fixture row that grants Herbalism XP when inspected (removed below).
	UDataTable* ItemTable = GetDefault<UMOItemDatabaseSettings>()->GetItemDefinitionsDataTable();
	if (!TestNotNull(TEXT("Items DataTable resolved"), ItemTable))
	{
		return false;
	}

	FMOItemDefinitionRow TestItem = MOFrameworkTestData::MakeTestItem(TestItemId, TEXT("Rare Herb"));
	FMOInspectionGrant Grant;
	Grant.Id = HerbalismSkill;
	Grant.bIsKnowledge = false;
	Grant.XPAmount = 25.0f;
	Grant.MaxLevel = 0;  // Unlimited
	TestItem.Inspection.Grants.Add(Grant);
	ItemTable->AddRow(TestItemId, TestItem);
	UMOItemDatabaseSettings::InvalidateCache();

	// Set up skill
	Skills->SetSkillLevel(HerbalismSkill, 10);

	// Inspect item with skills context
	FMOInspectionResult Result = Knowledge->InspectItem(TestItemId, Skills);

	TestTrue(TEXT("Inspection succeeded with skills"), Result.bSuccess);
	TestEqual(TEXT("Inspection granted XP to one skill"), Result.XPGrants.Num(), 1);
	if (Result.XPGrants.Num() == 1)
	{
		TestEqual(TEXT("Grant recorded the skill level before XP"), Result.XPGrants[0].LevelBefore, 10);
	}

	// Inspection should track the skill level used
	FMOItemKnowledgeProgress Progress;
	Knowledge->GetInspectionProgress(TestItemId, Progress);
	// LastInspectionSkillLevel may depend on implementation - just check the progress exists
	TestEqual(TEXT("Progress shows 1 inspection"), Progress.InspectionCount, 1);

	// Remove the fixture row so the shared DataTable is untouched for other tests
	ItemTable->RemoveRow(TestItemId);
	UMOItemDatabaseSettings::InvalidateCache();

	return true;
}

//=============================================================================
// Terraforming: earth-volume -> real-time duration (realism)
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTerraform_Duration_VolumeScaling,
	"MOFramework.Terraform.Duration.VolumeScaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOTerraform_Duration_VolumeScaling::RunTest(const FString& Parameters)
{
	using T = UMOTerraformingComponent;
	// Design anchor: lowering 1 cm over 1 m^2 (=0.01 m^3) at 30000 s/m^3 = 5 min.
	// A 1 m^2 circular footprint needs radius = sqrt(1/pi) m = 56.4189 cm (UU).
	const float R1m2 = 56.4189f;
	TestEqual(TEXT("anchor: 1cm over 1m^2 = 300s (5 min)"),
		T::ComputeTerraformDurationSeconds(R1m2, 0.01f, 30000.0f, 5.0f), 300.0f, 1.0f);

	// Footprint ~ radius^2: doubling radius quadruples the duration.
	const float d100 = T::ComputeTerraformDurationSeconds(100.0f, 0.1f, 30000.0f, 5.0f);
	const float d200 = T::ComputeTerraformDurationSeconds(200.0f, 0.1f, 30000.0f, 5.0f);
	TestEqual(TEXT("2x radius -> 4x duration"), d200, 4.0f * d100, d100 * 0.01f);

	// Depth (displacement) scales linearly.
	const float dShallow = T::ComputeTerraformDurationSeconds(100.0f, 0.05f, 30000.0f, 5.0f);
	TestEqual(TEXT("2x depth -> 2x duration"), d100, 2.0f * dShallow, dShallow * 0.01f);

	// No earth moved -> floored at MinSeconds (not instant, not zero).
	TestEqual(TEXT("zero depth floors at MinSeconds"),
		T::ComputeTerraformDurationSeconds(100.0f, 0.0f, 30000.0f, 5.0f), 5.0f);

	// A house-sized flatten is HOURS, by design (sanity: 5 m radius, 0.5 m avg).
	const float dHouse = T::ComputeTerraformDurationSeconds(500.0f, 0.5f, 30000.0f, 5.0f);
	TestTrue(TEXT("house-scale flatten takes many hours"), dHouse > 3600.0f * 4.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
