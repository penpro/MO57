#include "MOContextMenuBase.h"
#include "MOFramework.h"
#include "MOCommonButton.h"

UMOContextMenuBase::UMOContextMenuBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOContextMenuBase::SetPopupPosition(FVector2D ScreenPosition)
{
	// Use SetPositionInViewport for reliable screen positioning
	// Pass true to remove DPI scale since our coordinates are in screen pixels
	SetPositionInViewport(ScreenPosition, true);
}

void UMOContextMenuBase::RequestClose()
{
	OnCloseRequested.Broadcast();
}

FReply UMOContextMenuBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Close on Escape or Tab
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOContextMenuBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	bIsMouseOver = true;
	MouseLeaveTimer = 0.0f; // Cancel any pending close
}

void UMOContextMenuBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bIsMouseOver = false;

	// Start grace timer if close-on-mouse-leave is enabled
	if (ShouldCloseOnMouseLeave())
	{
		MouseLeaveTimer = MouseLeaveGraceTime;
	}
}

void UMOContextMenuBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Handle mouse leave grace timer
	if (MouseLeaveTimer > 0.0f && !bIsMouseOver)
	{
		MouseLeaveTimer -= InDeltaTime;
		if (MouseLeaveTimer <= 0.0f)
		{
			MouseLeaveTimer = 0.0f;
			RequestClose();
		}
	}
}

bool UMOContextMenuBase::ShouldCloseOnMouseLeave() const
{
	return bCloseOnMouseLeave;
}
