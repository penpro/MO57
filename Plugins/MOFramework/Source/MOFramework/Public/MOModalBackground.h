#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOModalBackground.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOModalBackgroundClickedSignature);

/**
 * Full-screen invisible widget that catches clicks outside menus.
 * When clicked, broadcasts OnBackgroundClicked so menus can close.
 */
UCLASS()
class MOFRAMEWORK_API UMOModalBackground : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOModalBackground(const FObjectInitializer& ObjectInitializer);

	/** Called when the background is clicked (outside any menu content). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI")
	FMOModalBackgroundClickedSignature OnBackgroundClicked;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
