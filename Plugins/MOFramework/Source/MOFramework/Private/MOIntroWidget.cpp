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

	UE_LOG(LogMOFramework, Warning, TEXT("[Intro-DIAG] NativeConstruct - VideoImage: %s, IsFocusable: %s, IsActivated: %d"),
		VideoImage ? TEXT("OK") : TEXT("NULL"),
		IsFocusable() ? TEXT("YES") : TEXT("NO"),
		IsActivated() ? 1 : 0);
}

UWidget* UMOIntroWidget::NativeGetDesiredFocusTarget() const
{
	UE_LOG(LogMOFramework, Warning, TEXT("[Intro-DIAG] NativeGetDesiredFocusTarget -> self"));
	return const_cast<UMOIntroWidget*>(this);
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

FReply UMOIntroWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Preview handlers fire first and catch input that might be consumed elsewhere
	if (bIsActive)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Preview key pressed: %s - skipping intro"), *InKeyEvent.GetKey().ToString());
		SkipIntro();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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

FReply UMOIntroWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsActive)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOIntroWidget] Preview mouse clicked - skipping intro"));
		SkipIntro();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
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
	UE_LOG(LogMOFramework, Warning, TEXT("[Intro-DIAG] SetVideoMaterial: VideoImage=%s, Material=%s"),
		VideoImage ? TEXT("OK") : TEXT("NULL"),
		InVideoMaterial ? *InVideoMaterial->GetName() : TEXT("NULL"));

	if (VideoImage && InVideoMaterial)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(InVideoMaterial);
		Brush.ImageSize = FVector2D(1920, 1080);  // Will be stretched to fit
		Brush.DrawAs = ESlateBrushDrawType::Image;
		VideoImage->SetBrush(Brush);

		UE_LOG(LogMOFramework, Warning, TEXT("[Intro-DIAG] Video material applied to brush"));
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
	UE_LOG(LogMOFramework, Warning, TEXT("[Intro-DIAG] SkipIntro called, bIsActive=%d"), bIsActive ? 1 : 0);
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;
	OnIntroSkipped.Broadcast();
	UE_LOG(LogMOFramework, Warning, TEXT("[Intro-DIAG] SkipIntro: OnIntroSkipped broadcast complete, calling DeactivateWidget"));
	DeactivateWidget();
}

void UMOIntroWidget::OnVideoFinished()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[Intro-DIAG] OnVideoFinished called, bIsActive=%d"), bIsActive ? 1 : 0);
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;
	OnIntroCompleted.Broadcast();
	DeactivateWidget();
}
