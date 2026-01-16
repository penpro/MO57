#include "MOInventorySlot.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

#include "MOInventoryComponent.h"

UMOInventorySlot::UMOInventorySlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOInventorySlot::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(SlotButton))
	{
		SlotButton->OnClicked.RemoveAll(this);
		SlotButton->OnClicked.AddDynamic(this, &UMOInventorySlot::HandleSlotButtonClicked);
	}

	// Default: hide quantity visuals until we have real data.
	// This will only work if QuantityBox is correctly bound (name + Is Variable).
	if (IsValid(QuantityBox))
	{
		QuantityBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventorySlot] QuantityBox not bound. Ensure the widget is named exactly 'QuantityBox' and 'Is Variable' is enabled."));
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
		UTexture2D* DesiredTexture = CachedVisualData.bHasItem ? DefaultItemIcon : EmptySlotIcon;
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
	if (!CachedVisualData.bHasItem)
	{
		OnSlotClicked.Broadcast(SlotIndex, FGuid());
		return;
	}

	OnSlotClicked.Broadcast(SlotIndex, CachedVisualData.ItemGuid);
}
