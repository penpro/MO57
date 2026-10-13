// The world-removal ledger (MOWorldRemovalTypes.h): a removed instance is keyed by place (10 cm cells) + mesh, and found again when its PCG cell regenerates the
// same instance -- through the float round trip of the instance transform, so one cell of tolerance either way, and never for another mesh or a place 30 cm off.

#include "Misc/AutomationTest.h"
#include "MOWorldRemovalTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldRemoval_Ledger,
	"MOFramework.WorldRemoval.Ledger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldRemoval_Ledger::RunTest(const FString& Parameters)
{
	const FName Stick(TEXT("wd5mbhd_tier_2"));
	const FName Tree(TEXT("SM_BlackAlder_Forest_06_PP"));
	const FVector Here(1221934.37, -679988.61, 312.0);   // a far-from-origin place: the coast spawn

	FMOWorldRemovalLedger Ledger;
	TestFalse(TEXT("an empty ledger removes nothing"), Ledger.IsRemoved(Here, Stick));
	TestTrue(TEXT("the first record is new"), Ledger.Add(FMOWorldRemovalLedger::MakeEntry(Here, Stick)));
	TestFalse(TEXT("the same record again is not"), Ledger.Add(FMOWorldRemovalLedger::MakeEntry(Here, Stick)));
	TestEqual(TEXT("one entry"), Ledger.Num(), 1);

	TestTrue(TEXT("the same place is removed"), Ledger.IsRemoved(Here, Stick));
	TestTrue(TEXT("the transform round trip (a few mm) is the same place"), Ledger.IsRemoved(Here + FVector(0.4, -0.3, 0.0), Stick));
	TestTrue(TEXT("a different height is the same place (the key is X / Y)"), Ledger.IsRemoved(Here + FVector(0.0, 0.0, 25.0), Stick));
	TestTrue(TEXT("6 cm off -- across a cell boundary -- is still the same place"), Ledger.IsRemoved(Here + FVector(6.0, 0.0, 0.0), Stick));
	TestFalse(TEXT("30 cm off is another place"), Ledger.IsRemoved(Here + FVector(30.0, 0.0, 0.0), Stick));
	TestFalse(TEXT("another mesh at the same place is not removed"), Ledger.IsRemoved(Here, Tree));

	const FMORemovedInstance E = FMOWorldRemovalLedger::MakeEntry(Here, Stick);
	TestEqual(TEXT("cell X is X / 10 cm, rounded"), E.CellX, FMath::RoundToInt32(Here.X / 10.0));
	TestEqual(TEXT("cell Y is Y / 10 cm, rounded"), E.CellY, FMath::RoundToInt32(Here.Y / 10.0));

	Ledger.Reset();
	TestFalse(TEXT("reset clears the ledger"), Ledger.IsRemoved(Here, Stick));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
