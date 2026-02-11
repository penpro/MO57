#include "MONotificationWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UMONotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure notification doesn't steal focus from other UI elements
	SetIsFocusable(false);

	// Apply pending message if set before construct
	if (bHasPendingMessage && MessageText)
	{
		MessageText->SetText(PendingMessage);
		bHasPendingMessage = false;
	}

	// Apply pending type if set before construct
	if (bHasPendingType)
	{
		CurrentType = PendingType;
		ApplyTypeColors();
		bHasPendingType = false;
	}

	// Hide icon by default if no icon is set
	if (IconImage)
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMONotificationWidget::SetupNotification(const FText& Message, EMONotificationType Type, UTexture2D* Icon)
{
	SetMessage(Message);
	SetNotificationType(Type);

	if (Icon)
	{
		SetIcon(Icon);
	}
}

void UMONotificationWidget::SetMessage(const FText& Message)
{
	PendingMessage = Message;

	if (MessageText)
	{
		MessageText->SetText(Message);
	}
	else
	{
		bHasPendingMessage = true;
	}
}

void UMONotificationWidget::SetNotificationType(EMONotificationType Type)
{
	CurrentType = Type;

	if (IsConstructed())
	{
		ApplyTypeColors();
	}
	else
	{
		PendingType = Type;
		bHasPendingType = true;
	}
}

void UMONotificationWidget::SetIcon(UTexture2D* Icon)
{
	if (!IconImage)
	{
		return;
	}

	if (Icon)
	{
		IconImage->SetBrushFromTexture(Icon);
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
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

FLinearColor UMONotificationWidget::GetColorForType(EMONotificationType Type)
{
	// Static default colors - instance colors can override via properties
	switch (Type)
	{
	case EMONotificationType::Info:
		return FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);  // Blue
	case EMONotificationType::Success:
		return FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);  // Green
	case EMONotificationType::Warning:
		return FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);  // Yellow/Gold
	case EMONotificationType::Error:
		return FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);  // Red
	case EMONotificationType::ItemPickup:
		return FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);  // Light Gray
	case EMONotificationType::SkillGain:
		return FLinearColor(0.6f, 0.4f, 1.0f, 1.0f);  // Purple
	case EMONotificationType::Custom:
	default:
		return FLinearColor::White;
	}
}

void UMONotificationWidget::ApplyTypeColors()
{
	FLinearColor TypeColor;

	switch (CurrentType)
	{
	case EMONotificationType::Info:
		TypeColor = InfoColor;
		break;
	case EMONotificationType::Success:
		TypeColor = SuccessColor;
		break;
	case EMONotificationType::Warning:
		TypeColor = WarningColor;
		break;
	case EMONotificationType::Error:
		TypeColor = ErrorColor;
		break;
	case EMONotificationType::ItemPickup:
		TypeColor = ItemPickupColor;
		break;
	case EMONotificationType::SkillGain:
		TypeColor = SkillGainColor;
		break;
	case EMONotificationType::Custom:
	default:
		TypeColor = FLinearColor::White;
		break;
	}

	// Apply text color with full opacity
	if (MessageText)
	{
		MessageText->SetColorAndOpacity(FSlateColor(TypeColor));
	}

	// Apply background color (use base background with type-tinted border if desired)
	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(BackgroundColorBase);
	}
}

void UMONotificationWidget::PlayShowAnimation_Implementation()
{
	// Default implementation: just make visible immediately
	// Override in Blueprint to add fade-in, slide-in, etc.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMONotificationWidget::PlayHideAnimation_Implementation()
{
	// Default implementation: hide immediately and broadcast
	// Override in Blueprint to add fade-out, slide-out, etc.
	// When animation completes, call OnHideAnimationComplete()
	OnHideAnimationComplete();
}

void UMONotificationWidget::OnHideAnimationComplete()
{
	// Broadcast that the notification is fully hidden
	OnNotificationHidden.Broadcast();
}
