/**
 * MOMenuWidget.cpp - Base class for gameplay menu widgets
 *
 * Input strategy:
 *   Menus rely on three close paths, all routed through standard CommonUI mechanisms:
 *
 *     1. CloseAction binding (UMOActivatableWidget base) — registers a CommonUI action
 *        binding for the InputAction set in the widget's Blueprint defaults. Pressing
 *        any key mapped to that action fires DeactivateWidget. Works regardless of
 *        Menu input mode. This is "press the key that opened the menu to close it."
 *
 *     2. Tab via NativeOnPreviewKeyDown (this class) — calls RequestClose so the
 *        controller's HandleXMenuRequestClose path runs (modal background, reticle,
 *        etc. cleanup). The base UMOActivatableWidget also has a Tab handler that
 *        just calls DeactivateWidget for non-MOMenuWidget activatables (e.g. journal).
 *
 *     3. Escape via NativeOnHandleBackAction (CommonUI's back action chain, gated by
 *        bIsBackHandler in the constructor).
 */

#include "MOMenuWidget.h"
#include "MOFramework.h"
#include "MOUIManagerComponent.h"
#include "MOUIDebugSubsystem.h"
#include "GameFramework/PlayerController.h"

UMOMenuWidget::UMOMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// We don't use CommonUI's back-action chain (bIsBackHandler) because it
	// auto-registers a project-level Back InputAction which we don't configure.
	// Tab/Escape close paths live in UMOActivatableWidget::NativeOnPreviewKeyDown
	// (base) and our own NativeOnPreviewKeyDown override below — both route Tab
	// through RequestClose for controller cleanup.
	bIsBackHandler = false;
}

void UMOMenuWidget::RequestClose()
{
	MOUI_LOG(this, "Menu", "RequestClose CALLED on %s", *GetName());
	OnRequestClose.Broadcast();
}

TOptional<FUIInputConfig> UMOMenuWidget::GetDesiredInputConfig() const
{
	// ECommonInputMode::Menu = BLOCKS all game input (movement, mouse look, Enhanced Input).
	// Cursor is visible, mouse is free for UI interaction.
	// Toggle keys are handled in NativeOnPreviewKeyDown below.
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false  // bHideCursorDuringViewportCapture - keep cursor visible
	);
}

FReply UMOMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Override Tab/Escape to go through RequestClose (broadcasts OnRequestClose so
	// the controller's full close path runs — modal background, reticle, etc.).
	// The base class also handles these keys but with a direct DeactivateWidget
	// that skips the controller cleanup.
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UMOMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOMenuWidget] activated %s (frame %llu)"), *GetName(), GFrameCounter);
}

void UMOMenuWidget::NativeOnDeactivated()
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOMenuWidget] deactivated %s (frame %llu)"), *GetName(), GFrameCounter);
	Super::NativeOnDeactivated();
}
