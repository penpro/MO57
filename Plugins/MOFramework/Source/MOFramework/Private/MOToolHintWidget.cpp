#include "MOToolHintWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UMOToolHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Start hidden
	SetRenderOpacity(0.0f);
	bIsVisible = false;
}

void UMOToolHintWidget::NativeDestruct()
{
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	Super::NativeDestruct();
}

void UMOToolHintWidget::ShowHint(const FText& InHintText, float Duration)
{
	if (HintText)
	{
		HintText->SetText(InHintText);
	}

	// Show
	bIsVisible = true;
	PlayFadeIn();

	// Always clear any pending hide first. Re-issuing a show while a previous
	// timer is in flight must NOT inherit that timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	// Duration semantics:
	//   > 0  -> transient: auto-hide after that many seconds
	//   == 0 -> transient: auto-hide after DefaultDuration (legacy default)
	//   < 0  -> PERSISTENT: remembered as the widget's background. No timer
	//           is scheduled. A subsequent transient show displays over it;
	//           when the transient ends (timer fires or EndTransientHint is
	//           called), the persistent text re-displays automatically.
	if (Duration < 0.0f)
	{
		PersistentText = InHintText;
		bHasPersistent = true;
		// No ScheduleHide — persistent stays until HideHint clears it.
	}
	else
	{
		const float ActualDuration = Duration > 0.0f ? Duration : DefaultDuration;
		ScheduleHide(ActualDuration);
	}

	OnHintShown.Broadcast(InHintText);
}

void UMOToolHintWidget::ShowHintWithIcon(const FText& InHintText, UTexture2D* Icon, float Duration)
{
	// For now, just show text. Icon support can be added in Blueprint override.
	ShowHint(InHintText, Duration);
}

void UMOToolHintWidget::HideHint()
{
	// Full hide — clear persistent state too. This is the API for "the owner
	// of the persistent indicator is done" (e.g. terraform mode exit).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	PersistentText = FText::GetEmpty();
	bHasPersistent = false;

	if (!bIsVisible)
	{
		return;
	}

	bIsVisible = false;
	PlayFadeOut();
	OnHintHidden.Broadcast();
}

void UMOToolHintWidget::EndTransientHint()
{
	// Drop any in-flight transient timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	if (bHasPersistent)
	{
		// Re-display the persistent text. This is the focus-hint path:
		// player looked at a tree (transient "Black Alder" shown), then
		// looked away (focus cleared) — but they're still in terraform mode
		// so the "Dig" indicator must remain.
		if (HintText)
		{
			HintText->SetText(PersistentText);
		}
		// Already visible from the transient show; nothing else to do.
		if (!bIsVisible)
		{
			bIsVisible = true;
			PlayFadeIn();
		}
		return;
	}

	// No persistent backing — fully hide, matching the original HideHint
	// behavior for the no-persistent case.
	if (!bIsVisible)
	{
		return;
	}
	bIsVisible = false;
	PlayFadeOut();
	OnHintHidden.Broadcast();
}

void UMOToolHintWidget::PlayFadeIn_Implementation()
{
	// Simple opacity fade - can be overridden in Blueprint for fancier animation
	SetRenderOpacity(1.0f);
}

void UMOToolHintWidget::PlayFadeOut_Implementation()
{
	// Simple opacity fade - can be overridden in Blueprint for fancier animation
	SetRenderOpacity(0.0f);
}

void UMOToolHintWidget::ScheduleHide(float Duration)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Clear existing timer
	World->GetTimerManager().ClearTimer(HideTimerHandle);

	// Schedule new hide
	World->GetTimerManager().SetTimer(
		HideTimerHandle,
		this,
		&UMOToolHintWidget::OnHideTimerExpired,
		Duration,
		false
	);
}

void UMOToolHintWidget::OnHideTimerExpired()
{
	// Transient timer fired. Route through EndTransientHint so we restore the
	// persistent background (if any) instead of clearing it.
	EndTransientHint();
}
