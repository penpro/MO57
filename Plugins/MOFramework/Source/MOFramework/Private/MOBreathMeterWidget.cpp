#include "MOBreathMeterWidget.h"
#include "MOCharacter.h"
#include "MORespirationComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

FLinearColor UMOBreathMeterWidget::ColorForReserve(float Reserve01, float UrgeReserve, float& OutPulse)
{
	OutPulse = 0.0f;
	const FLinearColor Calm(0.55f, 0.82f, 1.00f);
	const FLinearColor Warn(1.00f, 0.72f, 0.18f);
	const FLinearColor Danger(1.00f, 0.18f, 0.12f);
	if (Reserve01 > UrgeReserve)
	{
		return Calm;
	}
	const float DangerReserve = 0.18f;
	if (Reserve01 > DangerReserve)
	{
		// amber in the middle of the urge range, easing toward red
		return FMath::Lerp(Warn, Danger, 1.0f - FMath::Clamp((Reserve01 - DangerReserve) / FMath::Max(UrgeReserve - DangerReserve, 0.01f), 0.0f, 1.0f));
	}
	OutPulse = 1.0f - FMath::Clamp(Reserve01 / DangerReserve, 0.0f, 1.0f) * 0.5f;   // pulses harder as it empties
	return Danger;
}

bool UMOBreathMeterWidget::ShouldShow(bool bAirwayClosed, float Reserve01)
{
	return bAirwayClosed || Reserve01 < 0.995f;
}

void UMOBreathMeterWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;   // a Widget Blueprint subclass supplied its own tree
	}

	// Full-screen overlay (the viewport slot's default), the meter aligned bottom-centre inside it: it follows any resolution / DPI scale. (SetPositionInViewport would reset a point anchor to the
	// top-left, so the bar must not rely on a viewport slot for its place.)
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BreathRoot"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BreathColumn"));
	if (UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column))
	{
		ColumnSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnSlot->SetVerticalAlignment(VAlign_Bottom);
		ColumnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 150.0f));   // above where a hotbar would sit
	}

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BreathLabel"));
	Label->SetText(NSLOCTEXT("MOBreath", "BreathLabel", "BREATH"));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 11;
	Label->SetFont(Font);
	Label->SetJustification(ETextJustify::Center);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.92f, 1.0f, 0.9f)));
	if (UVerticalBoxSlot* LabelSlot = Column->AddChildToVerticalBox(Label))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BreathBox"));
	Box->SetWidthOverride(280.0f);
	Box->SetHeightOverride(10.0f);
	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BreathBar"));
	Bar->SetPercent(1.0f);
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.02f, 0.05f, 0.09f, 0.55f));
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(FLinearColor(0.55f, 0.82f, 1.0f));
	Box->AddChild(Bar);
	Column->AddChildToVerticalBox(Box);

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.0f);
}

void UMOBreathMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	const AMOCharacter* Pawn = PC ? Cast<AMOCharacter>(PC->GetPawn()) : nullptr;
	const UMORespirationComponent* Breath = Pawn ? Pawn->GetRespirationComponent() : nullptr;

	float Target = 0.0f;
	if (Breath && !Pawn->IsDead())
	{
		const float Reserve = Breath->GetOxygenReserve();
		if (ShouldShow(Breath->IsAirwayClosed(), Reserve))
		{
			Target = 1.0f;
		}
		if (Bar)
		{
			float Pulse = 0.0f;
			FLinearColor Color = ColorForReserve(Reserve, Breath->Params.UrgeReserve, Pulse);
			if (Pulse > 0.0f)
			{
				PulseTime += InDeltaTime * (5.0f + 6.0f * Pulse);
				Color.A = 0.65f + 0.35f * (0.5f + 0.5f * FMath::Sin(PulseTime));
			}
			Bar->SetPercent(Reserve);
			Bar->SetFillColorAndOpacity(Color);
		}
	}
	Alpha = FMath::FInterpConstantTo(Alpha, Target, InDeltaTime, Target > Alpha ? 5.0f : 1.5f);   // quick in, slow out
	SetRenderOpacity(Alpha);
}
