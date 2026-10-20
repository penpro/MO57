#include "MOSkillToastWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "MOSkillDatabaseSettings.h"
#include "MOUITokens.h"

#define LOCTEXT_NAMESPACE "MOSkillToast"

FText UMOSkillToastWidget::Headline(const FMOSkillToastData& Data)
{
	const FText Name = Data.DisplayName.IsEmpty() ? UMOSkillDatabaseSettings::GetSkillOrKnowledgeDisplayName(Data.Id) : Data.DisplayName;
	if (Data.LeveledUp())
	{
		if (Data.bIsKnowledge)
		{
			return Data.LevelBefore <= 0
				? FText::Format(LOCTEXT("NewKnowledge", "New knowledge: {0}"), Name)
				: FText::Format(LOCTEXT("KnowledgeLevel", "{0} knowledge: level {1}"), Name, FText::AsNumber(Data.LevelAfter));
		}
		return FText::Format(LOCTEXT("SkillLevel", "{0} reached level {1}"), Name, FText::AsNumber(Data.LevelAfter));
	}
	if (Data.XPGained > 0.0f)
	{
		const FText XP = FText::AsNumber(FMath::RoundToInt(Data.XPGained));
		return Data.bIsKnowledge
			? FText::Format(LOCTEXT("KnowledgeXP", "{0} knowledge +{1} XP"), Name, XP)
			: FText::Format(LOCTEXT("SkillXP", "{0} +{1} XP"), Name, XP);
	}
	return Name;
}

FText UMOSkillToastWidget::Detail(const FMOSkillToastData& Data)
{
	if (Data.Level >= Data.MaxLevel)
	{
		return FText::Format(LOCTEXT("Mastered", "Level {0} (mastered)"), FText::AsNumber(Data.Level));
	}
	return FText::Format(LOCTEXT("Standing", "Level {0}  ·  {1} / {2} XP to level {3}"), FText::AsNumber(Data.Level),
		FText::AsNumber(FMath::FloorToInt(Data.CurrentXP)), FText::AsNumber(FMath::RoundToInt(Data.XPToNextLevel)), FText::AsNumber(Data.Level + 1));
}

void UMOSkillToastWidget::SetupToast(const FMOSkillToastData& InData)
{
	if (HeadlineText)
	{
		HeadlineText->SetText(Headline(InData));
		HeadlineText->SetColorAndOpacity(FSlateColor(UMOUITokens::Color("color.text.primary")));
	}
	if (DetailText)
	{
		DetailText->SetText(Detail(InData));
		DetailText->SetColorAndOpacity(FSlateColor(UMOUITokens::Color("color.text.secondary")));
	}
	const FLinearColor Accent = UMOUITokens::Color(InData.bIsKnowledge ? FName("color.focus.ring") : FName("color.accent"));
	if (XPProgressBar)
	{
		XPProgressBar->SetPercent(InData.LevelProgress());
		XPProgressBar->SetFillColorAndOpacity(Accent);
	}
	if (AccentBar)
	{
		AccentBar->SetBrushColor(Accent);
	}
	if (ToastIcon)
	{
		if (UTexture2D* Glyph = UMOUITokens::Icon(InData.bIsKnowledge ? FName("ic_item_book") : FName("ic_ui_skill")))
		{
			ToastIcon->SetBrushFromTexture(Glyph, /*bMatchSize=*/false);
		}
		ToastIcon->SetColorAndOpacity(Accent);
	}
}

#undef LOCTEXT_NAMESPACE
