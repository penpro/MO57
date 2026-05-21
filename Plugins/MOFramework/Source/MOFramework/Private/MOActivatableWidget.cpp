/**
 * MOActivatableWidget.cpp - Base class implementation
 */

#include "MOActivatableWidget.h"
#include "MOFramework.h"
#include "MOUIDebugSubsystem.h"
#include "MOGameUIManagerSubsystem.h"
#include "CommonInputModeTypes.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SViewport.h"
#include "MOPlayerController.h"
#include "InputAction.h"
#include "Input/CommonUIInputTypes.h"

UMOActivatableWidget::UMOActivatableWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable focus restoration when this widget resurfaces after a modal closes
	bAutoRestoreFocus = true;

	// Default: focusable
	SetIsFocusable(true);
}

TOptional<FUIInputConfig> UMOActivatableWidget::GetDesiredInputConfig() const
{
	// Default: Menu mode (UI consumes input, cursor visible, no capture).
	// This covers menus, modals, dialogs, panels — anything that's a UI surface.
	// Passive overlays (progress bars, HUD widgets) override to Game mode.
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false  // keep cursor visible
	);
}

FReply UMOActivatableWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Tab and Escape are universal close keys for any activatable widget.
	// We handle them here instead of opting into CommonUI's back-action chain
	// (bIsBackHandler) because that auto-registers a project-level Back input
	// action — if the project's CommonUI input data isn't fully configured,
	// CommonUI complains "Cannot create action binding ... no action provided".
	// MOMenuWidget / MOModalWidget override this and route through RequestClose
	// for controller-side cleanup (modal background, reticle).
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape)
	{
		MOUI_LOG(this, "Menu", "%s pressed on %s -> DeactivateWidget",
			*PressedKey.ToString(), *GetName());
		DeactivateWidget();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UMOActivatableWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Only fires when a click hits THIS widget's bounds but no child consumed it.
	// That means the click landed on our own transparent/background area — i.e.,
	// it's INSIDE us, not outside. Job: absorb it, reclaim focus, return.
	//
	// We intentionally do NOT close on this event. Outside-click closing has one
	// owner: UMOGameUIManagerSubsystem::HandleFocusChanging, which only fires when
	// focus actually moves to SViewport (i.e., the click truly fell outside all UI).
	//
	// Gate: Game-mode passive overlays (progress bars) let clicks pass through.
	TOptional<FUIInputConfig> Config = GetDesiredInputConfig();
	const bool bIsUISurface = Config.IsSet()
		&& Config->GetInputMode() != ECommonInputMode::Game;

	if (!bIsUISurface)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	MOUI_LOG(this, "Click", "%s absorbing in-widget click on transparent area", *GetName());
	if (TSharedPtr<SWidget> SafeWidget = GetCachedWidget())
	{
		return FReply::Handled().SetUserFocus(SafeWidget.ToSharedRef(), EFocusCause::Mouse);
	}
	return FReply::Handled();
}

UWidget* UMOActivatableWidget::NativeGetDesiredFocusTarget() const
{
	UWidget* Target = nullptr;
	if (IsValid(DefaultFocusWidget))
	{
		Target = DefaultFocusWidget;
	}
	else
	{
		Target = Super::NativeGetDesiredFocusTarget();
	}

	MOUI_LOG(this, "Focus", "GetDesiredFocusTarget on %s -> %s",
		*GetName(),
		Target ? *Target->GetName() : TEXT("<null>"));

	return Target;
}

void UMOActivatableWidget::NativeOnActivated()
{
	MOUI_LOG(this, "Activate", "ENTRY  %s", *GetName());
	Super::NativeOnActivated();

	if (APlayerController* PC = GetOwningPlayer())
	{
		TOptional<FUIInputConfig> Config = GetDesiredInputConfig();
		if (Config.IsSet())
		{
			const ECommonInputMode Mode = Config->GetInputMode();
			bool bWantsCursor = (Mode != ECommonInputMode::Game);
			MOUI_LOG(this, "Activate", "  %s: InputMode=%d wantsCursor=%s -> set bShowMouseCursor=%s (was %s)",
				*GetName(), (int32)Mode,
				bWantsCursor ? TEXT("YES") : TEXT("no"),
				bWantsCursor ? TEXT("true") : TEXT("false"),
				PC->bShowMouseCursor ? TEXT("true") : TEXT("false"));
			PC->bShowMouseCursor = bWantsCursor;
		}
		else
		{
			MOUI_LOG(this, "Activate", "  %s: GetDesiredInputConfig returned NO VALUE (cursor unchanged)", *GetName());
		}
	}

	// CRITICAL: explicitly claim Slate focus. CommonUI activates the widget but
	// doesn't automatically set focus — so without this, focus stays on SViewport
	// and clicks/keys go to the game, not the widget.
	TOptional<FUIInputConfig> ActivationConfig = GetDesiredInputConfig();
	const bool bNeedsFocus = ActivationConfig.IsSet()
		&& ActivationConfig->GetInputMode() != ECommonInputMode::Game;

	if (bNeedsFocus && FSlateApplication::IsInitialized())
	{
		// CRITICAL: Release any lingering pointer/cursor capture from the previous
		// gameplay session. If SViewport still has capture, mouse clicks bypass
		// our focused widget entirely.
		FSlateApplication& App = FSlateApplication::Get();
		if (TSharedPtr<FSlateUser> User = App.GetUser(0))
		{
			if (User->HasAnyCapture())
			{
				User->ReleaseAllCapture();
				MOUI_LOG(this, "Focus", "  %s: released lingering pointer capture", *GetName());
			}
		}

		TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
		UWidget* DesiredTarget = NativeGetDesiredFocusTarget();
		TSharedPtr<SWidget> TargetSlate = DesiredTarget ? DesiredTarget->GetCachedWidget() : SafeWidget;

		if (TargetSlate.IsValid())
		{
			App.SetAllUserFocus(TargetSlate.ToSharedRef(), EFocusCause::SetDirectly);
			MOUI_LOG(this, "Focus", "  %s: SetAllUserFocus -> %s",
				*GetName(),
				DesiredTarget ? *DesiredTarget->GetName() : TEXT("<self>"));
		}
		else
		{
			MOUI_LOG(this, "Focus", "  %s: SetUserFocus SKIPPED — no valid slate widget", *GetName());
		}

		// Block gameplay movement actions without ripping out the Enhanced Input
		// mapping context. We can't remove PawnControl — CommonUI's CloseAction
		// bindings query that context's key→action mappings to fire; removing it
		// breaks every menu's close-on-key behavior. But ECommonInputMode::Menu by
		// itself doesn't actually stop WASD/look from firing through Enhanced Input
		// in our setup (only bIsModal does, which is why the in-game menu blocks
		// movement and other menus didn't).
		//
		// SetIgnoreMoveInput / SetIgnoreLookInput block AddMovementInput /
		// AddYawInput at the PC level. Enhanced Input actions still fire, so UI
		// toggles still work, but the actual movement never reaches the pawn.
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
			MOUI_LOG(this, "Input", "  %s: set bIgnoreMoveInput/LookInput=true", *GetName());
		}

		// Register with the UI manager so outside-click handling can find us even
		// after Slate clears focus from this widget for non-Mouse reasons.
		if (UWorld* World = GetWorld())
		{
			if (UMOGameUIManagerSubsystem* UISub = World->GetSubsystem<UMOGameUIManagerSubsystem>())
			{
				UISub->RegisterActiveWidget(this);
			}
		}
	}

	// Register the CloseAction binding via CommonUI. This is the supported path
	// for "key that works while the menu is up." CommonUI's action router fires
	// the bound delegate regardless of Menu input mode blocking gameplay.
	// Guard against double-registration if NativeOnActivated fires twice without
	// an intervening NativeOnDeactivated — CommonUI errors with "already bound."
	if (CloseAction && !CloseActionBinding.IsValid())
	{
		CloseActionBinding = RegisterUIActionBinding(
			FBindUIActionArgs(
				CloseAction,
				FSimpleDelegate::CreateUObject(this, &UMOActivatableWidget::HandleCloseActionTriggered)
			)
		);
		MOUI_LOG(this, "Activate", "  %s: registered CloseAction binding (action=%s)",
			*GetName(), *CloseAction->GetName());
	}

	MOUI_LOG(this, "Activate", "EXIT   %s", *GetName());
	MOUI_DUMP_STATE(this, FString::Printf(TEXT("After activate %s"), *GetName()));
}

void UMOActivatableWidget::NativeOnDeactivated()
{
	MOUI_LOG(this, "Deactivate", "ENTRY  %s", *GetName());

	// Only run cleanup symmetric with what we did in NativeOnActivated.
	// For Game-mode widgets (e.g. harvest progress) we did nothing on activation,
	// so we must touch nothing on deactivation — otherwise we'd strip state
	// set up by an underlying menu (e.g. inventory) that's still on the stack.
	TOptional<FUIInputConfig> Config = GetDesiredInputConfig();
	const bool bDidSetupOnActivate = Config.IsSet()
		&& Config->GetInputMode() != ECommonInputMode::Game;

	if (bDidSetupOnActivate)
	{
		// Unregister from the UI manager's active-widget stack.
		if (UWorld* World = GetWorld())
		{
			if (UMOGameUIManagerSubsystem* UISub = World->GetSubsystem<UMOGameUIManagerSubsystem>())
			{
				UISub->UnregisterActiveWidget(this);
			}
		}

		// CRITICAL: GetOwningPlayer() can return null during deactivation if the stack
		// has already detached this widget from its player. Fall back to the world's
		// first PC so cleanup still runs — otherwise cursor stays visible, input mode
		// isn't restored, and Enhanced Input contexts aren't re-armed, which causes
		// the "press the key twice" symptom users see after closing a menu.
		APlayerController* PC = GetOwningPlayer();
		if (!PC)
		{
			if (UWorld* World = GetWorld())
			{
				PC = World->GetFirstPlayerController();
				MOUI_LOG(this, "Deactivate", "  %s: GetOwningPlayer null, fell back to FirstPlayerController=%s",
					*GetName(), PC ? *PC->GetName() : TEXT("<still null>"));
			}
		}

		if (PC)
		{
			MOUI_LOG(this, "Deactivate", "  %s: clearing bShowMouseCursor (was %s)",
				*GetName(), PC->bShowMouseCursor ? TEXT("true") : TEXT("false"));
			PC->bShowMouseCursor = false;

			if (FSlateApplication::IsInitialized() && PC->GetLocalPlayer())
			{
				FSlateApplication& App = FSlateApplication::Get();

				if (TSharedPtr<FSlateUser> User = App.GetUser(0))
				{
					if (User->HasAnyCapture())
					{
						User->ReleaseAllCapture();
						MOUI_LOG(this, "Focus", "  %s: released pointer capture", *GetName());
					}
				}

				if (UGameViewportClient* VC = PC->GetLocalPlayer()->ViewportClient)
				{
					TSharedPtr<SViewport> ViewportWidget = VC->GetGameViewportWidget();
					if (ViewportWidget.IsValid())
					{
						App.SetAllUserFocus(ViewportWidget.ToSharedRef(), EFocusCause::SetDirectly);
						MOUI_LOG(this, "Focus", "  %s: returned focus to viewport", *GetName());
					}
				}
			}

			// Explicitly restore game-only input mode. A previous context menu may
			// have left FInputModeGameAndUI set, which prevents Enhanced Input axes
			// (mouse look) from firing on the PlayerController.
			FInputModeGameOnly GameOnlyMode;
			PC->SetInputMode(GameOnlyMode);
			MOUI_LOG(this, "Input", "  %s: applied FInputModeGameOnly", *GetName());

			// Re-enable gameplay movement actions (symmetric with NativeOnActivated).
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			MOUI_LOG(this, "Input", "  %s: set bIgnoreMoveInput/LookInput=false", *GetName());
		}
	}
	else
	{
		MOUI_LOG(this, "Deactivate", "  %s: Game-mode widget — no cleanup needed", *GetName());
	}

	// Unregister the CloseAction binding (symmetric with activation).
	if (CloseActionBinding.IsValid())
	{
		RemoveActionBinding(CloseActionBinding);
		CloseActionBinding = FUIActionBindingHandle();
	}

	Super::NativeOnDeactivated();
	MOUI_LOG(this, "Deactivate", "EXIT   %s", *GetName());
}

void UMOActivatableWidget::HandleCloseActionTriggered()
{
	MOUI_LOG(this, "Activate", "  %s: CloseAction triggered -> DeactivateWidget", *GetName());
	DeactivateWidget();
}
