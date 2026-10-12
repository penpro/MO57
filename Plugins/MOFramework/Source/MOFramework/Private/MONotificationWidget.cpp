#include "MONotificationWidget.h"
#include "MOUITokens.h"
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

	bIconExplicit = Icon != nullptr;
	if (Icon)
	{
		IconImage->SetBrushFromTexture(Icon);
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// no caller icon: show the type's own glyph (ApplyTypeColors collapses the image only for a type with none)
		bIconExplicit = false;
		ApplyTypeColors();
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

namespace
{
	FName NotificationColorToken(EMONotificationType Type)
	{
		switch (Type)
		{
		case EMONotificationType::Info:       return FName("color.severity.info");
		case EMONotificationType::Success:    return FName("color.severity.ok");
		case EMONotificationType::Warning:    return FName("color.severity.warning");
		case EMONotificationType::Error:      return FName("color.severity.critical");
		case EMONotificationType::ItemPickup: return FName("color.text.secondary");
		case EMONotificationType::SkillGain:  return FName("color.accent");
		case EMONotificationType::Custom:
		default:                              return FName("color.text.primary");
		}
	}

	FName NotificationShapeIcon(EMONotificationType Type)
	{
		switch (Type)
		{
		case EMONotificationType::Info:       return FName("ic_sev_info");
		case EMONotificationType::Success:    return FName("ic_sev_ok");
		case EMONotificationType::Warning:    return FName("ic_sev_warning");
		case EMONotificationType::Error:      return FName("ic_sev_critical");
		case EMONotificationType::ItemPickup: return FName("ic_item_unknown");
		case EMONotificationType::SkillGain:  return FName("ic_ui_skill");
		case EMONotificationType::Custom:
		default:                              return NAME_None;
		}
	}
}

FLinearColor UMONotificationWidget::GetColorForType(EMONotificationType Type)
{
	return UMOUITokens::Color(NotificationColorToken(Type));
}

void UMONotificationWidget::ApplyTypeColors()
{
	const FLinearColor TypeColor = GetColorForType(CurrentType);

	// The message reads as primary text; the SEVERITY lives in the glyph and the leading bar (shape + colour, never text colour alone)
	if (MessageText)
	{
		MessageText->SetColorAndOpacity(FSlateColor(UMOUITokens::Color("color.text.primary")));
	}
	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(UMOUITokens::Color("color.hud.plate"));
	}
	if (SeverityBar)
	{
		SeverityBar->SetBrushColor(TypeColor);
	}
	if (IconImage)
	{
		if (!bIconExplicit)
		{
			const FName Shape = NotificationShapeIcon(CurrentType);
			if (UTexture2D* Glyph = Shape.IsNone() ? nullptr : UMOUITokens::Icon(Shape))
			{
				IconImage->SetBrushFromTexture(Glyph, /*bMatchSize*/ false);
				IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				IconImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		IconImage->SetColorAndOpacity(TypeColor);
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
