/**
 * MOClockDial.cpp - A 24-hour clock face whose hand is dragged to pick a time of day
 */

#include "MOClockDial.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

#define LOCTEXT_NAMESPACE "MOClockDial"

namespace
{
	constexpr int32 MinutesPerDay = 24 * 60;

	int32 WrapMinutes(int32 Minutes)
	{
		return ((Minutes % MinutesPerDay) + MinutesPerDay) % MinutesPerDay;
	}
}

// ============================================================================
// SMOClockDial: the Slate leaf that paints the face and owns the pointer
// ============================================================================

class SMOClockDial : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMOClockDial) {}
	SLATE_END_ARGS()

	/** (minutes, committed) -- every snapped step, and the release. */
	TFunction<void(int32, bool)> OnMinutes;

	int32 Minutes = 12 * 60;
	float SunriseHour = 6.0f;
	float SunsetHour = 18.0f;
	int32 SnapMinutes = 5;
	float Diameter = 360.0f;
	FLinearColor FaceColor, DayColor, NightColor, TickColor, HandColor, LabelColor;
	FSlateFontInfo LabelFont;

	void Construct(const FArguments& InArgs)
	{
		// A rounded box rounded by half its height is a circle on a square: the face, the knob and the hub.
		Circle.DrawAs = ESlateBrushDrawType::RoundedBox;
		Circle.TintColor = FSlateColor(FLinearColor::White);
		Circle.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		SetCursor(EMouseCursor::GrabHand);
	}

	void SetMinutes(int32 InMinutes)
	{
		const int32 Wrapped = WrapMinutes(InMinutes);
		if (Wrapped != Minutes)
		{
			Minutes = Wrapped;
			Invalidate(EInvalidateWidgetReason::Paint);
		}
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(Diameter, Diameter);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Out, int32 LayerId,
		const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2f Size = FVector2f(Geometry.GetLocalSize());
		const FVector2f C = Size * 0.5f;
		const float R = FMath::Max(8.0f, FMath::Min(Size.X, Size.Y) * 0.5f - 2.0f);
		const ESlateDrawEffect Effect = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		const float Alpha = Style.GetColorAndOpacityTint().A;
		auto Tinted = [Alpha](FLinearColor Color) { Color.A *= Alpha; return Color; };
		auto At = [&C](float InMinutes, float Radius) { return C + FVector2f(UMOClockDial::MinutesToUnitOffset(InMinutes)) * Radius; };
		auto Disc = [&](const FVector2f& Centre, float Radius, const FLinearColor& Color, int32 Layer)
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2f(Radius * 2.0f, Radius * 2.0f), FSlateLayoutTransform(Centre - FVector2f(Radius, Radius))),
				&Circle, Effect, Tinted(Color));
		};
		auto Arc = [&](float FromMinutes, float ToMinutes, float Radius, const FLinearColor& Color, float Thickness, int32 Layer)
		{
			TArray<FVector2f> Points;
			const int32 Steps = FMath::Max(2, FMath::CeilToInt((ToMinutes - FromMinutes) / 10.0f));
			for (int32 I = 0; I <= Steps; ++I)
			{
				Points.Add(At(FMath::Lerp(FromMinutes, ToMinutes, static_cast<float>(I) / Steps), Radius));
			}
			FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), Effect, Tinted(Color), true, Thickness);
		};

		// The face.
		Disc(C, R, FaceColor, LayerId);

		// Daylight and night round the rim: golden hour is where they meet.
		const float Sunrise = FMath::Clamp(SunriseHour, 0.0f, 24.0f) * 60.0f;
		float Sunset = FMath::Clamp(SunsetHour, 0.0f, 24.0f) * 60.0f;
		if (Sunset <= Sunrise)
		{
			Sunset = Sunrise + 1.0f;
		}
		const float RimRadius = R * 0.9f, RimThickness = FMath::Max(3.0f, R * 0.06f);
		Arc(Sunset, Sunrise + MinutesPerDay, RimRadius, NightColor, RimThickness, LayerId + 1);
		Arc(Sunrise, Sunset, RimRadius, DayColor, RimThickness, LayerId + 1);

		// Hour ticks (long at midnight, 6, noon, 18) and a label every three hours.
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		for (int32 Hour = 0; Hour < 24; ++Hour)
		{
			const bool bMajor = Hour % 6 == 0;
			TArray<FVector2f> Tick = { At(Hour * 60.0f, R * (bMajor ? 0.70f : 0.75f)), At(Hour * 60.0f, R * 0.80f) };
			FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), MoveTemp(Tick), Effect, Tinted(TickColor), true, bMajor ? 2.5f : 1.25f);
			if (Hour % 3 == 0)
			{
				const FString Label = FString::FromInt(Hour);
				const FVector2f LabelSize = FVector2f(Measure->Measure(Label, LabelFont));
				FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(At(Hour * 60.0f, R * 0.57f) - LabelSize * 0.5f)),
					Label, LabelFont, Effect, Tinted(bMajor ? HandColor : LabelColor));
			}
		}

		// The hand, its knob (bigger while held) and the hub.
		const FVector2f Tip = At(static_cast<float>(Minutes), R * 0.82f);
		TArray<FVector2f> Hand = { C, Tip };
		FSlateDrawElement::MakeLines(Out, LayerId + 2, Geometry.ToPaintGeometry(), MoveTemp(Hand), Effect, Tinted(HandColor), true, FMath::Max(2.0f, R * 0.022f));
		Disc(Tip, R * (HasMouseCapture() ? 0.075f : 0.06f), HandColor, LayerId + 3);
		Disc(Tip, R * 0.035f, Minutes >= Sunrise && Minutes <= Sunset ? DayColor : NightColor, LayerId + 4);
		Disc(C, R * 0.035f, HandColor, LayerId + 3);
		return LayerId + 4;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}
		PointAt(Geometry, Event, false);
		Invalidate(EInvalidateWidgetReason::Paint);   // the knob grows while held
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (!HasMouseCapture())
		{
			return FReply::Unhandled();
		}
		PointAt(Geometry, Event, false);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (!HasMouseCapture() || Event.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}
		PointAt(Geometry, Event, false);
		Invalidate(EInvalidateWidgetReason::Paint);
		if (OnMinutes)
		{
			OnMinutes(Minutes, true);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		// Wheel up = later, a quarter hour a notch.
		const int32 Step = Event.GetWheelDelta() > 0.0f ? 15 : -15;
		SetMinutes(Minutes + Step);
		if (OnMinutes)
		{
			OnMinutes(Minutes, false);
			OnMinutes(Minutes, true);
		}
		return FReply::Handled();
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override
	{
		return FCursorReply::Cursor(HasMouseCapture() ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
	}

private:
	FSlateBrush Circle;

	void PointAt(const FGeometry& Geometry, const FPointerEvent& Event, bool bCommit)
	{
		const FVector2D Local = FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
		const FVector2D Offset = Local - FVector2D(Geometry.GetLocalSize()) * 0.5;
		if (Offset.SizeSquared() < 16.0)
		{
			return;   // on the hub: no direction
		}
		const int32 New = UMOClockDial::OffsetToMinutes(Offset, SnapMinutes);
		if (New != Minutes)
		{
			SetMinutes(New);
			if (OnMinutes)
			{
				OnMinutes(Minutes, bCommit);
			}
		}
	}
};

// ============================================================================
// UMOClockDial
// ============================================================================

UMOClockDial::UMOClockDial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 13);
}

FVector2D UMOClockDial::MinutesToUnitOffset(float Minutes)
{
	// Midnight at the bottom, 6 on the left, noon at the top, 18 on the right: the hand rises and sets like the sun, clockwise.
	const double Theta = static_cast<double>(Minutes) / MinutesPerDay * 2.0 * UE_DOUBLE_PI;
	return FVector2D(-FMath::Sin(Theta), FMath::Cos(Theta));
}

int32 UMOClockDial::OffsetToMinutes(const FVector2D& Offset, int32 InSnapMinutes)
{
	double Theta = FMath::Atan2(-Offset.X, Offset.Y);   // the inverse of MinutesToUnitOffset
	if (Theta < 0.0)
	{
		Theta += 2.0 * UE_DOUBLE_PI;
	}
	const int32 Snap = FMath::Clamp(InSnapMinutes, 1, 60);
	const int32 Raw = FMath::RoundToInt(Theta / (2.0 * UE_DOUBLE_PI) * MinutesPerDay / Snap) * Snap;
	return WrapMinutes(Raw);
}

void UMOClockDial::SetMinutesOfDay(int32 Minutes)
{
	MinutesOfDay = WrapMinutes(Minutes);
	if (MyDial.IsValid())
	{
		MyDial->SetMinutes(MinutesOfDay);
	}
}

void UMOClockDial::SetDaylight(float InSunriseHour, float InSunsetHour)
{
	SunriseHour = InSunriseHour;
	SunsetHour = InSunsetHour;
	if (MyDial.IsValid())
	{
		MyDial->SunriseHour = SunriseHour;
		MyDial->SunsetHour = SunsetHour;
		MyDial->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

TSharedRef<SWidget> UMOClockDial::RebuildWidget()
{
	MyDial = SNew(SMOClockDial);
	MyDial->OnMinutes = [WeakThis = TWeakObjectPtr<UMOClockDial>(this)](int32 Minutes, bool bCommit)
	{
		if (UMOClockDial* This = WeakThis.Get())
		{
			This->HandleSlateMinutes(Minutes, bCommit);
		}
	};
	return MyDial.ToSharedRef();
}

void UMOClockDial::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyDial.IsValid())
	{
		return;
	}
	MyDial->Minutes = MinutesOfDay;
	MyDial->SunriseHour = SunriseHour;
	MyDial->SunsetHour = SunsetHour;
	MyDial->SnapMinutes = SnapMinutes;
	MyDial->Diameter = Diameter;
	MyDial->FaceColor = FaceColor;
	MyDial->DayColor = DayColor;
	MyDial->NightColor = NightColor;
	MyDial->TickColor = TickColor;
	MyDial->HandColor = HandColor;
	MyDial->LabelColor = LabelColor;
	MyDial->LabelFont = LabelFont;
	MyDial->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void UMOClockDial::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDial.Reset();
}

void UMOClockDial::HandleSlateMinutes(int32 Minutes, bool bCommit)
{
	MinutesOfDay = Minutes;
	(bCommit ? OnMinutesCommitted : OnMinutesChanged).Broadcast(Minutes);
}

#if WITH_EDITOR
const FText UMOClockDial::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "MO");
}
#endif

#undef LOCTEXT_NAMESPACE
