/**
 * MOWindDirectionWidget.cpp - HUD wind direction indicator
 */

#include "MOWindDirectionWidget.h"
#include "MOFramework.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOWeatherTypes.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UMOWindDirectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		// Recurring tick via timer rather than overriding NativeTick — the
		// refresh is rare enough that a 4Hz timer is more honest than a
		// frame-rate tick that throws most of its work away.
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle, this, &UMOWindDirectionWidget::RefreshFromWeather,
			RefreshIntervalSeconds, /*bLoop=*/true, /*FirstDelay=*/0.0f);
	}

	RefreshFromWeather();
}

void UMOWindDirectionWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void UMOWindDirectionWidget::RefreshFromWeather()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMOWeatherIntegrationSubsystem* WeatherSys = World->GetSubsystem<UMOWeatherIntegrationSubsystem>();
	if (!WeatherSys)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FMOWeatherState State = WeatherSys->GetCurrentWeatherState();

	// Calm days don't display the indicator — useless cognitive load to show
	// an arrow that means "nothing's blowing." Above the threshold we render.
	if (State.WindIntensity < HideBelowIntensity)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// "Relative wind" = where the wind is going minus where the player is
	// facing. Subtracting yaws puts the arrow in the player's local frame.
	float PlayerYaw = 0.0f;
	if (APlayerController* PC = GetOwningPlayer())
	{
		PlayerYaw = PC->GetControlRotation().Yaw;
	}

	const float RelativeYaw = FRotator::NormalizeAxis(State.WindDirection.Yaw - PlayerYaw);

	if (WindArrow)
	{
		// UMG render transform rotates clockwise positive — Unreal world yaw
		// is also clockwise positive (looking down +Z), so we can map straight
		// across. If the arrow visually feels backwards in-game, flip the
		// sign here once and document why.
		WindArrow->SetRenderTransformAngle(RelativeYaw);
	}

	if (WindLabel)
	{
		// Short qualitative descriptor based on relative angle:
		// front 45° = "in your face", back = "at your back", etc. Gives the
		// player text-readable info too without needing to parse the arrow.
		FText Direction;
		if      (FMath::Abs(RelativeYaw) < 45.0f)         Direction = NSLOCTEXT("MO", "Wind.Behind", "at your back");
		else if (FMath::Abs(RelativeYaw) > 135.0f)        Direction = NSLOCTEXT("MO", "Wind.Face",   "in your face");
		else if (RelativeYaw > 0)                          Direction = NSLOCTEXT("MO", "Wind.Right",  "from your left");
		else                                               Direction = NSLOCTEXT("MO", "Wind.Left",   "from your right");

		// Intensity descriptor — paleolithic naming, no numbers.
		FText Strength;
		if      (State.WindIntensity < 0.30f) Strength = NSLOCTEXT("MO", "Wind.Light",     "Light wind");
		else if (State.WindIntensity < 0.65f) Strength = NSLOCTEXT("MO", "Wind.Moderate",  "Wind");
		else                                  Strength = NSLOCTEXT("MO", "Wind.Strong",    "Strong wind");

		WindLabel->SetText(FText::Format(
			NSLOCTEXT("MO", "Wind.LabelFormat", "{0} {1}"), Strength, Direction));
	}
}
