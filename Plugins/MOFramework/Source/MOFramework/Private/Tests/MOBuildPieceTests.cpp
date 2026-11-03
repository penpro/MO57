// Building pieces (Nov 2 2026; Wes: "let's flesh out a lot of the building system and remember to have the assets 'pop in' as building goes on"):
// the posts under a floor and the work they add, and what a porous wall lets through.

#include "Misc/AutomationTest.h"
#include "MOBuildPieceActor.h"
#include "MOBuildProgressComponent.h"
#include "MORecipeDefinitionRow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOBuildPiece_PostItems,
	"MOFramework.Building.PieceSupportPostItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOBuildPiece_PostItems::RunTest(const FString& Parameters)
{
	// A post is cut from straight branches, one per 150 cm of its length, rounded up.
	TestEqual(TEXT("no post: nothing"), AMOBuildPieceActor::ItemsForPost(0.0f, 150.0f), 0);
	TestEqual(TEXT("a short post: one branch"), AMOBuildPieceActor::ItemsForPost(20.0f, 150.0f), 1);
	TestEqual(TEXT("exactly one branch long: one"), AMOBuildPieceActor::ItemsForPost(150.0f, 150.0f), 1);
	TestEqual(TEXT("a little longer: two"), AMOBuildPieceActor::ItemsForPost(151.0f, 150.0f), 2);
	// CONTROL (can fail): a nonsense length per item does not divide by zero.
	TestEqual(TEXT("CONTROL: a zero length per item still counts"), AMOBuildPieceActor::ItemsForPost(10.0f, 0.0f), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOBuildPiece_ExtraWork,
	"MOFramework.Building.PieceExtraWorkJoinsTheRecipe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOBuildPiece_ExtraWork::RunTest(const FString& Parameters)
{
	// A floor's posts add their material after the recipe's and their time on top of its time, whichever arrives first (a joiner may get the
	// posts before the recipe), and never once the work is under way.
	FMORecipeDefinitionRow Recipe;
	Recipe.RecipeId = TEXT("TestFloor");
	Recipe.TotalBuildTime = 1000.0f;
	FMOBuildPart Planks;
	Planks.ItemDefinitionId = TEXT("Plank01");
	Planks.Quantity = 6;
	Recipe.BuildParts.Add(Planks);

	TArray<FMOBuildPart> Posts;
	FMOBuildPart Branches;
	Branches.ItemDefinitionId = TEXT("StraightBranch01");
	Branches.Quantity = 3;
	Posts.Add(Branches);

	UMOBuildProgressComponent* After = NewObject<UMOBuildProgressComponent>();
	After->InitializeFromRecipe(Recipe);
	After->SetExtraWork(Posts, 900.0f);
	TestEqual(TEXT("posts after the recipe: plank and branch lines"), After->GetRequiredCount(TEXT("StraightBranch01")), 3);
	TestEqual(TEXT("... the recipe's own line kept"), After->GetRequiredCount(TEXT("Plank01")), 6);
	TestEqual(TEXT("... the time summed"), After->GetProgressData().TotalBuildTime, 1900.0f);

	UMOBuildProgressComponent* Before = NewObject<UMOBuildProgressComponent>();
	Before->SetExtraWork(Posts, 900.0f);
	Before->InitializeFromRecipe(Recipe);
	TestEqual(TEXT("posts before the recipe: the same list"), Before->GetRequiredCount(TEXT("StraightBranch01")), 3);
	TestEqual(TEXT("... the same time"), Before->GetProgressData().TotalBuildTime, 1900.0f);

	// Measured again (the preview moved before it was placed): replaced, not added twice.
	After->SetExtraWork(Posts, 900.0f);
	TestEqual(TEXT("set twice: still three branches"), After->GetRequiredCount(TEXT("StraightBranch01")), 3);
	After->SetExtraWork(TArray<FMOBuildPart>(), 0.0f);
	TestEqual(TEXT("no posts any more: no branch line"), After->GetRequiredCount(TEXT("StraightBranch01")), 0);
	TestEqual(TEXT("... and the recipe's time alone"), After->GetProgressData().TotalBuildTime, 1000.0f);

	// CONTROL (can fail): a floor with no posts needs none.
	UMOBuildProgressComponent* None = NewObject<UMOBuildProgressComponent>();
	None->InitializeFromRecipe(Recipe);
	TestEqual(TEXT("CONTROL: no extra work, no branches"), None->GetRequiredCount(TEXT("StraightBranch01")), 0);
	return true;
}
