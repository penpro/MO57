// Tests for the below-the-world light attenuation (UMOSubterraneanLightingSubsystem). The world behaviour (the overlay on the sky's intensity) is verified in game
// with MO.Light.Test; these pin the attenuation curve, which is the rule the sky and the caves depend on.

#include "Misc/AutomationTest.h"
#include "MOSubterraneanLightingSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSubterraneanLighting_AllowedFraction,
	"MOFramework.Lighting.BelowHorizon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSubterraneanLighting_AllowedFraction::RunTest(const FString& Parameters)
{
	auto Forward = [](double PitchDeg) { return FRotator(PitchDeg, 0.0, 0.0).Vector(); };   // pitch > 0 = the light shines UP

	TestEqual(TEXT("a light shining straight down is not attenuated"), UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(-90.0)), 1.0f);
	TestEqual(TEXT("a light shining down at 40 degrees is not attenuated"), UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(-40.0)), 1.0f);
	TestEqual(TEXT("a light exactly at the horizon is not attenuated"), UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(0.0)), 1.0f);
	TestEqual(TEXT("5 degrees below the horizon (pointing 5 degrees up) is fully blocked"), UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(5.0)), 0.0f, 1e-3f);
	TestEqual(TEXT("40 degrees below the horizon (the MO.Light.Test case) is fully blocked"), UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(40.0)), 0.0f);
	TestEqual(TEXT("straight up is fully blocked"), UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(90.0)), 0.0f);

	// The ramp is monotonic from the horizon to 5 degrees below it, so twilight fades instead of stepping.
	float Previous = 1.0f;
	bool bMonotonic = true;
	for (int32 I = 0; I <= 50; ++I)
	{
		const float F = UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(0.1 * I));
		bMonotonic &= (F <= Previous + 1e-6f);
		Previous = F;
	}
	TestTrue(TEXT("the attenuation never increases as the light points further up"), bMonotonic);
	TestEqual(TEXT("halfway (~2.5 degrees) is about half"), UMOSubterraneanLightingSubsystem::AllowedFraction(Forward(2.5)), 0.5f, 0.02f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
