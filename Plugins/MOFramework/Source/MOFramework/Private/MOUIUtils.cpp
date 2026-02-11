#include "MOUIUtils.h"
#include "MORecipeDefinitionRow.h"
#include "MOItemDatabaseSettings.h"
#include "Components/TextBlock.h"

// Color constants
const FLinearColor UMOUIUtils::ColorAvailable = FLinearColor::White;
const FLinearColor UMOUIUtils::ColorInsufficient = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);
const FLinearColor UMOUIUtils::ColorDisabled = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

FText UMOUIUtils::FormatDurationAsText(float Seconds)
{
	if (Seconds <= 0.0f)
	{
		return NSLOCTEXT("MOUI", "Instant", "Instant");
	}

	if (Seconds >= 86400.0f) // 24 hours = 1 day
	{
		// Days format
		float Days = Seconds / 86400.0f;
		float RoundedDays = FMath::RoundToInt(Days * 10) / 10.0f;
		return FText::Format(NSLOCTEXT("MOUI", "TimeDays", "{0}d"), FText::AsNumber(RoundedDays));
	}
	else if (Seconds >= 3600.0f)
	{
		// Hours and minutes format
		int32 Hours = FMath::FloorToInt(Seconds / 3600.0f);
		int32 Minutes = FMath::FloorToInt(FMath::Fmod(Seconds, 3600.0f) / 60.0f);
		return FText::Format(NSLOCTEXT("MOUI", "TimeHoursMinutes", "{0}h {1}m"), FText::AsNumber(Hours), FText::AsNumber(Minutes));
	}
	else if (Seconds >= 60.0f)
	{
		// Minutes and seconds format
		int32 Minutes = FMath::FloorToInt(Seconds / 60.0f);
		int32 Secs = FMath::FloorToInt(FMath::Fmod(Seconds, 60.0f));
		return FText::Format(NSLOCTEXT("MOUI", "TimeMinutesSeconds", "{0}m {1}s"), FText::AsNumber(Minutes), FText::AsNumber(Secs));
	}
	else
	{
		// Seconds only format
		int32 RoundedSeconds = FMath::RoundToInt(Seconds);
		return FText::Format(NSLOCTEXT("MOUI", "TimeSeconds", "{0}s"), FText::AsNumber(RoundedSeconds));
	}
}

FText UMOUIUtils::FormatDurationAsTimeCode(float Seconds)
{
	if (Seconds <= 0.0f)
	{
		return FText::FromString(TEXT("0:00"));
	}

	int32 TotalSeconds = FMath::FloorToInt(Seconds);
	int32 Hours = TotalSeconds / 3600;
	int32 Minutes = (TotalSeconds % 3600) / 60;
	int32 Secs = TotalSeconds % 60;

	if (Hours > 0)
	{
		return FText::FromString(FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, Secs));
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Secs));
	}
}

FText UMOUIUtils::FormatQuantityDisplay(const FText& DisplayName, int32 Available, int32 Required)
{
	return FText::Format(
		NSLOCTEXT("MOUI", "QuantityFormat", "{0} ({1}/{2})"),
		DisplayName,
		FText::AsNumber(Available),
		FText::AsNumber(Required)
	);
}

FText UMOUIUtils::FormatOutputDisplay(const FText& DisplayName, int32 Quantity, float Chance)
{
	if (Chance < 1.0f && Chance > 0.0f)
	{
		// Show chance if not 100%
		return FText::Format(
			NSLOCTEXT("MOUI", "OutputFormatChance", "{0} x{1} ({2}%)"),
			DisplayName,
			FText::AsNumber(Quantity),
			FText::AsNumber(FMath::RoundToInt(Chance * 100))
		);
	}
	else if (Quantity > 1)
	{
		return FText::Format(
			NSLOCTEXT("MOUI", "OutputFormatQty", "{0} x{1}"),
			DisplayName,
			FText::AsNumber(Quantity)
		);
	}
	else
	{
		return DisplayName;
	}
}

FText UMOUIUtils::FormatActionDisplay(const FText& ActionName, int32 Quantity)
{
	return FText::Format(
		NSLOCTEXT("MOUI", "ActionFormat", "{0} x{1}"),
		ActionName,
		FText::AsNumber(Quantity)
	);
}

FText UMOUIUtils::FormatSkillRequirement(FName SkillId, int32 CurrentLevel, int32 RequiredLevel)
{
	return FText::Format(
		NSLOCTEXT("MOUI", "SkillReq", "{0}: {1}/{2}"),
		FText::FromName(SkillId),
		FText::AsNumber(CurrentLevel),
		FText::AsNumber(RequiredLevel)
	);
}

UTextBlock* UMOUIUtils::CreateSimpleTextWidget(UObject* Outer, const FText& Text, bool bHasEnough, int32 FontSize)
{
	if (!Outer)
	{
		return nullptr;
	}

	UTextBlock* TextWidget = NewObject<UTextBlock>(Outer);
	if (!TextWidget)
	{
		return nullptr;
	}

	TextWidget->SetText(Text);
	TextWidget->SetAutoWrapText(true);

	// Set font size
	FSlateFontInfo FontInfo = TextWidget->GetFont();
	FontInfo.Size = FontSize;
	TextWidget->SetFont(FontInfo);

	// Set color based on whether we have enough
	if (bHasEnough)
	{
		TextWidget->SetColorAndOpacity(FSlateColor(ColorAvailable));
	}
	else
	{
		TextWidget->SetColorAndOpacity(FSlateColor(ColorInsufficient));
	}

	return TextWidget;
}

UTexture2D* UMOUIUtils::LoadRecipeIcon(const FMORecipeDefinitionRow* Recipe)
{
	if (!Recipe)
	{
		return nullptr;
	}

	// First try recipe's own icon
	if (!Recipe->Icon.IsNull())
	{
		UTexture2D* IconTexture = Recipe->Icon.LoadSynchronous();
		if (IconTexture)
		{
			return IconTexture;
		}
	}

	// Fall back to first output item's icon
	if (Recipe->Outputs.Num() > 0)
	{
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Recipe->Outputs[0].ItemDefinitionId, ItemDef))
		{
			if (!ItemDef.UI.IconSmall.IsNull())
			{
				return ItemDef.UI.IconSmall.LoadSynchronous();
			}
		}
	}

	return nullptr;
}

UTexture2D* UMOUIUtils::LoadItemIconSmall(FName ItemDefId)
{
	FMOItemDefinitionRow ItemDef;
	if (UMOItemDatabaseSettings::GetItemDefinition(ItemDefId, ItemDef))
	{
		if (!ItemDef.UI.IconSmall.IsNull())
		{
			return ItemDef.UI.IconSmall.LoadSynchronous();
		}
	}
	return nullptr;
}
