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
	// Return the actual slot count from the inventory component
	// Only use MinimumVisibleSlotCount as fallback when no inventory is bound
	if (IsValid(InventoryComponent))
	{
		return InventoryComponent->GetSlotCount();
	}

	// No inventory bound - use minimum for placeholder display
	return MinimumVisibleSlotCount;
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
		ReleaseAllSlotsToPool();
		return;
	}

	// Release current slots to pool before rebuilding
	SlotsUniformGrid->ClearChildren();
	ReleaseAllSlotsToPool();

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
		UMOInventorySlot* NewSlotWidget = AcquireSlotWidget();
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

	// Release all slots to pool (handles delegate unbinding)
	ReleaseAllSlotsToPool();

	InventoryComponent = nullptr;

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryGrid] Grid cleared (Pooled=%d)"), PooledSlotWidgets.Num());
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryGrid] SetSlotVisualData: All checks passed, acquiring slots from pool..."));

	// Release existing slots to pool
	SlotsUniformGrid->ClearChildren();
	ReleaseAllSlotsToPool();

	// Create exactly the number of slots needed for the visual data (no padding)
	const int32 SlotCount = VisualData.Num();

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UMOInventorySlot* NewSlotWidget = AcquireSlotWidget();
		if (!IsValid(NewSlotWidget))
		{
			continue;
		}

		// Initialize without an inventory component
		NewSlotWidget->InitializeSlot(nullptr, SlotIndex);

		// Set visual data directly
		NewSlotWidget->SetVisualData(VisualData[SlotIndex]);

		NewSlotWidget->OnSlotClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotClicked);
		NewSlotWidget->OnSlotShiftClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotShiftClicked);
		NewSlotWidget->OnSlotRightClicked.AddDynamic(this, &UMOInventoryGrid::HandleSlotRightClicked);
		NewSlotWidget->OnSlotDropReceived.AddDynamic(this, &UMOInventoryGrid::HandleSlotDropReceived);

		const int32 RowIndex = (Columns > 0) ? (SlotIndex / Columns) : SlotIndex;
		const int32 ColumnIndex = (Columns > 0) ? (SlotIndex % Columns) : 0;

		SlotsUniformGrid->AddChildToUniformGrid(NewSlotWidget, RowIndex, ColumnIndex);
		SlotWidgets.Add(NewSlotWidget);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryGrid] SetSlotVisualData: Acquired %d slots (SlotWidgets=%d, Pooled=%d) from %d visual data entries"),
		SlotCount, SlotWidgets.Num(), PooledSlotWidgets.Num(), VisualData.Num());
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

// ============================================================================
// WIDGET POOLING
// ============================================================================

UMOInventorySlot* UMOInventoryGrid::AcquireSlotWidget()
{
	// Try to get from pool first
	if (PooledSlotWidgets.Num() > 0)
	{
		UMOInventorySlot* SlotWidget = PooledSlotWidgets.Pop();
		if (IsValid(SlotWidget))
		{
			SlotWidget->SetVisibility(ESlateVisibility::Visible);
			return SlotWidget;
		}
	}

	// Create new widget if pool is empty
	APlayerController* OwningPlayerController = GetOwningPlayer();
	if (!OwningPlayerController || !SlotWidgetClass)
	{
		return nullptr;
	}

	return CreateWidget<UMOInventorySlot>(OwningPlayerController, SlotWidgetClass);
}

void UMOInventoryGrid::ReleaseSlotWidget(UMOInventorySlot* SlotWidget)
{
	if (!IsValid(SlotWidget))
	{
		return;
	}

	// Unbind event delegates
	SlotWidget->OnSlotClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotClicked);
	SlotWidget->OnSlotShiftClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotShiftClicked);
	SlotWidget->OnSlotRightClicked.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotRightClicked);
	SlotWidget->OnSlotDropReceived.RemoveDynamic(this, &UMOInventoryGrid::HandleSlotDropReceived);

	// Hide and clear data
	SlotWidget->SetVisibility(ESlateVisibility::Collapsed);
	SlotWidget->ClearVisualData();

	// Return to pool
	PooledSlotWidgets.Add(SlotWidget);
}

void UMOInventoryGrid::ReleaseAllSlotsToPool()
{
	for (UMOInventorySlot* SlotWidget : SlotWidgets)
	{
		ReleaseSlotWidget(SlotWidget);
	}
	SlotWidgets.Reset();
}
