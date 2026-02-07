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

	// Show and schedule hide
	bIsVisible = true;
	PlayFadeIn();

	float ActualDuration = Duration > 0.0f ? Duration : DefaultDuration;
	ScheduleHide(ActualDuration);

	OnHintShown.Broadcast(InHintText);
}

void UMOToolHintWidget::ShowHintWithIcon(const FText& InHintText, UTexture2D* Icon, float Duration)
{
	// For now, just show text. Icon support can be added in Blueprint override.
	ShowHint(InHintText, Duration);
}

void UMOToolHintWidget::HideHint()
{
	if (!bIsVisible)
	{
		return;
	}

	// Clear pending timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
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
	HideHint();
}
