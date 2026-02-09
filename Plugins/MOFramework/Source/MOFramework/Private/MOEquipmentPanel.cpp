#include "MOEquipmentPanel.h"
#include "MOFramework.h"
#include "MOEquipmentComponent.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOInventoryDragDropOperation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

UMOEquipmentPanel::UMOEquipmentPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOEquipmentPanel::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMOEquipmentPanel::NativeDestruct()
{
	UnbindEquipmentEvents();
	Super::NativeDestruct();
}

void UMOEquipmentPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update swap progress bars
	if (EquipmentComponent.IsValid())
	{
		UpdateSwapProgress();
	}
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOEquipmentPanel::InitializePanel(UMOEquipmentComponent* InEquipment, UMOInventoryComponent* InInventory)
{
	UnbindEquipmentEvents();

	EquipmentComponent = InEquipment;
	InventoryComponent = InInventory;

	BindEquipmentEvents();
	RefreshDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOEquipmentPanel] Initialized with equipment component"));
}

void UMOEquipmentPanel::RefreshDisplay()
{
	UpdateSlotDisplay(EMOEquipmentSlot::LeftHand);
	UpdateSlotDisplay(EMOEquipmentSlot::RightHand);
}

void UMOEquipmentPanel::ClearPanel()
{
	UnbindEquipmentEvents();
	EquipmentComponent.Reset();
	InventoryComponent.Reset();

	// Reset to empty state
	if (LeftHandIcon)
	{
		LeftHandIcon->SetBrushFromTexture(EmptySlotIcon.LoadSynchronous());
		LeftHandIcon->SetColorAndOpacity(EmptySlotTint);
	}
	if (RightHandIcon)
	{
		RightHandIcon->SetBrushFromTexture(EmptySlotIcon.LoadSynchronous());
		RightHandIcon->SetColorAndOpacity(EmptySlotTint);
	}
}

// ============================================================================
// EVENTS
// ============================================================================

void UMOEquipmentPanel::BindEquipmentEvents()
{
	if (EquipmentComponent.IsValid())
	{
		EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &UMOEquipmentPanel::HandleEquipmentChanged);
		EquipmentComponent->OnSwapStarted.AddDynamic(this, &UMOEquipmentPanel::HandleSwapStarted);
		EquipmentComponent->OnSwapCompleted.AddDynamic(this, &UMOEquipmentPanel::HandleSwapCompleted);
	}
}

void UMOEquipmentPanel::UnbindEquipmentEvents()
{
	if (EquipmentComponent.IsValid())
	{
		EquipmentComponent->OnEquipmentChanged.RemoveDynamic(this, &UMOEquipmentPanel::HandleEquipmentChanged);
		EquipmentComponent->OnSwapStarted.RemoveDynamic(this, &UMOEquipmentPanel::HandleSwapStarted);
		EquipmentComponent->OnSwapCompleted.RemoveDynamic(this, &UMOEquipmentPanel::HandleSwapCompleted);
	}
}

void UMOEquipmentPanel::HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& Item)
{
	UpdateSlotDisplay(EquipSlot);
}

void UMOEquipmentPanel::HandleSwapStarted(EMOEquipmentSlot EquipSlot)
{
	// Show swap progress bar
	if (EquipSlot == EMOEquipmentSlot::LeftHand && LeftHandSwapProgress)
	{
		LeftHandSwapProgress->SetVisibility(ESlateVisibility::Visible);
	}
	else if (EquipSlot == EMOEquipmentSlot::RightHand && RightHandSwapProgress)
	{
		RightHandSwapProgress->SetVisibility(ESlateVisibility::Visible);
	}
}

void UMOEquipmentPanel::HandleSwapCompleted(EMOEquipmentSlot EquipSlot)
{
	// Hide swap progress bar
	if (EquipSlot == EMOEquipmentSlot::LeftHand && LeftHandSwapProgress)
	{
		LeftHandSwapProgress->SetVisibility(ESlateVisibility::Collapsed);
	}
	else if (EquipSlot == EMOEquipmentSlot::RightHand && RightHandSwapProgress)
	{
		RightHandSwapProgress->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ============================================================================
// DISPLAY UPDATE
// ============================================================================

void UMOEquipmentPanel::UpdateSlotDisplay(EMOEquipmentSlot EquipSlot)
{
	if (!EquipmentComponent.IsValid())
	{
		return;
	}

	const FMOEquippedItem Item = EquipmentComponent->GetEquippedItem(EquipSlot);
	const bool bIsSwapping = EquipmentComponent->IsSlotSwapping(EquipSlot);

	UImage* IconWidget = (EquipSlot == EMOEquipmentSlot::LeftHand) ? LeftHandIcon.Get() : RightHandIcon.Get();
	UTextBlock* LabelWidget = (EquipSlot == EMOEquipmentSlot::LeftHand) ? LeftHandLabel.Get() : RightHandLabel.Get();
	UProgressBar* DurabilityWidget = (EquipSlot == EMOEquipmentSlot::LeftHand) ? LeftHandDurability.Get() : RightHandDurability.Get();
	UBorder* BorderWidget = (EquipSlot == EMOEquipmentSlot::LeftHand) ? LeftHandSlotBorder.Get() : RightHandSlotBorder.Get();

	if (!IconWidget)
	{
		return;
	}

	if (Item.IsValid())
	{
		// Get item definition for icon
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Item.ItemDefinitionId, ItemDef))
		{
			// Set icon
			if (!ItemDef.UI.IconSmall.IsNull())
			{
				IconWidget->SetBrushFromTexture(ItemDef.UI.IconSmall.LoadSynchronous());
			}

			// Set label
			if (LabelWidget)
			{
				LabelWidget->SetText(ItemDef.DisplayName);
			}

			// Set durability
			if (DurabilityWidget && ItemDef.MaxDurability > 0)
			{
				const float DurabilityPercent = static_cast<float>(Item.CurrentDurability) / ItemDef.MaxDurability;
				DurabilityWidget->SetPercent(DurabilityPercent);
				DurabilityWidget->SetVisibility(ESlateVisibility::Visible);
			}
			else if (DurabilityWidget)
			{
				DurabilityWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		// Set tint based on state
		IconWidget->SetColorAndOpacity(bIsSwapping ? SwappingSlotTint : OccupiedSlotTint);
	}
	else
	{
		// Empty slot
		if (!EmptySlotIcon.IsNull())
		{
			IconWidget->SetBrushFromTexture(EmptySlotIcon.LoadSynchronous());
		}
		IconWidget->SetColorAndOpacity(EmptySlotTint);

		if (LabelWidget)
		{
			LabelWidget->SetText(EquipSlot == EMOEquipmentSlot::LeftHand
				? FText::FromString(TEXT("Left Hand"))
				: FText::FromString(TEXT("Right Hand")));
		}

		if (DurabilityWidget)
		{
			DurabilityWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMOEquipmentPanel::UpdateSwapProgress()
{
	if (!EquipmentComponent.IsValid())
	{
		return;
	}

	if (LeftHandSwapProgress && EquipmentComponent->IsSlotSwapping(EMOEquipmentSlot::LeftHand))
	{
		LeftHandSwapProgress->SetPercent(EquipmentComponent->GetSwapProgress(EMOEquipmentSlot::LeftHand));
	}

	if (RightHandSwapProgress && EquipmentComponent->IsSlotSwapping(EMOEquipmentSlot::RightHand))
	{
		RightHandSwapProgress->SetPercent(EquipmentComponent->GetSwapProgress(EMOEquipmentSlot::RightHand));
	}
}

// ============================================================================
// DRAG/DROP
// ============================================================================

bool UMOEquipmentPanel::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UMOInventoryDragDropOperation* DragOp = Cast<UMOInventoryDragDropOperation>(InOperation);
	if (!DragOp || !EquipmentComponent.IsValid())
	{
		return false;
	}

	// Determine which slot was dropped on
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	EMOEquipmentSlot TargetSlot = GetSlotAtPosition(InGeometry, LocalPos);

	// Broadcast the drop event
	OnSlotDropReceived.Broadcast(TargetSlot, DragOp->DraggedItemGuid, DragOp->SourceInventory);

	// Try to equip
	if (DragOp->SourceInventory)
	{
		EquipmentComponent->EquipFromInventory(DragOp->SourceInventory, DragOp->DraggedItemGuid, TargetSlot);
	}

	return true;
}

FReply UMOEquipmentPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!EquipmentComponent.IsValid())
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	EMOEquipmentSlot ClickedSlot = GetSlotAtPosition(InGeometry, LocalPos);
	FMOEquippedItem Item = EquipmentComponent->GetEquippedItem(ClickedSlot);

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Right-click: context menu
		OnSlotRightClicked.Broadcast(ClickedSlot, Item, InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	else if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Left-click: unequip if has item and inventory
		OnSlotClicked.Broadcast(ClickedSlot, Item);

		if (Item.IsValid() && InventoryComponent.IsValid())
		{
			EquipmentComponent->UnequipToInventory(ClickedSlot, InventoryComponent.Get());
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

EMOEquipmentSlot UMOEquipmentPanel::GetSlotAtPosition(const FGeometry& Geometry, const FVector2D& LocalPosition) const
{
	// Simple left/right split - left half = left hand, right half = right hand
	const FVector2D Size = Geometry.GetLocalSize();
	if (LocalPosition.X < Size.X * 0.5f)
	{
		return EMOEquipmentSlot::LeftHand;
	}
	return EMOEquipmentSlot::RightHand;
}
