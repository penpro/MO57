// Copyright Penumbra Group Inc. All Rights Reserved.

#include "MOGameUIManagerSubsystem.h"
#include "MOFramework.h"
#include "MOPrimaryGameLayout.h"
#include "MOUISettings.h"
#include "MOActivatableWidget.h"
#include "MOUIDebugSubsystem.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "Widgets/SViewport.h"

UMOGameUIManagerSubsystem::UMOGameUIManagerSubsystem()
{
}

void UMOGameUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	HookOutsideClickHandler();

	UE_LOG(LogTemp, Log, TEXT("MOGameUIManagerSubsystem: Initialized for world %s"), *GetWorld()->GetName());
}

void UMOGameUIManagerSubsystem::Deinitialize()
{
	UnhookOutsideClickHandler();

	// Clean up all layouts
	for (auto& Pair : PlayerLayouts)
	{
		if (UMOPrimaryGameLayout* Layout = Pair.Value)
		{
			Layout->RemoveFromParent();
		}
	}
	PlayerLayouts.Empty();
	CachedPrimaryLayout.Reset();

	Super::Deinitialize();
}

void UMOGameUIManagerSubsystem::HookOutsideClickHandler()
{
	if (FSlateApplication::IsInitialized())
	{
		FocusChangingHandle = FSlateApplication::Get().OnFocusChanging().AddUObject(
			this, &UMOGameUIManagerSubsystem::HandleFocusChanging);
	}
}

void UMOGameUIManagerSubsystem::UnhookOutsideClickHandler()
{
	if (FocusChangingHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnFocusChanging().Remove(FocusChangingHandle);
		FocusChangingHandle.Reset();
	}
}

void UMOGameUIManagerSubsystem::HandleFocusChanging(const FFocusEvent& FocusEvent,
	const FWeakWidgetPath& /*OldFocusedWidgetPath*/, const TSharedPtr<SWidget>& /*OldFocusedWidget*/,
	const FWidgetPath& /*NewFocusedWidgetPath*/, const TSharedPtr<SWidget>& NewFocusedWidget)
{
	// We only care about mouse clicks that land outside any UI and on the game viewport.
	// Anything else (programmatic focus, keyboard nav, hover transitions) is irrelevant here.
	if (FocusEvent.GetCause() != EFocusCause::Mouse) return;
	if (!NewFocusedWidget.IsValid()) return;
	if (NewFocusedWidget->GetTypeAsString() != TEXT("SViewport")) return;

	// The topmost-active widget is the authoritative answer to "which UI surface was the
	// user interacting with?" — it's maintained by NativeOnActivated/Deactivated, so it
	// reflects CommonUI's truth regardless of where Slate has scattered focus.
	UMOActivatableWidget* TopWidget = GetTopmostActiveWidget();
	if (!TopWidget)
	{
		return;
	}

	// [DIAG] Dump widget identity at the decision point so we can see exactly
	// what's getting closed and what its policy was.
	UE_LOG(LogMOFramework, Warning,
		TEXT("[OutsideClick] decision for TopWidget '%s' (class=%s parent=%s)  bClosesOnOutsideClick=%s"),
		*TopWidget->GetName(),
		*TopWidget->GetClass()->GetName(),
		TopWidget->GetClass()->GetSuperClass() ? *TopWidget->GetClass()->GetSuperClass()->GetName() : TEXT("(none)"),
		TopWidget->bClosesOnOutsideClick ? TEXT("TRUE") : TEXT("false"));

	if (!TopWidget->bClosesOnOutsideClick)
	{
		// Defer the reclaim to next tick. If we call SetAllUserFocus here, Slate's
		// in-progress Mouse focus change finalizes AFTER our SetDirectly and clobbers
		// it — the widget ends up un-focused even though it's still visually present,
		// and keyboard input dies until something else explicitly takes focus. Deferring
		// lets the Mouse focus change complete first, then we steal it back.
		MOUI_LOG(this, "OutsideClick", "%s opts out — reclaiming focus next tick", *TopWidget->GetName());
		TWeakObjectPtr<UMOActivatableWidget> WeakTop(TopWidget);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([WeakTop]()
			{
				UMOActivatableWidget* Top = WeakTop.Get();
				if (!Top || !Top->IsActivated()) return;
				if (TSharedPtr<SWidget> ActiveSlate = Top->GetCachedWidget())
				{
					FSlateApplication::Get().SetAllUserFocus(ActiveSlate.ToSharedRef(), EFocusCause::SetDirectly);
				}
			});
		}
		return;
	}

	// Defer the close to next tick for the same reason the opt-out branch above
	// defers its focus reclaim: we're inside Slate's in-progress OnFocusChanging
	// broadcast. Deactivating synchronously runs the widget-underneath's
	// NativeOnDeactivated → ClaimFocusForReactivation right now, and the in-flight
	// Mouse focus change then finalizes AFTER it, clobbering the survivor's
	// reclaimed focus (audit H44). Letting the Mouse focus change complete first,
	// then deactivating, keeps the survivor focused.
	MOUI_LOG(this, "OutsideClick", "closing %s next tick", *TopWidget->GetName());
	TWeakObjectPtr<UMOActivatableWidget> WeakTop(TopWidget);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([WeakTop]()
		{
			if (UMOActivatableWidget* Top = WeakTop.Get())
			{
				if (Top->IsActivated())
				{
					Top->DeactivateWidget();
				}
			}
		});
	}
	else
	{
		// No world to schedule on — fall back to synchronous close.
		TopWidget->DeactivateWidget();
	}
}

void UMOGameUIManagerSubsystem::RegisterActiveWidget(UMOActivatableWidget* Widget)
{
	if (!Widget) return;

	// Drop dead refs and any prior entry for this widget (re-activation case).
	ActiveWidgetStack.RemoveAll([Widget](const TWeakObjectPtr<UMOActivatableWidget>& W)
	{
		return !W.IsValid() || W.Get() == Widget;
	});

	ActiveWidgetStack.Add(Widget);
	MOUI_LOG(this, "ActiveStack", "register %s (depth=%d)", *Widget->GetName(), ActiveWidgetStack.Num());

	// Fan out a "widget activated" signal for listeners that want to ride
	// menu opens for opportunistic work (e.g. weather sub syncing UDS time
	// at a moment the player isn't looking at the sky).
	OnActivatableWidgetRegistered.Broadcast(Widget);
}

void UMOGameUIManagerSubsystem::UnregisterActiveWidget(UMOActivatableWidget* Widget)
{
	if (!Widget) return;

	ActiveWidgetStack.RemoveAll([Widget](const TWeakObjectPtr<UMOActivatableWidget>& W)
	{
		return !W.IsValid() || W.Get() == Widget;
	});

	MOUI_LOG(this, "ActiveStack", "unregister %s (depth=%d)", *Widget->GetName(), ActiveWidgetStack.Num());
}

UMOActivatableWidget* UMOGameUIManagerSubsystem::GetTopmostActiveWidget() const
{
	for (int32 i = ActiveWidgetStack.Num() - 1; i >= 0; --i)
	{
		if (UMOActivatableWidget* W = ActiveWidgetStack[i].Get())
		{
			return W;
		}
	}
	return nullptr;
}

bool UMOGameUIManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create for game worlds, not editor preview worlds
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

UMOGameUIManagerSubsystem* UMOGameUIManagerSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UMOGameUIManagerSubsystem>();
}

void UMOGameUIManagerSubsystem::NotifyPlayerAdded(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	// Check if we already have a layout for this player
	if (PlayerLayouts.Contains(PlayerController))
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: Layout already exists for player %s"), *PlayerController->GetName());
		return;
	}

	CreateLayoutForPlayer(PlayerController);
}

void UMOGameUIManagerSubsystem::NotifyPlayerRemoved(APlayerController* PlayerController)
{
	if (TObjectPtr<UMOPrimaryGameLayout>* FoundLayout = PlayerLayouts.Find(PlayerController))
	{
		if (UMOPrimaryGameLayout* Layout = *FoundLayout)
		{
			Layout->RemoveFromParent();
		}
		PlayerLayouts.Remove(PlayerController);

		// Update cached reference if it was for this player
		if (CachedPrimaryLayout.Get() == *FoundLayout)
		{
			CachedPrimaryLayout.Reset();
		}
	}
}

void UMOGameUIManagerSubsystem::CreateLayoutForPlayer(APlayerController* PlayerController)
{
	// Try to get layout class from direct property first, then from settings
	TSubclassOf<UMOPrimaryGameLayout> LayoutClass = PrimaryLayoutClass;
	if (!LayoutClass)
	{
		LayoutClass = UMOUISettings::GetPrimaryGameLayoutClass();
	}

	if (!LayoutClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: PrimaryLayoutClass is not set! Configure it in Project Settings -> Plugins -> MO UI Settings."));
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: PlayerController has no LocalPlayer"));
		return;
	}

	// Create the layout widget
	UMOPrimaryGameLayout* NewLayout = CreateWidget<UMOPrimaryGameLayout>(PlayerController, LayoutClass);
	if (!NewLayout)
	{
		UE_LOG(LogTemp, Error, TEXT("MOGameUIManagerSubsystem: Failed to create PrimaryGameLayout widget"));
		return;
	}

	// Add to viewport at base layer
	NewLayout->AddToPlayerScreen(0);

	// Store in map
	PlayerLayouts.Add(PlayerController, NewLayout);

	// Cache as primary if this is the first/only local player
	if (!CachedPrimaryLayout.IsValid())
	{
		CachedPrimaryLayout = NewLayout;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("MOGameUIManagerSubsystem: Created layout for player %s (class: %s)"),
		*PlayerController->GetName(), *LayoutClass->GetName());

	// Notify subscribers that the layout is ready. Push-on-startup consumers
	// (HUD root, persistent toasters, etc.) subscribe in BeginPlay and push
	// from this callback rather than polling. Single-fire — late subscribers
	// must query GetRootLayoutForPlayer() and push directly.
	OnPrimaryLayoutCreated.Broadcast(PlayerController, NewLayout);
}

UMOPrimaryGameLayout* UMOGameUIManagerSubsystem::GetRootLayoutForPlayer(APlayerController* PlayerController) const
{
	if (const TObjectPtr<UMOPrimaryGameLayout>* FoundLayout = PlayerLayouts.Find(PlayerController))
	{
		return *FoundLayout;
	}
	return nullptr;
}

UMOPrimaryGameLayout* UMOGameUIManagerSubsystem::GetRootLayout() const
{
	return CachedPrimaryLayout.Get();
}

UCommonActivatableWidget* UMOGameUIManagerSubsystem::PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: No root layout available to push widget"));
		return nullptr;
	}

	return Layout->PushWidgetToLayer(LayerTag, WidgetClass);
}

UCommonActivatableWidget* UMOGameUIManagerSubsystem::PushWidgetToLayerForPlayer(APlayerController* PlayerController, FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UMOPrimaryGameLayout* Layout = GetRootLayoutForPlayer(PlayerController);
	if (!Layout)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: No layout for player %s"), PlayerController ? *PlayerController->GetName() : TEXT("null"));
		return nullptr;
	}

	return Layout->PushWidgetToLayer(LayerTag, WidgetClass);
}

UCommonActivatableWidget* UMOGameUIManagerSubsystem::PushWidgetToLayerInstance(FGameplayTag LayerTag, UCommonActivatableWidget* Widget)
{
	UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: No root layout available"));
		return nullptr;
	}

	return Layout->PushWidgetToLayerInstance(LayerTag, Widget);
}

void UMOGameUIManagerSubsystem::PopWidgetFromLayer(FGameplayTag LayerTag)
{
	UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (Layout)
	{
		Layout->PopWidgetFromLayer(LayerTag);
	}
}

UCommonActivatableWidget* UMOGameUIManagerSubsystem::PushModalWidget(const UObject* WorldContextObject,
	TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!WorldContextObject || !WidgetClass)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Preferred path: CommonUI Modal layer via the project's primary game
	// layout. Stack handles input-config push/pop and widget removal on
	// deactivate — all the "input handoff" stuff happens for free.
	if (UMOGameUIManagerSubsystem* Subsystem = UMOGameUIManagerSubsystem::Get(World))
	{
		if (UMOPrimaryGameLayout* Layout = Subsystem->GetRootLayout())
		{
			UCommonActivatableWidget* Pushed = Layout->PushWidgetToLayer(MOUILayerTags::Layer_Modal, WidgetClass);
			if (Pushed)
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameUIManagerSubsystem] PushModalWidget: layer-stack path → %s"), *Pushed->GetName());
				return Pushed;
			}
			// Layout exists but push failed (e.g. Modal layer not registered).
			// Fall through to viewport fallback rather than failing outright.
		}
	}

	// Fallback path: main menu / unparented contexts that don't run on top
	// of UMOPrimaryGameLayout. AddToViewport at z=300 (above main menu's
	// z=100), manual activation, auto-RemoveFromParent on OnDeactivated so
	// modals don't leak. Input handoff here is trivial since main menu has
	// nothing to compete with — the modal owns input until it deactivates,
	// then the main menu widget below regains it via normal Slate focus.
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameUIManagerSubsystem] PushModalWidget fallback: no player controller available"));
		return nullptr;
	}

	UCommonActivatableWidget* Modal = CreateWidget<UCommonActivatableWidget>(PC, WidgetClass);
	if (!Modal)
	{
		return nullptr;
	}

	Modal->AddToViewport(300);

	// Bind auto-cleanup BEFORE Activate so we never miss an early deactivation.
	// Weak lambda — modal removes itself from the viewport on deactivate
	// (covers confirm AND cancel paths since OnDeactivated fires either way).
	const TWeakObjectPtr<UCommonActivatableWidget> WeakModal(Modal);
	Modal->OnDeactivated().AddWeakLambda(Modal, [WeakModal]()
	{
		if (UCommonActivatableWidget* W = WeakModal.Get())
		{
			W->RemoveFromParent();
		}
	});

	Modal->ActivateWidget();

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameUIManagerSubsystem] PushModalWidget: viewport-fallback path → %s"), *Modal->GetName());
	return Modal;
}

// =============================================================================
// CONTEXT MENU INPUT LIFECYCLE
// =============================================================================
//
// MOContextMenuBase and MOItemContextMenu are sibling UCommonUserWidget popup
// classes (not activatable widgets), so they don't get CommonUI's automatic
// input-config stacking via GetDesiredInputConfig. Each used to roll its own
// copy of "show cursor + GameAndUI" on construct, then "if no menu open
// restore game mode else refocus active widget" on destruct.
//
// Two copies meant they drifted: MOItemContextMenu had the refocus-active-
// widget fallback (so closing an item context menu over the inventory hands
// focus back to the inventory), MOContextMenuBase did not (so closing a
// ground/station context menu while a menu was open left focus stranded —
// the next Tab/Escape went nowhere).
//
// Both helpers now live here as the single source of truth. New context-menu
// subclasses call these instead of reimplementing the lifecycle.

void UMOGameUIManagerSubsystem::ApplyContextMenuInputState(APlayerController* PC)
{
	if (!IsValid(PC))
	{
		return;
	}

	PC->bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
}

void UMOGameUIManagerSubsystem::RestoreContextMenuInputState(const UObject* WorldContextObject, APlayerController* PC)
{
	if (!IsValid(PC))
	{
		return;
	}

	// Release any pointer capture left over from clicking a button inside the
	// menu. If we don't, the next click / key may route to the captured widget
	// (now invalid) instead of whatever's underneath.
	if (FSlateApplication::IsInitialized())
	{
		if (TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(0))
		{
			if (User->HasAnyCapture())
			{
				User->ReleaseAllCapture();
			}
		}
	}

	// Ask the world subsystem whether anything else is still showing. If yes,
	// the underlying widget keeps owning input/cursor — we just need to hand
	// Slate focus back so its key handlers (Tab/Escape) take effect.
	const UMOGameUIManagerSubsystem* Subsystem = UMOGameUIManagerSubsystem::Get(WorldContextObject);
	const bool bAnyMenuOpen = Subsystem ? Subsystem->IsAnyMenuOpen() : false;

	if (!bAnyMenuOpen)
	{
		// No other UI active — context menu was the last thing showing, so
		// restore gameplay state fully (cursor hidden, game-only input,
		// viewport focused for the next interaction trace).
		PC->bShowMouseCursor = false;

		FInputModeGameOnly GameOnlyMode;
		PC->SetInputMode(GameOnlyMode);

		if (FSlateApplication::IsInitialized() && PC->GetLocalPlayer())
		{
			if (UGameViewportClient* VC = PC->GetLocalPlayer()->ViewportClient)
			{
				if (TSharedPtr<SViewport> ViewportWidget = VC->GetGameViewportWidget())
				{
					FSlateApplication::Get().SetAllUserFocus(ViewportWidget.ToSharedRef(), EFocusCause::SetDirectly);
				}
			}
		}
		return;
	}

	// A menu is still active beneath the context menu. Find the topmost
	// active widget and hand Slate focus back to it so the menu's key
	// handlers come alive again. Without this, the cursor stays visible
	// (correct) but keystrokes go nowhere until the user clicks the menu —
	// a "press twice to close" symptom.
	//
	// Layer scan walks top-down (Modal → Menu → GameOverlay → Game). The
	// .Num() > 0 check skips stacks that only contain the
	// GameplayInputStub root-content widget.
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	if (!Subsystem)
	{
		return;
	}

	UMOPrimaryGameLayout* Layout = Subsystem->GetRootLayout();
	if (!Layout)
	{
		return;
	}

	static const TArray<FName> LayerPriority = {
		FName("MO.UI.Layer.Modal"),
		FName("MO.UI.Layer.Menu"),
		FName("MO.UI.Layer.GameOverlay"),
		FName("MO.UI.Layer.Game"),
	};

	for (const FName& LayerName : LayerPriority)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(LayerName, false);
		if (!Tag.IsValid())
		{
			continue;
		}

		UCommonActivatableWidgetContainerBase* Stack = Layout->GetLayerStack(Tag);
		if (!Stack || Stack->GetWidgetList().Num() == 0)
		{
			continue;
		}

		UCommonActivatableWidget* Active = Cast<UCommonActivatableWidget>(Stack->GetActiveWidget());
		if (!Active)
		{
			continue;
		}

		if (TSharedPtr<SWidget> ActiveSlate = Active->GetCachedWidget())
		{
			FSlateApplication::Get().SetAllUserFocus(ActiveSlate.ToSharedRef(), EFocusCause::SetDirectly);
			return;
		}
	}
}

void UMOGameUIManagerSubsystem::ClearLayer(FGameplayTag LayerTag)
{
	UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (Layout)
	{
		Layout->ClearLayer(LayerTag);
	}
}

bool UMOGameUIManagerSubsystem::IsLayerActive(FGameplayTag LayerTag) const
{
	const UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (Layout)
	{
		return Layout->IsLayerActive(LayerTag);
	}
	return false;
}

bool UMOGameUIManagerSubsystem::IsAnyMenuOpen() const
{
	return GetActiveMenuCount() > 0;
}

int32 UMOGameUIManagerSubsystem::GetActiveMenuCount() const
{
	const UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (Layout)
	{
		return Layout->GetActiveMenuCount();
	}
	return 0;
}

UCommonActivatableWidgetContainerBase* UMOGameUIManagerSubsystem::GetGameLayer() const
{
	const UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (Layout)
	{
		return Layout->GetLayerStack(MOUILayerTags::Layer_Game);
	}
	return nullptr;
}

UCommonActivatableWidgetContainerBase* UMOGameUIManagerSubsystem::GetMenuLayer() const
{
	const UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (Layout)
	{
		return Layout->GetLayerStack(MOUILayerTags::Layer_Menu);
	}
	return nullptr;
}

UCommonActivatableWidgetContainerBase* UMOGameUIManagerSubsystem::GetModalLayer() const
{
	const UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (Layout)
	{
		return Layout->GetLayerStack(MOUILayerTags::Layer_Modal);
	}
	return nullptr;
}

void UMOGameUIManagerSubsystem::RemoveActiveMenu()
{
	// Safety: no UI on dedicated server
	if (IsRunningDedicatedServer())
	{
		return;
	}

	UCommonActivatableWidgetContainerBase* Stack = GetGameLayer();
	if (!Stack)
	{
		return;
	}

	if (UCommonActivatableWidget* Active = Stack->GetActiveWidget())
	{
		Stack->RemoveWidget(*Active);
	}
}

bool UMOGameUIManagerSubsystem::IsMenuTypeActive(TSubclassOf<UCommonActivatableWidget> MenuClass) const
{
	const UCommonActivatableWidgetContainerBase* Stack = GetGameLayer();
	if (!Stack)
	{
		return false;
	}

	const UCommonActivatableWidget* Active = Stack->GetActiveWidget();
	if (!Active)
	{
		return false;
	}

	return Active->IsA(MenuClass);
}
