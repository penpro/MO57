#include "MONotificationWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

void UMONotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create text block if not bound from Blueprint
	if (!MessageText)
	{
		MessageText = NewObject<UTextBlock>(this);
		if (MessageText)
		{
			// Style the text
			FSlateFontInfo FontInfo = MessageText->GetFont();
			FontInfo.Size = 24;
			MessageText->SetFont(FontInfo);
			MessageText->SetJustification(ETextJustify::Center);
			MessageText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			MessageText->SetShadowOffset(FVector2D(2.0f, 2.0f));
			MessageText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		}
	}

	// Apply pending message if set before construct
	if (bHasPendingMessage && MessageText)
	{
		MessageText->SetText(PendingMessage);
		bHasPendingMessage = false;
	}
}

void UMONotificationWidget::SetMessage(const FText& Message)
{
	if (MessageText)
	{
		MessageText->SetText(Message);
	}
	else
	{
		// Store for later if not yet constructed
		PendingMessage = Message;
		bHasPendingMessage = true;
	}
}

void UMONotificationWidget::SetTextColor(FLinearColor Color)
{
	if (MessageText)
	{
		MessageText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UMONotificationWidget::SetBackgroundColor(FLinearColor Color)
{
	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(Color);
	}
}
