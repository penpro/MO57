#include "MOItemInfoPanel.h"

#include "Components/TextBlock.h"
#include "MOInventoryComponent.h"

void UMOItemInfoPanel::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshPanel();
}

void UMOItemInfoPanel::InitializePanel(UMOInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
	RefreshPanel();
}

void UMOItemInfoPanel::SetSelectedItemGuid(const FGuid& InSelectedGuid)
{
	SelectedGuid = InSelectedGuid;
	RefreshPanel();
}

void UMOItemInfoPanel::RefreshPanel()
{
	if (PlaceholderText)
	{
		PlaceholderText->SetText(FText::FromString(TEXT("Item info coming soon.")));
	}

	if (!SelectedGuid.IsValid() || !IsValid(InventoryComponent))
	{
		if (DebugSelectedGuidText) { DebugSelectedGuidText->SetText(FText::FromString(TEXT("(none)"))); }
		if (DebugItemIdText) { DebugItemIdText->SetText(FText::GetEmpty()); }
		if (DebugQuantityText) { DebugQuantityText->SetText(FText::GetEmpty()); }
		return;
	}

	FMOInventoryEntry FoundEntry;
	if (!InventoryComponent->TryGetEntryByGuid(SelectedGuid, FoundEntry))
	{
		if (DebugSelectedGuidText) { DebugSelectedGuidText->SetText(FText::FromString(TEXT("(missing)"))); }
		if (DebugItemIdText) { DebugItemIdText->SetText(FText::GetEmpty()); }
		if (DebugQuantityText) { DebugQuantityText->SetText(FText::GetEmpty()); }
		return;
	}

	if (DebugSelectedGuidText)
	{
		DebugSelectedGuidText->SetText(FText::FromString(SelectedGuid.ToString(EGuidFormats::Short)));
	}

	if (DebugItemIdText)
	{
		DebugItemIdText->SetText(FText::FromName(FoundEntry.ItemDefinitionId));
	}

	if (DebugQuantityText)
	{
		DebugQuantityText->SetText(FText::AsNumber(FoundEntry.Quantity));
	}
}
