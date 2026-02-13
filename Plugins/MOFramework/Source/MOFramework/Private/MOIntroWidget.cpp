#include "MOIntroWidget.h"
#include "MOFramework.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

UMOIntroWidget::UMOIntroWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOIntroWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Hide skip hint initially
	if (SkipHintText)
	{
		SkipHintText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] NativeConstruct - VideoImage: %s"),
		VideoImage ? TEXT("OK") : TEXT("NULL"));
}

void UMOIntroWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsActive)
	{
		ElapsedTime += InDeltaTime;

		// Show skip hint after delay
		if (ElapsedTime >= SkipHintDelay && SkipHintText)
		{
			if (SkipHintText->GetVisibility() == ESlateVisibility::Collapsed)
			{
				SkipHintText->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

FReply UMOIntroWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bIsActive)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Key pressed: %s - skipping intro"), *InKeyEvent.GetKey().ToString());
		SkipIntro();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UMOIntroWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsActive)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Mouse clicked - skipping intro"));
		SkipIntro();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMOIntroWidget::SetVideoMaterial(UMaterialInstanceDynamic* InVideoMaterial)
{
	if (VideoImage && InVideoMaterial)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(InVideoMaterial);
		Brush.ImageSize = FVector2D(1920, 1080);  // Will be stretched to fit
		Brush.DrawAs = ESlateBrushDrawType::Image;
		VideoImage->SetBrush(Brush);

		UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Video material set"));
	}
}

void UMOIntroWidget::OnPlaybackStarted()
{
	bIsActive = true;
	ElapsedTime = 0.0f;

	if (SkipHintText)
	{
		SkipHintText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Playback started"));
}

void UMOIntroWidget::SkipIntro()
{
	if (!bIsActive)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Intro skipped"));
	bIsActive = false;
	OnIntroSkipped.Broadcast();
}

void UMOIntroWidget::OnVideoFinished()
{
	if (!bIsActive)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Video finished"));
	bIsActive = false;
	OnIntroCompleted.Broadcast();
}
