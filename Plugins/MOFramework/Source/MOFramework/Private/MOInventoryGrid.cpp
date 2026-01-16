#include "MOInventoryGrid.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

#include "MOInventoryComponent.h"
#include "MOInventorySlot.h"

UMOInventoryGrid::UMOInventoryGrid(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOInventoryGrid::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMOInventoryGrid::InitializeGrid(UMOInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
	RebuildGrid();
}

int32 UMOInventoryGrid::GetDesiredSlotCount() const
{
	int32 SlotCountFromInventory = 0;

	if (IsValid(InventoryComponent))
	{
		SlotCountFromInventory = InventoryComponent->GetSlotCount();
	}

	// Always show at least MinimumVisibleSlotCount so the user sees inventory size even when empty.
	const int32 Desired = FMath::Max(SlotCountFromInventory, MinimumVisibleSlotCount);
	return Desired;
}

void UMOInventoryGrid::RebuildGrid()
{
	if (!SlotsUniformGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventoryGrid] Missing SlotsUniformGrid (BindWidget). Check the widget name and ensure it is marked as Variable."));
		return;
	}

	if (!SlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventoryGrid] SlotWidgetClass is not set. Set it in the WBP_InventoryGrid defaults to your WBP_InventorySlot."));
		SlotsUniformGrid->ClearChildren();
		SlotWidgets.Reset();
		return;
	}

	SlotsUniformGrid->ClearChildren();
	SlotWidgets.Reset();

	const int32 SlotCount = GetDesiredSlotCount();
	if (SlotCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventoryGrid] Computed SlotCount <= 0 (InventorySlotCount + MinimumVisibleSlotCount)."));
		return;
	}

	APlayerController* OwningPlayerController = GetOwningPlayer();
	if (!OwningPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventoryGrid] No owning player yet. Grid will not build slots until it has an owning player."));
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UMOInventorySlot* NewSlotWidget = CreateWidget<UMOInventorySlot>(OwningPlayerController, SlotWidgetClass);
		if (!IsValid(NewSlotWidget))
		{
			continue;
		}

		NewSlotWidget->InitializeSlot(InventoryComponent, SlotIndex);
		NewSlotWidget->OnSlotClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotClicked);

		const int32 RowIndex = (Columns > 0) ? (SlotIndex / Columns) : SlotIndex;
		const int32 ColumnIndex = (Columns > 0) ? (SlotIndex % Columns) : 0;

		UUniformGridSlot* GridSlot = SlotsUniformGrid->AddChildToUniformGrid(NewSlotWidget, RowIndex, ColumnIndex);
		(void)GridSlot;

		SlotWidgets.Add(NewSlotWidget);
	}
}

void UMOInventoryGrid::RefreshAllSlots()
{
	for (UMOInventorySlot* SlotWidget : SlotWidgets)
	{
		if (IsValid(SlotWidget))
		{
			SlotWidget->RefreshFromInventory();
		}
	}
}

void UMOInventoryGrid::HandleSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	OnGridSlotClicked.Broadcast(SlotIndex, ItemGuid);
}
