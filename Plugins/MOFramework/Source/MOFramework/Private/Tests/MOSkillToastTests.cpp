// The skill / knowledge toast says what changed (UMOSkillToastWidget::Headline / Detail) and names an id as words (UMOSkillDatabaseSettings::HumanizeId).
// Until Oct 20 2026 the popup showed only "Woodworking Lv. 0" in faint text, and knowledge raw ("WoodProperties"). Run: python Tools/ue.py auto --filter "MOFramework.UI.SkillToast"

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "MOSkillsComponent.h"
#include "MOSkillToastWidget.h"
#include "MOSkillDatabaseSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

// A skill left unused goes on going unused through a save (N-298): the decay clock was in no save, so every load gave every skill a fresh grace and a player who reloaded never lost any. Expected:
// the time last used back as saved; failing before: none, and a new grace.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSkills_UnusedClockSaved, "MOFramework.Skills.UnusedClockSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSkills_UnusedClockSaved::RunTest(const FString& Parameters)
{
	FMOSkillsSaveData Saved;
	Saved.LastUsedGameSeconds.Add(TEXT("Melee"), 1234.0);
	UMOSkillsComponent* Skills = NewObject<UMOSkillsComponent>(NewObject<AActor>());
	TestTrue(TEXT("the save applies"), Skills->ApplySaveData(Saved));
	FMOSkillsSaveData Again;
	Skills->BuildSaveData(Again);
	const double* LastUsed = Again.LastUsedGameSeconds.Find(TEXT("Melee"));
	if (TestNotNull(TEXT("the clock is held and goes out again in the next save"), LastUsed))
	{
		TestEqual(TEXT("as it was"), *LastUsed, 1234.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSkillToast_SaysWhatChanged, "MOFramework.UI.SkillToast.SaysWhatChanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSkillToast_SaysWhatChanged::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("an id as words"), UMOSkillDatabaseSettings::HumanizeId(TEXT("WoodProperties")).ToString(), FString(TEXT("Wood Properties")));
	TestEqual(TEXT("... without its variant number"), UMOSkillDatabaseSettings::HumanizeId(TEXT("BarkFiber01")).ToString(), FString(TEXT("Bark Fiber")));
	TestEqual(TEXT("... an acronym kept whole"), UMOSkillDatabaseSettings::HumanizeId(TEXT("CPRTraining")).ToString(), FString(TEXT("CPRTraining")));

	FMOSkillToastData Skill;
	Skill.Id = TEXT("Woodworking");
	Skill.DisplayName = FText::FromString(TEXT("Woodworking"));
	Skill.XPGained = 2.0f;
	Skill.LevelBefore = 0;
	Skill.LevelAfter = 0;
	Skill.Level = 0;
	Skill.CurrentXP = 2.0f;
	Skill.XPToNextLevel = 100.0f;
	TestEqual(TEXT("an XP gain names the skill and the XP"), UMOSkillToastWidget::Headline(Skill).ToString(), FString(TEXT("Woodworking +2 XP")));
	TestTrue(TEXT("the detail gives the standing"), UMOSkillToastWidget::Detail(Skill).ToString().Contains(TEXT("2 / 100 XP to level 1")));

	Skill.LevelAfter = 1;
	Skill.Level = 1;
	TestEqual(TEXT("a level-up says so"), UMOSkillToastWidget::Headline(Skill).ToString(), FString(TEXT("Woodworking reached level 1")));

	FMOSkillToastData Knowledge;
	Knowledge.Id = TEXT("WoodProperties");
	Knowledge.DisplayName = UMOSkillDatabaseSettings::HumanizeId(Knowledge.Id);
	Knowledge.bIsKnowledge = true;
	Knowledge.XPGained = 100.0f;
	Knowledge.LevelBefore = 0;
	Knowledge.LevelAfter = 1;
	TestEqual(TEXT("first knowledge reads as new knowledge, by name"), UMOSkillToastWidget::Headline(Knowledge).ToString(), FString(TEXT("New knowledge: Wood Properties")));
	Knowledge.LevelBefore = 1;
	Knowledge.LevelAfter = 2;
	TestEqual(TEXT("a deeper level of it"), UMOSkillToastWidget::Headline(Knowledge).ToString(), FString(TEXT("Wood Properties knowledge: level 2")));

	FMOSkillToastData Unknown;
	Unknown.DisplayName = FText::FromString(TEXT("Foraging"));
	TestEqual(TEXT("no grant known: the name alone"), UMOSkillToastWidget::Headline(Unknown).ToString(), FString(TEXT("Foraging")));
	Unknown.Level = 100;
	Unknown.MaxLevel = 100;
	TestEqual(TEXT("at the top"), UMOSkillToastWidget::Detail(Unknown).ToString(), FString(TEXT("Level 100 (mastered)")));
	return true;
}

#endif
