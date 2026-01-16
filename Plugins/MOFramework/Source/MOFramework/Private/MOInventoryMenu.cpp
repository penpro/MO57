#include "MOInventoryMenu.h"

#include "Input/Reply.h"
#include "InputCoreTypes.h"

#include "MOInventoryComponent.h"
#include "MOInventoryGrid.h"
#include "MOItemInfoPanel.h"

UMOInventoryMenu::UMOInventoryMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOInventoryMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure Tab input is received.
	SetKeyboardFocus();
}

void UMOInventoryMenu::NativeDestruct()
{
	if (IsValid(InventoryComponent))
	{
		// These delegate names assume you exposed them as BlueprintAssignable.
		// If your names differ, update them here.
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UMOInventoryMenu::HandleInventoryChanged);
		InventoryComponent->OnSlotsChanged.RemoveDynamic(this, &UMOInventoryMenu::HandleSlotsChanged);
	}

	Super::NativeDestruct();
}

void UMOInventoryMenu::InitializeMenu(UMOInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;

	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.AddDynamic(this, &UMOInventoryMenu::HandleInventoryChanged);
		InventoryComponent->OnSlotsChanged.AddDynamic(this, &UMOInventoryMenu::HandleSlotsChanged);
	}

	if (InventoryGrid)
	{
		InventoryGrid->InitializeGrid(InventoryComponent);
		InventoryGrid->OnGridSlotClicked.AddDynamic(this, &UMOInventoryMenu::HandleGridSlotClicked);
	}

	if (ItemInfoPanel)
	{
		ItemInfoPanel->InitializePanel(InventoryComponent);
		ItemInfoPanel->SetSelectedItemGuid(FGuid());
	}

	RefreshAll();
}

FReply UMOInventoryMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::Tab)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOInventoryMenu::HandleInventoryChanged()
{
	RefreshAll();
}

void UMOInventoryMenu::HandleSlotsChanged()
{
	if (InventoryGrid)
	{
		InventoryGrid->RebuildGrid();
	}

	RefreshAll();
}

void UMOInventoryMenu::HandleGridSlotClicked(int32 /*SlotIndex*/, const FGuid& ItemGuid)
{
	SelectedItemGuid = ItemGuid;

	if (ItemInfoPanel)
	{
		ItemInfoPanel->SetSelectedItemGuid(SelectedItemGuid);
	}
}

void UMOInventoryMenu::RefreshAll()
{
	if (InventoryGrid)
	{
		// If slot count can change at runtime, call RebuildGrid when needed.
		// For now, refresh is typically enough.
		InventoryGrid->RefreshAllSlots();
	}

	if (ItemInfoPanel)
	{
		ItemInfoPanel->SetSelectedItemGuid(SelectedItemGuid);
	}
}
