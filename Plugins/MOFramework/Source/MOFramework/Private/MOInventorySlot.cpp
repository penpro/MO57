#include "MOInventorySlot.h"
#include "MOFramework.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"

#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MODragVisualWidget.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"

UMOInventorySlot::UMOInventorySlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable this widget to receive mouse input for drag-drop
	SetIsFocusable(true);
}

void UMOInventorySlot::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply empty icon in PreConstruct for editor preview
	if (IsValid(ItemIconImage))
	{
		UTexture2D* EmptyIcon = GetEffectiveEmptyIcon();
		if (EmptyIcon)
		{
			ItemIconImage->SetBrushFromTexture(EmptyIcon, true);
		}
	}
}

void UMOInventorySlot::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeConstruct - SlotIndex=%d, Visibility=%d"),
		SlotIndex,
		(int32)GetVisibility());

	// Keep button visible and use its press/release events for drag detection
	if (IsValid(SlotButton))
	{
		SlotButton->OnClicked.RemoveAll(this);
		SlotButton->OnPressed.RemoveAll(this);
		SlotButton->OnReleased.RemoveAll(this);

		SlotButton->OnClicked.AddDynamic(this, &UMOInventorySlot::HandleSlotButtonClicked);
		SlotButton->OnPressed.AddDynamic(this, &UMOInventorySlot::HandleSlotButtonPressed);
		SlotButton->OnReleased.AddDynamic(this, &UMOInventorySlot::HandleSlotButtonReleased);

		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] SlotButton configured with press/release handlers"));
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOInventorySlot] SlotButton is NULL!"));
	}

	// Default: hide quantity visuals until we have real data.
	if (IsValid(QuantityBox))
	{
		QuantityBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] QuantityBox not bound. Ensure the widget is named exactly 'QuantityBox' and 'Is Variable' is enabled."));
	}

	// Initialize border to normal color
	if (IsValid(SlotBorder))
	{
		SlotBorder->SetBrushColor(NormalBorderColor);
	}

	// If visual data was already set (e.g., via SetVisualData before NativeConstruct),
	// just re-apply it now that the widget is fully constructed.
	// Otherwise, refresh from the inventory component.
	if (CachedVisualData.bHasItem)
	{
		// Visual data was pre-set, just apply it to the now-constructed widget
		ApplyVisualDataToWidget();
	}
	else
	{
		// No pre-set data, try to load from inventory
		RefreshFromInventory();
	}
}

void UMOInventorySlot::InitializeSlot(UMOInventoryComponent* InInventoryComponent, int32 InSlotIndex)
{
	InventoryComponent = InInventoryComponent;
	SlotIndex = InSlotIndex;

	RefreshFromInventory();
}

void UMOInventorySlot::RefreshFromInventory()
{
	CachedVisualData = FMOInventorySlotVisualData();

	if (!IsValid(InventoryComponent) || SlotIndex < 0)
	{
		ApplyVisualDataToWidget();
		OnVisualDataUpdated(CachedVisualData);
		return;
	}

	FMOInventoryEntry SlotEntry;
	if (InventoryComponent->TryGetSlotEntry(SlotIndex, SlotEntry))
	{
		CachedVisualData.bHasItem = true;
		CachedVisualData.ItemGuid = SlotEntry.ItemGuid;
		CachedVisualData.ItemDefinitionId = SlotEntry.ItemDefinitionId;
		CachedVisualData.Quantity = SlotEntry.Quantity;
	}

	ApplyVisualDataToWidget();
	OnVisualDataUpdated(CachedVisualData);
}

void UMOInventorySlot::SetVisualData(const FMOInventorySlotVisualData& InVisualData)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInventorySlot] SetVisualData: SlotIndex=%d, bHasItem=%s, ItemDefId=%s, Qty=%d, HasWorldItem=%s"),
		SlotIndex,
		InVisualData.bHasItem ? TEXT("true") : TEXT("false"),
		*InVisualData.ItemDefinitionId.ToString(),
		InVisualData.Quantity,
		InVisualData.SourceWorldItem.IsValid() ? TEXT("yes") : TEXT("no"));

	CachedVisualData = InVisualData;

	// Extract world item reference for drag operations
	SourceWorldItem = InVisualData.SourceWorldItem;

	ApplyVisualDataToWidget();
	OnVisualDataUpdated(CachedVisualData);
}

void UMOInventorySlot::ClearVisualData()
{
	CachedVisualData = FMOInventorySlotVisualData();
	ApplyVisualDataToWidget();
	OnVisualDataUpdated(CachedVisualData);
}

void UMOInventorySlot::SetSourceWorldItem(AMOWorldItem* InWorldItem)
{
	SourceWorldItem = InWorldItem;
}

AMOWorldItem* UMOInventorySlot::GetSourceWorldItem() const
{
	return SourceWorldItem.Get();
}

void UMOInventorySlot::SetCustomEmptyIcon(UTexture2D* InIcon)
{
	CustomEmptyIcon = InIcon;

	// Re-apply visuals if currently empty to show new icon
	if (!CachedVisualData.bHasItem)
	{
		ApplyVisualDataToWidget();
	}
}

UTexture2D* UMOInventorySlot::GetEffectiveEmptyIcon() const
{
	return IsValid(CustomEmptyIcon) ? CustomEmptyIcon.Get() : EmptySlotIcon.Get();
}

void UMOInventorySlot::ApplyVisualDataToWidget()
{
	// Quantity: show the box only for stacks > 1.
	UpdateQuantityBoxVisibility(CachedVisualData.bHasItem ? CachedVisualData.Quantity : 0);

	// Optional: keep text correct (safe even if you do not strictly need it).
	if (IsValid(QuantityText))
	{
		if (CachedVisualData.bHasItem && CachedVisualData.Quantity > 1)
		{
			QuantityText->SetText(FText::AsNumber(CachedVisualData.Quantity));
		}
		else
		{
			QuantityText->SetText(FText::GetEmpty());
		}
	}

	if (IsValid(DebugItemIdText))
	{
		if (CachedVisualData.bHasItem)
		{
			DebugItemIdText->SetText(FText::FromName(CachedVisualData.ItemDefinitionId));
			DebugItemIdText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			DebugItemIdText->SetText(FText::GetEmpty());
			DebugItemIdText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (IsValid(ItemIconImage))
	{
		UTexture2D* DesiredTexture = GetEffectiveEmptyIcon();

		if (CachedVisualData.bHasItem)
		{
			// Try to get icon from DataTable first
			UTexture2D* DataTableIcon = UMOItemDatabaseSettings::GetItemIconSmall(CachedVisualData.ItemDefinitionId);
			if (IsValid(DataTableIcon))
			{
				DesiredTexture = DataTableIcon;
			}
			else
			{
				// Fall back to default item icon
				DesiredTexture = DefaultItemIcon;
			}
		}

		ItemIconImage->SetBrushFromTexture(DesiredTexture, true);
	}
}

void UMOInventorySlot::UpdateQuantityBoxVisibility(int32 InQuantity)
{
	const bool bShouldShow = (InQuantity > 1);

	if (IsValid(QuantityBox))
	{
		QuantityBox->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// If you want the text to also not take layout space when hidden, you can do this too.
	// It is optional because the box is already collapsed.
	if (IsValid(QuantityText))
	{
		QuantityText->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMOInventorySlot::HandleSlotButtonClicked()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] HandleSlotButtonClicked - SlotIndex=%d, DragStarted=%s"),
		SlotIndex, bDragStarted ? TEXT("true") : TEXT("false"));

	// If a drag happened, don't also fire click
	if (bDragStarted)
	{
		bDragStarted = false;
		return;
	}

	if (!CachedVisualData.bHasItem)
	{
		OnSlotClicked.Broadcast(SlotIndex, FGuid());
		return;
	}

	OnSlotClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
}

void UMOInventorySlot::HandleSlotButtonPressed()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] HandleSlotButtonPressed - SlotIndex=%d, HasItem=%s"),
		SlotIndex, CachedVisualData.bHasItem ? TEXT("true") : TEXT("false"));

	if (bEnableDragDrop && CachedVisualData.bHasItem)
	{
		bButtonPressed = true;
		bDragStarted = false;

		// Store mouse position for drag threshold detection
		if (FSlateApplication::IsInitialized())
		{
			PressedMousePosition = FSlateApplication::Get().GetCursorPos();
		}
	}
}

void UMOInventorySlot::HandleSlotButtonReleased()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] HandleSlotButtonReleased - SlotIndex=%d, DragStarted=%s"),
		SlotIndex, bDragStarted ? TEXT("true") : TEXT("false"));

	const bool bWasDragging = bDragStarted;
	bButtonPressed = false;
	bDragStarted = false;

	// Restore visual state if we were dragging
	if (bWasDragging)
	{
		if (IsValid(SlotBorder))
		{
			SlotBorder->SetBrushColor(NormalBorderColor);
		}
		if (IsValid(SlotButton))
		{
			SlotButton->SetColorAndOpacity(FLinearColor::White);
		}
	}

	// Note: Actual drop is handled by NativeOnDrop on the target slot
	// If drag was cancelled (released outside any slot), NativeOnDragCancelled handles it
}

void UMOInventorySlot::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Native drag system handles everything via NativeOnPreviewMouseButtonDown -> DetectDrag
}

FReply UMOInventorySlot::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Preview gets called BEFORE the button sees the event
	// This is where we initiate drag detection for items
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bEnableDragDrop && CachedVisualData.bHasItem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnPreviewMouseButtonDown - initiating drag detect for slot %d"), SlotIndex);

		bButtonPressed = true;
		PressedMousePosition = InMouseEvent.GetScreenSpacePosition();

		// Tell Unreal to detect drag - when threshold exceeded, NativeOnDragDetected is called
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMOInventorySlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Handle right-click for context menu
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (CachedVisualData.bHasItem)
		{
			// Get screen position for menu placement
			const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();

			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Right-click on slot %d, item %s, screen pos %s"),
				SlotIndex, *CachedVisualData.ItemGuid.ToString(), *ScreenPosition.ToString());

			OnSlotRightClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid, ScreenPosition);
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMOInventorySlot::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnMouseButtonUp - SlotIndex=%d, ButtonPressed=%s, DragStarted=%s"),
		SlotIndex, bButtonPressed ? TEXT("true") : TEXT("false"), bDragStarted ? TEXT("true") : TEXT("false"));

	// If we initiated drag detection in NativeOnPreviewMouseButtonDown but the user
	// released before the drag threshold was exceeded, we need to manually fire the click
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bButtonPressed && !bDragStarted)
	{
		bButtonPressed = false;

		// Check if shift is held for quick transfer
		const bool bShiftHeld = InMouseEvent.IsShiftDown();

		if (CachedVisualData.bHasItem)
		{
			if (bShiftHeld)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventorySlot] Shift+Click on slot %d - quick transfer"), SlotIndex);
				OnSlotShiftClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Click on slot %d"), SlotIndex);
				OnSlotClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
			}
		}
		else
		{
			OnSlotClicked.Broadcast(SlotIndex, FGuid());
		}

		return FReply::Handled();
	}

	bButtonPressed = false;
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UMOInventorySlot::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnDragDetected! SlotIndex=%d, HasItem=%s"),
		SlotIndex, CachedVisualData.bHasItem ? TEXT("true") : TEXT("false"));

	if (!bEnableDragDrop || !CachedVisualData.bHasItem)
	{
		return;
	}

	bDragStarted = true;

	// Create the drag operation
	UMOInventorySlotDragOperation* DragOp = NewObject<UMOInventorySlotDragOperation>();
	DragOp->SourceInventoryComponent = InventoryComponent;
	DragOp->SourceWorldItem = SourceWorldItem;  // Set world item source (for nearby items)
	DragOp->SourceSlotIndex = SlotIndex;
	DragOp->ItemGuid = CachedVisualData.ItemGuid;
	DragOp->ItemDefinitionId = CachedVisualData.ItemDefinitionId;
	DragOp->Quantity = CachedVisualData.Quantity;

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Creating drag op: FromInventory=%s, FromWorldItem=%s"),
		DragOp->IsFromInventory() ? TEXT("yes") : TEXT("no"),
		DragOp->IsFromWorldItem() ? TEXT("yes") : TEXT("no"));

	// Create the drag visual - Unreal handles positioning automatically
	DragOp->DefaultDragVisual = CreateDragVisual();
	DragOp->Pivot = EDragPivot::CenterCenter;

	// Visual feedback on source slot
	if (IsValid(SlotBorder))
	{
		SlotBorder->SetBrushColor(DraggingBorderColor);
	}
	if (IsValid(SlotButton))
	{
		SlotButton->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 0.5f));
	}

	OutOperation = DragOp;

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drag operation created! Visual=%s"),
		IsValid(DragOp->DefaultDragVisual) ? TEXT("Valid") : TEXT("NULL"));
}

void UMOInventorySlot::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnDragCancelled! SlotIndex=%d"), SlotIndex);
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

	// Check if this is our drag operation
	UMOInventorySlotDragOperation* SlotDragOp = Cast<UMOInventorySlotDragOperation>(InOperation);
	if (IsValid(SlotDragOp) && SlotDragOp->SourceSlotIndex == SlotIndex)
	{
		// Restore visual state
		if (IsValid(SlotBorder))
		{
			SlotBorder->SetBrushColor(NormalBorderColor);
		}
		if (IsValid(SlotButton))
		{
			SlotButton->SetColorAndOpacity(FLinearColor::White);
		}

		// Try world drop if enabled
		if (bEnableWorldDrop && CachedVisualData.bHasItem)
		{
			TryDropIntoWorld();
		}
	}

	bDragStarted = false;
	bButtonPressed = false;
}

bool UMOInventorySlot::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnDrop called! TargetSlot=%d"), SlotIndex);

	SetDragHoverVisual(false);

	UMOInventorySlotDragOperation* SlotDragOp = Cast<UMOInventorySlotDragOperation>(InOperation);
	if (!SlotDragOp)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop failed - not a slot drag operation"));
		return false;
	}

	const int32 SourceSlot = SlotDragOp->SourceSlotIndex;
	const int32 TargetSlot = SlotIndex;
	const FGuid ItemGuid = SlotDragOp->ItemGuid;

	// Handle drag from world item (nearby items panel)
	if (SlotDragOp->IsFromWorldItem())
	{
		AMOWorldItem* DraggedWorldItem = SlotDragOp->SourceWorldItem.Get();
		if (!IsValid(DraggedWorldItem))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop failed - world item no longer valid"));
			return false;
		}

		// If target has no inventory, can't pick up
		if (!IsValid(InventoryComponent))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop failed - can't pick up world item to slot without inventory"));
			return false;
		}

		// Pick up the world item into the target inventory
		UMOItemComponent* ItemComp = DraggedWorldItem->GetItemComponent();
		if (!IsValid(ItemComp))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop failed - world item has no ItemComponent"));
			return false;
		}

		// Generate a new GUID for the picked up item
		FGuid NewItemGuid = FGuid::NewGuid();
		bool bAdded = InventoryComponent->AddItemByGuid(NewItemGuid, ItemComp->ItemDefinitionId, ItemComp->Quantity);

		if (bAdded)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInventorySlot] Picked up world item %s into inventory"),
				*DraggedWorldItem->GetName());

			// Destroy or hide the world item
			if (DraggedWorldItem->bDestroyAfterPickup)
			{
				DraggedWorldItem->Destroy();
			}
			else
			{
				DraggedWorldItem->SetActorHiddenInGame(true);
				DraggedWorldItem->SetActorEnableCollision(false);
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Failed to add world item to inventory (full?)"));
		}

		// Refresh this slot
		RefreshFromInventory();

		// Broadcast drop received so parent widgets can refresh (e.g., nearby items panel)
		// Using nullptr for SourceInventory since this came from a world item
		OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, nullptr);
		return true;
	}

	// Handle drag from inventory
	UMOInventoryComponent* SourceInventory = SlotDragOp->SourceInventoryComponent.Get();
	if (!IsValid(SourceInventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop failed - invalid source inventory"));
		return false;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop: Source=%d -> Target=%d, ItemGuid=%s"),
		SourceSlot, TargetSlot, *ItemGuid.ToString(EGuidFormats::Short));

	// Case 1: Target slot has no inventory (e.g., nearby items panel, equipment slots)
	// Either drop to world or just broadcast for parent widget to handle
	if (!IsValid(InventoryComponent))
	{
		// If world drop is disabled, just broadcast the event for parent handling (e.g., equipment panel)
		if (!bEnableWorldDrop)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInventorySlot] Target has no inventory, world drop disabled - broadcasting for parent handling"));
			OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, SourceInventory);
			return true;
		}

		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Target has no inventory, dropping item to world"));

		// Get drop location from player
		APlayerController* PC = GetOwningPlayer();
		if (IsValid(PC))
		{
			APawn* Pawn = PC->GetPawn();
			if (IsValid(Pawn))
			{
				FVector DropLocation = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 100.0f;
				FRotator DropRotation = FRotator(0, FMath::RandRange(0.0f, 360.0f), 0);

				AActor* DroppedActor = SourceInventory->DropItemByGuid(ItemGuid, DropLocation, DropRotation);
				if (IsValid(DroppedActor))
				{
					UE_LOG(LogMOFramework, Log, TEXT("[MOInventorySlot] Dropped item to world: %s"), *DroppedActor->GetName());
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Failed to drop item to world"));
				}
			}
		}

		// Also broadcast for any additional handling
		OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, SourceInventory);
		return true;
	}

	// Case 2: Same slot in same inventory, no action needed
	if (SourceInventory == InventoryComponent && SourceSlot == TargetSlot)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop on same slot, ignoring"));
		return true;
	}

	// Case 3: Same inventory - swap slots
	if (SourceInventory == InventoryComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Same inventory - swapping slots %d <-> %d"), SourceSlot, TargetSlot);
		InventoryComponent->SwapSlots(SourceSlot, TargetSlot);
	}
	// Case 4: Different inventories - transfer item
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Cross-inventory transfer: %s from %s to %s"),
			*ItemGuid.ToString(EGuidFormats::Short),
			*SourceInventory->GetOwner()->GetName(),
			*InventoryComponent->GetOwner()->GetName());

		// Transfer the item from source to target inventory
		bool bTransferred = SourceInventory->TransferItem(ItemGuid, InventoryComponent);

		if (bTransferred)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInventorySlot] Cross-inventory transfer successful"));
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Cross-inventory transfer failed (inventory full?)"));
		}

		// Also broadcast for any additional handling
		OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, SourceInventory);
	}

	// Refresh this slot's visual
	if (IsValid(InventoryComponent))
	{
		RefreshFromInventory();
	}

	return true;
}

void UMOInventorySlot::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnDragEnter called! SlotIndex=%d"), SlotIndex);
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (Cast<UMOInventorySlotDragOperation>(InOperation))
	{
		SetDragHoverVisual(true);
	}
}

void UMOInventorySlot::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnDragLeave called! SlotIndex=%d"), SlotIndex);
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	SetDragHoverVisual(false);
}

UUserWidget* UMOInventorySlot::CreateDragVisual()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] CreateDragVisual called, OwningPlayer=%s"),
		IsValid(GetOwningPlayer()) ? TEXT("Valid") : TEXT("NULL"));

	// Try to get the item icon
	UTexture2D* IconTexture = nullptr;
	if (CachedVisualData.bHasItem)
	{
		IconTexture = UMOItemDatabaseSettings::GetItemIconSmall(CachedVisualData.ItemDefinitionId);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Got icon from DataTable: %s"),
			IsValid(IconTexture) ? *IconTexture->GetName() : TEXT("NULL"));
	}

	if (!IsValid(IconTexture))
	{
		IconTexture = DefaultItemIcon;
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Using DefaultItemIcon: %s"),
			IsValid(IconTexture) ? *IconTexture->GetName() : TEXT("NULL"));
	}

	// Create the drag visual widget (now uses pure Slate, no Blueprint needed)
	UMODragVisualWidget* DragWidget = CreateWidget<UMODragVisualWidget>(GetOwningPlayer(), UMODragVisualWidget::StaticClass());
	if (!IsValid(DragWidget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOInventorySlot] Failed to create MODragVisualWidget"));
		return nullptr;
	}

	// Set icon BEFORE adding to viewport so RebuildWidget has the texture
	DragWidget->SetIcon(IconTexture);
	DragWidget->SetVisualSize(FVector2D(64.0f, 64.0f));

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Created DragWidget successfully"));
	return DragWidget;
}

void UMOInventorySlot::SetDragHoverVisual(bool bHovered)
{
	bIsDragHovered = bHovered;

	// Update border color for hover feedback
	if (IsValid(SlotBorder))
	{
		SlotBorder->SetBrushColor(bHovered ? HoverBorderColor : NormalBorderColor);
	}

	// Also tint the button slightly for extra feedback (optional)
	if (IsValid(SlotButton))
	{
		FLinearColor TintColor = bHovered ? FLinearColor(0.8f, 1.0f, 0.8f, 1.0f) : FLinearColor::White;
		SlotButton->SetColorAndOpacity(TintColor);
	}
}

void UMOInventorySlot::TryDropIntoWorld()
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOInventorySlot] TryDropIntoWorld: slot %d"), SlotIndex);

	// Cache values we need before any operations that might invalidate 'this'
	UMOInventoryComponent* InvComp = InventoryComponent;
	const int32 CachedSlotIndex = SlotIndex;
	const FGuid CachedItemGuid = CachedVisualData.ItemGuid;

	if (!IsValid(InvComp) || !CachedVisualData.bHasItem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: No inventory component or no item"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: No player controller"));
		return;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!IsValid(PlayerPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: No pawn"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: No world"));
		return;
	}

	// Get player forward direction (ignore pitch so items don't drop into ground/sky)
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	FRotator PlayerRotation = PlayerPawn->GetActorRotation();
	PlayerRotation.Pitch = 0.0f; // Flatten to horizontal

	// Random offset in front of player (150-250cm forward, -50 to +50cm sideways)
	const float ForwardDistance = FMath::RandRange(150.0f, 250.0f);
	const float SideOffset = FMath::RandRange(-50.0f, 50.0f);

	FVector ForwardDir = PlayerRotation.Vector();
	FVector RightDir = FRotationMatrix(PlayerRotation).GetScaledAxis(EAxis::Y);

	FVector DropBaseLocation = PlayerLocation + (ForwardDir * ForwardDistance) + (RightDir * SideOffset);

	// Trace down from above to find the ground
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(PlayerPawn);

	// Start trace from 200cm above the base location
	const FVector TraceStart = DropBaseLocation + FVector(0.0f, 0.0f, 200.0f);
	const FVector TraceEnd = DropBaseLocation - FVector(0.0f, 0.0f, 500.0f);

	FVector DropLocation = DropBaseLocation;

	if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		// Spawn 100cm above the ground surface
		DropLocation = HitResult.Location + FVector(0.0f, 0.0f, 100.0f);
	}
	else
	{
		// No ground found, just use base location + 100cm up
		DropLocation = DropBaseLocation + FVector(0.0f, 0.0f, 100.0f);
	}

	// Random rotation for variety
	const FRotator DropRotation(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

	// Call the inventory component to drop the item
	// NOTE: This may trigger UI rebuild which could destroy 'this' widget
	AActor* DroppedActor = InvComp->DropItemFromSlot(CachedSlotIndex, DropLocation, DropRotation);

	// At this point 'this' might be invalid if the UI was rebuilt, but DroppedActor should still be valid
	if (IsValid(DroppedActor))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOInventorySlot] TryDropIntoWorld: Dropped '%s' at %s"),
			*DroppedActor->GetName(), *DroppedActor->GetActorLocation().ToString());

		// Broadcast drop event so nearby panel can refresh
		// Use nullptr for SourceInventory to indicate "dropped to world"
		OnSlotDropReceived.Broadcast(CachedSlotIndex, CachedSlotIndex, nullptr);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: DroppedActor is null/invalid"));
	}
}
