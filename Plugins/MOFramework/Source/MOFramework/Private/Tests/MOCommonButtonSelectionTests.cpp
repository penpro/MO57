#include "Misc/AutomationTest.h"
#include "MOCommonButton.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// Tab rows (UMOTabBarWidget, UMOStatusPanel) own their buttons' selection from code. CommonUI's SetIsSelected(false) is a no-op on a button that is not toggleable, so a row built on it kept
// every previously clicked tab selected (an underline under each). UMOCommonButton::SetSelectedState is the one way to do it; this pins that it really deselects.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUI_CommonButton_SetSelectedStateDeselectsNonToggleable,
	"MOFramework.UI.Button.SetSelectedStateDeselectsNonToggleable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUI_CommonButton_SetSelectedStateDeselectsNonToggleable::RunTest(const FString& Parameters)
{
	UMOCommonButton* A = NewObject<UMOCommonButton>(GetTransientPackage());
	UMOCommonButton* B = NewObject<UMOCommonButton>(GetTransientPackage());
	if (!TestNotNull(TEXT("button A"), A) || !TestNotNull(TEXT("button B"), B))
	{
		return false;
	}
	for (UMOCommonButton* Button : {A, B})
	{
		Button->SetIsSelectable(true);
		Button->SetIsToggleable(false);   // the tab configuration
	}

	A->SetSelectedState(true);
	TestTrue(TEXT("A is selected"), A->GetSelected());
	TestFalse(TEXT("B is not"), B->GetSelected());

	// a tab row moves the selection: B on, A off
	B->SetSelectedState(true);
	A->SetSelectedState(false);
	TestTrue(TEXT("B is selected"), B->GetSelected());
	TestFalse(TEXT("A was deselected although it is not toggleable (SetIsSelected(false) would have left it on)"), A->GetSelected());

	// idempotent both ways
	A->SetSelectedState(false);
	B->SetSelectedState(true);
	TestFalse(TEXT("deselecting an unselected button changes nothing"), A->GetSelected());
	TestTrue(TEXT("selecting a selected button changes nothing"), B->GetSelected());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
