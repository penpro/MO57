// Copyright Penumbra Group Inc. All Rights Reserved.

#include "MOPrimaryGameLayout.h"
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

	UE_LOG(LogTemp, Log, TEXT("MOPrimaryGameLayout: Registered %d layers"), LayerStacks.Num());
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
		return nullptr;
	}

	// AddWidget creates the widget and activates it
	UCommonActivatableWidget* Widget = Stack->AddWidget(WidgetClass);
	if (Widget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Pushed %s to layer %s"),
			*WidgetClass->GetName(), *LayerTag.ToString());
	}

	return Widget;
}

void UMOPrimaryGameLayout::PushWidgetToLayerInstance(FGameplayTag LayerTag, UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOPrimaryGameLayout::PushWidgetToLayerInstance: Widget is null"));
		return;
	}

	UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		return;
	}

	// For existing widget instances, we need to add them to the stack
	// Note: AddWidget<> is the preferred method, but for instances we activate directly
	Widget->AddToViewport(0); // Temporary - proper implementation would use stack's internal mechanisms
	Widget->ActivateWidget();

	UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Pushed widget instance to layer %s"), *LayerTag.ToString());
}

void UMOPrimaryGameLayout::PopWidgetFromLayer(FGameplayTag LayerTag)
{
	UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		return;
	}

	// Get the active widget and deactivate it
	UWidget* ActiveWidget = Stack->GetActiveWidget();
	if (UCommonActivatableWidget* ActivatableWidget = Cast<UCommonActivatableWidget>(ActiveWidget))
	{
		ActivatableWidget->DeactivateWidget();
		UE_LOG(LogTemp, Verbose, TEXT("MOPrimaryGameLayout: Popped widget from layer %s"), *LayerTag.ToString());
	}
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

	// Count widgets in menu layers (excluding HUD which is always-visible)
	auto CountLayer = [this, &Count](FGameplayTag LayerTag)
	{
		if (const UCommonActivatableWidgetContainerBase* Stack = GetLayerStack(LayerTag))
		{
			if (Stack->GetActiveWidget() != nullptr)
			{
				Count++;
			}
		}
	};

	CountLayer(MOUILayerTags::Layer_Game);
	CountLayer(MOUILayerTags::Layer_GameOverlay);
	CountLayer(MOUILayerTags::Layer_Menu);
	CountLayer(MOUILayerTags::Layer_Modal);

	return Count;
}
