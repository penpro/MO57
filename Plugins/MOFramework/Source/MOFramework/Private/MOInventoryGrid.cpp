#include "MOInventoryGrid.h"
#include "MOFramework.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Engine.h"

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
	// Unbind from old inventory if switching
	UnbindInventoryDelegates();

	InventoryComponent = InInventoryComponent;

	// Bind to new inventory's change events
	BindInventoryDelegates();

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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryGrid] Missing SlotsUniformGrid (BindWidget). Check the widget name and ensure it is marked as Variable."));
		return;
	}

	if (!SlotWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryGrid] SlotWidgetClass is not set. Set it in the WBP_InventoryGrid defaults to your WBP_InventorySlot."));
		SlotsUniformGrid->ClearChildren();
		SlotWidgets.Reset();
		return;
	}

	SlotsUniformGrid->ClearChildren();
	SlotWidgets.Reset();

	const int32 SlotCount = GetDesiredSlotCount();
	if (SlotCount <= 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryGrid] Computed SlotCount <= 0 (InventorySlotCount + MinimumVisibleSlotCount)."));
		return;
	}

	APlayerController* OwningPlayerController = GetOwningPlayer();
	if (!OwningPlayerController)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryGrid] No owning player yet. Grid will not build slots until it has an owning player."));
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
		NewSlotWidget->OnSlotShiftClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotShiftClicked);
		NewSlotWidget->OnSlotRightClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotRightClicked);
		NewSlotWidget->OnSlotDropReceived.AddDynamic(this, &UMOInventoryGrid::HandleSlotDropReceived);

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

void UMOInventoryGrid::ClearGrid()
{
	// Unbind from inventory change events
	UnbindInventoryDelegates();

	if (SlotsUniformGrid)
	{
		SlotsUniformGrid->ClearChildren();
	}

	// Unbind delegates from slots before clearing
	for (UMOInventorySlot* SlotWidget : SlotWidgets)
	{
		if (IsValid(SlotWidget))
		{
			SlotWidget->OnSlotClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotClicked);
			SlotWidget->OnSlotShiftClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotShiftClicked);
			SlotWidget->OnSlotRightClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotRightClicked);
			SlotWidget->OnSlotDropReceived.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotDropReceived);
		}
	}

	SlotWidgets.Reset();
	InventoryComponent = nullptr;

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryGrid] Grid cleared"));
}

void UMOInventoryGrid::SetSlotVisualData(const TArray<FMOInventorySlotVisualData>& VisualData)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryGrid] SetSlotVisualData called with %d entries"), VisualData.Num());

	if (!SlotsUniformGrid)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryGrid] SetSlotVisualData: SlotsUniformGrid is NULL (BindWidget not set?)"));
		return;
	}

	if (!SlotWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryGrid] SetSlotVisualData: SlotWidgetClass is NULL (not configured in blueprint?)"));
		return;
	}

	APlayerController* OwningPlayerController = GetOwningPlayer();
	if (!OwningPlayerController)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryGrid] SetSlotVisualData: No owning player"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryGrid] SetSlotVisualData: All checks passed, creating slots..."));

	// Clear existing slots
	SlotsUniformGrid->ClearChildren();

	for (UMOInventorySlot* SlotWidget : SlotWidgets)
	{
		if (IsValid(SlotWidget))
		{
			SlotWidget->OnSlotClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotClicked);
			SlotWidget->OnSlotShiftClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotShiftClicked);
			SlotWidget->OnSlotRightClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotRightClicked);
			SlotWidget->OnSlotDropReceived.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotDropReceived);
		}
	}
	SlotWidgets.Reset();

	// Create new slots based on visual data
	const int32 SlotCount = FMath::Max(VisualData.Num(), MinimumVisibleSlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UMOInventorySlot* NewSlotWidget = CreateWidget<UMOInventorySlot>(OwningPlayerController, SlotWidgetClass);
		if (!IsValid(NewSlotWidget))
		{
			continue;
		}

		// Initialize without an inventory component
		NewSlotWidget->InitializeSlot(nullptr, SlotIndex);

		// Set visual data directly if available
		if (SlotIndex < VisualData.Num())
		{
			NewSlotWidget->SetVisualData(VisualData[SlotIndex]);
		}
		else
		{
			NewSlotWidget->ClearVisualData();
		}

		NewSlotWidget->OnSlotClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotClicked);
		NewSlotWidget->OnSlotShiftClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotShiftClicked);
		NewSlotWidget->OnSlotRightClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotRightClicked);
		NewSlotWidget->OnSlotDropReceived.AddDynamic(this, &UMOInventoryGrid::HandleSlotDropReceived);

		const int32 RowIndex = (Columns > 0) ? (SlotIndex / Columns) : SlotIndex;
		const int32 ColumnIndex = (Columns > 0) ? (SlotIndex % Columns) : 0;

		SlotsUniformGrid->AddChildToUniformGrid(NewSlotWidget, RowIndex, ColumnIndex);
		SlotWidgets.Add(NewSlotWidget);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryGrid] SetSlotVisualData: Created %d slots (SlotWidgets=%d) from %d visual data entries"),
		SlotCount, SlotWidgets.Num(), VisualData.Num());
}

void UMOInventoryGrid::HandleSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	OnGridSlotClicked.Broadcast(SlotIndex, ItemGuid);
}

void UMOInventoryGrid::HandleSlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	OnGridSlotShiftClicked.Broadcast(SlotIndex, ItemGuid);
}

void UMOInventoryGrid::HandleSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition)
{
	OnGridSlotRightClicked.Broadcast(SlotIndex, ItemGuid, ScreenPosition);
}

void UMOInventoryGrid::HandleSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOInventoryGrid] HandleSlotDropReceived: Target=%d, Source=%d"), TargetSlotIndex, SourceSlotIndex);
	OnGridSlotDropReceived.Broadcast(TargetSlotIndex, SourceSlotIndex, SourceInventory);
}

void UMOInventoryGrid::HandleInventoryChanged()
{
	// Refresh all slots when inventory contents change
	RefreshAllSlots();
}

void UMOInventoryGrid::HandleSlotsChanged()
{
	// Rebuild grid when slot count changes
	RebuildGrid();
}

void UMOInventoryGrid::BindInventoryDelegates()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.AddDynamic(this, &UMOInventoryGrid::HandleInventoryChanged);
		InventoryComponent->OnSlotsChanged.AddDynamic(this, &UMOInventoryGrid::HandleSlotsChanged);
	}
}

void UMOInventoryGrid::UnbindInventoryDelegates()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UMOInventoryGrid::HandleInventoryChanged);
		InventoryComponent->OnSlotsChanged.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotsChanged);
	}
}
