#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOInventorySlot.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;
class UMOInventoryComponent;

USTRUCT(BlueprintType)
struct FMOInventorySlotVisualData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	bool bHasItem = false;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FGuid ItemGuid;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FName ItemDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	int32 Quantity = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInventorySlotClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid);

UCLASS()
class MOFRAMEWORK_API UMOInventorySlot : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOInventorySlot(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializeSlot(UMOInventoryComponent* InInventoryComponent, int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void RefreshFromInventory();

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	FGuid GetItemGuid() const { return CachedVisualData.ItemGuid; }

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotClickedSignature OnSlotClicked;

protected:
	virtual void NativeConstruct() override;

	// Optional Blueprint hook to customize visuals.
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Inventory|UI")
	void OnVisualDataUpdated(const FMOInventorySlotVisualData& NewVisualData);

private:
	UFUNCTION()
	void HandleSlotButtonClicked();

	void ApplyVisualDataToWidget();
	void UpdateQuantityBoxVisibility(int32 InQuantity);

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

	// Text inside QuantityBox (optional to touch, but helpful to keep correct when shown)
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	// Debug only
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DebugItemIdText;

	// You said you named this in the WBP. Keep exact name: QuantityBox
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> QuantityBox;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY()
	FMOInventorySlotVisualData CachedVisualData;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TObjectPtr<UTexture2D> DefaultItemIcon;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TObjectPtr<UTexture2D> EmptySlotIcon;
};
