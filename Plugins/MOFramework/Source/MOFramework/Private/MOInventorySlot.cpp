#include "MOInventorySlot.h"
#include "MOInventoryRequest.h"
#include "MOUIDebugSubsystem.h"
#include "MOUITokens.h"
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
#include "MOEquipmentComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MODragVisualWidget.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOInteractorComponent.h"
#include "GameFramework/Pawn.h"
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

void UMOInventorySlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// The icon's visibility as authored, read once. It used to be re-read on every NativeConstruct -- and a slot widget is constructed again whenever
	// the grid re-adds it. A widget re-added while it showed an EMPTY slot (icon Collapsed) then took Collapsed as its "visible" state, and every item
	// it showed afterwards had a count but no icon (Steam playtest Oct 16 2026: "after eating berries inventory icons disappear" -- the last berry
	// emptied a slot, the grid was rebuilt, the reused widgets kept the collapsed icon).
	if (IsValid(ItemIconImage))
	{
		const ESlateVisibility Authored = ItemIconImage->GetVisibility();
		IconDefaultVisibility = (Authored == ESlateVisibility::Collapsed || Authored == ESlateVisibility::Hidden) ? ESlateVisibility::SelfHitTestInvisible : Authored;
	}
}

void UMOInventorySlot::NativeConstruct()
{
	Super::NativeConstruct();

	// CRITICAL: Set the button to HitTestInvisible so it doesn't consume mouse events.
	// All mouse input is handled at the UMOInventorySlot level via native widget events.
	// This prevents the UButton from eating events and allows proper double-click detection.
	if (IsValid(SlotButton))
	{
		SlotButton->SetVisibility(ESlateVisibility::HitTestInvisible);
		// Clear any existing bindings (safety measure)
		SlotButton->OnClicked.RemoveAll(this);
		SlotButton->OnPressed.RemoveAll(this);
		SlotButton->OnReleased.RemoveAll(this);
	}

	// Default: hide quantity visuals until we have real data.
	if (IsValid(QuantityBox))
	{
		QuantityBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	// The inner border of the slot chrome (not a BindWidget: looked up by name so older slot WBPs without it still work).
	SlotTicksBorder = Cast<UBorder>(GetWidgetFromName(TEXT("SlotBorderBorder")));

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
	CachedVisualData = InVisualData;

	// Extract world item reference for drag operations
	SourceWorldItem = InVisualData.SourceWorldItem;

	// Extract equipment source for drag operations
	SourceEquipmentComponent = InVisualData.SourceEquipmentComponent;
	SourceEquipmentSlot = InVisualData.SourceEquipmentSlot;

	ApplyVisualDataToWidget();
	OnVisualDataUpdated(CachedVisualData);
}

void UMOInventorySlot::ClearVisualData()
{
	CachedVisualData = FMOInventorySlotVisualData();
	SourceWorldItem.Reset();
	// Note: Equipment source is intentionally NOT cleared here
	// Equipment slots call SetEquipmentSource after ClearVisualData to maintain their identity
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

void UMOInventorySlot::SetEquipmentSource(UMOEquipmentComponent* InEquipment, EMOEquipmentSlot InSlot)
{
	SourceEquipmentComponent = InEquipment;
	SourceEquipmentSlot = InSlot;
}

UMOEquipmentComponent* UMOInventorySlot::GetSourceEquipmentComponent() const
{
	return SourceEquipmentComponent.Get();
}

EMOEquipmentSlot UMOInventorySlot::GetSourceEquipmentSlot() const
{
	return SourceEquipmentSlot;
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

		// An empty slot shows the kit's corner ticks only. The generic weave icon is hidden; a slot-specific
		// silhouette (equipment slots set one via SetCustomEmptyIcon) stays, dimmed.
		const bool bShowSilhouette = !CachedVisualData.bHasItem && IsValid(CustomEmptyIcon);
		const bool bShowIcon = CachedVisualData.bHasItem || bShowSilhouette;
		ItemIconImage->SetVisibility(bShowIcon ? IconDefaultVisibility : ESlateVisibility::Collapsed);
		ItemIconImage->SetColorAndOpacity(bShowSilhouette ? FLinearColor(1.0f, 1.0f, 1.0f, 0.4f) : FLinearColor::White);
	}

	ApplySlotLook();
}

void UMOInventorySlot::ApplySlotLook()
{
	// Priority: the source of a drag, then a drop target under the cursor, then plain mouse hover.
	FName FillToken = TEXT("color.surface.2");
	FName TickToken = CachedVisualData.bHasItem ? FName(TEXT("color.border.subtle")) : FName(TEXT("color.border.strong"));
	if (bIsBeingDragged)
	{
		FillToken = TEXT("color.surface.1");
		TickToken = TEXT("color.border.subtle");
	}
	else if (bIsDragHovered)
	{
		FillToken = TEXT("color.accent.muted");
		TickToken = TEXT("color.accent");
	}
	else if (bIsMouseHovered)
	{
		FillToken = TEXT("color.surface.3");
		TickToken = TEXT("color.accent");
	}

	if (IsValid(SlotBorder))
	{
		SlotBorder->SetBrush(UMOUITokens::NineSliceBrush(TEXT("slot_fill"), FillToken));
		SlotBorder->SetBrushColor(FLinearColor::White);   // the tint lives in the brush; BrushColor would multiply it
	}
	if (IsValid(SlotTicksBorder))
	{
		SlotTicksBorder->SetBrush(UMOUITokens::NineSliceBrush(TEXT("slot_ticks"), TickToken));
		SlotTicksBorder->SetBrushColor(FLinearColor::White);
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
	const bool bWasDragging = bDragStarted;
	bButtonPressed = false;
	bDragStarted = false;

	// Restore visual state if we were dragging
	if (bWasDragging)
	{
		bIsBeingDragged = false;
		ApplySlotLook();
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
}

FReply UMOInventorySlot::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Preview gets called BEFORE child widgets see the event (tunneling phase)
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bButtonPressed = true;
		bDragStarted = false;
		PressedMousePosition = InMouseEvent.GetScreenSpacePosition();

		// Enable drag detection for items (NativeOnMouseButtonDoubleClick handles double-click separately)
		if (bEnableDragDrop && CachedVisualData.bHasItem)
		{
			return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		}

		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMOInventorySlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	MOUI_LOG(this, "Slot", "NativeOnMouseButtonDown slot=%d button=%s hasItem=%s",
		SlotIndex, *InMouseEvent.GetEffectingButton().ToString(),
		CachedVisualData.bHasItem ? TEXT("YES") : TEXT("no"));

	// Handle right-click for context menu
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (CachedVisualData.bHasItem)
		{
			const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
			OnSlotRightClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid, ScreenPosition);
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMOInventorySlot::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Handle left mouse button release
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const bool bWasPressed = bButtonPressed;
		const bool bWasDragging = bDragStarted;
		bButtonPressed = false;

		// If button was pressed and no drag occurred, this is a click
		if (bWasPressed && !bWasDragging)
		{
			// Check if shift is held for quick transfer
			const bool bShiftHeld = InMouseEvent.IsShiftDown();

			if (CachedVisualData.bHasItem)
			{
				if (bShiftHeld)
				{
					OnSlotShiftClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
				}
				else
				{
					OnSlotClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
				}
			}
			else
			{
				OnSlotClicked.Broadcast(SlotIndex, FGuid());
			}
		}

		return FReply::Handled();
	}

	bButtonPressed = false;
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UMOInventorySlot::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Double-click triggers quick transfer (same as Shift+Click)
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (CachedVisualData.bHasItem)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[Slot %d] Double-click: transferring item %s"),
				SlotIndex, *CachedVisualData.ItemDefinitionId.ToString());
			OnSlotShiftClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
		}
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UMOInventorySlot::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!bEnableDragDrop || !CachedVisualData.bHasItem)
	{
		return;
	}

	bDragStarted = true;

	// Create the drag operation
	UMOInventorySlotDragOperation* DragOp = NewObject<UMOInventorySlotDragOperation>();
	DragOp->SourceInventoryComponent = InventoryComponent;
	DragOp->SourceWorldItem = SourceWorldItem;
	DragOp->SourceEquipmentComponent = SourceEquipmentComponent;
	DragOp->SourceEquipmentSlot = SourceEquipmentSlot;
	DragOp->SourceSlotIndex = SlotIndex;
	DragOp->ItemGuid = CachedVisualData.ItemGuid;
	DragOp->ItemDefinitionId = CachedVisualData.ItemDefinitionId;
	DragOp->Quantity = CachedVisualData.Quantity;

	// Create the drag visual - Unreal handles positioning automatically
	DragOp->DefaultDragVisual = CreateDragVisual();
	DragOp->Pivot = EDragPivot::CenterCenter;

	// Visual feedback on source slot
	bIsBeingDragged = true;
	ApplySlotLook();
	if (IsValid(SlotButton))
	{
		SlotButton->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 0.5f));
	}

	OutOperation = DragOp;
}

void UMOInventorySlot::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

	// Check if this is our drag operation
	UMOInventorySlotDragOperation* SlotDragOp = Cast<UMOInventorySlotDragOperation>(InOperation);
	if (IsValid(SlotDragOp) && SlotDragOp->SourceSlotIndex == SlotIndex)
	{
		// Restore visual state
		bIsBeingDragged = false;
		ApplySlotLook();
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
	SetDragHoverVisual(false);

	UMOInventorySlotDragOperation* SlotDragOp = Cast<UMOInventorySlotDragOperation>(InOperation);
	if (!SlotDragOp)
	{
		return false;
	}

	const int32 SourceSlot = SlotDragOp->SourceSlotIndex;
	const int32 TargetSlot = SlotIndex;
	const FGuid ItemGuid = SlotDragOp->ItemGuid;

	// Handle drag from world item (nearby items panel)
	if (SlotDragOp->IsFromWorldItem())
	{
		AMOWorldItem* DraggedWorldItem = SlotDragOp->SourceWorldItem.Get();
		if (!IsValid(DraggedWorldItem) || !IsValid(InventoryComponent))
		{
			return false;
		}

		// Pick up the world item through the canonical authoritative path: the pawn's
		// interactor Server RPC preserves the world-item GUID into the inventory entry
		// and replicates the actor's disappearance -- no client-side GUID mint, no
		// non-replicated Destroy/Hide (H21).
		UMOItemComponent* ItemComp = DraggedWorldItem->GetItemComponent();
		if (!IsValid(ItemComp))
		{
			return false;
		}

		if (APawn* OwningPawn = GetOwningPlayerPawn())
		{
			if (UMOInteractorComponent* Interactor = OwningPawn->FindComponentByClass<UMOInteractorComponent>())
			{
				Interactor->RequestInteractWithActor(DraggedWorldItem);
			}
		}

		// Inventory/world-item replication drives the actual update; refresh the view.
		RefreshFromInventory();

		// Broadcast drop received so parent widgets can refresh (e.g., nearby items panel)
		OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, nullptr);
		return true;
	}

	// Handle drag from equipment slot
	if (SlotDragOp->IsFromEquipment())
	{
		UMOEquipmentComponent* SourceEquipment = SlotDragOp->SourceEquipmentComponent.Get();
		EMOEquipmentSlot EquipSlot = SlotDragOp->SourceEquipmentSlot;

		if (!IsValid(SourceEquipment))
		{
			return false;
		}

		// If target has no inventory, we can't unequip here
		if (!IsValid(InventoryComponent))
		{
			// Broadcast drop event so parent widget (equipment panel) can handle equipment-to-equipment swaps
			OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, nullptr);
			return true;
		}

		// Unequip the item to the target inventory -- through the inventory request (server-side on a joiner)
		FMOInventoryRequest Request = FMOInventoryRequest::Make(EMOInventoryOp::Unequip, nullptr, InventoryComponent);
		Request.EquipSlot = static_cast<uint8>(EquipSlot);
		FMOInventoryRequests::Submit(GetOwningPlayer(), Request);

		// Refresh this slot
		RefreshFromInventory();

		// Broadcast drop received
		OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, nullptr);
		return true;
	}

	// Handle drag from inventory
	UMOInventoryComponent* SourceInventory = SlotDragOp->SourceInventoryComponent.Get();
	if (!IsValid(SourceInventory))
	{
		return false;
	}

	// Case 1: Target slot has no inventory (e.g., nearby items panel, equipment slots)
	// Either drop to world or just broadcast for parent widget to handle
	if (!IsValid(InventoryComponent))
	{
		// If world drop is disabled, just broadcast the event for parent handling (e.g., equipment panel)
		if (!bEnableWorldDrop)
		{
			OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, SourceInventory);
			return true;
		}

		// Drop to the ground: the server places it in front of the pawn (FMOInventoryRequests::ComputeDropTransform)
		FMOInventoryRequests::Submit(GetOwningPlayer(), FMOInventoryRequest::Make(EMOInventoryOp::Drop, SourceInventory, nullptr, ItemGuid));

		OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, SourceInventory);
		return true;
	}

	// Case 2: Same slot in same inventory, no action needed
	if (SourceInventory == InventoryComponent && SourceSlot == TargetSlot)
	{
		return true;
	}

	// Case 3: Same inventory - swap slots
	if (SourceInventory == InventoryComponent)
	{
		FMOInventoryRequest Request = FMOInventoryRequest::Make(EMOInventoryOp::Swap, InventoryComponent);
		Request.SlotA = SourceSlot;
		Request.SlotB = TargetSlot;
		FMOInventoryRequests::Submit(GetOwningPlayer(), Request);
	}
	// Case 4: Different inventories - transfer item
	else
	{
		FMOInventoryRequests::Submit(GetOwningPlayer(), FMOInventoryRequest::Make(EMOInventoryOp::Transfer, SourceInventory, InventoryComponent, ItemGuid));
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
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (Cast<UMOInventorySlotDragOperation>(InOperation))
	{
		SetDragHoverVisual(true);
	}
}

void UMOInventorySlot::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	SetDragHoverVisual(false);
}

void UMOInventorySlot::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	bIsMouseHovered = true;

	ApplySlotLook();
}

void UMOInventorySlot::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bIsMouseHovered = false;

	ApplySlotLook();
}

UUserWidget* UMOInventorySlot::CreateDragVisual()
{
	// Try to get the item icon
	UTexture2D* IconTexture = nullptr;
	if (CachedVisualData.bHasItem)
	{
		IconTexture = UMOItemDatabaseSettings::GetItemIconSmall(CachedVisualData.ItemDefinitionId);
	}

	if (!IsValid(IconTexture))
	{
		IconTexture = DefaultItemIcon;
	}

	// Create the drag visual widget (now uses pure Slate, no Blueprint needed)
	UMODragVisualWidget* DragWidget = CreateWidget<UMODragVisualWidget>(GetOwningPlayer(), UMODragVisualWidget::StaticClass());
	if (!IsValid(DragWidget))
	{
		return nullptr;
	}

	// Set icon BEFORE adding to viewport so RebuildWidget has the texture
	DragWidget->SetIcon(IconTexture);
	DragWidget->SetVisualSize(FVector2D(64.0f, 64.0f));

	return DragWidget;
}

void UMOInventorySlot::SetDragHoverVisual(bool bHovered)
{
	bIsDragHovered = bHovered;

	ApplySlotLook();

	// Also tint the button slightly for extra feedback (optional)
	if (IsValid(SlotButton))
	{
		FLinearColor TintColor = bHovered ? FLinearColor(0.8f, 1.0f, 0.8f, 1.0f) : FLinearColor::White;
		SlotButton->SetColorAndOpacity(TintColor);
	}
}

void UMOInventorySlot::TryDropIntoWorld()
{
	// Cache values we need before any operations that might invalidate 'this'
	UMOInventoryComponent* InvComp = InventoryComponent;
	const int32 CachedSlotIndex = SlotIndex;
	const FGuid CachedItemGuid = CachedVisualData.ItemGuid;

	if (!IsValid(InvComp) || !CachedVisualData.bHasItem)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		return;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!IsValid(PlayerPawn))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
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
		// Broadcast drop event so nearby panel can refresh
		OnSlotDropReceived.Broadcast(CachedSlotIndex, CachedSlotIndex, nullptr);
	}
}
