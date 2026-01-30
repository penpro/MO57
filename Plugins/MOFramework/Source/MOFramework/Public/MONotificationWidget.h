#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MONotificationWidget.generated.h"

class UTextBlock;
class UBorder;

/**
 * Simple notification widget for displaying centered messages.
 * Used for "no pawn" notifications and other temporary messages.
 */
UCLASS()
class MOFRAMEWORK_API UMONotificationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set the message text to display. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void SetMessage(const FText& Message);

	/** Set the text color. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void SetTextColor(FLinearColor Color);

	/** Set the background color/opacity. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void SetBackgroundColor(FLinearColor Color);

protected:
	virtual void NativeConstruct() override;

	/** The text block displaying the message. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	/** Optional background border. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> BackgroundBorder;

private:
	/** Pending message to set after construct. */
	FText PendingMessage;

	/** Whether we have a pending message. */
	bool bHasPendingMessage = false;
};
