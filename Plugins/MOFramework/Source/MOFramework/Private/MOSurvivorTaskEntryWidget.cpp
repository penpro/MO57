#include "MOSurvivorTaskEntryWidget.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UMOSurvivorTaskEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AddButton)
	{
		AddButton->OnClicked().RemoveAll(this);
		AddButton->OnClicked().AddUObject(this, &UMOSurvivorTaskEntryWidget::HandleAddClicked);
	}
}

void UMOSurvivorTaskEntryWidget::InitializeFromJobDefinition(const FMOSurvivorJobDefinitionRow& JobDef)
{
	JobType = JobDef.JobType;
	CachedJobDef = JobDef;

	// Update display
	if (TaskNameText)
	{
		TaskNameText->SetText(JobDef.DisplayName);
	}

	if (TaskDescriptionText)
	{
		TaskDescriptionText->SetText(JobDef.Description);
	}

	if (TaskIcon && !JobDef.Icon.IsNull())
	{
		if (UTexture2D* IconTexture = JobDef.Icon.LoadSynchronous())
		{
			TaskIcon->SetBrushFromTexture(IconTexture);
			TaskIcon->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			TaskIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else if (TaskIcon)
	{
		TaskIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorTaskEntry] Initialized for job type %d: %s"),
		static_cast<int32>(JobType), *JobDef.DisplayName.ToString());
}

void UMOSurvivorTaskEntryWidget::HandleAddClicked()
{
	if (JobType != EMOSurvivorJobType::None)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorTaskEntry] Task clicked: %s (Type: %d)"),
			*CachedJobDef.DisplayName.ToString(), static_cast<int32>(JobType));
		OnTaskClicked.Broadcast(JobType);
	}
}
