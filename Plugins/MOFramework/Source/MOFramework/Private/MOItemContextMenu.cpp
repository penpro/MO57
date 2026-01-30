#include "MOItemContextMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "TimerManager.h"

void UMOItemContextMenu::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtonEvents();

	// Reset timer when menu opens
	MouseOutsideTimer = 0.0f;

	UE_LOG(LogMOFramework, Warning, TEXT("[ContextMenu] NativeConstruct - ButtonContainer=%s"),
		ButtonContainer ? TEXT("valid") : TEXT("NULL"));

	// Start timer-based mouse check (more reliable than NativeTick for CommonUI widgets)
	StartMouseCheckTimer();
}

void UMOItemContextMenu::NativeDestruct()
{
	StopMouseCheckTimer();
	OnMenuClosed.Broadcast();
	Super::NativeDestruct();
}

void UMOItemContextMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const bool bMouseOver = IsMouseOverMenu();

	// Debug logging
	UE_LOG(LogMOFramework, Verbose, TEXT("[ContextMenu] Tick - MouseOver=%s, Timer=%.2f, ButtonContainer=%s"),
		bMouseOver ? TEXT("true") : TEXT("false"),
		MouseOutsideTimer,
		ButtonContainer ? TEXT("valid") : TEXT("NULL"));

	// Auto-close when mouse leaves the menu
	if (!bMouseOver)
	{
		MouseOutsideTimer += InDeltaTime;
		if (MouseOutsideTimer >= AutoCloseDelay)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[ContextMenu] Auto-closing menu"));
			CloseMenu();
		}
	}
	else
	{
		MouseOutsideTimer = 0.0f;
	}
}

UWidget* UMOItemContextMenu::NativeGetDesiredFocusTarget() const
{
	// Focus the first visible button
	if (UseButton && UseButton->IsVisible())
	{
		return UseButton;
	}
	if (Drop1Button && Drop1Button->IsVisible())
	{
		return Drop1Button;
	}
	return nullptr;
}

void UMOItemContextMenu::InitializeForItem(UMOInventoryComponent* InInventoryComponent, const FGuid& InItemGuid, int32 InSlotIndex)
{
	InventoryComponent = InInventoryComponent;
	ItemGuid = InItemGuid;
	SlotIndex = InSlotIndex;
	bInitialized = true;

	RefreshButtonVisibility();
}

void UMOItemContextMenu::SetMenuPosition(FVector2D ScreenPosition)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Get mouse position in viewport coordinates
	float MouseX, MouseY;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	// Offset so menu spawns with cursor inside it (not at corner)
	// Negative X = spawn to the left, Negative Y = spawn above
	const float OffsetX = -15.0f;
	const float OffsetY = -15.0f;

	// Get the DPI scale to convert viewport coords to widget coords
	const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(PC);

	// SetPositionInViewport expects coordinates that will be multiplied by DPI scale internally
	// GetMousePosition returns already-scaled coordinates, so we need to divide
	if (DPIScale > 0.0f)
	{
		const FVector2D AdjustedPosition = (FVector2D(MouseX, MouseY) + FVector2D(OffsetX, OffsetY)) / DPIScale;
		SetPositionInViewport(AdjustedPosition, false);
	}
	else
	{
		SetPositionInViewport(FVector2D(MouseX + OffsetX, MouseY + OffsetY), false);
	}
}

void UMOItemContextMenu::RefreshButtonVisibility_Implementation()
{
	if (!bInitialized || !IsValid(InventoryComponent))
	{
		return;
	}

	// Get item info
	FMOInventoryEntry SlotEntry;
	if (!InventoryComponent->TryGetSlotEntry(SlotIndex, SlotEntry) || !SlotEntry.ItemGuid.IsValid())
	{
		CloseMenu();
		return;
	}

	// Lookup item definition
	FMOItemDefinitionRow ItemDef;
	const bool bFoundDef = UMOItemDatabaseSettings::GetItemDefinition(SlotEntry.ItemDefinitionId, ItemDef);

	const bool bIsConsumable = bFoundDef ? ItemDef.bConsumable : false;
	const bool bHasMultiple = SlotEntry.Quantity > 1;

	// Show/hide buttons based on item properties
	if (UseButton)
	{
		UseButton->SetVisibility(bIsConsumable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		// Set button text to "Consume" for consumable items
		if (bIsConsumable)
		{
			UseButton->SetButtonText(NSLOCTEXT("MOContextMenu", "Consume", "Consume"));
		}
	}

	if (Drop1Button)
	{
		Drop1Button->SetVisibility(ESlateVisibility::Visible);
	}

	if (DropAllButton)
	{
		// Only show "Drop All" if we have more than 1
		DropAllButton->SetVisibility(bHasMultiple ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (InspectButton)
	{
		InspectButton->SetVisibility(ESlateVisibility::Visible);
	}

	if (SplitStackButton)
	{
		SplitStackButton->SetVisibility(bHasMultiple ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CraftButton)
	{
		CraftButton->SetVisibility(ESlateVisibility::Visible);
	}
}

void UMOItemContextMenu::BindButtonEvents()
{
	if (UseButton)
	{
		UseButton->OnClicked().RemoveAll(this);
		UseButton->OnClicked().AddUObject(this, &UMOItemContextMenu::HandleUseClicked);
	}
	if (Drop1Button)
	{
		Drop1Button->OnClicked().RemoveAll(this);
		Drop1Button->OnClicked().AddUObject(this, &UMOItemContextMenu::HandleDrop1Clicked);
	}
	if (DropAllButton)
	{
		DropAllButton->OnClicked().RemoveAll(this);
		DropAllButton->OnClicked().AddUObject(this, &UMOItemContextMenu::HandleDropAllClicked);
	}
	if (InspectButton)
	{
		InspectButton->OnClicked().RemoveAll(this);
		InspectButton->OnClicked().AddUObject(this, &UMOItemContextMenu::HandleInspectClicked);
	}
	if (SplitStackButton)
	{
		SplitStackButton->OnClicked().RemoveAll(this);
		SplitStackButton->OnClicked().AddUObject(this, &UMOItemContextMenu::HandleSplitStackClicked);
	}
	if (CraftButton)
	{
		CraftButton->OnClicked().RemoveAll(this);
		CraftButton->OnClicked().AddUObject(this, &UMOItemContextMenu::HandleCraftClicked);
	}
}

void UMOItemContextMenu::CloseMenu()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[ContextMenu] CloseMenu called"));

	// Stop the timer first
	StopMouseCheckTimer();

	// Remove from parent to close
	RemoveFromParent();
}

bool UMOItemContextMenu::IsMouseOverMenu() const
{
	if (!ButtonContainer)
	{
		return false;
	}

	// Get absolute cursor position (screen space)
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();

	// Get the widget's geometry and check if mouse is inside
	const FGeometry& Geometry = ButtonContainer->GetCachedGeometry();
	const FVector2D LocalMousePos = Geometry.AbsoluteToLocal(AbsoluteMousePos);
	const FVector2D LocalSize = Geometry.GetLocalSize();

	return LocalMousePos.X >= 0.0f && LocalMousePos.X <= LocalSize.X &&
	       LocalMousePos.Y >= 0.0f && LocalMousePos.Y <= LocalSize.Y;
}

void UMOItemContextMenu::HandleUseClicked()
{
	OnActionSelected.Broadcast(TEXT("Use"), ItemGuid);
	CloseMenu();
}

void UMOItemContextMenu::HandleDrop1Clicked()
{
	OnActionSelected.Broadcast(TEXT("Drop1"), ItemGuid);
	CloseMenu();
}

void UMOItemContextMenu::HandleDropAllClicked()
{
	OnActionSelected.Broadcast(TEXT("DropAll"), ItemGuid);
	CloseMenu();
}

void UMOItemContextMenu::HandleInspectClicked()
{
	OnActionSelected.Broadcast(TEXT("Inspect"), ItemGuid);
	CloseMenu();
}

void UMOItemContextMenu::HandleSplitStackClicked()
{
	OnActionSelected.Broadcast(TEXT("SplitStack"), ItemGuid);
	CloseMenu();
}

void UMOItemContextMenu::HandleCraftClicked()
{
	OnActionSelected.Broadcast(TEXT("Craft"), ItemGuid);
	CloseMenu();
}

void UMOItemContextMenu::StartMouseCheckTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Check mouse position every 0.05 seconds (20 times per second)
	World->GetTimerManager().SetTimer(
		MouseCheckTimerHandle,
		this,
		&UMOItemContextMenu::CheckMousePosition,
		0.05f,
		true  // Looping
	);

	UE_LOG(LogMOFramework, Warning, TEXT("[ContextMenu] Started mouse check timer"));
}

void UMOItemContextMenu::StopMouseCheckTimer()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(MouseCheckTimerHandle);
	}
}

void UMOItemContextMenu::CheckMousePosition()
{
	const bool bMouseOver = IsMouseOverMenu();

	if (!bMouseOver)
	{
		MouseOutsideTimer += 0.05f;  // Timer interval
		if (MouseOutsideTimer >= AutoCloseDelay)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[ContextMenu] Auto-closing - mouse outside for %.2f seconds"), MouseOutsideTimer);
			CloseMenu();
		}
	}
	else
	{
		MouseOutsideTimer = 0.0f;
	}
}
