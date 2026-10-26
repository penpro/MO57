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
#include "MOCreature.h"
#include "GameFramework/Pawn.h"

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

// A group is one species, its size the species' own (FMOCreatureDefinitionRow::Min / MaxGroupSize) or else the category's (Oct 26).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSpawn_GroupSizeIsTheSpecies,
	"MOFramework.Spawn.GroupSizeIsTheSpecies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSpawn_GroupSizeIsTheSpecies::RunTest(const FString& Parameters)
{
	FMOSpawnCategoryConfig Prey;
	Prey.MinGroupSize = 2;
	Prey.MaxGroupSize = 4;
	TestEqual(TEXT("no class: the category's size"), UMOSpawnManagerSubsystem::GroupSizeFor(nullptr, Prey), FIntPoint(2, 4));
	TestEqual(TEXT("a pawn that is no creature: the category's"), UMOSpawnManagerSubsystem::GroupSizeFor(APawn::StaticClass(), Prey), FIntPoint(2, 4));
	TestEqual(TEXT("a creature with no definition row: the category's"), UMOSpawnManagerSubsystem::GroupSizeFor(AMOCreature::StaticClass(), Prey), FIntPoint(2, 4));
	FMOSpawnCategoryConfig Broken;
	Broken.MinGroupSize = 0;
	Broken.MaxGroupSize = -3;
	TestEqual(TEXT("a broken category still spawns one"), UMOSpawnManagerSubsystem::GroupSizeFor(nullptr, Broken), FIntPoint(1, 1));
	if (UClass* Fox = LoadClass<APawn>(nullptr, TEXT("/MOFramework/MOBS/Fox/BP_Fox.BP_Fox_C")))
	{
		TestEqual(TEXT("a fox goes alone (its row: 1-1)"), UMOSpawnManagerSubsystem::GroupSizeFor(Fox, Prey), FIntPoint(1, 1));
	}
	if (UClass* Pig = LoadClass<APawn>(nullptr, TEXT("/MOFramework/MOBS/Pig/BP_Pig.BP_Pig_C")))
	{
		TestEqual(TEXT("pigs go in a sounder (its row: 3-6)"), UMOSpawnManagerSubsystem::GroupSizeFor(Pig, Prey), FIntPoint(3, 6));
	}
	return true;
}

#endif
