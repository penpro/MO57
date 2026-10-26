// A tool named for a tool does that tool's work (Oct 26): 18 tools -- the bronze and iron axes and knives, the saw, the chisels, the smith's
// hammer and tongs, the sickles, the hoe -- had no tool capability in DT_Items, so none of them counted as a tool anywhere (a ripe crop said
// "Needs a tool: Sickle" to a survivor carrying a stone sickle). An item's tool power comes only from its row (FMOItemDefinitionRow::
// ToolCapabilities), so this reads the live table: every bIsTool item whose name says Axe, Knife, Saw, Chisel, Hoe, Sickle, Tongs or Awl
// must be able to do it.
// Run: python Tools/ue.py auto --filter "MOFramework.Items.ToolsDoTheirWork"

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "MOItemDefinitionRow.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOItems_ToolsDoTheirWork, "MOFramework.Items.ToolsDoTheirWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOItems_ToolsDoTheirWork::RunTest(const FString& Parameters)
{
	const UDataTable* Items = LoadObject<UDataTable>(nullptr, TEXT("/MOFramework/Data/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items loads"), Items))
	{
		return false;
	}
	// The word in the name (case-sensitive: "Pickaxe" is not an Axe, "Horseshoe" not a Hoe) and the tool it promises.
	const TPair<const TCHAR*, EMOToolType> Words[] = {
		{ TEXT("Axe"), EMOToolType::Axe }, { TEXT("Knife"), EMOToolType::Knife }, { TEXT("Saw"), EMOToolType::Saw },
		{ TEXT("Chisel"), EMOToolType::Chisel }, { TEXT("Hoe"), EMOToolType::Hoe }, { TEXT("Sickle"), EMOToolType::Sickle },
		{ TEXT("Tongs"), EMOToolType::Tongs }, { TEXT("Awl"), EMOToolType::Awl } };
	int32 Checked = 0;
	for (const TPair<FName, uint8*>& Row : Items->GetRowMap())
	{
		const FMOItemDefinitionRow& Item = *reinterpret_cast<const FMOItemDefinitionRow*>(Row.Value);
		if (!Item.bIsTool)
		{
			continue;
		}
		const FString Name = Row.Key.ToString();
		for (const TPair<const TCHAR*, EMOToolType>& Word : Words)
		{
			if (Name.Contains(Word.Key, ESearchCase::CaseSensitive))
			{
				++Checked;
				TestTrue(FString::Printf(TEXT("%s works as a %s"), *Name, Word.Key), Item.GetToolEffectiveness(Word.Value) > 0.0f);
			}
		}
	}
	TestTrue(TEXT("the check saw the tools (at least the 14 axes, knives, saw, chisels, sickles, hoe, tongs and awls)"), Checked >= 14);
	return true;
}

#endif
