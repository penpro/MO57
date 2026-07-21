#include "Misc/AutomationTest.h"
#include "MOUIInteractionState.h"
#include "MOListSelectionModel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_InspectableEntry_UnavailableRemainsSelectable,
	"MOFramework.UI.Catalog.UnavailableRemainsSelectable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_InspectableEntry_UnavailableRemainsSelectable::RunTest(const FString& Parameters)
{
	const FMOInspectableEntryState State = MOUIInteractionState::MakeInspectableEntry(false);
	TestTrue(TEXT("Known unavailable row remains selectable for requirement inspection"), State.bSelectable);
	TestFalse(TEXT("Unavailable action remains unavailable"), State.bActionAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_InspectableEntry_AvailableStatesIndependent,
	"MOFramework.UI.Catalog.AvailableStatesIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_InspectableEntry_AvailableStatesIndependent::RunTest(const FString& Parameters)
{
	const FMOInspectableEntryState State = MOUIInteractionState::MakeInspectableEntry(true);
	TestTrue(TEXT("Available row is selectable"), State.bSelectable);
	TestTrue(TEXT("Available action is available"), State.bActionAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_CatalogSelection_StableUniqueIds,
	"MOFramework.UI.Catalog.Selection.StableUniqueIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_CatalogSelection_StableUniqueIds::RunTest(const FString& Parameters)
{
	FMOListSelectionModel Model;
	Model.ReplaceEntries({FName(TEXT("RecipeA")), NAME_None, FName(TEXT("RecipeA")), FName(TEXT("RecipeB"))});

	TestEqual(TEXT("Invalid and duplicate IDs are removed"), Model.GetEntryIds().Num(), 2);
	TestEqual(TEXT("Source order remains stable"), Model.GetEntryIds()[0], FName(TEXT("RecipeA")));
	TestEqual(TEXT("Second unique ID remains stable"), Model.GetEntryIds()[1], FName(TEXT("RecipeB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_CatalogSelection_IdempotentSelection,
	"MOFramework.UI.Catalog.Selection.IdempotentSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_CatalogSelection_IdempotentSelection::RunTest(const FString& Parameters)
{
	FMOListSelectionModel Model;
	Model.ReplaceEntries({FName(TEXT("RecipeA")), FName(TEXT("RecipeB"))});

	const FMOListSelectionTransition First = Model.Select(FName(TEXT("RecipeA")));
	const FMOListSelectionTransition Repeat = Model.Select(FName(TEXT("RecipeA")));

	TestTrue(TEXT("First selection produces one selected transition"), First.Change == EMOListSelectionChange::Selected);
	TestTrue(TEXT("Selecting the same ID is silent"), Repeat.Change == EMOListSelectionChange::None);
	TestEqual(TEXT("Selected ID is authoritative"), Model.GetSelectedId(), FName(TEXT("RecipeA")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_CatalogSelection_RefreshPreservesSelection,
	"MOFramework.UI.Catalog.Selection.RefreshPreservesSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_CatalogSelection_RefreshPreservesSelection::RunTest(const FString& Parameters)
{
	FMOListSelectionModel Model;
	Model.ReplaceEntries({FName(TEXT("RecipeA")), FName(TEXT("RecipeB"))});
	Model.Select(FName(TEXT("RecipeB")));

	const FMOListSelectionTransition Refresh =
		Model.ReplaceEntries({FName(TEXT("RecipeB")), FName(TEXT("RecipeC"))});

	TestTrue(TEXT("Refresh does not rebroadcast a still-valid selection"), Refresh.Change == EMOListSelectionChange::None);
	TestEqual(TEXT("Selection survives reorder and replacement"), Model.GetSelectedId(), FName(TEXT("RecipeB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_CatalogSelection_RemovalClearsOnce,
	"MOFramework.UI.Catalog.Selection.RemovalClearsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_CatalogSelection_RemovalClearsOnce::RunTest(const FString& Parameters)
{
	FMOListSelectionModel Model;
	Model.ReplaceEntries({FName(TEXT("RecipeA")), FName(TEXT("RecipeB"))});
	Model.Select(FName(TEXT("RecipeA")));

	const FMOListSelectionTransition Removal = Model.ReplaceEntries({FName(TEXT("RecipeB"))});
	const FMOListSelectionTransition RepeatedRefresh = Model.ReplaceEntries({FName(TEXT("RecipeB"))});

	TestTrue(TEXT("Removing the selected ID produces one clear transition"), Removal.Change == EMOListSelectionChange::Cleared);
	TestTrue(TEXT("Subsequent refresh is silent"), RepeatedRefresh.Change == EMOListSelectionChange::None);
	TestTrue(TEXT("Selected ID is cleared"), Model.GetSelectedId().IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_CatalogSelection_UnknownIdClears,
	"MOFramework.UI.Catalog.Selection.UnknownIdClears",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_CatalogSelection_UnknownIdClears::RunTest(const FString& Parameters)
{
	FMOListSelectionModel Model;
	Model.ReplaceEntries({FName(TEXT("RecipeA")), FName(TEXT("RecipeB"))});
	Model.Select(FName(TEXT("RecipeA")));

	const FMOListSelectionTransition Unknown = Model.Select(FName(TEXT("NotInCatalog")));
	const FMOListSelectionTransition Repeat = Model.Select(FName(TEXT("StillNotInCatalog")));

	TestTrue(TEXT("Unknown ID clears the current selection"), Unknown.Change == EMOListSelectionChange::Cleared);
	TestTrue(TEXT("Repeated unknown ID remains silent"), Repeat.Change == EMOListSelectionChange::None);
	TestTrue(TEXT("Unknown ID is never stored as selection"), Model.GetSelectedId().IsNone());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
