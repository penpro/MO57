/**
 * MOUIStyleConstants.cpp - UI style constant implementations
 */

#include "UI/MOUIStyleConstants.h"

FLinearColor UMOUIStyleConstants::GetColor(EMOUIColor ColorType)
{
	switch (ColorType)
	{
	// Text colors
	case EMOUIColor::TextPrimary:		return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);		// #FFFFFF
	case EMOUIColor::TextSecondary:		return FLinearColor(0.28f, 0.28f, 0.28f, 1.0f);		// #474747
	case EMOUIColor::TextMuted:			return FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);		// #5A5A5A
	case EMOUIColor::TextAccent:		return FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);		// Blue accent
	case EMOUIColor::TextError:			return FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);		// Red
	case EMOUIColor::TextSuccess:		return FLinearColor(0.3f, 1.0f, 0.3f, 1.0f);		// Green
	case EMOUIColor::TextWarning:		return FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);		// Yellow

	// Background colors
	case EMOUIColor::BackgroundDark:	return FLinearColor(0.086f, 0.086f, 0.086f, 1.0f);	// #161616
	case EMOUIColor::BackgroundMedium:	return FLinearColor(0.12f, 0.12f, 0.12f, 1.0f);		// #1F1F1F
	case EMOUIColor::BackgroundLight:	return FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);		// #343434
	case EMOUIColor::BackgroundOverlay:	return FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);		// Semi-transparent

	// Interactive colors
	case EMOUIColor::ButtonNormal:		return FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
	case EMOUIColor::ButtonHovered:		return FLinearColor(0.25f, 0.25f, 0.25f, 1.0f);
	case EMOUIColor::ButtonPressed:		return FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
	case EMOUIColor::ButtonDisabled:	return FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);

	// Border colors
	case EMOUIColor::BorderDefault:		return FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
	case EMOUIColor::BorderFocused:		return FLinearColor(0.4f, 0.6f, 1.0f, 1.0f);
	case EMOUIColor::BorderError:		return FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

	// Progress/Status colors
	case EMOUIColor::ProgressFill:		return FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);
	case EMOUIColor::ProgressBackground:return FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
	case EMOUIColor::HealthBar:			return FLinearColor(0.8f, 0.2f, 0.2f, 1.0f);
	case EMOUIColor::StaminaBar:		return FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);
	case EMOUIColor::HungerBar:			return FLinearColor(0.9f, 0.6f, 0.1f, 1.0f);

	default:
		return FLinearColor::White;
	}
}

FSlateColor UMOUIStyleConstants::GetSlateColor(EMOUIColor ColorType)
{
	return FSlateColor(GetColor(ColorType));
}

int32 UMOUIStyleConstants::GetFontSize(EMOFontSize SizePreset)
{
	switch (SizePreset)
	{
	case EMOFontSize::Small:		return 10;
	case EMOFontSize::Body:			return 12;
	case EMOFontSize::BodyLarge:	return 14;
	case EMOFontSize::Header:		return 16;
	case EMOFontSize::HeaderLarge:	return 20;
	case EMOFontSize::Title:		return 24;
	case EMOFontSize::TitleLarge:	return 32;
	case EMOFontSize::Default:
	default:						return 12;
	}
}

FVector2D UMOUIStyleConstants::GetWidgetSize(EMOWidgetSize SizePreset)
{
	switch (SizePreset)
	{
	// Buttons
	case EMOWidgetSize::ButtonSmall:	return FVector2D(100.0f, 30.0f);
	case EMOWidgetSize::ButtonMedium:	return FVector2D(150.0f, 40.0f);
	case EMOWidgetSize::ButtonLarge:	return FVector2D(200.0f, 50.0f);

	// List entries
	case EMOWidgetSize::EntryCompact:	return FVector2D(0.0f, 30.0f);
	case EMOWidgetSize::EntryNormal:	return FVector2D(0.0f, 40.0f);
	case EMOWidgetSize::EntryLarge:		return FVector2D(0.0f, 60.0f);

	// Icons
	case EMOWidgetSize::IconSmall:		return FVector2D(24.0f, 24.0f);
	case EMOWidgetSize::IconMedium:		return FVector2D(48.0f, 48.0f);
	case EMOWidgetSize::IconLarge:		return FVector2D(80.0f, 80.0f);
	case EMOWidgetSize::IconXLarge:		return FVector2D(100.0f, 100.0f);

	// Panels
	case EMOWidgetSize::PanelSmall:		return FVector2D(300.0f, 200.0f);
	case EMOWidgetSize::PanelMedium:	return FVector2D(500.0f, 400.0f);
	case EMOWidgetSize::PanelLarge:		return FVector2D(800.0f, 600.0f);
	case EMOWidgetSize::PanelFullscreen:return FVector2D(0.0f, 0.0f); // Use stretch anchors

	default:
		return FVector2D(100.0f, 100.0f);
	}
}

FVector2D UMOUIStyleConstants::GetButtonSize(bool bLarge)
{
	return GetWidgetSize(bLarge ? EMOWidgetSize::ButtonLarge : EMOWidgetSize::ButtonMedium);
}

float UMOUIStyleConstants::GetEntryHeight(bool bCompact)
{
	return GetWidgetSize(bCompact ? EMOWidgetSize::EntryCompact : EMOWidgetSize::EntryNormal).Y;
}

float UMOUIStyleConstants::GetIconSize(bool bLarge)
{
	return GetWidgetSize(bLarge ? EMOWidgetSize::IconLarge : EMOWidgetSize::IconMedium).X;
}

float UMOUIStyleConstants::GetPadding(bool bLarge)
{
	return bLarge ? 16.0f : 8.0f;
}

FMargin UMOUIStyleConstants::GetMargin(bool bLarge)
{
	float Value = GetPadding(bLarge);
	return FMargin(Value);
}

float UMOUIStyleConstants::GetSpacing(bool bLarge)
{
	return bLarge ? 12.0f : 6.0f;
}
