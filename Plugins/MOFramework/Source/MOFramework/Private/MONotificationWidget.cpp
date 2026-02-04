#include "MONotificationWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

void UMONotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure notification doesn't steal focus from other UI elements
	SetIsFocusable(false);

	// Apply pending message if set before construct
	if (bHasPendingMessage)
	{
		if (SlateTextBlock.IsValid())
		{
			SlateTextBlock->SetText(PendingMessage);
		}
		bHasPendingMessage = false;
	}
}

TSharedRef<SWidget> UMONotificationWidget::RebuildWidget()
{
	// Build a simple notification using Slate directly
	// Positioned 400px from the bottom of the screen, centered horizontally
	// Use HitTestInvisible so mouse clicks pass through to widgets below
	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", 24);

	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 320.0f)) // Bottom padding to position ~400px from bottom (accounting for widget height)
		.Visibility(EVisibility::HitTestInvisible) // Don't block mouse clicks
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
			.Padding(FMargin(40.0f, 20.0f))
			.Visibility(EVisibility::HitTestInvisible) // Don't block mouse clicks
			[
				SAssignNew(SlateTextBlock, STextBlock)
				.Text(PendingMessage.IsEmpty() ? FText::FromString(TEXT("Notification")) : PendingMessage)
				.Font(FontInfo)
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(2.0f, 2.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
				.Justification(ETextJustify::Center)
			]
		];
}

void UMONotificationWidget::SetMessage(const FText& Message)
{
	PendingMessage = Message;

	// Update Slate widget if available
	if (SlateTextBlock.IsValid())
	{
		SlateTextBlock->SetText(Message);
	}
	// Update UMG widget if available (Blueprint usage)
	else if (MessageText)
	{
		MessageText->SetText(Message);
	}
	else
	{
		bHasPendingMessage = true;
	}
}

void UMONotificationWidget::SetTextColor(FLinearColor Color)
{
	if (SlateTextBlock.IsValid())
	{
		SlateTextBlock->SetColorAndOpacity(Color);
	}
	else if (MessageText)
	{
		MessageText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UMONotificationWidget::SetBackgroundColor(FLinearColor Color)
{
	// Note: For Slate version, would need to store border reference
	// For now, only works with Blueprint version
	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(Color);
	}
}
