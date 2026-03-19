// Copyright Penumbra Group Inc. All Rights Reserved.

#include "MOGameUIManagerSubsystem.h"
#include "MOPrimaryGameLayout.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

UMOGameUIManagerSubsystem::UMOGameUIManagerSubsystem()
{
}

void UMOGameUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("MOGameUIManagerSubsystem: Initialized for world %s"), *GetWorld()->GetName());
}

void UMOGameUIManagerSubsystem::Deinitialize()
{
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
	if (!PrimaryLayoutClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: PrimaryLayoutClass is not set! Set it in Project Settings or Blueprint."));
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: PlayerController has no LocalPlayer"));
		return;
	}

	// Create the layout widget
	UMOPrimaryGameLayout* NewLayout = CreateWidget<UMOPrimaryGameLayout>(PlayerController, PrimaryLayoutClass);
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

	UE_LOG(LogTemp, Log, TEXT("MOGameUIManagerSubsystem: Created layout for player %s"), *PlayerController->GetName());
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

void UMOGameUIManagerSubsystem::PushWidgetToLayerInstance(FGameplayTag LayerTag, UCommonActivatableWidget* Widget)
{
	UMOPrimaryGameLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOGameUIManagerSubsystem: No root layout available"));
		return;
	}

	Layout->PushWidgetToLayerInstance(LayerTag, Widget);
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
