#include "MOItemInfoPanel.h"

#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"

void UMOItemInfoPanel::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Warning, TEXT("[ItemInfoPanel] NativeConstruct - InfoGrid=%s, PlaceholderText=%s"),
		InfoGrid ? TEXT("valid") : TEXT("NULL"),
		PlaceholderText ? TEXT("valid") : TEXT("NULL"));

	RefreshPanel();
}

void UMOItemInfoPanel::InitializePanel(UMOInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
	UE_LOG(LogMOFramework, Warning, TEXT("[ItemInfoPanel] InitializePanel - InventoryComponent=%s"),
		IsValid(InventoryComponent) ? TEXT("valid") : TEXT("NULL"));
	RefreshPanel();
}

void UMOItemInfoPanel::SetSelectedItemGuid(const FGuid& InSelectedGuid)
{
	SelectedGuid = InSelectedGuid;
	// Clear direct mode when switching to GUID mode
	DirectItemDefinitionId = NAME_None;
	DirectQuantity = 0;
	UE_LOG(LogMOFramework, Warning, TEXT("[ItemInfoPanel] SetSelectedItemGuid - Guid=%s, IsValid=%s"),
		*SelectedGuid.ToString(EGuidFormats::Short),
		SelectedGuid.IsValid() ? TEXT("true") : TEXT("false"));
	RefreshPanel();
}

void UMOItemInfoPanel::SetItemByDefinitionId(FName InItemDefinitionId, int32 InQuantity)
{
	// Clear GUID mode when switching to direct mode
	SelectedGuid.Invalidate();
	InventoryComponent = nullptr;
	DirectItemDefinitionId = InItemDefinitionId;
	DirectQuantity = FMath::Max(1, InQuantity);
	UE_LOG(LogMOFramework, Log, TEXT("[ItemInfoPanel] SetItemByDefinitionId - ItemId=%s, Quantity=%d"),
		*InItemDefinitionId.ToString(), DirectQuantity);
	RefreshPanel();
}

void UMOItemInfoPanel::ClearSelection()
{
	SelectedGuid.Invalidate();
	RefreshPanel();
}

void UMOItemInfoPanel::ClearAllFields()
{
	if (ItemNameText) { ItemNameText->SetText(FText::GetEmpty()); }
	if (ItemTypeText) { ItemTypeText->SetText(FText::GetEmpty()); }
	if (RarityText) { RarityText->SetText(FText::GetEmpty()); }
	if (DescriptionText) { DescriptionText->SetText(FText::GetEmpty()); }
	if (ShortDescriptionText) { ShortDescriptionText->SetText(FText::GetEmpty()); }
	if (QuantityText) { QuantityText->SetText(FText::GetEmpty()); }
	if (MaxStackText) { MaxStackText->SetText(FText::GetEmpty()); }
	if (WeightText) { WeightText->SetText(FText::GetEmpty()); }
	if (ValueText) { ValueText->SetText(FText::GetEmpty()); }
	if (FlagsText) { FlagsText->SetText(FText::GetEmpty()); }
	if (TagsText) { TagsText->SetText(FText::GetEmpty()); }
	if (PropertiesText) { PropertiesText->SetText(FText::GetEmpty()); }
	if (ItemIconImage) { ItemIconImage->SetBrushFromTexture(nullptr); }

	// Debug fields
	if (DebugSelectedGuidText) { DebugSelectedGuidText->SetText(FText::FromString(TEXT("(none)"))); }
	if (DebugItemIdText) { DebugItemIdText->SetText(FText::GetEmpty()); }
	if (DebugQuantityText) { DebugQuantityText->SetText(FText::GetEmpty()); }
}

void UMOItemInfoPanel::SetDetailWidgetsVisibility(ESlateVisibility InVisibility)
{
	// If InfoGrid is bound, just toggle that - it contains all detail widgets
	if (InfoGrid)
	{
		InfoGrid->SetVisibility(InVisibility);
	}
}

FString UMOItemInfoPanel::GetItemTypeString(EMOItemType ItemType) const
{
	switch (ItemType)
	{
		case EMOItemType::Consumable: return TEXT("Consumable");
		case EMOItemType::Material: return TEXT("Material");
		case EMOItemType::Tool: return TEXT("Tool");
		case EMOItemType::Weapon: return TEXT("Weapon");
		case EMOItemType::Ammo: return TEXT("Ammo");
		case EMOItemType::Armor: return TEXT("Armor");
		case EMOItemType::KeyItem: return TEXT("Key Item");
		case EMOItemType::Quest: return TEXT("Quest");
		case EMOItemType::Currency: return TEXT("Currency");
		case EMOItemType::Misc: return TEXT("Misc");
		default: return TEXT("Unknown");
	}
}

FString UMOItemInfoPanel::GetRarityString(EMOItemRarity Rarity) const
{
	switch (Rarity)
	{
		case EMOItemRarity::Common: return TEXT("Common");
		case EMOItemRarity::Uncommon: return TEXT("Uncommon");
		case EMOItemRarity::Rare: return TEXT("Rare");
		case EMOItemRarity::Epic: return TEXT("Epic");
		case EMOItemRarity::Legendary: return TEXT("Legendary");
		default: return TEXT("Unknown");
	}
}

FLinearColor UMOItemInfoPanel::GetRarityColor(EMOItemRarity Rarity) const
{
	switch (Rarity)
	{
		case EMOItemRarity::Common: return FLinearColor(0.7f, 0.7f, 0.7f, 1.0f); // Gray
		case EMOItemRarity::Uncommon: return FLinearColor(0.2f, 0.8f, 0.2f, 1.0f); // Green
		case EMOItemRarity::Rare: return FLinearColor(0.2f, 0.4f, 1.0f, 1.0f); // Blue
		case EMOItemRarity::Epic: return FLinearColor(0.6f, 0.2f, 0.8f, 1.0f); // Purple
		case EMOItemRarity::Legendary: return FLinearColor(1.0f, 0.6f, 0.0f, 1.0f); // Orange
		default: return FLinearColor::White;
	}
}

void UMOItemInfoPanel::RefreshPanel()
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[ItemInfoPanel] RefreshPanel - SelectedGuid=%s, InventoryComponent=%s, DirectItemId=%s"),
		*SelectedGuid.ToString(EGuidFormats::Short),
		IsValid(InventoryComponent) ? TEXT("valid") : TEXT("NULL"),
		*DirectItemDefinitionId.ToString());

	// Direct definition ID mode (for world items)
	if (!DirectItemDefinitionId.IsNone())
	{
		// Hide placeholder, show detail widgets
		if (PlaceholderText)
		{
			PlaceholderText->SetVisibility(ESlateVisibility::Collapsed);
		}
		SetDetailWidgetsVisibility(ESlateVisibility::SelfHitTestInvisible);

		// Debug fields
		if (DebugSelectedGuidText)
		{
			DebugSelectedGuidText->SetText(FText::FromString(TEXT("(world item)")));
		}
		if (DebugItemIdText)
		{
			DebugItemIdText->SetText(FText::FromName(DirectItemDefinitionId));
		}
		if (DebugQuantityText)
		{
			DebugQuantityText->SetText(FText::AsNumber(DirectQuantity));
		}

		// Get the item definition and display it
		FMOItemDefinitionRow ItemDef;
		if (!UMOItemDatabaseSettings::GetItemDefinition(DirectItemDefinitionId, ItemDef))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[ItemInfoPanel] RefreshPanel - No item definition found for %s"),
				*DirectItemDefinitionId.ToString());
			if (ItemNameText) { ItemNameText->SetText(FText::FromName(DirectItemDefinitionId)); }
			if (QuantityText) { QuantityText->SetText(FText::AsNumber(DirectQuantity)); }
			return;
		}

		// Display item info using the definition
		DisplayItemDefinition(ItemDef, DirectQuantity);
		return;
	}

	// Standard GUID + Inventory mode
	if (!SelectedGuid.IsValid() || !IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[ItemInfoPanel] RefreshPanel - EARLY EXIT: GuidValid=%s, InvCompValid=%s"),
			SelectedGuid.IsValid() ? TEXT("true") : TEXT("false"),
			IsValid(InventoryComponent) ? TEXT("true") : TEXT("false"));
		ClearAllFields();
		SetDetailWidgetsVisibility(ESlateVisibility::Collapsed);
		if (PlaceholderText)
		{
			PlaceholderText->SetText(NoSelectionMessage);
			PlaceholderText->SetVisibility(ESlateVisibility::Visible);
		}
		return;
	}

	FMOInventoryEntry FoundEntry;
	if (!InventoryComponent->TryGetEntryByGuid(SelectedGuid, FoundEntry))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[ItemInfoPanel] RefreshPanel - TryGetEntryByGuid FAILED for Guid=%s"),
			*SelectedGuid.ToString(EGuidFormats::Short));
		ClearAllFields();
		SetDetailWidgetsVisibility(ESlateVisibility::Collapsed);
		if (PlaceholderText)
		{
			PlaceholderText->SetText(FText::FromString(TEXT("Item not found.")));
			PlaceholderText->SetVisibility(ESlateVisibility::Visible);
		}
		return;
	}

	// Hide placeholder, show detail widgets
	if (PlaceholderText)
	{
		PlaceholderText->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetDetailWidgetsVisibility(ESlateVisibility::SelfHitTestInvisible);

	UE_LOG(LogMOFramework, Verbose, TEXT("[ItemInfoPanel] RefreshPanel - Found entry: ItemDefId=%s, Quantity=%d"),
		*FoundEntry.ItemDefinitionId.ToString(), FoundEntry.Quantity);

	// Debug fields (backwards compatibility)
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

	// Get the item definition from the DataTable
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(FoundEntry.ItemDefinitionId, ItemDef))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[ItemInfoPanel] RefreshPanel - No item definition found for %s, showing basic info"),
			*FoundEntry.ItemDefinitionId.ToString());
		// No definition found, show basic info
		if (ItemNameText) { ItemNameText->SetText(FText::FromName(FoundEntry.ItemDefinitionId)); }
		if (QuantityText) { QuantityText->SetText(FText::AsNumber(FoundEntry.Quantity)); }
		return;
	}

	// Display item info using the definition
	DisplayItemDefinition(ItemDef, FoundEntry.Quantity);
}

void UMOItemInfoPanel::DisplayItemDefinition(const FMOItemDefinitionRow& ItemDef, int32 Quantity)
{
	// Display Name
	if (ItemNameText)
	{
		ItemNameText->SetText(ItemDef.DisplayName.IsEmpty() ? FText::FromString(TEXT("Unknown Item")) : ItemDef.DisplayName);
		ItemNameText->SetColorAndOpacity(FSlateColor(GetRarityColor(ItemDef.Rarity)));
	}

	// Item Type
	if (ItemTypeText)
	{
		ItemTypeText->SetText(FText::FromString(GetItemTypeString(ItemDef.ItemType)));
	}

	// Rarity
	if (RarityText)
	{
		RarityText->SetText(FText::FromString(GetRarityString(ItemDef.Rarity)));
		RarityText->SetColorAndOpacity(FSlateColor(GetRarityColor(ItemDef.Rarity)));
	}

	// Description
	if (DescriptionText)
	{
		DescriptionText->SetText(ItemDef.Description);
	}

	// Short Description
	if (ShortDescriptionText)
	{
		ShortDescriptionText->SetText(ItemDef.ShortDescription);
	}

	// Quantity
	if (QuantityText)
	{
		QuantityText->SetText(FText::Format(NSLOCTEXT("MOItemInfo", "Quantity", "x{0}"), FText::AsNumber(Quantity)));
	}

	// Max Stack
	if (MaxStackText)
	{
		MaxStackText->SetText(FText::Format(NSLOCTEXT("MOItemInfo", "MaxStack", "Max Stack: {0}"), FText::AsNumber(ItemDef.MaxStackSize)));
	}

	// Weight
	if (WeightText)
	{
		WeightText->SetText(FText::Format(NSLOCTEXT("MOItemInfo", "Weight", "Weight: {0}"), FText::AsNumber(ItemDef.Weight)));
	}

	// Value
	if (ValueText)
	{
		ValueText->SetText(FText::Format(NSLOCTEXT("MOItemInfo", "Value", "Value: {0}"), FText::AsNumber(ItemDef.BaseValue)));
	}

	// Flags
	if (FlagsText)
	{
		TArray<FString> FlagsList;
		if (ItemDef.bConsumable) FlagsList.Add(TEXT("Consumable"));
		if (ItemDef.bEquippable) FlagsList.Add(TEXT("Equippable"));
		if (ItemDef.bQuestItem) FlagsList.Add(TEXT("Quest Item"));
		if (!ItemDef.bCanDrop) FlagsList.Add(TEXT("Cannot Drop"));
		if (!ItemDef.bCanTrade) FlagsList.Add(TEXT("Cannot Trade"));

		if (FlagsList.Num() > 0)
		{
			FlagsText->SetText(FText::FromString(FString::Join(FlagsList, TEXT(", "))));
		}
		else
		{
			FlagsText->SetText(FText::GetEmpty());
		}
	}

	// Tags
	if (TagsText)
	{
		if (ItemDef.Tags.Num() > 0)
		{
			TArray<FString> TagStrings;
			for (const FName& Tag : ItemDef.Tags)
			{
				TagStrings.Add(Tag.ToString());
			}
			TagsText->SetText(FText::FromString(FString::Join(TagStrings, TEXT(", "))));
		}
		else
		{
			TagsText->SetText(FText::GetEmpty());
		}
	}

	// Scalar Properties
	if (PropertiesText)
	{
		if (ItemDef.ScalarProperties.Num() > 0)
		{
			TArray<FString> PropStrings;
			for (const FMOItemScalarProperty& Prop : ItemDef.ScalarProperties)
			{
				PropStrings.Add(FString::Printf(TEXT("%s: %.1f"), *Prop.Key.ToString(), Prop.Value));
			}
			PropertiesText->SetText(FText::FromString(FString::Join(PropStrings, TEXT("\n"))));
		}
		else
		{
			PropertiesText->SetText(FText::GetEmpty());
		}
	}

	// Icon
	if (ItemIconImage)
	{
		UTexture2D* IconTexture = nullptr;
		if (!ItemDef.UI.IconLarge.IsNull())
		{
			IconTexture = ItemDef.UI.IconLarge.LoadSynchronous();
		}
		if (!IconTexture && !ItemDef.UI.IconSmall.IsNull())
		{
			IconTexture = ItemDef.UI.IconSmall.LoadSynchronous();
		}

		if (IconTexture)
		{
			ItemIconImage->SetBrushFromTexture(IconTexture, true);
			ItemIconImage->SetColorAndOpacity(ItemDef.UI.Tint);
		}
		else
		{
			ItemIconImage->SetBrushFromTexture(nullptr);
		}
	}
}
