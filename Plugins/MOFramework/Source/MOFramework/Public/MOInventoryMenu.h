#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOInventoryMenu.generated.h"

class UMOInventoryComponent;
class UMOInventoryGrid;
class UMOItemInfoPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInventoryMenuRequestCloseSignature);

UCLASS()
class MOFRAMEWORK_API UMOInventoryMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOInventoryMenu(const FObjectInitializer& ObjectInitializer);

	// Call this right after CreateWidget in your PlayerController.
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializeMenu(UMOInventoryComponent* InInventoryComponent);

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryMenuRequestCloseSignature OnRequestClose;

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
