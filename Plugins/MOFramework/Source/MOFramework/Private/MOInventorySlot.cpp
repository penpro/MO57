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
#include "MOEquipmentComponent.h"
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

	// Keep button visible and use its press/release events for drag detection
	if (IsValid(SlotButton))
	{
		SlotButton->OnClicked.RemoveAll(this);
		SlotButton->OnPressed.RemoveAll(this);
		SlotButton->OnReleased.RemoveAll(this);

		SlotButton->OnClicked.AddDynamic(this, &UMOInventorySlot::HandleSlotButtonClicked);
		SlotButton->OnPressed.AddDynamic(this, &UMOInventorySlot::HandleSlotButtonPressed);
		SlotButton->OnReleased.AddDynamic(this, &UMOInventorySlot::HandleSlotButtonReleased);
	}

	// Default: hide quantity visuals until we have real data.
	if (IsValid(QuantityBox))
	{
		QuantityBox->SetVisibility(ESlateVisibility::Collapsed);
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
			const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
			OnSlotRightClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid, ScreenPosition);
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMOInventorySlot::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// If we initiated drag detection in NativeOnPreviewMouseButtonDown but the user
	// released before the drag threshold was exceeded, we need to manually fire the click
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bButtonPressed && !bDragStarted)
	{
		bButtonPressed = false;

		// Check if shift is held for quick transfer
		const bool bShiftHeld = InMouseEvent.IsShiftDown();

		// Check for double-click (same slot clicked within threshold)
		const double CurrentTime = FPlatformTime::Seconds();
		const bool bIsDoubleClick = (SlotIndex == LastClickSlotIndex) &&
			((CurrentTime - LastClickTime) <= DoubleClickThreshold);

		// Update tracking for next click
		LastClickTime = CurrentTime;
		LastClickSlotIndex = SlotIndex;

		if (CachedVisualData.bHasItem)
		{
			// Double-click or shift-click triggers quick transfer
			if (bShiftHeld || bIsDoubleClick)
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

		return FReply::Handled();
	}

	bButtonPressed = false;
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
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
	if (IsValid(SlotBorder))
	{
		SlotBorder->SetBrushColor(DraggingBorderColor);
	}
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

		// Pick up the world item into the target inventory
		UMOItemComponent* ItemComp = DraggedWorldItem->GetItemComponent();
		if (!IsValid(ItemComp))
		{
			return false;
		}

		// Generate a new GUID for the picked up item
		FGuid NewItemGuid = FGuid::NewGuid();
		bool bAdded = InventoryComponent->AddItemByGuid(NewItemGuid, ItemComp->ItemDefinitionId, ItemComp->Quantity);

		if (bAdded)
		{
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

		// Refresh this slot
		RefreshFromInventory();

		// Broadcast drop received so parent widgets can refresh (e.g., nearby items panel)
		// Using nullptr for SourceInventory since this came from a world item
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

		// Unequip the item to the target inventory
		SourceEquipment->UnequipToInventory(EquipSlot, InventoryComponent);

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

		// Get drop location from player
		APlayerController* PC = GetOwningPlayer();
		if (IsValid(PC))
		{
			APawn* Pawn = PC->GetPawn();
			if (IsValid(Pawn))
			{
				FVector DropLocation = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 100.0f;
				FRotator DropRotation = FRotator(0, FMath::RandRange(0.0f, 360.0f), 0);
				SourceInventory->DropItemByGuid(ItemGuid, DropLocation, DropRotation);
			}
		}

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
		InventoryComponent->SwapSlots(SourceSlot, TargetSlot);
	}
	// Case 4: Different inventories - transfer item
	else
	{
		SourceInventory->TransferItem(ItemGuid, InventoryComponent);
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
