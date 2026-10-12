// The upland-lake query cache: the freshness rule that decides when the (expensive) lake query is re-run. Found by the CSV profiler: the old subsystem re-ran the ~30 ms lake query on the game thread
// every second (Exclusive/GameThread/Tickables = 27.7 ms on every hitch frame), although the players move a few metres a second.

#include "Misc/AutomationTest.h"
#include "MOUplandLakeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOLakeCache_Freshness,
	"MOFramework.Lakes.QueryCacheFreshness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOLakeCache_Freshness::RunTest(const FString& Parameters)
{
	const float Tolerance = 20000.0f;   // 200 m
	const TArray<FVector2D> Cached = { FVector2D(1000000.0, -500000.0) };

	TestTrue(TEXT("a standing player keeps the cached query"), UMOUplandLakeSubsystem::IsQueryFresh(Cached, { FVector(1000000.0, -500000.0, 3000.0) }, Tolerance));
	TestTrue(TEXT("100 m of walking is well inside the tolerance"), UMOUplandLakeSubsystem::IsQueryFresh(Cached, { FVector(1010000.0, -500000.0, 3000.0) }, Tolerance));
	TestTrue(TEXT("just inside the tolerance"), UMOUplandLakeSubsystem::IsQueryFresh(Cached, { FVector(1019900.0, -500000.0, 0.0) }, Tolerance));
	TestFalse(TEXT("just outside it: a new query"), UMOUplandLakeSubsystem::IsQueryFresh(Cached, { FVector(1020100.0, -500000.0, 0.0) }, Tolerance));
	TestTrue(TEXT("height does not matter"), UMOUplandLakeSubsystem::IsQueryFresh(Cached, { FVector(1000000.0, -500000.0, 90000.0) }, Tolerance));

	TestFalse(TEXT("no viewers: nothing to be fresh for"), UMOUplandLakeSubsystem::IsQueryFresh(Cached, {}, Tolerance));
	TestFalse(TEXT("no cached query yet"), UMOUplandLakeSubsystem::IsQueryFresh({}, { FVector(0.0, 0.0, 0.0) }, Tolerance));
	TestFalse(TEXT("a second player joined: a new query"), UMOUplandLakeSubsystem::IsQueryFresh(Cached, { FVector(1000000.0, -500000.0, 0.0), FVector(0.0, 0.0, 0.0) }, Tolerance));

	const TArray<FVector2D> Two = { FVector2D(0.0, 0.0), FVector2D(500000.0, 0.0) };
	TestTrue(TEXT("two players, both still"), UMOUplandLakeSubsystem::IsQueryFresh(Two, { FVector(0.0, 0.0, 0.0), FVector(500000.0, 0.0, 0.0) }, Tolerance));
	TestFalse(TEXT("two players, one of them travelled 1 km"), UMOUplandLakeSubsystem::IsQueryFresh(Two, { FVector(0.0, 0.0, 0.0), FVector(500000.0, 100000.0, 0.0) }, Tolerance));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
