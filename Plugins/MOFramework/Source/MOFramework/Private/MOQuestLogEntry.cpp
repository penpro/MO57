#include "MOQuestLogEntry.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"

UMOQuestLogEntry::UMOQuestLogEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOQuestLogEntry::NativeConstruct()
{
	Super::NativeConstruct();

	if (SelectButton)
	{
		SelectButton->OnClicked().RemoveAll(this);
		SelectButton->OnClicked().AddUObject(this, &UMOQuestLogEntry::HandleSelectClicked);
	}
}

void UMOQuestLogEntry::SetupEntry(const FMOQuestLogDisplayData& InData)
{
	CachedData = InData;
	RefreshDisplay();
}

void UMOQuestLogEntry::SetSelected(bool bSelected)
{
	if (bIsSelected == bSelected)
	{
		return;
	}

	bIsSelected = bSelected;
	OnSelectionChanged(bSelected);
}

void UMOQuestLogEntry::HandleSelectClicked()
{
	OnQuestSelected.ExecuteIfBound(CachedData.QuestId);
}

void UMOQuestLogEntry::RefreshDisplay()
{
	if (QuestNameText)
	{
		QuestNameText->SetText(CachedData.DisplayName);
	}

	// Set status text based on quest state
	if (StatusText)
	{
		FText Status;
		if (CachedData.bIsComplete)
		{
			Status = NSLOCTEXT("MOQuest", "StatusComplete", "Complete");
		}
		else if (CachedData.bIsTracked)
		{
			Status = NSLOCTEXT("MOQuest", "StatusTracking", "Tracking");
		}
		else
		{
			Status = NSLOCTEXT("MOQuest", "StatusActive", "Active");
		}
		StatusText->SetText(Status);
	}

	// Show objective progress for incomplete quests
	if (ProgressText)
	{
		if (CachedData.bIsComplete)
		{
			ProgressText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ProgressText->SetVisibility(ESlateVisibility::Visible);
			const FText ProgressFormat = FText::Format(
				NSLOCTEXT("MOQuest", "ObjectiveProgress", "{0}/{1}"),
				FText::AsNumber(CachedData.CompletedObjectives),
				FText::AsNumber(CachedData.TotalObjectives)
			);
			ProgressText->SetText(ProgressFormat);
		}
	}
}
