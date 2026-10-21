// A creature flinches toward the side a blow came from (AMOCreature::ClassifyHitSide picks the definition's HitReactFront / Left / Right / Back):
// N-169, Oct 20 2026. The animation itself plays on every machine (AMOCreature::PlayAnimationEverywhere; nettest actions _client_sees_creature_flinch).
// Run: python Tools/ue.py auto --filter "MOFramework.Creature.HitSide"

#include "Misc/AutomationTest.h"
#include "MOCreature.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCreatureHitSide_FromTheAttackersSide, "MOFramework.Creature.HitSide.FromTheAttackersSide",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCreatureHitSide_FromTheAttackersSide::RunTest(const FString& Parameters)
{
	const FVector Forward(1.0, 0.0, 0.0);   // facing +X: +Y is its right (UE's left-handed frame)
	TestEqual(TEXT("an attacker ahead"), AMOCreature::ClassifyHitSide(Forward, FVector(300.0, 0.0, 0.0)), EMOCreatureHitSide::Front);
	TestEqual(TEXT("an attacker behind"), AMOCreature::ClassifyHitSide(Forward, FVector(-300.0, 20.0, 0.0)), EMOCreatureHitSide::Back);
	TestEqual(TEXT("an attacker on its right (+Y)"), AMOCreature::ClassifyHitSide(Forward, FVector(0.0, 250.0, 0.0)), EMOCreatureHitSide::Right);
	TestEqual(TEXT("an attacker on its left (-Y)"), AMOCreature::ClassifyHitSide(Forward, FVector(0.0, -250.0, 0.0)), EMOCreatureHitSide::Left);
	TestEqual(TEXT("40 deg off the nose is still the front"), AMOCreature::ClassifyHitSide(Forward, FVector(100.0, 84.0, 0.0)), EMOCreatureHitSide::Front);
	TestEqual(TEXT("50 deg off the nose is the side"), AMOCreature::ClassifyHitSide(Forward, FVector(100.0, 119.0, 0.0)), EMOCreatureHitSide::Right);
	TestEqual(TEXT("height does not count: a man standing over a wolf's flank is on its flank"),
		AMOCreature::ClassifyHitSide(Forward, FVector(0.0, -60.0, 120.0)), EMOCreatureHitSide::Left);
	TestEqual(TEXT("straight above: no side to tell, the front reaction"), AMOCreature::ClassifyHitSide(Forward, FVector(0.0, 0.0, 200.0)), EMOCreatureHitSide::Front);
	TestEqual(TEXT("turned: facing +Y, an attacker at +X is on its left"), AMOCreature::ClassifyHitSide(FVector(0.0, 1.0, 0.0), FVector(300.0, 0.0, 0.0)),
		EMOCreatureHitSide::Left);
	return true;
}

#endif
