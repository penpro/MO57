#include "MOReticleWidget.h"
#include "MOUITokens.h"

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
	const FLinearColor Fill = bUseThemeColor ? UMOUITokens::Color("color.text.primary") : ReticleColor;
	const FLinearColor Outline = UMOUITokens::Color("color.hud.outline");   // 1 px dark edge so the reticle reads on snow and on sky

	// each shape is drawn twice: a dark copy 1 px larger on every side, then the coloured one
	auto AddShape = [&](const FMargin& Offset)
	{
		const FMargin Grown(Offset.Left - 1.0f, Offset.Top - 1.0f, Offset.Right + 2.0f, Offset.Bottom + 2.0f);
		Canvas->AddSlot().Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f)).Offset(Grown).AutoSize(false)[SNew(SImage).Image(&WhiteBrush).ColorAndOpacity(Outline)];
		Canvas->AddSlot().Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f)).Offset(Offset).AutoSize(false)[SNew(SImage).Image(&WhiteBrush).ColorAndOpacity(Fill)];
	};

	AddShape(FMargin(-HalfThickness, -(ReticleGap + LineLength), ReticleThickness, LineLength));   // top
	AddShape(FMargin(-HalfThickness, ReticleGap, ReticleThickness, LineLength));                   // bottom
	AddShape(FMargin(-(ReticleGap + LineLength), -HalfThickness, LineLength, ReticleThickness));   // left
	AddShape(FMargin(ReticleGap, -HalfThickness, LineLength, ReticleThickness));                   // right
	if (bShowCenterDot)
	{
		const float HalfDot = CenterDotSize * 0.5f;
		AddShape(FMargin(-HalfDot, -HalfDot, CenterDotSize, CenterDotSize));
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
	bUseThemeColor = false;
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
