#include "MOSkillEntryWidget.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "TimerManager.h"

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

	// Clear flash timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlashTimerHandle);
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

void UMOSkillEntryWidget::PlayFlashAnimation(float Duration)
{
	if (!XPProgressBar)
	{
		return;
	}

	// Store original color
	OriginalFillColor = XPProgressBar->GetFillColorAndOpacity();

	FlashDuration = FMath::Max(0.1f, Duration);
	FlashElapsedTime = 0.0f;
	bIsFlashing = true;

	// Set initial flash color (bright yellow/gold)
	XPProgressBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f));

	// Start timer for animation
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlashTimerHandle);
		World->GetTimerManager().SetTimer(
			FlashTimerHandle,
			this,
			&UMOSkillEntryWidget::TickFlashAnimation,
			0.03f, // ~30fps tick
			true
		);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSkillEntry] Playing flash animation for skill: %s"), *SkillData.SkillId.ToString());
}

void UMOSkillEntryWidget::TickFlashAnimation()
{
	if (!bIsFlashing || !XPProgressBar)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlashTimerHandle);
		}
		return;
	}

	FlashElapsedTime += 0.03f;

	if (FlashElapsedTime >= FlashDuration)
	{
		// Animation complete - restore original color
		XPProgressBar->SetFillColorAndOpacity(OriginalFillColor);
		bIsFlashing = false;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlashTimerHandle);
		}
		return;
	}

	// Calculate flash effect - pulse between flash color and original
	// Use a sine wave for smooth pulsing effect
	float Alpha = FlashElapsedTime / FlashDuration;
	float PulseFrequency = 3.0f; // Number of pulses during animation
	float PulseValue = FMath::Sin(Alpha * PulseFrequency * PI * 2.0f) * 0.5f + 0.5f;

	// Fade out the pulse intensity over time
	float FadeOut = 1.0f - Alpha;
	PulseValue *= FadeOut;

	// Lerp between flash color (gold) and original
	FLinearColor FlashColor(1.0f, 0.85f, 0.0f, 1.0f);
	FLinearColor CurrentColor = FMath::Lerp(OriginalFillColor, FlashColor, PulseValue);
	XPProgressBar->SetFillColorAndOpacity(CurrentColor);
}
