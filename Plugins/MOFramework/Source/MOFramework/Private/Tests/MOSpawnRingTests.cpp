/**
 * The spawn ring: UMOSpawnManagerSubsystem::IsInSpawnRing. A category's Min/MaxSpawnDistance are METERS; the distance to the nearest player is cm.
 * Run: python Tools/ue.py auto --filter "MOFramework.Spawn.Ring"
 *
 * Found Oct 15 2026: GetValidSpawnPoints compared the cm distance with the meter values, so a prey spawn point (80-400 m) counted only
 * 0.8-4 m from the player -- the ~18,500 spawn points the PCG graph places on the ground were never used and every spawn took the fallback.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MOSpawnManagerSubsystem.h"
#include "MOSpawnTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSpawn_RingIsInMeters,
	"MOFramework.Spawn.RingIsInMeters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSpawn_RingIsInMeters::RunTest(const FString& Parameters)
{
	FMOSpawnCategoryConfig Prey;
	Prey.MinSpawnDistance = 80.0f;    // meters
	Prey.MaxSpawnDistance = 400.0f;

	TestFalse(TEXT("3 m from the player is far too close for a prey spawn"), UMOSpawnManagerSubsystem::IsInSpawnRing(300.0f, Prey));
	TestFalse(TEXT("79 m is inside the minimum"), UMOSpawnManagerSubsystem::IsInSpawnRing(7900.0f, Prey));
	TestTrue(TEXT("80 m is in the ring"), UMOSpawnManagerSubsystem::IsInSpawnRing(8000.0f, Prey));
	TestTrue(TEXT("250 m is in the ring"), UMOSpawnManagerSubsystem::IsInSpawnRing(25000.0f, Prey));
	TestTrue(TEXT("400 m is in the ring"), UMOSpawnManagerSubsystem::IsInSpawnRing(40000.0f, Prey));
	TestFalse(TEXT("401 m is past the maximum"), UMOSpawnManagerSubsystem::IsInSpawnRing(40100.0f, Prey));
	TestFalse(TEXT("no player at all (the float max) is never in the ring"), UMOSpawnManagerSubsystem::IsInSpawnRing(TNumericLimits<float>::Max(), Prey));
	return true;
}

#endif
