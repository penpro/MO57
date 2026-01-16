#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOItemInfoPanel.generated.h"

class UTextBlock;
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

protected:
	virtual void NativeConstruct() override;

private:
	void RefreshPanel();

private:
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
