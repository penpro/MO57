// The right stick looks (AMOPlayerController::GamepadLookToLookInput): IMC_MODefault mapped it to IA_MOLook but nothing bound that action, so a gamepad could not
// turn the camera (N-078, Oct 20 2026). A held deflection becomes a per-frame look delta in the mouse's convention. Run: python Tools/ue.py auto --filter "MOFramework.Input.GamepadLook"

#include "Misc/AutomationTest.h"
#include "MOPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOGamepadLook_StickTurnsTheView, "MOFramework.Input.GamepadLook.StickTurnsTheView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOGamepadLook_StickTurnsTheView::RunTest(const FString& Parameters)
{
	const FVector2D Right = AMOPlayerController::GamepadLookToLookInput(FVector2D(1.0f, 0.0f), 0.5f, 180.0f);
	TestEqual(TEXT("full right for half a second turns right by half the rate"), Right.X, 90.0, 1e-3);
	TestEqual(TEXT("... and does not pitch"), Right.Y, 0.0, 1e-3);
	const FVector2D Up = AMOPlayerController::GamepadLookToLookInput(FVector2D(0.0f, 1.0f), 0.1f, 180.0f);
	TestTrue(TEXT("pushing up gives the mouse's look-up sign (its mapping negates Y)"), Up.Y < 0.0f);
	TestEqual(TEXT("a frame's share of the rate"), FMath::Abs(Up.Y), 18.0, 1e-3);
	TestEqual(TEXT("a centred stick does nothing"), AMOPlayerController::GamepadLookToLookInput(FVector2D::ZeroVector, 0.016f, 180.0f), FVector2D::ZeroVector);
	TestEqual(TEXT("frame-rate independent: two half frames = one frame"),
		AMOPlayerController::GamepadLookToLookInput(FVector2D(0.6f, 0.0f), 0.008f, 180.0f).X * 2.0f,
		AMOPlayerController::GamepadLookToLookInput(FVector2D(0.6f, 0.0f), 0.016f, 180.0f).X, 1e-4);
	return true;
}

#endif
