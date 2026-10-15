// The HUD modes (F1) and the time dialog's clock dial: pure policy and geometry, headless.

#include "Misc/AutomationTest.h"
#include "MOHUDMode.h"
#include "MOClockDial.h"
#include "MOTimeOfDayDialog.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOHUDMode_Policy,
	"MOFramework.UI.HUDModePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOHUDMode_Policy::RunTest(const FString& Parameters)
{
	// Wes, Oct 15: full / minimal / off; "minimal would hide everything but vital moodles and such, so no quests etc".
	for (const EMOHUDElementKind Kind : { EMOHUDElementKind::Vital, EMOHUDElementKind::Feedback, EMOHUDElementKind::Info })
	{
		TestTrue(TEXT("Full draws every kind"), FMOHUDModePolicy::Shows(EMOHUDMode::Full, Kind));
		TestFalse(TEXT("Off draws nothing"), FMOHUDModePolicy::Shows(EMOHUDMode::Off, Kind));
	}
	TestTrue(TEXT("Minimal keeps the moodles and the breath meter"), FMOHUDModePolicy::Shows(EMOHUDMode::Minimal, EMOHUDElementKind::Vital));
	TestTrue(TEXT("Minimal keeps the reticle, the focus hint and action progress"), FMOHUDModePolicy::Shows(EMOHUDMode::Minimal, EMOHUDElementKind::Feedback));
	TestFalse(TEXT("Minimal drops quests, the tutorial, the mode plate"), FMOHUDModePolicy::Shows(EMOHUDMode::Minimal, EMOHUDElementKind::Info));

	TestEqual(TEXT("F1: Full -> Minimal"), FMOHUDModePolicy::Next(EMOHUDMode::Full), EMOHUDMode::Minimal);
	TestEqual(TEXT("F1: Minimal -> Off"), FMOHUDModePolicy::Next(EMOHUDMode::Minimal), EMOHUDMode::Off);
	TestEqual(TEXT("F1: Off -> Full"), FMOHUDModePolicy::Next(EMOHUDMode::Off), EMOHUDMode::Full);

	EMOHUDMode Parsed = EMOHUDMode::Full;
	TestTrue(TEXT("parse 'OFF'"), FMOHUDModePolicy::Parse(TEXT("OFF"), Parsed) && Parsed == EMOHUDMode::Off);
	TestTrue(TEXT("parse ' minimal '"), FMOHUDModePolicy::Parse(TEXT(" minimal "), Parsed) && Parsed == EMOHUDMode::Minimal);
	TestTrue(TEXT("parse 'full'"), FMOHUDModePolicy::Parse(TEXT("full"), Parsed) && Parsed == EMOHUDMode::Full);
	TestFalse(TEXT("CONTROL: 'loud' is no mode"), FMOHUDModePolicy::Parse(TEXT("loud"), Parsed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOClockDial_Geometry,
	"MOFramework.UI.ClockDialGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOClockDial_Geometry::RunTest(const FString& Parameters)
{
	// A 24-hour face that moves like the sun: midnight at the bottom, 6 on the left, noon at the top, 18 on the right (Y down, as Slate).
	TestTrue(TEXT("midnight points down"), UMOClockDial::MinutesToUnitOffset(0.0f).Equals(FVector2D(0.0, 1.0), 1e-6));
	TestTrue(TEXT("06:00 points left"), UMOClockDial::MinutesToUnitOffset(360.0f).Equals(FVector2D(-1.0, 0.0), 1e-6));
	TestTrue(TEXT("noon points up"), UMOClockDial::MinutesToUnitOffset(720.0f).Equals(FVector2D(0.0, -1.0), 1e-6));
	TestTrue(TEXT("18:00 points right"), UMOClockDial::MinutesToUnitOffset(1080.0f).Equals(FVector2D(1.0, 0.0), 1e-6));

	// Grabbing the hand anywhere along a direction gives that direction's time, snapped; every snapped time round-trips.
	int32 Bad = 0;
	for (int32 M = 0; M < 1440; M += 5)
	{
		const FVector2D Dir = UMOClockDial::MinutesToUnitOffset(static_cast<float>(M));
		Bad += UMOClockDial::OffsetToMinutes(Dir * 137.0, 5) != M ? 1 : 0;
	}
	TestEqual(TEXT("every 5-minute time round-trips through a point on the face"), Bad, 0);
	TestEqual(TEXT("06:37 snaps to 06:35"), UMOClockDial::OffsetToMinutes(UMOClockDial::MinutesToUnitOffset(397.0f) * 50.0, 5), 395);
	TestEqual(TEXT("06:38 snaps to 06:40"), UMOClockDial::OffsetToMinutes(UMOClockDial::MinutesToUnitOffset(398.0f) * 50.0, 5), 400);
	TestEqual(TEXT("23:58 wraps to midnight"), UMOClockDial::OffsetToMinutes(UMOClockDial::MinutesToUnitOffset(1438.0f) * 50.0, 5), 0);
	TestEqual(TEXT("a quarter-hour snap"), UMOClockDial::OffsetToMinutes(UMOClockDial::MinutesToUnitOffset(1000.0f) * 50.0, 15), 1005);

	TestEqual(TEXT("time text"), UMOTimeOfDayDialog::FormatMinutes(6 * 60 + 5).ToString(), FString(TEXT("06:05")));
	TestEqual(TEXT("time text wraps"), UMOTimeOfDayDialog::FormatMinutes(1440 + 75).ToString(), FString(TEXT("01:15")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
