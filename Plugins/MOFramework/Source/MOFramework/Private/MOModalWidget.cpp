/**
 * MOModalWidget.cpp - Base class for modal dialog widgets
 */

#include "MOModalWidget.h"
#include "MOFramework.h"
#include "GameFramework/PlayerController.h"

UMOModalWidget::UMOModalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// We don't use CommonUI's back-action chain (bIsBackHandler) — it auto-registers
	// a project-level Back InputAction that we don't configure. Tab/Escape are
	// dispatched by the base preview handler through NativeOnCloseKeyRequested
	// (this class routes them to RequestClose; the base default deactivates).
	bIsBackHandler = false;

	// CRITICAL: bIsModal = true blocks ALL input from reaching other widgets.
	// This is the CommonUI flag that actually stops gameplay input from firing —
	// non-modal menus must use bIgnoreMoveInput/bIgnoreLookInput instead.
	bIsModal = true;

	// Modals must be acknowledged — don't dismiss them on outside click
	bClosesOnOutsideClick = false;
}

void UMOModalWidget::RequestClose()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOModalWidget] RequestClose called on %s"), *GetName());
	OnRequestClose.Broadcast();
}

TOptional<FUIInputConfig> UMOModalWidget::GetDesiredInputConfig() const
{
	// ECommonInputMode::Menu = Input goes ONLY to UI, not game
	// Combined with bIsModal, this completely blocks all game input
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false  // bHideCursorDuringViewportCapture - keep cursor visible
	);
}

bool UMOModalWidget::NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent)
{
	// Route through RequestClose so the controller's cleanup path runs (modal
	// background, reticle, etc.). The base default is a direct DeactivateWidget,
	// which skips the controller cleanup.
	RequestClose();
	return true;
}

void UMOModalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	UE_LOG(LogMOFramework, Log, TEXT("[MOModalWidget] Activated: %s"), *GetName());

	// CommonUI handles input mode via GetDesiredInputConfig() + bIsModal
	// No manual SetShowMouseCursor/SetIgnoreMoveInput/SetIgnoreLookInput needed

	// Ensure this widget can receive key events
	SetKeyboardFocus();
}

void UMOModalWidget::NativeOnDeactivated()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOModalWidget] Deactivated: %s"), *GetName());

	// CommonUI handles input mode restoration via GetDesiredInputConfig() + bIsModal
	// No manual SetShowMouseCursor/SetIgnoreMoveInput/SetIgnoreLookInput needed

	Super::NativeOnDeactivated();
}

