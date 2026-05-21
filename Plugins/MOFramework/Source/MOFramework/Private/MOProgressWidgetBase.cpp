/**
 * MOProgressWidgetBase.cpp - Generic Progress Widget Base Implementation
 */

#include "MOProgressWidgetBase.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"

UMOProgressWidgetBase::UMOProgressWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UMOProgressWidgetBase::GetDesiredInputConfig() const
{
	// Game mode — progress bars are passive HUD overlays, not interactive menus.
	// ESC cancellation works via CommonUI back handler (bIsBackHandler on base class).
	// Cursor stays hidden so the progress bar doesn't fight context menu cursor state.
	return FUIInputConfig(
		ECommonInputMode::Game,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		true
	);
}

void UMOProgressWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOProgressWidgetBase::HandleCancelClicked);
	}

	// Start with progress at zero
	if (ProgressBar)
	{
		ProgressBar->SetPercent(0.0f);
	}
}

void UMOProgressWidgetBase::NativeDestruct()
{
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMOProgressWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsProgressActive && bIsTimedProgress && TotalDuration > 0.0f)
	{
		// Use wall-clock time for accurate progress even if frame rate drops
		const double CurrentTime = FPlatformTime::Seconds();
		const float ElapsedTime = static_cast<float>(CurrentTime - ProgressStartTime);
		CurrentProgress = FMath::Clamp(ElapsedTime / TotalDuration, 0.0f, 1.0f);

		const float Remaining = FMath::Max(0.0f, TotalDuration - ElapsedTime);
		UpdateDisplay(CurrentProgress, Remaining, ActionName);

		// Check for completion
		if (ElapsedTime >= TotalDuration)
		{
			CompleteProgress(true);
		}
	}
}

// ============================================================================
// PROGRESS CONTROL
// ============================================================================

void UMOProgressWidgetBase::StartProgress(const FText& InActionName, float Duration)
{
	ActionName = InActionName;
	TotalDuration = Duration;
	CurrentProgress = 0.0f;
	bIsProgressActive = true;
	bIsTimedProgress = true;
	ProgressStartTime = FPlatformTime::Seconds();

	UpdateDisplay(0.0f, Duration, ActionName);
}

void UMOProgressWidgetBase::CancelProgress()
{
	if (bIsProgressActive)
	{
		bIsProgressActive = false;
		bIsTimedProgress = false;
		OnCancelled.Broadcast();
	}
}

void UMOProgressWidgetBase::SetProgress(float Progress, float TimeRemaining)
{
	CurrentProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	bIsProgressActive = true;
	bIsTimedProgress = false;

	if (TimeRemaining >= 0.0f)
	{
		// Estimate total duration based on progress and remaining time
		if (CurrentProgress > 0.0f && CurrentProgress < 1.0f)
		{
			TotalDuration = TimeRemaining / (1.0f - CurrentProgress);
		}
	}

	UpdateDisplay(CurrentProgress, TimeRemaining, ActionName);
}

void UMOProgressWidgetBase::CompleteProgress(bool bSuccess)
{
	if (bIsProgressActive)
	{
		bIsProgressActive = false;
		bIsTimedProgress = false;
		CurrentProgress = 1.0f;

		UpdateDisplay(1.0f, 0.0f, ActionName);

		if (bSuccess)
		{
			OnProgressSuccess();
		}

		OnCompleted.Broadcast(bSuccess);
	}
}

// ============================================================================
// STATE QUERIES
// ============================================================================

float UMOProgressWidgetBase::GetElapsedTime() const
{
	if (!bIsProgressActive)
	{
		return 0.0f;
	}

	if (bIsTimedProgress)
	{
		const double CurrentTime = FPlatformTime::Seconds();
		return static_cast<float>(CurrentTime - ProgressStartTime);
	}

	// For non-timed progress, estimate from current progress
	return CurrentProgress * TotalDuration;
}

float UMOProgressWidgetBase::GetTimeRemaining() const
{
	if (!bIsProgressActive)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, TotalDuration - GetElapsedTime());
}

// ============================================================================
// DISPLAY
// ============================================================================

void UMOProgressWidgetBase::UpdateDisplay_Implementation(float Progress, float TimeRemaining, const FText& InActionName)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Progress);
	}

	if (ActionNameText)
	{
		ActionNameText->SetText(InActionName);
	}

	if (TimeRemainingText)
	{
		// Format as X.Xs
		const FText TimeText = FText::Format(
			NSLOCTEXT("MOProgress", "TimeFormat", "{0}s"),
			FText::AsNumber(FMath::CeilToInt(TimeRemaining))
		);
		TimeRemainingText->SetText(TimeText);
	}
}

void UMOProgressWidgetBase::OnProgressSuccess_Implementation()
{
	// Base implementation does nothing
	// Subclasses override to add domain-specific completion logic
}

void UMOProgressWidgetBase::HandleCancelClicked()
{
	CancelProgress();
}
