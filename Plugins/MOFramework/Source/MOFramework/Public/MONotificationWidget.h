#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "MONotificationWidget.generated.h"

class UTextBlock;
class UBorder;

/**
 * Simple notification widget for displaying centered messages.
 * Used for "no pawn" notifications and other temporary messages.
 * Can be used directly from C++ without a Blueprint.
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
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** UMG text block (used if Blueprint provides one). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	/** UMG background border (used if Blueprint provides one). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> BackgroundBorder;

private:
	/** Slate text block for C++-only usage. */
	TSharedPtr<STextBlock> SlateTextBlock;

	/** Pending message to set after construct. */
	FText PendingMessage;

	/** Whether we have a pending message. */
	bool bHasPendingMessage = false;
};
