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
#include "MOGameClockSubsystem.h"
#include "MOQuestSubsystem.h"
#include "MOPossessionTypes.h"
#include "MOWorldSeedSubsystem.h"
#include "MOGameState.h"
#include "MOWorldSyncSubsystem.h"
#include "MOPlayerController.h"
#include "MOGameMode.h"
#include "MOPersistenceSubsystem.h"
#include "MOUIUtils.h"
#include "MOCommunitySettings.h"
#include "Components/EditableTextBox.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
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

//=============================================================================
// Game clock: offline advance is capped (no starvation / no year-of-work bank)
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOClock_OfflineAdvance_Cap,
	"MOFramework.Clock.OfflineAdvanceCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOClock_OfflineAdvance_Cap::RunTest(const FString& Parameters)
{
	using C = UMOGameClockSubsystem;
	// A short gap advances 1:1.
	TestTrue(TEXT("1h gap advances 1h"),
		FMath::IsNearlyEqual(C::ComputeOfflineAdvanceSeconds(3600.0, 4.0), 3600.0, 0.001));
	// A year offline caps at 4h — you can't bank a year of queued work.
	TestTrue(TEXT("1yr gap caps at 4h"),
		FMath::IsNearlyEqual(C::ComputeOfflineAdvanceSeconds(365.0 * 24 * 3600, 4.0), 4.0 * 3600.0, 0.001));
	// Exactly at the cap stays at the cap.
	TestTrue(TEXT("exactly 4h stays 4h"),
		FMath::IsNearlyEqual(C::ComputeOfflineAdvanceSeconds(4.0 * 3600, 4.0), 4.0 * 3600.0, 0.001));
	// Clock skew / same instant never rewinds.
	TestTrue(TEXT("negative gap -> 0"),
		FMath::IsNearlyEqual(C::ComputeOfflineAdvanceSeconds(-500.0, 4.0), 0.0, 0.001));
	// A zero cap disables offline advance entirely.
	TestTrue(TEXT("zero cap -> 0"),
		FMath::IsNearlyEqual(C::ComputeOfflineAdvanceSeconds(9999.0, 0.0), 0.0, 0.001));
	return true;
}

//=============================================================================
// Tutorial hint ordering + possession list entry
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOQuest_TutorialHintOrdering_BySortOrderNotIteration,
	"MOFramework.Quest.TutorialHintOrdering.BySortOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOQuest_TutorialHintOrdering_BySortOrderNotIteration::RunTest(const FString& Parameters)
{
	using Q = UMOQuestSubsystem;

	// Tutorial_Possession (SortOrder 0) must come before Tutorial_Movement (1) no matter which was activated first or
	// how the data table happens to be ordered -- the old code returned whichever the TMap iterated first.
	TestTrue(TEXT("possession (0) precedes movement (1)"), Q::TutorialHintPrecedes(0, TEXT("Tutorial_Possession"), 1, TEXT("Tutorial_Movement")));
	TestFalse(TEXT("movement (1) does not precede possession (0)"), Q::TutorialHintPrecedes(1, TEXT("Tutorial_Movement"), 0, TEXT("Tutorial_Possession")));

	// Equal SortOrder: deterministic by QuestId, and never "both precede each other".
	TestTrue(TEXT("ties break by QuestId"), Q::TutorialHintPrecedes(5, TEXT("A_Quest"), 5, TEXT("B_Quest")));
	TestFalse(TEXT("ties break by QuestId (reverse)"), Q::TutorialHintPrecedes(5, TEXT("B_Quest"), 5, TEXT("A_Quest")));
	TestFalse(TEXT("a quest does not precede itself"), Q::TutorialHintPrecedes(5, TEXT("A_Quest"), 5, TEXT("A_Quest")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPossessionListEntry_RoundTripsDisplayFields,
	"MOFramework.Possession.ListEntry.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPossessionListEntry_RoundTripsDisplayFields::RunTest(const FString& Parameters)
{
	// The slim row a client receives must rebuild exactly the fields the pawn entry widget displays.
	FMOPersistedPawnRecord Record;
	Record.PawnGuid = FGuid::NewGuid();
	Record.CharacterName = TEXT("Ada Quill");
	Record.Gender = TEXT("Female");
	Record.AgeInDays = 30 * 365;
	Record.bIsDeceased = false;
	Record.HealthPercent = 0.75f;
	Record.StatusText = TEXT("Recruited");
	Record.LocationName = TEXT("Riverbend");
	Record.LastPlayedTime = FDateTime(2026, 10, 6, 12, 0, 0);

	const FMOPersistedPawnRecord Back = FMOPossessionListEntry::FromRecord(Record).ToDisplayRecord();
	TestEqual(TEXT("guid"), Back.PawnGuid, Record.PawnGuid);
	TestEqual(TEXT("name"), Back.CharacterName, Record.CharacterName);
	TestEqual(TEXT("gender"), Back.Gender, Record.Gender);
	TestEqual(TEXT("age"), Back.AgeInDays, Record.AgeInDays);
	TestEqual(TEXT("deceased"), Back.bIsDeceased, Record.bIsDeceased);
	TestTrue(TEXT("health"), FMath::IsNearlyEqual(Back.HealthPercent, Record.HealthPercent));
	TestEqual(TEXT("status"), Back.StatusText, Record.StatusText);
	TestEqual(TEXT("location"), Back.LocationName, Record.LocationName);
	TestEqual(TEXT("last played"), Back.LastPlayedTime, Record.LastPlayedTime);
	TestTrue(TEXT("always player-controllable (the server only lists controllable pawns)"), Back.bIsPlayerControllable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldSeed_VoxelSeedStringIsDeterministic,
	"MOFramework.WorldSeed.VoxelSeedString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldSeed_VoxelSeedStringIsDeterministic::RunTest(const FString& Parameters)
{
	// Host and client each turn the replicated int seed into the voxel seed string; if this were not a pure function the
	// two machines would generate different terrain from the "same" seed.
	const FString A1 = UMOWorldSeedSubsystem::IntSeedToVoxelSeedString(16423);
	const FString A2 = UMOWorldSeedSubsystem::IntSeedToVoxelSeedString(16423);
	const FString B = UMOWorldSeedSubsystem::IntSeedToVoxelSeedString(16424);
	TestEqual(TEXT("same seed -> same string"), A1, A2);
	TestNotEqual(TEXT("different seeds -> different strings"), A1, B);
	TestEqual(TEXT("8 characters (the voxel plugin's exposed-seed format)"), A1.Len(), 8);
	for (const TCHAR C : A1)
	{
		TestTrue(TEXT("A-Z only"), C >= TEXT('A') && C <= TEXT('Z'));
	}
	TestEqual(TEXT("the GameMode Blueprint forwarder agrees with the shared implementation"),
		AMOGameMode::IntSeedToVoxelSeedString(16423), A1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldSeed_ReachesClientsThroughGameState,
	"MOFramework.WorldSeed.Replication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldSeed_ReachesClientsThroughGameState::RunTest(const FString& Parameters)
{
	// The co-op terrain bug was "the seed exists only on the host". Pin the three things that carry it to clients.
	const FProperty* Prop = AMOGameState::StaticClass()->FindPropertyByName(TEXT("WorldSeed"));
	if (!TestNotNull(TEXT("AMOGameState::WorldSeed exists"), Prop))
	{
		return false;
	}
	TestTrue(TEXT("WorldSeed is replicated"), Prop->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("WorldSeed has an OnRep (the client regenerates terrain from it)"), Prop->HasAnyPropertyFlags(CPF_RepNotify));

	const AMOGameMode* DefaultMode = GetDefault<AMOGameMode>();
	TestTrue(TEXT("AMOGameMode uses AMOGameState, or nothing can publish the seed"),
		DefaultMode->GameStateClass && DefaultMode->GameStateClass->IsChildOf(AMOGameState::StaticClass()));

	TestFalse(TEXT("unpublished by default: a client must never generate terrain before the host has published a seed"),
		FMOWorldSeedInfo().bPublished);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOClock_FreshWorldStartsAt8AM,
	"MOFramework.Clock.FreshWorldStartsAt8AM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOClock_FreshWorldStartsAt8AM::RunTest(const FString& Parameters)
{
	// "New games always start at 8 AM." The clock's default start is the policy for every fresh world (no save to restore).
	const UMOGameClockSubsystem* Clock = GetDefault<UMOGameClockSubsystem>();
	const FProperty* Prop = UMOGameClockSubsystem::StaticClass()->FindPropertyByName(TEXT("DefaultStartDateTime"));
	if (!TestNotNull(TEXT("DefaultStartDateTime exists"), Prop))
	{
		return false;
	}
	const FDateTime* Start = Prop->ContainerPtrToValuePtr<FDateTime>(Clock);
	TestEqual(TEXT("fresh worlds start at hour 8"), Start->GetHour(), 8);
	TestEqual(TEXT("... and minute 0"), Start->GetMinute(), 0);
	TestEqual(TEXT("... and second 0"), Start->GetSecond(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOConsole_PopupInputIsSanitised,
	"MOFramework.Console.PopupInputIsSanitised",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOConsole_PopupInputIsSanitised::RunTest(const FString& Parameters)
{
	// The Tilde that opens the console popup can land in its text box as a typed character; it must not reach the console.
	TestEqual(TEXT("plain command untouched"), AMOPlayerController::SanitizeDevConsoleInput(TEXT("starter")), FString(TEXT("starter")));
	TestEqual(TEXT("leading backtick stripped"), AMOPlayerController::SanitizeDevConsoleInput(TEXT("`starter")), FString(TEXT("starter")));
	TestEqual(TEXT("leading tildes and spaces stripped"), AMOPlayerController::SanitizeDevConsoleInput(TEXT("  ~~ MO.Clock.Info  ")), FString(TEXT("MO.Clock.Info")));
	TestEqual(TEXT("only the opening key press is stripped"), AMOPlayerController::SanitizeDevConsoleInput(TEXT("a`b~c")), FString(TEXT("a`b~c")));
	TestTrue(TEXT("a lone tilde becomes empty (nothing to run)"), AMOPlayerController::SanitizeDevConsoleInput(TEXT(" ` ")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldSync_ClockAndWeatherReachClients,
	"MOFramework.WorldSync.Replication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldSync_ClockAndWeatherReachClients::RunTest(const FString& Parameters)
{
	// Co-op clients used to run a private day and a private sky. The host's clock and weather travel on AMOGameState.
	for (const TCHAR* Name : { TEXT("WorldClock"), TEXT("WorldWeather") })
	{
		const FProperty* Prop = AMOGameState::StaticClass()->FindPropertyByName(Name);
		if (!TestNotNull(FString::Printf(TEXT("AMOGameState::%s exists"), Name), Prop))
		{
			return false;
		}
		TestTrue(FString::Printf(TEXT("%s is replicated"), Name), Prop->HasAnyPropertyFlags(CPF_Net));
		TestTrue(FString::Printf(TEXT("%s has an OnRep (the client's clock / sky follows it)"), Name), Prop->HasAnyPropertyFlags(CPF_RepNotify));
	}
	TestFalse(TEXT("an unpublished clock must never touch a client's clock"), FMOWorldClockInfo().bPublished);
	TestFalse(TEXT("an unpublished weather must never touch a client's sky"), FMOWorldWeatherInfo().bPublished);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldSync_SnapThresholdScalesWithTimeScale,
	"MOFramework.WorldSync.SnapThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldSync_SnapThresholdScalesWithTimeScale::RunTest(const FString& Parameters)
{
	// Ordinary jitter between two machines is tens of milliseconds of REAL time; at 60x that is seconds of GAME time and must
	// not read as drift, or a fast-forwarded clock would stutter. At normal speed the bar is one game-second.
	TestEqual(TEXT("1x: one game-second"), UMOWorldSyncSubsystem::SnapThresholdGameSeconds(1.0f), 1.0);
	TestEqual(TEXT("60x: scales up"), UMOWorldSyncSubsystem::SnapThresholdGameSeconds(60.0f), 15.0);
	TestTrue(TEXT("never below one second (slow-motion clocks)"), UMOWorldSyncSubsystem::SnapThresholdGameSeconds(0.1f) >= 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPersistence_ThumbnailEncoding,
	"MOFramework.Persistence.ThumbnailEncoding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPersistence_ThumbnailEncoding::RunTest(const FString& Parameters)
{
	// The packaged game once wrote a BLANK 193-byte thumbnail (the viewport read failed and the zeros were encoded anyway). The encoder is
	// pure now, so pin what a thumbnail must be: square, the picture's centre (not a squashed 16:9 frame), and not uniform.
	const int32 N = UMOPersistenceSubsystem::ThumbnailSize;
	auto Decode = [this, N](const TArray<uint8>& Png, TArray<FColor>& Out) -> bool
	{
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
		TArray64<uint8> Raw;
		if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Png.GetData(), Png.Num()) || !Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw)
			|| Wrapper->GetWidth() != N || Wrapper->GetHeight() != N)
		{
			return false;
		}
		Out.SetNumUninitialized(N * N);
		FMemory::Memcpy(Out.GetData(), Raw.GetData(), N * N * sizeof(FColor));
		return true;
	};

	// 160x90 frame: left half red, right half blue, with a per-pixel green pattern so it compresses like a picture and not like a flat fill
	// (a flat two-colour frame is as small as a blank one, which would make the control below meaningless).
	const int32 W = 160, H = 90;
	TArray<FColor> Frame;
	Frame.SetNumUninitialized(W * H);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const uint8 Detail = static_cast<uint8>((X * 7 + Y * 13 + (X * Y) % 31) & 0xFF);
			Frame[Y * W + X] = X < W / 2 ? FColor(255, Detail, 0, 255) : FColor(0, Detail, 255, 255);
		}
	}
	TArray<uint8> Png;
	TestTrue(TEXT("a normal frame encodes"), UMOPersistenceSubsystem::EncodeThumbnailPng(W, H, Frame, Png));
	TArray<FColor> Pixels;
	if (TestTrue(TEXT("and decodes to a ThumbnailSize x ThumbnailSize image"), Decode(Png, Pixels)))
	{
		const FColor Left = Pixels[(N / 2) * N + 2], Right = Pixels[(N / 2) * N + N - 3];
		TestTrue(TEXT("left edge of the crop is red"), Left.R > 200 && Left.B < 50);
		TestTrue(TEXT("right edge of the crop is blue"), Right.B > 200 && Right.R < 50);
	}

	// CONTROL: the failure that shipped was a uniform image. It must encode to something far smaller than a real picture, so a size check
	// (and a "has more than one colour" check) can tell the two apart.
	TArray<FColor> Blank;
	Blank.Init(FColor(0, 0, 0, 255), W * H);
	TArray<uint8> BlankPng;
	TestTrue(TEXT("a blank frame still encodes"), UMOPersistenceSubsystem::EncodeThumbnailPng(W, H, Blank, BlankPng));
	TestTrue(TEXT("CONTROL: a blank frame is much smaller than a real one (the bug's 193-byte signature)"), BlankPng.Num() < Png.Num());

	// Bad input is refused, not encoded as zeros.
	TArray<uint8> Unused;
	TestFalse(TEXT("an empty bitmap is refused"), UMOPersistenceSubsystem::EncodeThumbnailPng(W, H, TArray<FColor>(), Unused));
	TestFalse(TEXT("a mis-sized bitmap is refused"), UMOPersistenceSubsystem::EncodeThumbnailPng(W + 1, H, Frame, Unused));
	TestFalse(TEXT("zero width is refused"), UMOPersistenceSubsystem::EncodeThumbnailPng(0, H, Frame, Unused));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_TextInputStyleIsReadable,
	"MOFramework.UI.TextInputStyleIsReadable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_TextInputStyleIsReadable::RunTest(const FString& Parameters)
{
	// The default UMG field is mid-grey text on a light-grey box. Typed text takes its colour from FocusedForegroundColor while the box has
	// focus, so that one matters most; the style is edited IN PLACE (UEditableTextBox::SetWidgetStyle leaves Slate pointing at its parameter).
	auto Luminance = [](const FSlateColor& C) { const FLinearColor L = C.GetSpecifiedColor(); return 0.2126f * L.R + 0.7152f * L.G + 0.0722f * L.B; };

	UEditableTextBox* Control = NewObject<UEditableTextBox>(GetTransientPackage());
	// CONTROL: the engine default is a mid grey (measured: linear luminance 0.285 for all three), so "dark" below is a real change.
	TestTrue(TEXT("CONTROL: the untouched default field's typed-text colour is NOT dark (the complaint)"),
		Luminance(Control->WidgetStyle.FocusedForegroundColor) > 0.2f && Luminance(Control->WidgetStyle.TextStyle.ColorAndOpacity) > 0.2f);

	UEditableTextBox* Box = NewObject<UEditableTextBox>(GetTransientPackage());
	Box->TakeWidget(); // the Slate widget exists, as in a live panel, when the style is applied
	UMOUIUtils::ApplyReadableTextInputStyle(Box);
	TestTrue(TEXT("focused (typing) colour is dark"), Luminance(Box->WidgetStyle.FocusedForegroundColor) < 0.1f);
	TestTrue(TEXT("unfocused colour is dark"), Luminance(Box->WidgetStyle.ForegroundColor) < 0.1f);
	TestTrue(TEXT("text style colour is dark (it overrides the plain foreground colour)"), Luminance(Box->WidgetStyle.TextStyle.ColorAndOpacity) < 0.1f);
	TestTrue(TEXT("the Slate widget survives a layout pass with the new style (no dangling style pointer)"), Box->TakeWidget()->GetDesiredSize().X >= 0.f);

	UMOUIUtils::ApplyReadableTextInputStyle(nullptr); // null-safe
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCommunity_BugReportUrlIsSafeToOpen,
	"MOFramework.Community.BugReportUrlIsSafeToOpen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCommunity_BugReportUrlIsSafeToOpen::RunTest(const FString& Parameters)
{
	// The configured link goes straight to the OS URL handler, so only a plain https link may pass.
	TestTrue(TEXT("a Discord invite is openable"), UMOCommunitySettings::IsOpenableUrl(TEXT("https://discord.gg/AbC123")));
	TestTrue(TEXT("scheme case does not matter"), UMOCommunitySettings::IsOpenableUrl(TEXT("HTTPS://github.com/penpro/MO57/issues")));
	TestTrue(TEXT("query and fragment are fine after a host"), UMOCommunitySettings::IsOpenableUrl(TEXT("https://example.com/a?b=1#c")));

	TestFalse(TEXT("CONTROL: empty (the 'not configured' case)"), UMOCommunitySettings::IsOpenableUrl(FString()));
	TestFalse(TEXT("CONTROL: plain http is not accepted"), UMOCommunitySettings::IsOpenableUrl(TEXT("http://discord.gg/AbC123")));
	TestFalse(TEXT("CONTROL: a file path is not a link"), UMOCommunitySettings::IsOpenableUrl(TEXT("file:///C:/Windows/System32/cmd.exe")));
	TestFalse(TEXT("CONTROL: another scheme is refused"), UMOCommunitySettings::IsOpenableUrl(TEXT("javascript:alert(1)")));
	TestFalse(TEXT("CONTROL: a bare scheme has no host"), UMOCommunitySettings::IsOpenableUrl(TEXT("https://")));
	TestFalse(TEXT("CONTROL: 'https:' -- what an unquoted // in an ini value is read as (the packaged game's first bug)"), UMOCommunitySettings::IsOpenableUrl(TEXT("https:")));
	TestFalse(TEXT("CONTROL: a scheme followed by a path has no host"), UMOCommunitySettings::IsOpenableUrl(TEXT("https:///etc/passwd")));
	TestFalse(TEXT("CONTROL: whitespace could smuggle a second argument"), UMOCommunitySettings::IsOpenableUrl(TEXT("https://discord.gg/x --flag")));
	TestFalse(TEXT("CONTROL: control characters are refused"), UMOCommunitySettings::IsOpenableUrl(FString(TEXT("https://discord.gg/x")) + TEXT("\r\ncalc")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCommunity_ConfiguredBugReportUrlLoads,
	"MOFramework.Community.ConfiguredBugReportUrlLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCommunity_ConfiguredBugReportUrlLoads::RunTest(const FString& Parameters)
{
	// The URL as the SHIPPED config produces it. An unquoted `//` in an ini value starts a comment, so DefaultGame.ini's https://... was read back as
	// "https:" and the packaged button did nothing. This reads the real config, not the class default.
	const FString Configured = UMOCommunitySettings::GetBugReportUrl();
	TestTrue(FString::Printf(TEXT("the configured bug report link is openable (got '%s')"), *Configured), UMOCommunitySettings::IsOpenableUrl(Configured));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
