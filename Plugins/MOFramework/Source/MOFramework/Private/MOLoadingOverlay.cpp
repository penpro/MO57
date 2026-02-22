#include "MOLoadingOverlay.h"
#include "MOFramework.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

UMOLoadingOverlay::UMOLoadingOverlay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOLoadingOverlay::NativeConstruct()
{
	Super::NativeConstruct();

	// Start fully opaque
	CurrentOpacity = 1.0f;
	bIsVisible = true;
	bIsFadingOut = false;
	UpdateOpacity(CurrentOpacity);
}

void UMOLoadingOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsFadingOut)
	{
		// Lerp opacity down to 0
		const float FadeSpeed = (FadeOutDuration > 0.0f) ? (1.0f / FadeOutDuration) : 10.0f;
		CurrentOpacity = FMath::Max(0.0f, CurrentOpacity - (FadeSpeed * InDeltaTime));
		UpdateOpacity(CurrentOpacity);

		if (CurrentOpacity <= 0.0f)
		{
			bIsFadingOut = false;
			bIsVisible = false;
			RemoveFromParent();
			UE_LOG(LogMOFramework, Log, TEXT("[MOLoadingOverlay] Fade complete, removed from viewport"));
		}
	}
}

void UMOLoadingOverlay::ShowOverlay()
{
	CurrentOpacity = 1.0f;
	bIsVisible = true;
	bIsFadingOut = false;
	UpdateOpacity(CurrentOpacity);

	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadingOverlay] Showing overlay"));
}

void UMOLoadingOverlay::FadeOutAndRemove()
{
	if (bIsFadingOut)
	{
		return; // Already fading
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadingOverlay] Starting fade out (duration=%.2fs)"), FadeOutDuration);
	bIsFadingOut = true;
}

void UMOLoadingOverlay::SetLoadingText(const FText& InText)
{
	if (LoadingText)
	{
		LoadingText->SetText(InText);
	}
}

void UMOLoadingOverlay::UpdateOpacity(float NewOpacity)
{
	if (BackgroundImage)
	{
		FLinearColor Color = BackgroundImage->GetColorAndOpacity();
		Color.A = NewOpacity;
		BackgroundImage->SetColorAndOpacity(Color);
	}

	if (LoadingText)
	{
		// TextBlock uses FSlateColor, so just set opacity directly
		LoadingText->SetOpacity(NewOpacity);
	}

	// Also set overall widget render opacity for any other elements
	SetRenderOpacity(NewOpacity);
}
