#include "MOReticleWidget.h"
#include "MOUITokens.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SLeafWidget.h"

/**
 * The ring itself: two outline-only rounded boxes (the dark edge, then the colour) drawn each paint in the colour of the moment. A rounded box's
 * OUTLINE is drawn in the brush's own outline colour -- an SImage's tint reaches only the fill (FSlateDrawElement::MakeBox, ET_RoundedBox) -- so
 * an image of a white ring stayed white whatever its ColorAndOpacity said (Oct 29: green / blue / red reported, a white ring on screen).
 */
class SMOReticleRing : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMOReticleRing) : _Diameter(12.0f), _Width(2.0f) {}
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ATTRIBUTE(FLinearColor, EdgeColor)
		SLATE_ATTRIBUTE(float, Diameter)
		SLATE_ARGUMENT(float, Width)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Color = InArgs._Color;
		EdgeColor = InArgs._EdgeColor;
		Diameter = InArgs._Diameter;
		Width = InArgs._Width;
		ForceVolatile(true);   // colour and size follow the game every frame, with no invalidation to announce it
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Size = Diameter.Get() + 2.0f;
		return FVector2D(Size, Size);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2f Size = AllottedGeometry.GetLocalSize();
		const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;   // the HUD mode's fade (F1)
		auto Ring = [&](float RingDiameter, float RingWidth, FLinearColor RingColor, int32 Layer)
		{
			RingColor.A *= Opacity;
			const FSlateRoundedBoxBrush Brush(FLinearColor::Transparent, RingColor, RingWidth);   // copied into the element: a local is fine
			const FVector2f Offset((Size.X - RingDiameter) * 0.5f, (Size.Y - RingDiameter) * 0.5f);
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2f(RingDiameter, RingDiameter), FSlateLayoutTransform(Offset)),
				&Brush, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, Opacity));
		};
		const float D = Diameter.Get();
		Ring(D + 2.0f, Width + 2.0f, EdgeColor.Get(), LayerId);   // the dark edge, a pixel wider on each side: the ring reads on snow and on sky
		Ring(D, Width, Color.Get(), LayerId + 1);
		return LayerId + 1;
	}

private:
	TAttribute<FLinearColor> Color;
	TAttribute<FLinearColor> EdgeColor;
	TAttribute<float> Diameter;
	float Width = 2.0f;
};

FLinearColor UMOReticleWidget::ColorForIntent(EMOReticleIntent InIntent)
{
	switch (InIntent)
	{
	case EMOReticleIntent::Interact:  return UMOUITokens::Color("color.severity.ok");
	case EMOReticleIntent::Secondary: return UMOUITokens::Color("color.severity.info");
	case EMOReticleIntent::Threat:    return UMOUITokens::Color("color.severity.critical");
	default:                          return UMOUITokens::Color("color.text.primary");
	}
}

float UMOReticleWidget::ClosingSpread(float Elapsed, float Seconds)
{
	if (Seconds <= 0.0f || Elapsed >= Seconds)
	{
		return 0.0f;
	}
	const float T = FMath::Clamp(Elapsed / Seconds, 0.0f, 1.0f);
	return FMath::Square(1.0f - T);   // closes fast, then settles onto the mark
}

void UMOReticleWidget::PlayClosingCircle(float Seconds)
{
	ClosingSeconds = FMath::Max(0.05f, Seconds);
	ClosingStartSeconds = FPlatformTime::Seconds();
	AimSpread = 0.0f;   // where it rests once the close is done
}

float UMOReticleWidget::GetAimSpread() const
{
	if (ClosingSeconds > 0.0f)
	{
		const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - ClosingStartSeconds);
		if (Elapsed < ClosingSeconds)
		{
			return ClosingSpread(Elapsed, ClosingSeconds);
		}
	}
	return AimSpread;
}

TSharedRef<SWidget> UMOReticleWidget::RebuildWidget()
{
	// Size and colour are read from this widget every frame (attributes), so an intent or a spread change never rebuilds anything.
	const TWeakObjectPtr<UMOReticleWidget> Self(this);
	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
	Canvas->AddSlot()
		.Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))
		.Alignment(FVector2D(0.5f, 0.5f))
		.AutoSize(true)
		[
			SNew(SMOReticleRing)
			.Width(RingWidth)
			.Diameter_Lambda([Self]() { return Self.IsValid() ? Self->GetCurrentDiameter() : 12.0f; })
			.Color_Lambda([Self]() { return Self.IsValid() ? Self->GetCurrentColor() : FLinearColor::White; })
			.EdgeColor_Lambda([]() { return UMOUITokens::Color("color.hud.outline"); })
		];
	return Canvas;
}

void UMOReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Center the widget on screen
	SetAnchorsInViewport(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
}
