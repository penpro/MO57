#include "MONotificationWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

void UMONotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Only build the widget tree if not using a Blueprint (no root widget set)
	if (!WidgetTree->RootWidget)
	{
		// Create canvas panel as root
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// Create background border for better visibility
		BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		BackgroundBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		BackgroundBorder->SetPadding(FMargin(40.0f, 20.0f));

		UCanvasPanelSlot* BorderSlot = Canvas->AddChildToCanvas(BackgroundBorder);
		if (BorderSlot)
		{
			BorderSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			BorderSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			BorderSlot->SetAutoSize(true);
		}

		// Create text block
		MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
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

			// Add text to border
			BackgroundBorder->AddChild(MessageText);
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
