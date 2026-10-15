/**
 * The interact reach rule: UMOInteractionSubsystem::IsWithinInteractReach -- the one check the server applies to the target the
 * client names, for primary (E, with the view cone) and secondary (right-click, distance only) interact.
 * Run: python Tools/ue.py auto --filter "MOFramework.Interaction.Reach"
 *
 * Found Oct 15 2026 (first-ten-minutes pass): E on a freshly built campfire did nothing. The server re-derived the target with
 * its own 12 cm sweep while the client's focus sweep is 30 cm; the crosshair ray passed ~30 cm over the 20 cm tall campfire, so
 * the client focused it and the server found nothing. The server now checks the client's claim with this rule instead.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MOInteractionSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOInteraction_ReachRule,
	"MOFramework.Interaction.Reach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOInteraction_ReachRule::RunTest(const FString& Parameters)
{
	const float MaxDistance = 500.0f;
	const float Cone = 75.0f;

	// The measured repro, relative to the camera: the view ray passes over the campfire, whose centre is 420 cm away.
	const FVector View = FVector::ZeroVector;
	const FVector Forward = FVector(243.4f, 415.2f, -135.6f).GetSafeNormal();
	const FVector FireOrigin(219.1f, 321.4f, -159.5f);
	const FVector FireExtent(81.6f, 80.4f, 19.7f);
	TestTrue(TEXT("the campfire under the crosshair is in reach"),
		UMOInteractionSubsystem::IsWithinInteractReach(View, Forward, FireOrigin, FireExtent, MaxDistance, Cone));

	// Behind the player: refused for E, still in range for the cone-less right-click rule.
	TestFalse(TEXT("the same campfire behind the player is outside the view cone"),
		UMOInteractionSubsystem::IsWithinInteractReach(View, -Forward, FireOrigin, FireExtent, MaxDistance, Cone));
	TestTrue(TEXT("with no cone (right-click) only distance counts"),
		UMOInteractionSubsystem::IsWithinInteractReach(View, -Forward, FireOrigin, FireExtent, MaxDistance, 0.0f));

	// Distance is measured to the bounds centre with the half-diagonal as tolerance (the client's sweep stops at the surface).
	const FVector Ahead(1.0f, 0.0f, 0.0f);
	const FVector Small(10.0f, 10.0f, 10.0f);
	TestTrue(TEXT("a small item 510 cm away is within 500 + its 17 cm half-diagonal"),
		UMOInteractionSubsystem::IsWithinInteractReach(View, Ahead, FVector(510.0f, 0.0f, 0.0f), Small, MaxDistance, Cone));
	TestFalse(TEXT("a small item 700 cm away is out of reach"),
		UMOInteractionSubsystem::IsWithinInteractReach(View, Ahead, FVector(700.0f, 0.0f, 0.0f), Small, MaxDistance, Cone));
	TestTrue(TEXT("a wall whose centre is 640 cm away but whose face is close is in reach"),
		UMOInteractionSubsystem::IsWithinInteractReach(View, Ahead, FVector(640.0f, 0.0f, 0.0f), FVector(150.0f, 50.0f, 150.0f), MaxDistance, Cone));
	TestFalse(TEXT("a spoofed claim across the map is refused"),
		UMOInteractionSubsystem::IsWithinInteractReach(View, Ahead, FVector(50000.0f, 0.0f, 0.0f), FireExtent, MaxDistance, Cone));
	return true;
}

#endif
