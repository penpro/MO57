// A creature flinches toward the side a blow came from (AMOCreature::ClassifyHitSide picks the definition's HitReactFront / Left / Right / Back):
// N-169, Oct 20 2026. The animation itself plays on every machine (AMOCreature::PlayAnimationEverywhere; nettest actions _client_sees_creature_flinch).
// Run: python Tools/ue.py auto --filter "MOFramework.Creature.HitSide"

#include "Misc/AutomationTest.h"
#include "MOCreature.h"
#include "MOCreatureAnimInstance.h"

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

// A species without an AnimBlueprint (UMOCreatureAnimInstance, Oct 26): its idle / walk / run cycles by the body's speed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCreatureLocomotionBlend_BySpeed, "MOFramework.Creature.LocomotionBlend.BySpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCreatureLocomotionBlend_BySpeed::RunTest(const FString& Parameters)
{
	// The pig's cycles: the walk covers 102 cm/s, the run 656 (measured from ANIM_Pig_Walk_RM / Run_RM).
	const auto At = [](float Speed) { return FMOCreatureLocomotionBlend::ForSpeed(Speed, 102.0f, 656.0f, 2.0f); };
	const FMOCreatureLocomotionBlend Still = At(0.0f);
	TestEqual(TEXT("standing: all idle"), Still.Idle, 1.0f);
	TestEqual(TEXT("standing: no walk"), Still.Walk, 0.0f);
	const FMOCreatureLocomotionBlend Slow = At(51.0f);
	TestEqual(TEXT("half the walk's speed: half walk"), Slow.Walk, 0.5f);
	TestEqual(TEXT("... half idle"), Slow.Idle, 0.5f);
	const FMOCreatureLocomotionBlend Walking = At(102.0f);
	TestEqual(TEXT("the walk's own speed: all walk"), Walking.Walk, 1.0f);
	TestEqual(TEXT("... no idle"), Walking.Idle, 0.0f);
	const FMOCreatureLocomotionBlend Trot = At(379.0f);
	TestTrue(TEXT("between the walk and the run: half each"), FMath::IsNearlyEqual(Trot.Run, 0.5f, 0.001f) && FMath::IsNearlyEqual(Trot.Walk, 0.5f, 0.001f));
	const FMOCreatureLocomotionBlend Running = At(656.0f);
	TestEqual(TEXT("the run's own speed: all run at rate 1"), Running.RunPlayRate, 1.0f);
	TestEqual(TEXT("... all run"), Running.Run, 1.0f);
	TestEqual(TEXT("faster than the run cycle: it plays faster"), At(984.0f).RunPlayRate, 1.5f);
	TestEqual(TEXT("... up to the cap"), At(3000.0f).RunPlayRate, 2.0f);
	TestEqual(TEXT("a NaN speed reads as standing"), At(NAN).Idle, 1.0f);
	TestEqual(TEXT("weights sum to 1 (a trot)"), Trot.Idle + Trot.Walk + Trot.Run, 1.0f);
	const FMOCreatureLocomotionBlend Odd = FMOCreatureLocomotionBlend::ForSpeed(300.0f, 0.0f, 0.0f, 0.5f);
	TestTrue(TEXT("unset cycle speeds still give a finite blend"), FMath::IsFinite(Odd.Run) && Odd.RunPlayRate >= 1.0f);
	return true;
}

#endif
