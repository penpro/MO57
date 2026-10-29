#include "Misc/AutomationTest.h"
#include "MOCharacter.h"
#include "MOSurvivalStatsComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

// Recovery follows the work (DECISIONS F52 B, Wes Oct 29): until then stamina recovered 10/s whatever the body did, more than any cost, and
// the bar never emptied. The curve on the character's class defaults and the stats tick that applies it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOStaminaRecoveryFollowsWork, "MOFramework.Survival.StaminaRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOStaminaRecoveryFollowsWork::RunTest(const FString&)
{
	const AMOCharacter* Body = GetDefault<AMOCharacter>();
	const UMOSurvivalStatsComponent* StatsDefaults = GetDefault<UMOSurvivalStatsComponent>();
	const float AtRest = StatsDefaults->Stamina.RegenRate;

	// The curve itself, then the gaits' own exertion levels on it.
	TestEqual(TEXT("standing still recovers in full"), Body->GetStaminaRecoveryFraction(0.0f), 1.0f, 1e-4f);
	TestEqual(TEXT("a walk recovers half"), Body->GetStaminaRecoveryFraction(Body->WalkingExertion), 0.5f, 1e-4f);
	TestEqual(TEXT("a jog recovers a quarter"), Body->GetStaminaRecoveryFraction(Body->JoggingExertion), 0.25f, 1e-4f);
	TestEqual(TEXT("a swim recovers nothing"), Body->GetStaminaRecoveryFraction(Body->SwimmingExertion), 0.0f, 1e-4f);
	TestEqual(TEXT("a climb up a sheer face recovers nothing"), Body->GetStaminaRecoveryFraction(70.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("a sprint recovers nothing"), Body->GetStaminaRecoveryFraction(Body->SprintingExertion), 0.0f, 1e-4f);
	TestEqual(TEXT("past the last key it stays at nothing"), Body->GetStaminaRecoveryFraction(150.0f), 0.0f, 1e-4f);

	// What the numbers mean together: a jog holds up (it recovers a little more than it costs); a sprint only spends (a climb: MOFramework.Traversal.ClimbEffort).
	const float JogNet = AtRest * Body->GetStaminaRecoveryFraction(Body->JoggingExertion) - Body->JoggingStaminaCostPerSecond;
	TestTrue(FString::Printf(TEXT("a jog nets %+.2f/s: it holds up"), JogNet), JogNet > 0.0f && JogNet < 1.0f);
	const float SprintSeconds = StatsDefaults->Stamina.Max / Body->SprintingStaminaCostPerSecond;
	TestTrue(FString::Printf(TEXT("a full bar lasts %.1f s of sprinting"), SprintSeconds), FMath::IsNearlyEqual(SprintSeconds, 12.5f, 0.5f));

	// The stats tick: the same 2 s recover 20 at rest, 10 walking, 0 sprinting; nothing set = full (a component no pawn drives).
	UMOSurvivalStatsComponent* Stats = NewObject<UMOSurvivalStatsComponent>();
	const auto RecoveredOver2s = [Stats](float Fraction)
	{
		Stats->Stamina.Current = 50.0f;
		if (Fraction >= 0.0f) Stats->SetStaminaRecoveryFraction(Fraction);
		Stats->AdvanceStats(2.0f);
		return Stats->Stamina.Current - 50.0f;
	};
	TestEqual(TEXT("nothing set: full recovery"), RecoveredOver2s(-1.0f), 2.0f * AtRest, 1e-3f);
	TestEqual(TEXT("walking: half"), RecoveredOver2s(Body->GetStaminaRecoveryFraction(Body->WalkingExertion)), AtRest, 1e-3f);
	TestEqual(TEXT("sprinting: none"), RecoveredOver2s(Body->GetStaminaRecoveryFraction(Body->SprintingExertion)), 0.0f, 1e-3f);
	Stats->SetStaminaRecoveryFraction(7.0f);
	TestEqual(TEXT("the fraction clamps to 1"), Stats->GetStaminaRecoveryFraction(), 1.0f);
	return true;
}

#endif
