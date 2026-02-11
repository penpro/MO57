#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOUIDelegates.h"
#include "MOInventoryMenu.generated.h"

class UMOInventoryComponent;
class UMOInventoryGrid;
class UMOItemInfoPanel;

// Legacy delegates - prefer standard delegates from MOUIDelegates.h for new code
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInventoryMenuRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOInventoryMenuSlotRightClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid, FVector2D, ScreenPosition);

UCLASS()
class MOFRAMEWORK_API UMOInventoryMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOInventoryMenu(const FObjectInitializer& ObjectInitializer);

	// Call this right after CreateWidget in your PlayerController.
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializeMenu(UMOInventoryComponent* InInventoryComponent);

	/** Get the inventory component this menu is displaying. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** Broadcast when the menu wants to close. Uses standard delegate signature. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOUIRequestClose OnCloseRequested;

	/** @deprecated Use OnCloseRequested instead. Broadcast for backward compatibility. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryMenuRequestCloseSignature OnRequestClose;

	/** Called when a slot is right-clicked. UIManager uses this to show context menu. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryMenuSlotRightClickedSignature OnSlotRightClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Tab should close the menu.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleSlotsChanged();

	UFUNCTION()
	void HandleGridSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleGridSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	void RefreshAll();

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOInventoryGrid> InventoryGrid;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOItemInfoPanel> ItemInfoPanel;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	FGuid SelectedItemGuid;
};
