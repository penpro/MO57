#include "MOSkillEntryWidget.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Button.h"

UMOSkillEntryWidget::UMOSkillEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOSkillEntryWidget::SetupEntry(const FMOSkillDisplayData& InData)
{
	SkillData = InData;
	UpdateVisuals();
}

void UMOSkillEntryWidget::UpdateProgress(float NewProgress, float NewCurrentXP, float NewXPToNext)
{
	SkillData.LevelProgress = NewProgress;
	SkillData.CurrentXP = NewCurrentXP;
	SkillData.XPToNextLevel = NewXPToNext;

	if (XPProgressBar)
	{
		XPProgressBar->SetPercent(NewProgress);
	}

	if (XPText)
	{
		XPText->SetText(FText::Format(
			NSLOCTEXT("MOSkills", "XPFormat", "{0} / {1} XP"),
			FText::AsNumber(FMath::RoundToInt(NewCurrentXP)),
			FText::AsNumber(FMath::RoundToInt(NewXPToNext))
		));
	}
}

void UMOSkillEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind the select button click to HandleClicked
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveAll(this);
		SelectButton->OnClicked.AddDynamic(this, &UMOSkillEntryWidget::HandleClicked);
	}
}

void UMOSkillEntryWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMOSkillEntryWidget::UpdateVisuals()
{
	if (SkillNameText)
	{
		SkillNameText->SetText(SkillData.DisplayName);
	}

	if (LevelText)
	{
		LevelText->SetText(FText::Format(
			NSLOCTEXT("MOSkills", "LevelFormat", "Lv. {0}"),
			FText::AsNumber(SkillData.Level)
		));
	}

	if (XPText)
	{
		XPText->SetText(FText::Format(
			NSLOCTEXT("MOSkills", "XPFormat", "{0} / {1} XP"),
			FText::AsNumber(FMath::RoundToInt(SkillData.CurrentXP)),
			FText::AsNumber(FMath::RoundToInt(SkillData.XPToNextLevel))
		));
	}

	if (XPProgressBar)
	{
		XPProgressBar->SetPercent(SkillData.LevelProgress);
	}

	if (SkillIcon && !SkillData.Icon.IsNull())
	{
		UTexture2D* IconTexture = SkillData.Icon.LoadSynchronous();
		if (IconTexture)
		{
			SkillIcon->SetBrushFromTexture(IconTexture);
			SkillIcon->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (SkillIcon)
	{
		SkillIcon->SetVisibility(ESlateVisibility::Hidden);
	}

	if (CategoryText)
	{
		FText CatText;
		switch (SkillData.Category)
		{
		case EMOSkillCategory::Survival:
			CatText = NSLOCTEXT("MOSkills", "CatSurvival", "Survival");
			break;
		case EMOSkillCategory::Crafting:
			CatText = NSLOCTEXT("MOSkills", "CatCrafting", "Crafting");
			break;
		case EMOSkillCategory::Combat:
			CatText = NSLOCTEXT("MOSkills", "CatCombat", "Combat");
			break;
		case EMOSkillCategory::Knowledge:
			CatText = NSLOCTEXT("MOSkills", "CatKnowledge", "Knowledge");
			break;
		case EMOSkillCategory::Social:
			CatText = NSLOCTEXT("MOSkills", "CatSocial", "Social");
			break;
		default:
			CatText = FText::GetEmpty();
			break;
		}
		CategoryText->SetText(CatText);
	}

	OnVisualsUpdated(SkillData);
}

void UMOSkillEntryWidget::HandleClicked()
{
	OnSkillSelected.Broadcast(SkillData.SkillId);
}
