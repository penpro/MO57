#include "MOEquipmentPanel.h"
#include "MOFramework.h"
#include "MOEquipmentComponent.h"
#include "MOInventoryComponent.h"
#include "MOInventorySlot.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "Components/ProgressBar.h"

UMOEquipmentPanel::UMOEquipmentPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOEquipmentPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Hide swap progress bars initially
	if (LeftHandSwapProgress)
	{
		LeftHandSwapProgress->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (RightHandSwapProgress)
	{
		RightHandSwapProgress->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMOEquipmentPanel::NativeDestruct()
{
	UnbindSlotEvents();
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
	UnbindSlotEvents();
	UnbindEquipmentEvents();

	EquipmentComponent = InEquipment;
	InventoryComponent = InInventory;

	BindEquipmentEvents();
	BindSlotEvents();
	RefreshDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOEquipmentPanel] Initialized with equipment component"));
}

void UMOEquipmentPanel::RefreshDisplay()
{
	for (int32 i = 0; i < static_cast<int32>(EMOEquipmentSlot::MAX); ++i)
	{
		UpdateSlotDisplay(static_cast<EMOEquipmentSlot>(i));
	}
}

void UMOEquipmentPanel::ClearPanel()
{
	UnbindSlotEvents();
	UnbindEquipmentEvents();
	EquipmentComponent.Reset();
	InventoryComponent.Reset();

	// Clear all slot visuals
	for (int32 i = 0; i < static_cast<int32>(EMOEquipmentSlot::MAX); ++i)
	{
		UMOInventorySlot* SlotWidget = GetSlotWidget(static_cast<EMOEquipmentSlot>(i));
		if (SlotWidget)
		{
			SlotWidget->ClearVisualData();
		}
	}
}

// ============================================================================
// SLOT WIDGET ACCESS
// ============================================================================

UMOInventorySlot* UMOEquipmentPanel::GetSlotWidget(EMOEquipmentSlot EquipSlot) const
{
	switch (EquipSlot)
	{
	case EMOEquipmentSlot::Head:      return HeadSlot.Get();
	case EMOEquipmentSlot::Chest:     return ChestSlot.Get();
	case EMOEquipmentSlot::Hands:     return HandsSlot.Get();
	case EMOEquipmentSlot::Legs:      return LegsSlot.Get();
	case EMOEquipmentSlot::Feet:      return FeetSlot.Get();
	case EMOEquipmentSlot::Back:      return BackSlot.Get();
	case EMOEquipmentSlot::LeftHand:  return LeftHandSlot.Get();
	case EMOEquipmentSlot::RightHand: return RightHandSlot.Get();
	default: return nullptr;
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

void UMOEquipmentPanel::BindSlotEvents()
{
	// Bind click events
	if (HeadSlot)
	{
		HeadSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleHeadSlotClicked);
		HeadSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleHeadSlotDropReceived);
	}
	if (ChestSlot)
	{
		ChestSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleChestSlotClicked);
		ChestSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleChestSlotDropReceived);
	}
	if (HandsSlot)
	{
		HandsSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleHandsSlotClicked);
		HandsSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleHandsSlotDropReceived);
	}
	if (LegsSlot)
	{
		LegsSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleLegsSlotClicked);
		LegsSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleLegsSlotDropReceived);
	}
	if (FeetSlot)
	{
		FeetSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleFeetSlotClicked);
		FeetSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleFeetSlotDropReceived);
	}
	if (BackSlot)
	{
		BackSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleBackSlotClicked);
		BackSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleBackSlotDropReceived);
	}
	if (LeftHandSlot)
	{
		LeftHandSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleLeftHandSlotClicked);
		LeftHandSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleLeftHandSlotDropReceived);
	}
	if (RightHandSlot)
	{
		RightHandSlot->OnSlotClicked.AddDynamic(this, &UMOEquipmentPanel::HandleRightHandSlotClicked);
		RightHandSlot->OnSlotDropReceived.AddDynamic(this, &UMOEquipmentPanel::HandleRightHandSlotDropReceived);
	}
}

void UMOEquipmentPanel::UnbindSlotEvents()
{
	if (HeadSlot)
	{
		HeadSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleHeadSlotClicked);
		HeadSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleHeadSlotDropReceived);
	}
	if (ChestSlot)
	{
		ChestSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleChestSlotClicked);
		ChestSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleChestSlotDropReceived);
	}
	if (HandsSlot)
	{
		HandsSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleHandsSlotClicked);
		HandsSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleHandsSlotDropReceived);
	}
	if (LegsSlot)
	{
		LegsSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleLegsSlotClicked);
		LegsSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleLegsSlotDropReceived);
	}
	if (FeetSlot)
	{
		FeetSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleFeetSlotClicked);
		FeetSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleFeetSlotDropReceived);
	}
	if (BackSlot)
	{
		BackSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleBackSlotClicked);
		BackSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleBackSlotDropReceived);
	}
	if (LeftHandSlot)
	{
		LeftHandSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleLeftHandSlotClicked);
		LeftHandSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleLeftHandSlotDropReceived);
	}
	if (RightHandSlot)
	{
		RightHandSlot->OnSlotClicked.RemoveDynamic(this, &UMOEquipmentPanel::HandleRightHandSlotClicked);
		RightHandSlot->OnSlotDropReceived.RemoveDynamic(this, &UMOEquipmentPanel::HandleRightHandSlotDropReceived);
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
	UMOInventorySlot* SlotWidget = GetSlotWidget(EquipSlot);
	if (!SlotWidget || !EquipmentComponent.IsValid())
	{
		return;
	}

	const FMOEquippedItem Item = EquipmentComponent->GetEquippedItem(EquipSlot);

	if (Item.IsValid())
	{
		// Build visual data from equipped item
		FMOInventorySlotVisualData VisualData;
		VisualData.bHasItem = true;
		VisualData.ItemGuid = Item.ItemGuid;
		VisualData.ItemDefinitionId = Item.ItemDefinitionId;
		VisualData.Quantity = 1; // Equipment is always quantity 1
		SlotWidget->SetVisualData(VisualData);
	}
	else
	{
		SlotWidget->ClearVisualData();
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
// SLOT CLICK HANDLERS
// ============================================================================

void UMOEquipmentPanel::HandleHeadSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::Head);
}

void UMOEquipmentPanel::HandleChestSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::Chest);
}

void UMOEquipmentPanel::HandleHandsSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::Hands);
}

void UMOEquipmentPanel::HandleLegsSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::Legs);
}

void UMOEquipmentPanel::HandleFeetSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::Feet);
}

void UMOEquipmentPanel::HandleBackSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::Back);
}

void UMOEquipmentPanel::HandleLeftHandSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::LeftHand);
}

void UMOEquipmentPanel::HandleRightHandSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	HandleSlotClickedInternal(EMOEquipmentSlot::RightHand);
}

void UMOEquipmentPanel::HandleSlotClickedInternal(EMOEquipmentSlot EquipSlot)
{
	if (!EquipmentComponent.IsValid() || !InventoryComponent.IsValid())
	{
		return;
	}

	FMOEquippedItem Item = EquipmentComponent->GetEquippedItem(EquipSlot);
	OnSlotClicked.Broadcast(EquipSlot, Item);

	// Clicking an equipped item unequips it to inventory
	if (Item.IsValid())
	{
		EquipmentComponent->UnequipToInventory(EquipSlot, InventoryComponent.Get());
	}
}

// ============================================================================
// SLOT DROP HANDLERS
// ============================================================================

void UMOEquipmentPanel::HandleHeadSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::Head, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleChestSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::Chest, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleHandsSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::Hands, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleLegsSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::Legs, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleFeetSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::Feet, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleBackSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::Back, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleLeftHandSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::LeftHand, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleRightHandSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	HandleSlotDropReceivedInternal(EMOEquipmentSlot::RightHand, SourceSlotIndex, SourceInventory);
}

void UMOEquipmentPanel::HandleSlotDropReceivedInternal(EMOEquipmentSlot EquipSlot, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	if (!EquipmentComponent.IsValid() || !IsValid(SourceInventory))
	{
		return;
	}

	// Get the item GUID from the source slot
	FGuid ItemGuid;
	if (SourceSlotIndex >= 0)
	{
		SourceInventory->TryGetSlotGuid(SourceSlotIndex, ItemGuid);
	}

	if (!ItemGuid.IsValid())
	{
		return;
	}

	// Broadcast the drop event
	OnSlotDropReceived.Broadcast(EquipSlot, ItemGuid, SourceInventory);

	// Try to equip
	EquipmentComponent->EquipFromInventory(SourceInventory, ItemGuid, EquipSlot);
}
