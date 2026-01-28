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

	RefreshFromInventory();
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
		UTexture2D* DesiredTexture = EmptySlotIcon;

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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] NativeOnMouseButtonUp - Firing manual click for slot %d"), SlotIndex);
		bButtonPressed = false;

		// Fire the click event
		if (CachedVisualData.bHasItem)
		{
			OnSlotClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
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

	UMOInventoryComponent* SourceInventory = SlotDragOp->SourceInventoryComponent.Get();
	if (!IsValid(SourceInventory) || !IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop failed - invalid inventory components"));
		return false;
	}

	const int32 SourceSlot = SlotDragOp->SourceSlotIndex;
	const int32 TargetSlot = SlotIndex;

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop: Source=%d -> Target=%d"), SourceSlot, TargetSlot);

	// Same slot, no action needed
	if (SourceInventory == InventoryComponent && SourceSlot == TargetSlot)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Drop on same slot, ignoring"));
		return true;
	}

	// Same inventory: swap slots
	if (SourceInventory == InventoryComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Swapping slots %d <-> %d"), SourceSlot, TargetSlot);
		InventoryComponent->SwapSlots(SourceSlot, TargetSlot);
	}
	else
	{
		// Different inventory - broadcast event for game-specific handling
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] Cross-inventory drop, broadcasting event"));
		OnSlotDropReceived.Broadcast(TargetSlot, SourceSlot, SourceInventory);
	}

	// Refresh slots
	RefreshFromInventory();

	// Note: Source slot visual restoration happens in NativeOnDragCancelled or
	// the source slot's own handling when it detects the drag ended

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
	UE_LOG(LogMOFramework, Error, TEXT(">>>>> [MOInventorySlot] TryDropIntoWorld: CALLED for slot %d <<<<<"), SlotIndex);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("TryDropIntoWorld CALLED slot %d"), SlotIndex));
	}

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

	UE_LOG(LogMOFramework, Error, TEXT(">>>>> [MOInventorySlot] Player at %s, rotation %s <<<<<"),
		*PlayerLocation.ToString(), *PlayerRotation.ToString());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("Player at %s"), *PlayerLocation.ToString()));
	}

	PlayerRotation.Pitch = 0.0f; // Flatten to horizontal

	// Random offset in front of player (150-250cm forward, -50 to +50cm sideways)
	const float ForwardDistance = FMath::RandRange(150.0f, 250.0f);
	const float SideOffset = FMath::RandRange(-50.0f, 50.0f);

	FVector ForwardDir = PlayerRotation.Vector();
	FVector RightDir = FRotationMatrix(PlayerRotation).GetScaledAxis(EAxis::Y);

	FVector DropBaseLocation = PlayerLocation + (ForwardDir * ForwardDistance) + (RightDir * SideOffset);

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: ForwardDir=%s, RightDir=%s, DropBase=%s"),
		*ForwardDir.ToString(), *RightDir.ToString(), *DropBaseLocation.ToString());

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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: Ground trace hit at %s, DropLocation=%s"),
			*HitResult.Location.ToString(), *DropLocation.ToString());
	}
	else
	{
		// No ground found, just use base location + 100cm up
		DropLocation = DropBaseLocation + FVector(0.0f, 0.0f, 100.0f);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: No ground hit, using DropLocation=%s"),
			*DropLocation.ToString());
	}

	// Random rotation for variety
	const FRotator DropRotation(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

	UE_LOG(LogMOFramework, Error, TEXT(">>>>> [MOInventorySlot] FINAL DROP Location=%s <<<<<"), *DropLocation.ToString());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("FINAL DROP at %s"), *DropLocation.ToString()));
	}

	// Call the inventory component to drop the item
	// NOTE: This may trigger UI rebuild which could destroy 'this' widget
	AActor* DroppedActor = InvComp->DropItemFromSlot(CachedSlotIndex, DropLocation, DropRotation);

	// At this point 'this' might be invalid if the UI was rebuilt, but DroppedActor should still be valid
	if (IsValid(DroppedActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: Dropped actor '%s' at location %s"),
			*DroppedActor->GetName(), *DroppedActor->GetActorLocation().ToString());

		// Enable drop physics on the world item
		if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(DroppedActor))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: Calling EnableDropPhysics on %s"), *WorldItem->GetName());
			WorldItem->EnableDropPhysics();
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: DroppedActor is NOT an AMOWorldItem"));
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventorySlot] TryDropIntoWorld: DroppedActor is null/invalid"));
	}
}
