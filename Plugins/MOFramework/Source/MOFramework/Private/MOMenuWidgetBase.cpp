/**
 * MOMenuWidgetBase.cpp - Base class implementation for menu widgets
 */

#include "MOMenuWidgetBase.h"
#include "MOUIManagerComponent.h"
#include "MOGameUIManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "MOFramework.h"

UMOMenuWidgetBase::UMOMenuWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable back action handling (Escape key)
	bIsBackHandler = true;
	bIsBackActionDisplayedInActionBar = false;

	// Enable focus restoration when reactivated
	bAutoRestoreFocus = true;

	// Default: focusable
	SetIsFocusable(true);
}

void UMOMenuWidgetBase::RequestClose()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMenuWidgetBase] RequestClose called on %s"), *GetName());
	OnRequestClose.Broadcast();
}

TOptional<FUIInputConfig> UMOMenuWidgetBase::GetDesiredInputConfig() const
{
	// Configure input using constructor (members are protected in UE5.7)
	// Parameters: InputMode, MouseCaptureMode, MouseLockMode, bHideCursorDuringViewportCapture
	//
	// IMPORTANT: Use ECommonInputMode::All to allow toggle keys (I, C, B) to reach PlayerController
	// via Enhanced Input while CommonUI handles UI navigation and back actions.
	// NoCapture allows mouse to move freely for UI interaction.
	// DoNotLock prevents mouse from being confined to viewport.
	// bHideCursorDuringViewportCapture=false ensures cursor stays visible.
	FUIInputConfig Config(
		ECommonInputMode::All,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false  // bHideCursorDuringViewportCapture
	);

	return Config;
}

bool UMOMenuWidgetBase::NativeOnHandleBackAction()
{
	// Back action (Escape or configured back key) closes the menu
	UE_LOG(LogMOFramework, Log, TEXT("[MOMenuWidgetBase] Back action received on %s"), *GetName());
	RequestClose();
	return true;
}

UWidget* UMOMenuWidgetBase::NativeGetDesiredFocusTarget() const
{
	// Return the configured default focus widget if set
	if (IsValid(DefaultFocusWidget))
	{
		return DefaultFocusWidget;
	}

	// Fall back to base behavior
	return Super::NativeGetDesiredFocusTarget();
}

void UMOMenuWidgetBase::NativeOnActivated()
{
	Super::NativeOnActivated();

	UE_LOG(LogMOFramework, Log, TEXT("[MOMenuWidgetBase] Activated: %s"), *GetName());

	// Notify UIManager that a menu is activating
	// This handles reference-counted cursor/input state
	if (bBlocksGameInput)
	{
		ApplyMenuInputState(true);
	}
}

void UMOMenuWidgetBase::NativeOnDeactivated()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMenuWidgetBase] Deactivated: %s"), *GetName());

	// Notify UIManager that a menu is deactivating
	if (bBlocksGameInput)
	{
		ApplyMenuInputState(false);
	}

	Super::NativeOnDeactivated();
}

void UMOMenuWidgetBase::ApplyMenuInputState(bool bActivating)
{
	// Find the owning player controller
	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		return;
	}

	// Query the CommonUI subsystem for active menu count
	// This provides automatic reference counting via the widget stacks
	int32 ActiveMenuCount = 0;
	if (UMOGameUIManagerSubsystem* UISubsystem = UMOGameUIManagerSubsystem::Get(PC))
	{
		ActiveMenuCount = UISubsystem->GetActiveMenuCount();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOMenuWidgetBase] ApplyMenuInputState: %s, activating=%d, activeCount=%d"),
		*GetName(), bActivating ? 1 : 0, ActiveMenuCount);

	if (bActivating)
	{
		// When activating, always apply menu state
		// The count already includes this widget since NativeOnActivated fires after activation
		if (bShowMouseCursor)
		{
			PC->bShowMouseCursor = true;
		}
		if (bIgnoreMoveInput)
		{
			PC->SetIgnoreMoveInput(true);
		}
		if (bIgnoreLookInput)
		{
			PC->SetIgnoreLookInput(true);
		}
	}
	else
	{
		// When deactivating, only restore game state if no other menus are active
		// Note: The count may or may not include this widget depending on timing
		// Use <= 1 to handle both cases (0 = already removed, 1 = still counted)
		if (ActiveMenuCount <= 1)
		{
			PC->bShowMouseCursor = false;
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
		}
	}

	// Update reticle visibility via UIManager (this is separate from menu counting)
	UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>();
	if (IsValid(UIManager))
	{
		UIManager->RequestUpdateReticleVisibility();
	}
}
