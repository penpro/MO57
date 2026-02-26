/**
 * =============================================================================
 * MOItemInfoPanel.h - Item Detail Display Panel
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Panel showing detailed item information when an item is selected in
 * inventory. Displays name, type, rarity, stats, flags, tags, and properties.
 *
 * DISPLAY MODES:
 * - Inventory Mode: SetSelectedItemGuid() looks up item in inventory
 * - Direct Mode: SetItemByDefinitionId() displays from definition only
 *
 * DISPLAYED INFO:
 * - Name, Type, Rarity (with color coding)
 * - Description (short and full)
 * - Icon image
 * - Quantity, Max Stack, Weight, Value
 * - Item flags (bConsumable, bEquippable, etc.)
 * - Tags array
 * - Scalar properties map
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DUAL MODE: DirectItemDefinitionId takes precedence over inventory
 *   lookup. Clear with ClearSelection() before switching modes.
 *
 * [2024-02] BIND WIDGET OPTIONAL: All display widgets are BindWidgetOptional.
 *   Panel works with partial binding - only shows what's bound.
 *
 * [2024-02] PLACEHOLDER STATE: PlaceholderText shown when no item selected.
 *   InfoGrid hidden via SetDetailWidgetsVisibility().
 *
 * =============================================================================
 * RELATED FILES: MOUnifiedInventoryMenu.h, MOItemDefinitionRow.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOItemDefinitionRow.h"
#include "MOItemInfoPanel.generated.h"

class UTextBlock;
class UImage;
class UMOInventoryComponent;

UCLASS()
class MOFRAMEWORK_API UMOItemInfoPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializePanel(UMOInventoryComponent* InInventoryComponent);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetSelectedItemGuid(const FGuid& InSelectedGuid);

	/** Display item info directly from definition (for world items without inventory). */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetItemByDefinitionId(FName InItemDefinitionId, int32 InQuantity = 1);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void ClearSelection();

protected:
	virtual void NativeConstruct() override;

private:
	void RefreshPanel();
	void ClearAllFields();
	void SetDetailWidgetsVisibility(ESlateVisibility InVisibility);
	void DisplayItemDefinition(const FMOItemDefinitionRow& ItemDef, int32 Quantity);
	FString GetItemTypeString(EMOItemType ItemType) const;
	FString GetRarityString(EMOItemRarity Rarity) const;
	FLinearColor GetRarityColor(EMOItemRarity Rarity) const;

	/** Message shown when no item is selected. Set in Blueprint defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	FText NoSelectionMessage = NSLOCTEXT("MOItemInfo", "NoSelection", "Click an item for details");

private:
	// Core info
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemTypeText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RarityText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ShortDescriptionText;

	// Icon
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

	// Stats
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxStackText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WeightText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	// Flags
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FlagsText;

	// Tags
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TagsText;

	// Scalar properties
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PropertiesText;

	// Container for all item detail widgets - hidden when no item selected
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> InfoGrid;

	// Shown when no item is selected
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaceholderText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DebugSelectedGuidText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DebugItemIdText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DebugQuantityText;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	FGuid SelectedGuid;

	/** When set, display info from this definition ID instead of looking up in inventory. */
	UPROPERTY()
	FName DirectItemDefinitionId;

	/** Quantity override when using DirectItemDefinitionId. */
	UPROPERTY()
	int32 DirectQuantity = 0;
};
