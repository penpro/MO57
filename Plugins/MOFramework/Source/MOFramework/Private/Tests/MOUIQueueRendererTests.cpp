/**
 * MOUIQueueRendererTests.cpp - headless coverage for the Stage-3 queue layer.
 *
 * Tests the PURE parts of the queue consolidation (MOFramework.UI.Queue.*):
 * the neutral display-row formatting chokepoint and both domains' adapter
 * translation statics. No world, no PIE, no DataTables — fixtures are
 * constructed directly, mirroring MOUIInteractionStateTests.cpp (Stage 2).
 * Behavior (rows/progress/cancel/reconstruct in a live menu) is covered by the
 * Queue.* live suite entries and Tools/validate_ui_queue_pie.py.
 */

#include "Misc/AutomationTest.h"
#include "MOQueueDisplayTypes.h"
#include "MOCraftingQueueWidget.h"
#include "MOBuildingQueueWidget.h"
#include "MOCraftingTypes.h"
#include "MOBuildingTypes.h"
#include "MORecipeDefinitionRow.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_QueueDisplay_FormatAndFinalize,
	"MOFramework.UI.Queue.Display.FormatAndFinalize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_QueueDisplay_FormatAndFinalize::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Count formats as current/total"),
		MOQueueDisplay::FormatCount(2, 5).ToString(), FString(TEXT("2/5")));
	TestEqual(TEXT("Percent rounds and clamps"),
		MOQueueDisplay::FormatPercent(0.499f).ToString(), FString(TEXT("50%")));
	TestEqual(TEXT("Percent clamps above 1"),
		MOQueueDisplay::FormatPercent(1.7f).ToString(), FString(TEXT("100%")));

	// FinalizeRowTexts fills empty texts from raw fields...
	FMOQueueDisplayRow Row;
	Row.CountCurrent = 3;
	Row.CountTotal = 4;
	Row.RemainingSeconds = 90.0f;
	MOQueueDisplay::FinalizeRowTexts(Row);
	TestEqual(TEXT("CountText filled from raw"), Row.CountText.ToString(), FString(TEXT("3/4")));
	TestFalse(TEXT("TimeRemainingText filled from raw"), Row.TimeRemainingText.IsEmpty());

	// ...but never overwrites preformatted legacy texts.
	FMOQueueDisplayRow Preformatted;
	Preformatted.CountText = FText::FromString(TEXT("legacy"));
	Preformatted.CountCurrent = 1;
	Preformatted.CountTotal = 9;
	MOQueueDisplay::FinalizeRowTexts(Preformatted);
	TestEqual(TEXT("Preformatted CountText carried through"),
		Preformatted.CountText.ToString(), FString(TEXT("legacy")));

	// Raw-less rows stay textless (0/0 count, unknown remaining).
	FMOQueueDisplayRow Bare;
	MOQueueDisplay::FinalizeRowTexts(Bare);
	TestTrue(TEXT("No count text without raw totals"), Bare.CountText.IsEmpty());
	TestTrue(TEXT("No time text without known remaining"), Bare.TimeRemainingText.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_QueueAdapter_CraftingRowMapping,
	"MOFramework.UI.Queue.Adapter.CraftingRowMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_QueueAdapter_CraftingRowMapping::RunTest(const FString& Parameters)
{
	FMOCraftingQueueEntry Entry;
	Entry.RecipeId = FName(TEXT("TestKnife"));
	Entry.Count = 5;
	Entry.CompletedCount = 2;
	Entry.Progress = 0.25f;

	FMORecipeDefinitionRow Recipe;
	Recipe.DisplayName = FText::FromString(TEXT("Stone Knife"));
	Recipe.CraftTime = 8.0f;

	// Active row: authoritative remaining, Active state, id passthrough.
	const FMOQueueDisplayRow Active =
		UMOCraftingQueueWidget::BuildCraftingDisplayRow(Entry, &Recipe, true, 6.0f);
	TestEqual(TEXT("Row id passes the stable EntryId through"), Active.RowId, Entry.EntryId);
	TestEqual(TEXT("Title from recipe display name"), Active.Title.ToString(), FString(TEXT("Stone Knife")));
	TestEqual(TEXT("Active state"), Active.State, EMOQueueRowState::Active);
	TestEqual(TEXT("Repeat count is CompletedCount+1"), Active.CountCurrent, 3);
	TestEqual(TEXT("Repeat total is Count"), Active.CountTotal, 5);
	TestEqual(TEXT("Active remaining is the authoritative value"), Active.RemainingSeconds, 6.0f);
	TestTrue(TEXT("Crafting rows are cancellable"), Active.bCancellable);

	// Queued row: base CraftTime * Count estimate (legacy parity).
	const FMOQueueDisplayRow Queued =
		UMOCraftingQueueWidget::BuildCraftingDisplayRow(Entry, &Recipe, false, 6.0f);
	TestEqual(TEXT("Queued state"), Queued.State, EMOQueueRowState::Queued);
	TestEqual(TEXT("Queued remaining is CraftTime * Count"), Queued.RemainingSeconds, 40.0f);

	// Missing recipe: title falls back to the recipe id.
	const FMOQueueDisplayRow NoRecipe =
		UMOCraftingQueueWidget::BuildCraftingDisplayRow(Entry, nullptr, false, 0.0f);
	TestEqual(TEXT("Missing recipe falls back to id text"),
		NoRecipe.Title.ToString(), FString(TEXT("TestKnife")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_QueueAdapter_BuildingStateMapping,
	"MOFramework.UI.Queue.Adapter.BuildingStateMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_QueueAdapter_BuildingStateMapping::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Constructing maps to Active"),
		UMOBuildingQueueWidget::RowStateForBuildState(EMOBuildState::Constructing), EMOQueueRowState::Active);
	TestEqual(TEXT("Paused maps to Paused (distinct visual state)"),
		UMOBuildingQueueWidget::RowStateForBuildState(EMOBuildState::Paused), EMOQueueRowState::Paused);
	TestEqual(TEXT("Ghost degenerates to Queued (never rendered as a row)"),
		UMOBuildingQueueWidget::RowStateForBuildState(EMOBuildState::Ghost), EMOQueueRowState::Queued);
	TestEqual(TEXT("Complete degenerates to Queued (never rendered as a row)"),
		UMOBuildingQueueWidget::RowStateForBuildState(EMOBuildState::Complete), EMOQueueRowState::Queued);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
