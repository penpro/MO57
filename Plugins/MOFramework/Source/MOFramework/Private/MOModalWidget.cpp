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
	// handled in NativeOnPreviewKeyDown (this class for RequestClose, base class
	// for direct DeactivateWidget).
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

FReply UMOModalWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Override Tab/Escape to route through RequestClose so the controller's
	// cleanup path runs (modal background, reticle, etc.). The base class
	// handles both keys but with DeactivateWidget directly, which skips the
	// controller cleanup.
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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

