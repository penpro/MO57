// The sky's sync throttle does not freeze when the game clock moves back (UMOWeatherIntegrationSubsystem::IsUdsSyncDue). Negative elapsed time read as "too soon", so after a
// load to an earlier time no sync went out until the clock caught up (the June audit's M20). Expected: due; failing before: not due for as long as the jump.
// Run: python Tools/ue.py auto --filter "MOFramework.Weather.UdsSyncAfterClockMovedBack"

#include "Misc/AutomationTest.h"
#include "MOWeatherIntegrationSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWeather_UdsSyncAfterClockMovedBack,
	"MOFramework.Weather.UdsSyncAfterClockMovedBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWeather_UdsSyncAfterClockMovedBack::RunTest(const FString& Parameters)
{
	const FDateTime Last(2026, 6, 1, 12, 0, 0);
	const float OneGameMinute = 1.0f / 60.0f;

	TestFalse(TEXT("the same moment: not due"), UMOWeatherIntegrationSubsystem::IsUdsSyncDue(Last, Last, OneGameMinute));
	TestFalse(TEXT("half a game minute on: the throttle holds"), UMOWeatherIntegrationSubsystem::IsUdsSyncDue(Last + FTimespan::FromSeconds(30.0), Last, OneGameMinute));
	TestTrue(TEXT("two game minutes on: due"), UMOWeatherIntegrationSubsystem::IsUdsSyncDue(Last + FTimespan::FromMinutes(2.0), Last, OneGameMinute));
	TestTrue(TEXT("a load six hours earlier: due at once, the sky is wrong"), UMOWeatherIntegrationSubsystem::IsUdsSyncDue(Last - FTimespan::FromHours(6.0), Last, OneGameMinute));
	TestTrue(TEXT("a second earlier counts too"), UMOWeatherIntegrationSubsystem::IsUdsSyncDue(Last - FTimespan::FromSeconds(1.0), Last, OneGameMinute));
	return true;
}

#endif
