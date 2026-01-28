#include "MOReticleWidget.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Styling/SlateBrush.h"

TSharedRef<SWidget> UMOReticleWidget::RebuildWidget()
{
	// Create a constraint canvas that allows precise positioning
	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);

	// Create a white brush for all elements
	static FSlateBrush WhiteBrush;
	static bool bBrushInitialized = false;
	if (!bBrushInitialized)
	{
		WhiteBrush.TintColor = FSlateColor(FLinearColor::White);
		WhiteBrush.DrawAs = ESlateBrushDrawType::Box;
		bBrushInitialized = true;
	}

	const float LineLength = ReticleSize - ReticleGap;
	const float HalfThickness = ReticleThickness * 0.5f;

	// Top line - positioned above center
	Canvas->AddSlot()
		.Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))
		.Offset(FMargin(-HalfThickness, -(ReticleGap + LineLength), ReticleThickness, LineLength))
		.AutoSize(false)
		[
			SNew(SImage)
			.Image(&WhiteBrush)
			.ColorAndOpacity(ReticleColor)
		];

	// Bottom line - positioned below center
	Canvas->AddSlot()
		.Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))
		.Offset(FMargin(-HalfThickness, ReticleGap, ReticleThickness, LineLength))
		.AutoSize(false)
		[
			SNew(SImage)
			.Image(&WhiteBrush)
			.ColorAndOpacity(ReticleColor)
		];

	// Left line - positioned left of center
	Canvas->AddSlot()
		.Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))
		.Offset(FMargin(-(ReticleGap + LineLength), -HalfThickness, LineLength, ReticleThickness))
		.AutoSize(false)
		[
			SNew(SImage)
			.Image(&WhiteBrush)
			.ColorAndOpacity(ReticleColor)
		];

	// Right line - positioned right of center
	Canvas->AddSlot()
		.Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))
		.Offset(FMargin(ReticleGap, -HalfThickness, LineLength, ReticleThickness))
		.AutoSize(false)
		[
			SNew(SImage)
			.Image(&WhiteBrush)
			.ColorAndOpacity(ReticleColor)
		];

	// Center dot
	if (bShowCenterDot)
	{
		const float HalfDot = CenterDotSize * 0.5f;
		Canvas->AddSlot()
			.Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))
			.Offset(FMargin(-HalfDot, -HalfDot, CenterDotSize, CenterDotSize))
			.AutoSize(false)
			[
				SNew(SImage)
				.Image(&WhiteBrush)
				.ColorAndOpacity(ReticleColor)
			];
	}

	RootCanvas = Canvas;
	return Canvas;
}

void UMOReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Center the widget on screen
	SetAnchorsInViewport(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
}

void UMOReticleWidget::RebuildReticle()
{
	// For dynamic rebuilding, we need to recreate the widget
	// This is called when properties change at runtime
	if (RootCanvas.IsValid())
	{
		// Force a full rebuild by invalidating
		TakeWidget();
	}
}

void UMOReticleWidget::SetReticleColor(FLinearColor InColor)
{
	ReticleColor = InColor;
	RebuildReticle();
}

void UMOReticleWidget::SetReticleSize(float InSize)
{
	ReticleSize = FMath::Max(1.0f, InSize);
	RebuildReticle();
}

void UMOReticleWidget::SetReticleThickness(float InThickness)
{
	ReticleThickness = FMath::Max(1.0f, InThickness);
	RebuildReticle();
}

void UMOReticleWidget::SetReticleGap(float InGap)
{
	ReticleGap = FMath::Max(0.0f, InGap);
	RebuildReticle();
}

void UMOReticleWidget::SetShowCenterDot(bool bShow)
{
	bShowCenterDot = bShow;
	RebuildReticle();
}
