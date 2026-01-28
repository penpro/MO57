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

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void ClearSelection();

protected:
	virtual void NativeConstruct() override;

private:
	void RefreshPanel();
	void ClearAllFields();
	void SetDetailWidgetsVisibility(ESlateVisibility InVisibility);
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
};
