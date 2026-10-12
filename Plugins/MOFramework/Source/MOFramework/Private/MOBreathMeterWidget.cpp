#include "MOBreathMeterWidget.h"
#include "MOCharacter.h"
#include "MORespirationComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "MOUITokens.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

FLinearColor UMOBreathMeterWidget::ColorForReserve(float Reserve01, float UrgeReserve, float& OutPulse)
{
	OutPulse = 0.0f;
	if (Reserve01 > UrgeReserve)
	{
		return UMOUITokens::Color("color.severity.info");
	}
	const float DangerReserve = 0.10f;   // kit: warning below the urge, critical below 10 %
	if (Reserve01 > DangerReserve)
	{
		return UMOUITokens::Color("color.severity.warning");
	}
	OutPulse = 1.0f - FMath::Clamp(Reserve01 / DangerReserve, 0.0f, 1.0f) * 0.5f;   // pulses harder as it empties
	return UMOUITokens::Color("color.severity.critical");
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

	// the plate (kit hud_plate, color.hud.plate) around the label and the segments
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BreathPlate"));
	Plate->SetBrush(UMOUITokens::NineSliceBrush("hud_plate", "color.hud.plate"));
	Plate->SetPadding(FMargin(14.0f, 8.0f));
	Column->AddChildToVerticalBox(Plate);
	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BreathInner"));
	Plate->SetContent(Inner);

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BreathLabel"));
	Label->SetText(NSLOCTEXT("MOBreath", "BreathLabel", "BREATH"));
	Label->SetFont(UMOUITokens::Font("label"));
	Label->SetJustification(ETextJustify::Center);
	Label->SetColorAndOpacity(FSlateColor(UMOUITokens::Color("color.text.secondary")));
	if (UVerticalBoxSlot* LabelSlot = Inner->AddChildToVerticalBox(Label))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BreathSegments"));
	Inner->AddChildToVerticalBox(Row);
	for (int32 i = 0; i < SegmentCount; ++i)
	{
		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(36.0f);
		Cell->SetHeightOverride(10.0f);
		UImage* Segment = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Segment->SetBrush(UMOUITokens::NineSliceBrush("progress_fill", "color.data.track"));
		Cell->AddChild(Segment);
		if (UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Cell))
		{
			CellSlot->SetPadding(FMargin(i == 0 ? 0.0f : 4.0f, 0.0f, 0.0f, 0.0f));
		}
		Segments.Add(Segment);
	}

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
		if (Segments.Num() > 0)
		{
			float Pulse = 0.0f;
			FLinearColor Color = ColorForReserve(Reserve, Breath->Params.UrgeReserve, Pulse);
			if (Pulse > 0.0f && UMOUITokens::Number("motion.pulse.critical") > 0.0f)   // reduced motion: static
			{
				PulseTime += InDeltaTime * (5.0f + 6.0f * Pulse);
				Color.A = 0.65f + 0.35f * (0.5f + 0.5f * FMath::Sin(PulseTime));
			}
			const FLinearColor Track = UMOUITokens::Color("color.data.track");
			const float Lit = FMath::Clamp(Reserve, 0.0f, 1.0f) * SegmentCount;
			for (int32 i = 0; i < Segments.Num(); ++i)
			{
				const float Fill = FMath::Clamp(Lit - i, 0.0f, 1.0f);   // the boundary segment is part-lit
				Segments[i]->SetColorAndOpacity(FMath::Lerp(Track, Color, Fill));
			}
		}
	}
	Alpha = FMath::FInterpConstantTo(Alpha, Target, InDeltaTime, Target > Alpha ? 5.0f : 1.5f);   // quick in, slow out
	SetRenderOpacity(Alpha);
}
