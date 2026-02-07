#include "MOStationContextMenu.h"
#include "MOCraftingStationActor.h"
#include "MOCommonButton.h"
#include "MOFramework.h"
#include "MORecipeDatabaseSettings.h"
#include "MONotificationComponent.h"
#include "MOInventoryComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

UMOStationContextMenu::UMOStationContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOStationContextMenu::InitializeForStation(AMOCraftingStationActor* Station)
{
	if (!IsValid(Station))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOStationContextMenu] InitializeForStation called with invalid station"));
		return;
	}

	TargetStation = Station;

	// Update display
	RefreshDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Initialized for station: %s"), *Station->GetName());
}

void UMOStationContextMenu::RefreshDisplay()
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	if (!Station)
	{
		return;
	}

	// Update station name
	if (StationNameText)
	{
		StationNameText->SetText(GetStationName());
	}

	// Update fuel time
	if (FuelTimeText)
	{
		FuelTimeText->SetText(GetFuelTimeText());
	}

	// Update button states
	UpdateButtonStates();
}

// ============================================================================
// ACTIONS
// ============================================================================

void UMOStationContextMenu::OpenInventory()
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	if (!Station)
	{
		return;
	}

	OnOpenClicked.Broadcast();
	OnRequestClose.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Open inventory requested"));
}

void UMOStationContextMenu::OpenCraftingMenu()
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	if (!Station)
	{
		return;
	}

	OnCraftClicked.Broadcast();
	OnRequestClose.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Open crafting menu requested"));
}

void UMOStationContextMenu::LightStation()
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	if (!Station)
	{
		return;
	}

	// Helper to show notification
	auto ShowNotification = [this](const FText& Message)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (UMONotificationComponent* NotifComp = PC->FindComponentByClass<UMONotificationComponent>())
			{
				NotifComp->ShowNotification(Message, 3.0f);
			}
		}
	};

	// Debug: Log accepted fuel items
	UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Station requires fuel: %s"), Station->bRequiresFuel ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Accepted fuel items (%d):"), Station->AcceptedFuelItems.Num());
	for (const FName& FuelId : Station->AcceptedFuelItems)
	{
		UE_LOG(LogMOFramework, Log, TEXT("  - %s"), *FuelId.ToString());
	}

	// Debug: Log inventory contents
	if (UMOInventoryComponent* Inv = Station->GetStationInventory())
	{
		TArray<FMOInventoryEntry> Entries;
		Inv->GetInventoryEntries(Entries);
		UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Station inventory (%d items):"), Entries.Num());
		for (const FMOInventoryEntry& Entry : Entries)
		{
			UE_LOG(LogMOFramework, Log, TEXT("  - %s x%d"), *Entry.ItemDefinitionId.ToString(), Entry.Quantity);
		}
	}

	if (!CanLightStation())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOStationContextMenu] Cannot light station - no fuel"));
		ShowNotification(FText::FromString(TEXT("No Compatible Fuel In Inventory")));
		return;
	}

	// Consume fuel from inventory before activating
	if (Station->bRequiresFuel && Station->CurrentFuel <= 0.0f)
	{
		float FuelAdded = Station->ConsumeFuelFromInventory();
		if (FuelAdded <= 0.0f)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOStationContextMenu] No fuel items in inventory to consume"));
			ShowNotification(FText::FromString(TEXT("No Compatible Fuel In Inventory")));
			return;
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Consumed fuel, added %.1f fuel"), FuelAdded);
	}

	Station->SetStationActive(true);
	OnLightClicked.Broadcast();
	RefreshDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOStationContextMenu] Station lit successfully"));
}

// ============================================================================
// STATE
// ============================================================================

bool UMOStationContextMenu::CanLightStation() const
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	if (!Station)
	{
		return false;
	}

	// Can light if station doesn't require fuel
	if (!Station->bRequiresFuel)
	{
		return true;
	}

	// Can light if has fuel OR has fuel items in inventory
	return Station->CurrentFuel > 0.0f || Station->HasFuelInInventory();
}

bool UMOStationContextMenu::IsStationActive() const
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	return Station ? Station->IsStationActive() : false;
}

FText UMOStationContextMenu::GetFuelTimeText() const
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	if (!Station)
	{
		return FText::FromString(TEXT("--:--"));
	}

	if (!Station->bRequiresFuel)
	{
		return FText::FromString(TEXT("No fuel needed"));
	}

	float TimeRemaining = Station->GetFuelTimeRemaining();
	if (TimeRemaining < 0.0f)
	{
		return FText::FromString(TEXT("No fuel needed"));
	}

	int32 Minutes = FMath::FloorToInt(TimeRemaining / 60.0f);
	int32 Seconds = FMath::FloorToInt(FMath::Fmod(TimeRemaining, 60.0f));
	return FText::FromString(FString::Printf(TEXT("%d:%02d remaining"), Minutes, Seconds));
}

FText UMOStationContextMenu::GetStationName() const
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	if (!Station)
	{
		return FText::FromString(TEXT("Unknown Station"));
	}

	// Try to get display name from recipe
	FName RecipeId = Station->GetRecipeId();
	if (!RecipeId.IsNone())
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (Recipe)
		{
			return Recipe->DisplayName;
		}
	}

	// Fallback to station type enum name
	return FText::FromString(UEnum::GetValueAsString(Station->GetStationType()));
}

// ============================================================================
// POSITIONING
// ============================================================================

void UMOStationContextMenu::SetPopupPosition(FVector2D ScreenPosition)
{
	SetPositionInViewport(ScreenPosition, true);

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOStationContextMenu] SetPopupPosition called with (%.0f, %.0f)"),
		ScreenPosition.X, ScreenPosition.Y);
}

// ============================================================================
// OVERRIDES
// ============================================================================

void UMOStationContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button handlers
	if (OpenButton)
	{
		OpenButton->OnClicked().AddUObject(this, &UMOStationContextMenu::HandleOpenClicked);
	}
	if (CraftButton)
	{
		CraftButton->OnClicked().AddUObject(this, &UMOStationContextMenu::HandleCraftClicked);
	}
	if (LightButton)
	{
		LightButton->OnClicked().AddUObject(this, &UMOStationContextMenu::HandleLightClicked);
	}
}

void UMOStationContextMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check if station is still valid
	if (!TargetStation.IsValid())
	{
		OnRequestClose.Broadcast();
		return;
	}

	// Update fuel time display if station is active
	if (IsStationActive())
	{
		if (FuelTimeText)
		{
			FuelTimeText->SetText(GetFuelTimeText());
		}

		AMOCraftingStationActor* Station = TargetStation.Get();
		if (Station)
		{
			OnFuelTimeUpdated(Station->GetFuelTimeRemaining(), true);
		}
	}
}

FReply UMOStationContextMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Close on Escape or Tab
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================================
// HANDLERS
// ============================================================================

void UMOStationContextMenu::HandleOpenClicked()
{
	OpenInventory();
}

void UMOStationContextMenu::HandleCraftClicked()
{
	OpenCraftingMenu();
}

void UMOStationContextMenu::HandleLightClicked()
{
	LightStation();
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOStationContextMenu::UpdateButtonStates()
{
	const bool bCanLight = CanLightStation();
	const bool bIsActive = IsStationActive();

	// Light button is disabled if already active or can't light
	if (LightButton)
	{
		LightButton->SetIsEnabled(bCanLight && !bIsActive);
	}

	// Blueprint callback
	OnButtonStatesUpdated(bCanLight, bIsActive);
}
