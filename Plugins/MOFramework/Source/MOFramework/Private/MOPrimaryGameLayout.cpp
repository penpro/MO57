// Copyright Penumbra Group Inc. All Rights Reserved.

#include "MOPrimaryGameLayout.h"
#include "MOFramework.h"
#include "MOActivatableWidget.h"
#include "MOGameplayInputStub.h"
#include "MOUIDebugSubsystem.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

// Define gameplay tags for UI layers
namespace MOUILayerTags
{
	UE_DEFINE_GAMEPLAY_TAG(Layer_HUD, "MO.UI.Layer.HUD");
	UE_DEFINE_GAMEPLAY_TAG(Layer_Game, "MO.UI.Layer.Game");
	UE_DEFINE_GAMEPLAY_TAG(Layer_GameOverlay, "MO.UI.Layer.GameOverlay");
	UE_DEFINE_GAMEPLAY_TAG(Layer_Menu, "MO.UI.Layer.Menu");
	UE_DEFINE_GAMEPLAY_TAG(Layer_Modal, "MO.UI.Layer.Modal");
}

UMOPrimaryGameLayout::UMOPrimaryGameLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOPrimaryGameLayout::NativeConstruct()
{
	Super::NativeConstruct();

	// Register layer stacks with their tags
	// The BindWidgetOptional properties are set by the Blueprint
	if (HUDLayer)
	{
		RegisterLayer(MOUILayerTags::Layer_HUD, HUDLayer);
	}
	if (GameLayer)
	{
		RegisterLayer(MOUILayerTags::Layer_Game, GameLayer);
	}
	if (GameOverlayLayer)
	{
		RegisterLayer(MOUILayerTags::Layer_GameOverlay, GameOverlayLayer);
	}
	if (MenuLayer)
	{
		RegisterLayer(MOUILayerTags::Layer_Menu, MenuLayer);
	}
	if (ModalLayer)
	{
		RegisterLayer(MOUILayerTags::Layer_Modal, ModalLayer);
	}

	// Log registration results - CRITICAL: if layers are null, Blueprint is misconfigured
	if (!HUDLayer) UE_LOG(LogMOFramework, Warning, TEXT("MOPrimaryGameLayout: HUDLayer is NULL - widget not bound in Blueprint!"));
	if (!GameLayer) UE_LOG(LogMOFramework, Warning, TEXT("MOPrimaryGameLayout: GameLayer is NULL - widget not bound in Blueprint!"));
	if (!GameOverlayLayer) UE_LOG(LogMOFramework, Warning, TEXT("MOPrimaryGameLayout: GameOverlayLayer is NULL - widget not bound in Blueprint!"));
	if (!MenuLayer) UE_LOG(LogMOFramework, Warning, TEXT("MOPrimaryGameLayout: MenuLayer is NULL - widget not bound in Blueprint!"));
	if (!ModalLayer) UE_LOG(LogMOFramework, Warning, TEXT("MOPrimaryGameLayout: ModalLayer is NULL - widget not bound in Blueprint!"));

	UE_LOG(LogMOFramework, Warning, TEXT("MOPrimaryGameLayout: Registered %d/5 layers"), LayerStacks.Num());
}

void UMOPrimaryGameLayout::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerStack)
{
	if (LayerStack && LayerTag.IsValid())
	{
		LayerStacks.Add(LayerTag, LayerStack);
		UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Registered layer %s"), *LayerTag.ToString());
	}
}

UCommonActivatableWidgetContainerBase* UMOPrimaryGameLayout::GetLayerStack(FGameplayTag LayerTag) const
{
	if (const TObjectPtr<UCommonActivatableWidgetContainerBase>* FoundStack = LayerStacks.Find(LayerTag))
	{
		return *FoundStack;
	}

	UE_LOG(LogTemp, Warning, TEXT("MOPrimaryGameLayout: Layer %s not found"), *LayerTag.ToString());
	return nullptr;
}

UCommonActivatableWidget* UMOPrimaryGameLayout::PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOPrimaryGameLayout::PushWidgetToLayer: WidgetClass is null"));
		return nullptr;
	}

	UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOPrimaryGameLayout::PushWidgetToLayer: No stack for layer %s"), *LayerTag.ToString());
		return nullptr;
	}

	MOUI_LOG(this, "Layer", "PUSH    layer=%s class=%s", *LayerTag.ToString(), *WidgetClass->GetName());

	UCommonActivatableWidget* Widget = Stack->AddWidget(WidgetClass);
	if (Widget)
	{
		MOUI_LOG(this, "Layer", "PUSHED  layer=%s widget=%s activated=%s",
			*LayerTag.ToString(), *Widget->GetName(),
			Widget->IsActivated() ? TEXT("YES") : TEXT("no"));
	}
	else
	{
		MOUI_LOG(this, "Layer", "PUSH FAILED  layer=%s class=%s (AddWidget returned null)",
			*LayerTag.ToString(), *WidgetClass->GetName());
	}

	return Widget;
}

UCommonActivatableWidget* UMOPrimaryGameLayout::PushWidgetToLayerInstance(FGameplayTag LayerTag, UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOPrimaryGameLayout::PushWidgetToLayerInstance: Widget is null"));
		return nullptr;
	}

	// If widget is already activated and in viewport, return it as-is
	if (Widget->IsActivated() && Widget->IsInViewport())
	{
		UE_LOG(LogTemp, Log, TEXT("MOPrimaryGameLayout: Widget %s already activated"), *Widget->GetName());
		return Widget;
	}

	UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOPrimaryGameLayout::PushWidgetToLayerInstance: No stack for layer %s"), *LayerTag.ToString());
		return nullptr;
	}

	// Widget was previously removed from stack - we need to add it back via AddWidget
	// CommonUI stacks don't support adding existing instances directly, so create new via class
	// AddWidget handles activation automatically (UE 5.5+) - DO NOT call ActivateWidget/SetFocus
	UClass* WidgetClass = Widget->GetClass();
	UCommonActivatableWidget* NewWidget = Stack->AddWidget(WidgetClass);

	if (NewWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOPrimaryGameLayout: Created NEW widget %s for layer %s - caller must update cached reference!"),
			*NewWidget->GetName(), *LayerTag.ToString());
	}

	return NewWidget;
}

void UMOPrimaryGameLayout::PopWidgetFromLayer(FGameplayTag LayerTag)
{
	UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		return;
	}

	UWidget* ActiveWidget = Stack->GetActiveWidget();
	if (UCommonActivatableWidget* ActivatableWidget = Cast<UCommonActivatableWidget>(ActiveWidget))
	{
		MOUI_LOG(this, "Layer", "POP     layer=%s widget=%s",
			*LayerTag.ToString(), *ActivatableWidget->GetName());
		ActivatableWidget->DeactivateWidget();
		UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Popped widget from layer %s"), *LayerTag.ToString());
	}
}

bool UMOPrimaryGameLayout::RemoveWidgetFromLayer(UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		return false;
	}

	// Try to remove from each layer stack
	// RemoveWidget will handle the case where the widget isn't in that stack
	for (const auto& Pair : LayerStacks)
	{
		UCommonActivatableWidgetContainerBase* Stack = Pair.Value;
		if (Stack)
		{
			// Try to remove - this is safe even if widget isn't in this stack
			Stack->RemoveWidget(*Widget);

			// Check if widget is no longer in viewport (removal succeeded)
			if (!Widget->IsInViewport())
			{
				UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Removed widget %s from layer %s"),
					*Widget->GetName(), *Pair.Key.ToString());
				return true;
			}
		}
	}

	// If widget is still in viewport (wasn't in any stack), it was added via AddToViewport
	// CRITICAL: Call DeactivateWidget() BEFORE RemoveFromParent() so NativeOnDeactivated fires
	// This ensures input state (cursor, ignore move/look) is properly restored
	if (Widget->IsInViewport())
	{
		if (Widget->IsActivated())
		{
			Widget->DeactivateWidget();
		}
		Widget->RemoveFromParent();
		UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Removed widget %s from viewport (not in stack, deactivated)"),
			*Widget->GetName());
		return true;
	}

	return false;
}

void UMOPrimaryGameLayout::ClearLayer(FGameplayTag LayerTag)
{
	UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		return;
	}

	Stack->ClearWidgets();
	UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Cleared layer %s"), *LayerTag.ToString());
}

bool UMOPrimaryGameLayout::IsLayerActive(FGameplayTag LayerTag) const
{
	const UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		return false;
	}

	return Stack->GetActiveWidget() != nullptr;
}

int32 UMOPrimaryGameLayout::GetActiveMenuCount() const
{
	int32 Count = 0;

	// Count only interactive Menu-mode UI surfaces. Two things must NOT count:
	//   1. The RootContentWidget stub (MOGameplayInputStub) on every layer —
	//      always "active" but never a menu.
	//   2. Passive Game-mode overlays (progress bars, tutorial hints) — counting
	//      them as menus parks input in Menu mode during timed gameplay actions
	//      and makes the stub defer its cursor/reticle reset (audit H43, and the
	//      H26/H43 interlock).
	// Classify by input mode, not by class (Principle 6) — via IsUISurface().
	//
	// The stub MUST be excluded by class FIRST: UMOGameplayInputStub's own
	// GetDesiredInputConfig calls back into GetActiveMenuCount, so probing its
	// input mode here would recurse infinitely.
	auto CountLayer = [this, &Count](FGameplayTag LayerTag)
	{
		const UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
		if (!Stack)
		{
			return;
		}
		UWidget* ActiveWidget = Stack->GetActiveWidget();
		if (!ActiveWidget || ActiveWidget->IsA<UMOGameplayInputStub>())
		{
			return;
		}
		if (const UMOActivatableWidget* MOWidget = Cast<UMOActivatableWidget>(ActiveWidget))
		{
			// Known MO widget — trust its declared input mode.
			if (MOWidget->IsUISurface())
			{
				Count++;
			}
			return;
		}
		// Non-MO activatable of unknown policy — preserve prior count-it behavior.
		Count++;
	};

	CountLayer(MOUILayerTags::Layer_Game);
	CountLayer(MOUILayerTags::Layer_GameOverlay);
	CountLayer(MOUILayerTags::Layer_Menu);
	CountLayer(MOUILayerTags::Layer_Modal);

	return Count;
}
