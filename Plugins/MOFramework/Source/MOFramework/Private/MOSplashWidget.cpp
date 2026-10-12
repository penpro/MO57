#include "MOSplashWidget.h"
#include "MOFramework.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

UMOSplashWidget::UMOSplashWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

UWidget* UMOSplashWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UMOSplashWidget*>(this);   // key / mouse events route to the handlers below, like the intro
}

void UMOSplashWidget::BeginSequence(const TArray<FMOSplashCard>& InCards, float InFadeSeconds)
{
	Cards = InCards;
	FadeSeconds = FMath::Max(InFadeSeconds, 0.05f);
	CardIndex = -1;
	Phase = EPhase::Gap;
	PhaseElapsed = 0.0;
	bRunning = true;
	SetCardOpacity(0.0f);
	// Start loading every mark now, while the black field is up: by the time a card is due its texture is usually compiled and resident.
	for (const FMOSplashCard& Card : Cards)
	{
		if (!Card.Logo.IsNull())
		{
			Card.Logo.LoadSynchronous();
		}
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOSplash] %d card(s), fade %.2fs"), Cards.Num(), FadeSeconds);
}

void UMOSplashWidget::SkipSequence()
{
	if (bRunning)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSplash] skipped at card %d"), CardIndex);
		Finish();
	}
}

void UMOSplashWidget::Finish()
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	SetCardOpacity(0.0f);
	DeactivateWidget();
	OnSplashFinished.Broadcast();
}

void UMOSplashWidget::SetCardOpacity(float Alpha)
{
	const float Smooth = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f));
	if (CardRoot)
	{
		CardRoot->SetRenderOpacity(Smooth);
		return;
	}
	if (LogoImage)
	{
		LogoImage->SetRenderOpacity(Smooth);
	}
	if (CaptionText)
	{
		CaptionText->SetRenderOpacity(Smooth);
	}
}

bool UMOSplashWidget::IsLogoReady(const UTexture2D* Texture) const
{
	if (!Texture)
	{
		return true;   // caption-only card, or a logo that failed to load (warned in ShowCard)
	}
#if WITH_EDITOR
	if (Texture->IsCompiling())
	{
		return false;   // async texture compilation: until it finishes the brush draws the default checker
	}
#endif
	return Texture->GetResource() != nullptr && !Texture->HasPendingInitOrStreaming();
}

void UMOSplashWidget::ShowCard(int32 Index)
{
	const FMOSplashCard& Card = Cards[Index];
	UTexture2D* Texture = Card.Logo.IsNull() ? nullptr : Card.Logo.LoadSynchronous();   // a few hundred KB, shown at once
	const bool bHasCaption = !Card.Caption.IsEmpty();
	if (!Texture && !Card.Logo.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSplash] card %d: logo %s did not load"), Index, *Card.Logo.ToString());
	}
	if (LogoImage)
	{
		LogoImage->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture)
		{
			LogoImage->SetBrushFromTexture(Texture, /*bMatchSize=*/false);
			LogoImage->SetDesiredSizeOverride(FVector2D(Card.LogoSize, Card.LogoSize));
		}
	}
	if (CaptionText)
	{
		CaptionText->SetVisibility(bHasCaption ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		CaptionText->SetText(Card.Caption);
	}
}

void UMOSplashWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bRunning)
	{
		return;
	}
	PhaseElapsed += InDeltaTime;

	switch (Phase)
	{
	case EPhase::Gap:
		if (PhaseElapsed >= GapSeconds)
		{
			++CardIndex;
			// skip a card that has nothing to show
			while (Cards.IsValidIndex(CardIndex) && Cards[CardIndex].Logo.IsNull() && Cards[CardIndex].Caption.IsEmpty())
			{
				++CardIndex;
			}
			if (!Cards.IsValidIndex(CardIndex))
			{
				Finish();
				return;
			}
			ShowCard(CardIndex);
			Phase = EPhase::Prepare;
			PhaseElapsed = 0.0;
		}
		break;
	case EPhase::Prepare:
		// The card stays invisible (black field) until its mark can really be drawn, so there is never a placeholder on screen; capped so a stuck texture cannot hold the sequence.
		if (IsLogoReady(Cards[CardIndex].Logo.Get()) || PhaseElapsed >= 4.0)
		{
			if (PhaseElapsed >= 4.0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSplash] card %d: logo still not ready after 4 s, showing it anyway"), CardIndex);
			}
			Phase = EPhase::FadeIn;
			PhaseElapsed = 0.0;
		}
		break;
	case EPhase::FadeIn:
		SetCardOpacity(static_cast<float>(PhaseElapsed / FadeSeconds));
		if (PhaseElapsed >= FadeSeconds)
		{
			Phase = EPhase::Hold;
			PhaseElapsed = 0.0;
		}
		break;
	case EPhase::Hold:
		if (PhaseElapsed >= Cards[CardIndex].HoldSeconds)
		{
			Phase = EPhase::FadeOut;
			PhaseElapsed = 0.0;
		}
		break;
	case EPhase::FadeOut:
		SetCardOpacity(1.0f - static_cast<float>(PhaseElapsed / FadeSeconds));
		if (PhaseElapsed >= FadeSeconds)
		{
			SetCardOpacity(0.0f);
			Phase = EPhase::Gap;
			PhaseElapsed = 0.0;
		}
		break;
	}
}

FReply UMOSplashWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bRunning)
	{
		SkipSequence();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UMOSplashWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bRunning)
	{
		SkipSequence();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UMOSplashWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bRunning)
	{
		SkipSequence();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMOSplashWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bRunning)
	{
		SkipSequence();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
