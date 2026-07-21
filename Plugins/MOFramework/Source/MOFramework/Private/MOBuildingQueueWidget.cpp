#include "MOBuildingQueueWidget.h"
#include "MOFramework.h"
#include "MOBuildProgressComponent.h"
#include "MOBuildingQueueEntryWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MOCommonButton.h"
#include "MOUIUtils.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"

UMOBuildingQueueWidget::UMOBuildingQueueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOBuildingQueueWidget::InitializeQueue(UMOBuildProgressComponent* InProgressComponent)
{
	// Unbind from previous component
	if (UMOBuildProgressComponent* OldComponent = ProgressComponent.Get())
	{
		OldComponent->OnConstructionStateChanged.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionStateChanged);
		OldComponent->OnConstructionProgress.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionProgress);
		OldComponent->OnConstructionCompleted.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionCompleted);
	}

	ProgressComponent = InProgressComponent;

	// Bind to new component
	if (InProgressComponent)
	{
		InProgressComponent->OnConstructionStateChanged.AddDynamic(this, &UMOBuildingQueueWidget::HandleConstructionStateChanged);
		InProgressComponent->OnConstructionProgress.AddDynamic(this, &UMOBuildingQueueWidget::HandleConstructionProgress);
		InProgressComponent->OnConstructionCompleted.AddDynamic(this, &UMOBuildingQueueWidget::HandleConstructionCompleted);
	}

	RefreshQueue();
}

void UMOBuildingQueueWidget::RefreshQueue()
{
	// Clear existing entries
	for (UMOBuildingQueueEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}
	EntryWidgets.Empty();

	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component)
	{
		return;
	}

	// Get container
	UPanelWidget* Container = QueueScrollBox ? Cast<UPanelWidget>(QueueScrollBox) : Cast<UPanelWidget>(QueueContainer);

	// Check if we have an active construction
	EMOBuildState State = Component->GetState();
	bool bHasActiveConstruction = (State == EMOBuildState::Constructing || State == EMOBuildState::Paused);

	// Update empty state visibility
	if (EmptyQueueText)
	{
		EmptyQueueText->SetVisibility(!bHasActiveConstruction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!Container || !QueueEntryWidgetClass || !bHasActiveConstruction)
	{
		OnQueueUpdated(bHasActiveConstruction ? 1 : 0);
		return;
	}

	// Create entry widget for the current construction
	UMOBuildingQueueEntryWidget* EntryWidget = CreateWidget<UMOBuildingQueueEntryWidget>(this, QueueEntryWidgetClass);
	if (EntryWidget)
	{
		// Build display data
		FMOBuildQueueEntryDisplayData DisplayData;
		DisplayData.EntryId = FGuid::NewGuid(); // Use a unique ID for tracking
		DisplayData.RecipeId = Component->GetRecipeId();
		DisplayData.Progress = Component->GetProgress();
		DisplayData.bIsActive = (State == EMOBuildState::Constructing);
		DisplayData.State = State;

		// Get recipe info
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(DisplayData.RecipeId);
		if (Recipe)
		{
			DisplayData.RecipeName = Recipe->DisplayName;
			DisplayData.Icon = Recipe->Icon;
		}
		else
		{
			DisplayData.RecipeName = FText::FromName(DisplayData.RecipeId);
		}

		// Format part count
		int32 CurrentPart = Component->GetCurrentPartIndex() + 1;
		int32 TotalParts = Recipe ? Recipe->BuildParts.Num() : 1;
		DisplayData.CountText = FText::Format(
			NSLOCTEXT("MOBuilding", "PartCount", "{0}/{1}"),
			FText::AsNumber(CurrentPart),
			FText::AsNumber(TotalParts)
		);

		// Calculate time remaining
		DisplayData.TimeRemainingText = UMOUIUtils::FormatDurationAsText(Component->GetTimeRemaining());

		EntryWidget->SetupEntry(DisplayData);
		EntryWidget->OnCancelRequested.AddDynamic(this, &UMOBuildingQueueWidget::HandleEntryCancelRequested);

		Container->AddChild(EntryWidget);
		EntryWidgets.Add(EntryWidget);
	}

	// Update current construction display
	if (bHasActiveConstruction)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Component->GetRecipeId());

		if (CurrentCraftNameText && Recipe)
		{
			CurrentCraftNameText->SetText(Recipe->DisplayName);
		}

		float Progress = Component->GetProgress();

		if (CurrentProgressBar)
		{
			CurrentProgressBar->SetPercent(Progress);
		}

		if (ProgressText)
		{
			ProgressText->SetText(FText::Format(
				NSLOCTEXT("MOBuilding", "ProgressPercent", "{0}%"),
				FText::AsNumber(FMath::RoundToInt(Progress * 100))
			));
		}

		FText TimeRemaining = UMOUIUtils::FormatDurationAsText(Component->GetTimeRemaining());

		if (TimeRemainingText)
		{
			TimeRemainingText->SetText(TimeRemaining);
		}

		if (TotalTimeRemainingText)
		{
			TotalTimeRemainingText->SetText(TimeRemaining);
		}
	}
	else
	{
		// No active construction - clear the display
		if (CurrentCraftNameText)
		{
			CurrentCraftNameText->SetText(FText::GetEmpty());
		}

		if (CurrentProgressBar)
		{
			CurrentProgressBar->SetPercent(0.0f);
		}

		if (ProgressText)
		{
			ProgressText->SetText(FText::GetEmpty());
		}

		if (TimeRemainingText)
		{
			TimeRemainingText->SetText(FText::GetEmpty());
		}

		if (TotalTimeRemainingText)
		{
			TotalTimeRemainingText->SetText(FText::GetEmpty());
		}
	}

	OnQueueUpdated(bHasActiveConstruction ? 1 : 0);
}

void UMOBuildingQueueWidget::UpdateProgress()
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component)
	{
		return;
	}

	EMOBuildState State = Component->GetState();
	bool bHasActiveConstruction = (State == EMOBuildState::Constructing || State == EMOBuildState::Paused);

	if (!bHasActiveConstruction)
	{
		// No active construction, ensure display is cleared
		if (CurrentProgressBar)
		{
			CurrentProgressBar->SetPercent(0.0f);
		}
		if (ProgressText)
		{
			ProgressText->SetText(FText::GetEmpty());
		}
		if (TimeRemainingText)
		{
			TimeRemainingText->SetText(FText::GetEmpty());
		}
		if (TotalTimeRemainingText)
		{
			TotalTimeRemainingText->SetText(FText::GetEmpty());
		}
		OnProgressUpdated(0.0f, FText::GetEmpty());
		return;
	}

	float Progress = Component->GetProgress();
	FText TimeRemaining = UMOUIUtils::FormatDurationAsText(Component->GetTimeRemaining());

	if (CurrentProgressBar)
	{
		CurrentProgressBar->SetPercent(Progress);
	}

	if (ProgressText)
	{
		ProgressText->SetText(FText::Format(
			NSLOCTEXT("MOBuilding", "ProgressPercent", "{0}%"),
			FText::AsNumber(FMath::RoundToInt(Progress * 100))
		));
	}

	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(TimeRemaining);
	}

	if (TotalTimeRemainingText)
	{
		TotalTimeRemainingText->SetText(TimeRemaining);
	}

	// Update entry widget
	if (EntryWidgets.Num() > 0 && EntryWidgets[0])
	{
		EntryWidgets[0]->UpdateProgress(Progress, TimeRemaining);
	}

	OnProgressUpdated(Progress, TimeRemaining);
}

bool UMOBuildingQueueWidget::IsQueueEmpty() const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component)
	{
		return true;
	}

	EMOBuildState State = Component->GetState();
	return (State == EMOBuildState::Ghost || State == EMOBuildState::Complete);
}

int32 UMOBuildingQueueWidget::GetQueueLength() const
{
	return IsQueueEmpty() ? 0 : 1;
}

float UMOBuildingQueueWidget::GetCurrentProgress() const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	return Component ? Component->GetProgress() : 0.0f;
}

FText UMOBuildingQueueWidget::GetTimeRemainingText() const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component)
	{
		return FText::GetEmpty();
	}
	return UMOUIUtils::FormatDurationAsText(Component->GetTimeRemaining());
}

void UMOBuildingQueueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CancelAllButton)
	{
		CancelAllButton->OnClicked().RemoveAll(this);
		CancelAllButton->OnClicked().AddUObject(this, &UMOBuildingQueueWidget::HandleCancelAllClicked);
	}
}

void UMOBuildingQueueWidget::NativeDestruct()
{
	if (CancelAllButton)
	{
		CancelAllButton->OnClicked().RemoveAll(this);
	}

	// Unbind from progress component
	if (UMOBuildProgressComponent* Component = ProgressComponent.Get())
	{
		Component->OnConstructionStateChanged.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionStateChanged);
		Component->OnConstructionProgress.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionProgress);
		Component->OnConstructionCompleted.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionCompleted);
	}

	Super::NativeDestruct();
}

void UMOBuildingQueueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Periodic progress update
	TimeSinceLastUpdate += InDeltaTime;
	if (TimeSinceLastUpdate >= ProgressUpdateInterval)
	{
		TimeSinceLastUpdate = 0.0f;
		UpdateProgress();
	}
}

void UMOBuildingQueueWidget::HandleConstructionStateChanged(EMOBuildState NewState)
{
	RefreshQueue();
}

void UMOBuildingQueueWidget::HandleConstructionProgress(float Progress)
{
	// Progress is handled by tick-based updates for smoother display
}

void UMOBuildingQueueWidget::HandleConstructionCompleted()
{
	RefreshQueue();
}

void UMOBuildingQueueWidget::HandleEntryCancelRequested(const FGuid& EntryId)
{
	if (UMOBuildProgressComponent* Component = ProgressComponent.Get())
	{
		Component->CancelConstruction(true); // true = refund materials
	}
}

void UMOBuildingQueueWidget::HandleCancelAllClicked()
{
	if (UMOBuildProgressComponent* Component = ProgressComponent.Get())
	{
		Component->CancelConstruction(true); // true = refund materials
	}
}
