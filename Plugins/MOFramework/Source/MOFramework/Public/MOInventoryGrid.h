#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOInventoryGrid.generated.h"

class UUniformGridPanel;
class UMOInventoryComponent;
class UMOInventorySlot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInventoryGridSlotClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOInventoryGridSlotRightClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid, FVector2D, ScreenPosition);

UCLASS()
class MOFRAMEWORK_API UMOInventoryGrid : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOInventoryGrid(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializeGrid(UMOInventoryComponent* InInventoryComponent);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void RebuildGrid();

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void RefreshAllSlots();

	/** Get the inventory component this grid is displaying. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryGridSlotClickedSignature OnGridSlotClicked;

	/** Called when a slot is right-clicked. Use for context menu. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryGridSlotRightClickedSignature OnGridSlotRightClicked;

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	int32 GetDesiredSlotCount() const;

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UUniformGridPanel> SlotsUniformGrid;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TSubclassOf<UMOInventorySlot> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI", meta=(ClampMin="1"))
	int32 Columns = 5;

	/**
	 * If the inventory reports 0 slots (or is not yet available), we still build this many slots
	 * so you can see the intended inventory size in the UI.
	 *
	 * Set this in your WBP_InventoryGrid defaults (recommended), or leave the default.
	 */
	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI", meta=(ClampMin="1"))
	int32 MinimumVisibleSlotCount = 20;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	TArray<TObjectPtr<UMOInventorySlot>> SlotWidgets;
};
