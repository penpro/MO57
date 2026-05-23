/**
 * MOActivatableWidget.cpp - Base class implementation
 */

#include "MOActivatableWidget.h"
#include "MOFramework.h"
#include "MOUIDebugSubsystem.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOUIManagerComponent.h"
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
		UE_LOG(LogMOFramework, Warning, TEXT("[Activatable-DIAG] %s: %s key pressed -> DeactivateWidget"),
			*GetName(), *PressedKey.ToString());
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

void UMOActivatableWidget::ClaimFocusForReactivation()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	// Walk the desired-focus-target chain. NativeGetDesiredFocusTarget can
	// return another UMOActivatableWidget (e.g. a sub-panel inside a
	// switcher). If we stop at the first level we'd set focus on a widget
	// that itself doesn't accept keyboard focus, leaving focus stranded
	// (player would have to click to focus a button manually). Keep walking
	// until we hit a non-activatable leaf or a self-loop.
	UWidget* Current = NativeGetDesiredFocusTarget();
	int32 SafetyCounter = 8; // guard against pathological cycles
	while (SafetyCounter-- > 0)
	{
		UMOActivatableWidget* AsActivatable = Cast<UMOActivatableWidget>(Current);
		if (!AsActivatable)
		{
			break;
		}
		UWidget* Next = AsActivatable->NativeGetDesiredFocusTarget();
		if (!Next || Next == Current)
		{
			// Self-target or null — best we can do is focus the activatable
			// itself (and rely on its key handlers).
			break;
		}
		Current = Next;
	}

	// Fall back to focusing self if the chain didn't yield a target.
	UWidget* FocusTarget = Current ? Current : this;
	TSharedPtr<SWidget> SlateWidget = FocusTarget->GetCachedWidget();
	if (SlateWidget.IsValid())
	{
		FSlateApplication::Get().SetAllUserFocus(SlateWidget.ToSharedRef(), EFocusCause::SetDirectly);
		UE_LOG(LogMOFramework, Verbose, TEXT("[Activatable] %s::ClaimFocusForReactivation -> %s"),
			*GetName(), *FocusTarget->GetName());
	}
}

void UMOActivatableWidget::NativeOnActivated()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[Activatable-DIAG] %s ACTIVATED"), *GetName());
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
	UE_LOG(LogMOFramework, Warning, TEXT("[Activatable-DIAG] %s DEACTIVATED"), *GetName());
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
		// Unregister FIRST so GetTopmostActiveWidget below returns the widget
		// underneath us (if any), not us.
		UMOActivatableWidget* RemainingActive = nullptr;
		if (UWorld* World = GetWorld())
		{
			if (UMOGameUIManagerSubsystem* UISub = World->GetSubsystem<UMOGameUIManagerSubsystem>())
			{
				UISub->UnregisterActiveWidget(this);
				RemainingActive = UISub->GetTopmostActiveWidget();
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
			UE_LOG(LogMOFramework, Warning, TEXT("[Activatable-DIAG] %s deactivating: RemainingActive=%s"),
				*GetName(),
				RemainingActive ? *RemainingActive->GetName() : TEXT("<none — last UI>"));

			// Release pointer capture either way — it's a Slate-state leftover
			// from the modal being shown, not a UI-vs-game policy decision.
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
			}

			// CRITICAL: SetIgnoreMoveInput/LookInput are stack-counted —
			// each (true) call needs a matching (false) call. NativeOnActivated
			// always calls (true), so NativeOnDeactivated must always call
			// (false), regardless of whether another UI is still active. If we
			// only decrement when "last UI standing," nested UI scenarios
			// (modal opens over menu, modal closes, menu closes) leave the
			// counter unbalanced and gameplay input stays blocked.
			//
			// Symmetry rule: anything Activate sets, Deactivate must reverse.
			// Cursor visibility / input mode / viewport focus restoration are
			// the only things that should be conditional on RemainingActive —
			// because they're driven by the active widget's GetDesiredInputConfig,
			// not stack-counted.
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			MOUI_LOG(this, "Input", "  %s: balanced SetIgnoreMoveInput/LookInput(false)", *GetName());

			if (RemainingActive)
			{
				// Another UI widget is still showing. Don't stomp input mode
				// or hide the cursor — defer to its desired config so the
				// player can keep using the underlying menu.
				TOptional<FUIInputConfig> NextConfig = RemainingActive->GetDesiredInputConfig();
				const bool bNextWantsCursor = NextConfig.IsSet()
					&& NextConfig->GetInputMode() != ECommonInputMode::Game;
				PC->bShowMouseCursor = bNextWantsCursor;

				// Hand focus to the remaining widget, walking its
				// desired-focus-target chain through any nested activatables to
				// a real focusable leaf.
				RemainingActive->ClaimFocusForReactivation();

				MOUI_LOG(this, "Input", "  %s: yielded to remaining UI %s (input-mode unchanged)",
					*GetName(), *RemainingActive->GetName());
			}
			else
			{
				// Last UI standing — restore gameplay state fully.
				MOUI_LOG(this, "Deactivate", "  %s: last UI standing — restoring gameplay mode", *GetName());

				PC->bShowMouseCursor = false;

				if (FSlateApplication::IsInitialized() && PC->GetLocalPlayer())
				{
					FSlateApplication& App = FSlateApplication::Get();
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

				FInputModeGameOnly GameOnlyMode;
				PC->SetInputMode(GameOnlyMode);
				MOUI_LOG(this, "Input", "  %s: applied FInputModeGameOnly", *GetName());
			}
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

	// SYSTEMATIC RULE: every activatable widget hides the modal background
	// (when no other menu is open) and refreshes the reticle on close. This
	// replaces the per-caller "DeactivateWidget + HideModalBackground +
	// UpdateReticleVisibility" pattern that was duplicated across the
	// terraform, harvest, and inspection teardown paths. By centralizing
	// here, every widget close — regardless of caller, regardless of input
	// mode — restores the modal/reticle state correctly. Callers can now
	// simply call DeactivateWidget() and trust the base class.
	//
	// Both calls are idempotent and read from CommonUI's stack as source of
	// truth, so calling for a widget that wasn't modal (e.g. progress bar
	// over no menus) is safe — HideModalBackground is no-op when there's
	// nothing to hide, UpdateReticleVisibility just reflects current stack
	// state. Game-mode widgets (progress bars) need this too: deactivating
	// them must restore reticle visibility when there's no underlying menu.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>())
		{
			// RequestHideModalBackground / RequestUpdateReticleVisibility are the
			// public BlueprintCallable wrappers. The raw HideModalBackground /
			// UpdateReticleVisibility are private to UMOUIManagerComponent.
			if (!UIManager->IsAnyMenuOpen())
			{
				UIManager->RequestHideModalBackground();
			}
			UIManager->RequestUpdateReticleVisibility();
		}
	}

	Super::NativeOnDeactivated();
	MOUI_LOG(this, "Deactivate", "EXIT   %s", *GetName());
}

void UMOActivatableWidget::HandleCloseActionTriggered()
{
	MOUI_LOG(this, "Activate", "  %s: CloseAction triggered -> DeactivateWidget", *GetName());
	DeactivateWidget();
}
