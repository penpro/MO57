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
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
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

	MOUI_LOG(this, "OutsideClick", "closing %s", *TopWidget->GetName());
	TopWidget->DeactivateWidget();
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
