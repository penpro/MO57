#include "MOStationContextMenu.h"
#include "MONotificationWidget.h"
#include "MOPlayerController.h"
#include "MOFireActions.h"
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
	// A real attempt, made by the SERVER on the pawn's timed action (FMOFireActions): tinder and a friction drill, it can fail. It used to be an instant switch
	// thrown on whichever machine clicked -- a co-op client's fire burned for that client only.
	OnFireAction(FMOFireActions::Light);
}

void UMOStationContextMenu::OnFireAction(FName ActionId)
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	APlayerController* PC = GetOwningPlayer();
	if (!Station || !PC)
	{
		return;
	}
	TArray<FMOItemActionOption> Options;
	FMOFireActions::GetActions(PC->GetPawn(), Station, Options);
	const FMOItemActionOption* Option = Options.FindByPredicate([ActionId](const FMOItemActionOption& O) { return O.Id == ActionId; });
	if (!Option || !Option->bEnabled)
	{
		if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC))
		{
			MOPC->ClientShowNotification(Option ? Option->DisabledReason : NSLOCTEXT("MOStation", "CannotNow", "You cannot do that now."), 3.0f, EMONotificationType::Warning);
		}
		return;
	}
	if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC))
	{
		MOPC->ServerStationAction(Station, ActionId);
	}
	if (ActionId == FMOFireActions::Light)
	{
		OnLightClicked.Broadcast();
	}
	RequestClose();
}

void UMOStationContextMenu::RefreshFireActions()
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	const APlayerController* PC = GetOwningPlayer();
	TArray<FMOItemActionOption> Options;
	if (Station && PC)
	{
		FMOFireActions::GetActions(PC->GetPawn(), Station, Options);
	}
	// Light keeps its own button (its label and enabled state come from the same option); the rest are runtime buttons under it.
	if (const FMOItemActionOption* LightOption = Options.FindByPredicate([](const FMOItemActionOption& O) { return O.Id == FMOFireActions::Light; }))
	{
		if (LightButton)
		{
			LightButton->SetButtonText(LightOption->Label);
			LightButton->SetIsEnabled(LightOption->bEnabled);
			LightButton->SetToolTipText(LightOption->bEnabled ? FText::GetEmpty() : LightOption->DisabledReason);
			LightButton->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (LightButton)
	{
		LightButton->SetVisibility(ESlateVisibility::Collapsed);   // burning, or not a fire
	}
	Options.RemoveAll([](const FMOItemActionOption& O) { return O.Id == FMOFireActions::Light; });
	// Styled like Open (secondary): only Light stands out.
	UMOUIUtils::RebuildActionButtons(this, FireActionButtons, LightButton ? LightButton->GetParent() : nullptr, LightButton, Options, OpenButton ? OpenButton.Get() : LightButton.Get(),
		[this](FName Id) { OnFireAction(Id); });
}

bool UMOStationContextMenu::CanLightStation() const
{
	AMOCraftingStationActor* Station = TargetStation.Get();
	const APlayerController* PC = GetOwningPlayer();
	TArray<FMOItemActionOption> Options;
	if (Station && PC)
	{
		FMOFireActions::GetActions(PC->GetPawn(), Station, Options);
	}
	const FMOItemActionOption* Light = Options.FindByPredicate([](const FMOItemActionOption& O) { return O.Id == FMOFireActions::Light; });
	return Light && Light->bEnabled;
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

	// The fire's state in words: burning (and for how long), laid but not lit, or cold and empty.
	if (Station->IsStationActive())
	{
		return FText::Format(NSLOCTEXT("MOStation", "FuelTimeRemaining", "Burning: {0} left"), UMOUIUtils::FormatDurationAsTimeCode(TimeRemaining));
	}
	if (Station->CurrentFuel > 0.0f)
	{
		return FText::Format(NSLOCTEXT("MOStation", "FuelLaid", "Not lit: fuel for {0}"), UMOUIUtils::FormatDurationAsTimeCode(TimeRemaining));
	}
	return NSLOCTEXT("MOStation", "Cold", "Cold: no fuel");
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

	// What can be done changes with what the pawn carries and the fire's state: refresh the actions twice a second.
	FireActionRefreshTimer += InDeltaTime;
	if (FireActionRefreshTimer >= 0.5f)
	{
		FireActionRefreshTimer = 0.0f;
		if (FuelTimeText)
		{
			FuelTimeText->SetText(GetFuelTimeText());
		}
		UpdateButtonStates();
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
	const bool bStationIsActive = IsStationActive();

	// The fire's actions (light / add fuel / put out) from the one registry the server runs.
	RefreshFireActions();

	// Blueprint callback
	OnButtonStatesUpdated(bCanLight, bStationIsActive);
}
