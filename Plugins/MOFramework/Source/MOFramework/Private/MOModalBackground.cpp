#include "MOModalBackground.h"
#include "Components/Border.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

UMOModalBackground::UMOModalBackground(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

TSharedRef<SWidget> UMOModalBackground::RebuildWidget()
{
	// Create a full-screen transparent border that captures all mouse input
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.Padding(0)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Visibility(EVisibility::Visible);
}

FReply UMOModalBackground::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnBackgroundClicked.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
