#include "MOStationContextMenu.h"
#include "MOCraftingStationActor.h"
#include "MOCommonButton.h"
#include "MOFramework.h"
#include "MORecipeDatabaseSettings.h"
#include "MONotificationComponent.h"
#include "MOInventoryComponent.h"
#include "MOUIUtils.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

UMOStationContextMenu::UMOStationContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOStationContextMenu::RequestClose()
{
	// Broadcast legacy delegate for backward compatibility
	OnRequestClose.Broadcast();
	// Also call base which broadcasts OnCloseRequested
	Super::RequestClose();
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
	RequestClose();

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
	RequestClose();

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

	return FText::Format(
		NSLOCTEXT("MOStation", "FuelTimeRemaining", "{0} remaining"),
		UMOUIUtils::FormatDurationAsTimeCode(TimeRemaining)
	);
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

// SetPopupPosition is inherited from UMOContextMenuBase

// ============================================================================
// OVERRIDES
// ============================================================================

void UMOStationContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button handlers
	if (OpenButton)
	{
		OpenButton->OnClicked().RemoveAll(this);
		OpenButton->OnClicked().AddUObject(this, &UMOStationContextMenu::HandleOpenClicked);
	}
	if (CraftButton)
	{
		CraftButton->OnClicked().RemoveAll(this);
		CraftButton->OnClicked().AddUObject(this, &UMOStationContextMenu::HandleCraftClicked);
	}
	if (LightButton)
	{
		LightButton->OnClicked().RemoveAll(this);
		LightButton->OnClicked().AddUObject(this, &UMOStationContextMenu::HandleLightClicked);
	}
}

void UMOStationContextMenu::NativeDestruct()
{
	// Clean up button bindings
	if (OpenButton)
	{
		OpenButton->OnClicked().RemoveAll(this);
	}
	if (CraftButton)
	{
		CraftButton->OnClicked().RemoveAll(this);
	}
	if (LightButton)
	{
		LightButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMOStationContextMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check if station is still valid
	if (!TargetStation.IsValid())
	{
		RequestClose();
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

// NativeOnKeyDown is inherited from UMOContextMenuBase

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
