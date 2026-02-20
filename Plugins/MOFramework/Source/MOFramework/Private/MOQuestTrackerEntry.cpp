#include "MOQuestTrackerEntry.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

UMOQuestTrackerEntry::UMOQuestTrackerEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOQuestTrackerEntry::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMOQuestTrackerEntry::SetupEntry(const FMOQuestTrackerDisplayData& InData)
{
	CachedData = InData;
	RefreshDisplay();
}

void UMOQuestTrackerEntry::UpdateProgress(int32 NewProgress, int32 RequiredProgress)
{
	CachedData.CurrentProgress = NewProgress;
	CachedData.RequiredProgress = RequiredProgress;
	CachedData.ProgressPercent = RequiredProgress > 0
		? static_cast<float>(NewProgress) / static_cast<float>(RequiredProgress)
		: 0.0f;

	RefreshDisplay();
	PlayUpdateAnimation();
}

void UMOQuestTrackerEntry::RefreshDisplay()
{
	if (QuestNameText)
	{
		QuestNameText->SetText(CachedData.QuestName);
	}

	if (ObjectiveText)
	{
		ObjectiveText->SetText(CachedData.CurrentObjectiveText);
	}

	if (ProgressText)
	{
		const FText ProgressFormat = FText::Format(
			NSLOCTEXT("MOQuest", "ProgressFormat", "{0}/{1}"),
			FText::AsNumber(CachedData.CurrentProgress),
			FText::AsNumber(CachedData.RequiredProgress)
		);
		ProgressText->SetText(ProgressFormat);
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(CachedData.ProgressPercent);
	}
}

void UMOQuestTrackerEntry::PlayUpdateAnimation_Implementation()
{
	// Default implementation does nothing - Blueprint can override
}

void UMOQuestTrackerEntry::PlayCompletionAnimation_Implementation()
{
	// Default implementation does nothing - Blueprint can override
}
