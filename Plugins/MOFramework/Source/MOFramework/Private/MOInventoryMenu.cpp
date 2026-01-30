#include "MOInventoryMenu.h"

#include "Input/Reply.h"
#include "InputCoreTypes.h"

#include "MOInventoryComponent.h"
#include "MOInventoryGrid.h"
#include "MOItemInfoPanel.h"
#include "MOFramework.h"

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

	UE_LOG(LogMOFramework, Warning, TEXT("[InventoryMenu] InitializeMenu - InventoryComponent=%s, InventoryGrid=%s, ItemInfoPanel=%s"),
		IsValid(InventoryComponent) ? TEXT("valid") : TEXT("NULL"),
		InventoryGrid ? TEXT("valid") : TEXT("NULL"),
		ItemInfoPanel ? TEXT("valid") : TEXT("NULL"));

	if (IsValid(InventoryComponent))
	{
		// Remove any existing bindings first to prevent duplicates
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UMOInventoryMenu::HandleInventoryChanged);
		InventoryComponent->OnSlotsChanged.RemoveDynamic(this, &UMOInventoryMenu::HandleSlotsChanged);
		InventoryComponent->OnInventoryChanged.AddDynamic(this, &UMOInventoryMenu::HandleInventoryChanged);
		InventoryComponent->OnSlotsChanged.AddDynamic(this, &UMOInventoryMenu::HandleSlotsChanged);
	}

	if (InventoryGrid)
	{
		// Remove any existing bindings first to prevent duplicates
		InventoryGrid->OnGridSlotClicked.RemoveDynamic(this, &UMOInventoryMenu::HandleGridSlotClicked);
		InventoryGrid->OnGridSlotRightClicked.RemoveDynamic(this, &UMOInventoryMenu::HandleGridSlotRightClicked);
		InventoryGrid->InitializeGrid(InventoryComponent);
		InventoryGrid->OnGridSlotClicked.AddDynamic(this, &UMOInventoryMenu::HandleGridSlotClicked);
		InventoryGrid->OnGridSlotRightClicked.AddDynamic(this, &UMOInventoryMenu::HandleGridSlotRightClicked);
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

void UMOInventoryMenu::HandleGridSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[InventoryMenu] HandleGridSlotClicked - SlotIndex=%d, ItemGuid=%s, ItemInfoPanel=%s"),
		SlotIndex,
		*ItemGuid.ToString(EGuidFormats::Short),
		ItemInfoPanel ? TEXT("valid") : TEXT("NULL"));

	SelectedItemGuid = ItemGuid;

	if (ItemInfoPanel)
	{
		ItemInfoPanel->SetSelectedItemGuid(SelectedItemGuid);
	}
}

void UMOInventoryMenu::HandleGridSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition)
{
	// Forward to whoever is listening (typically UIManager)
	OnSlotRightClicked.Broadcast(SlotIndex, ItemGuid, ScreenPosition);
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
