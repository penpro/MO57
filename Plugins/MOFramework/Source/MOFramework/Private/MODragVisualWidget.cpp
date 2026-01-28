#include "MODragVisualWidget.h"
#include "MOFramework.h"

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UMODragVisualWidget::RebuildWidget()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MODragVisual] RebuildWidget called"));

	// Initialize brush
	UpdateBrush();

	// Create the Slate image widget directly
	SlateImage = SNew(SImage)
		.Image(&IconBrush);

	// Wrap in a box to control size
	return SNew(SBox)
		.WidthOverride(VisualSize.X)
		.HeightOverride(VisualSize.Y)
		[
			SlateImage.ToSharedRef()
		];
}

void UMODragVisualWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Warning, TEXT("[MODragVisual] NativeConstruct - IconTexture=%s, VisualSize=%.0fx%.0f"),
		IsValid(IconTexture) ? *IconTexture->GetName() : TEXT("NULL"),
		VisualSize.X, VisualSize.Y);

	// Update the brush with current texture
	UpdateBrush();

	// Ensure visible
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.9f);
}

void UMODragVisualWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	UpdateBrush();
}

void UMODragVisualWidget::UpdateBrush()
{
	if (IsValid(IconTexture))
	{
		IconBrush.SetResourceObject(IconTexture);
		IconBrush.ImageSize = VisualSize;
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
		IconBrush.Tiling = ESlateBrushTileType::NoTile;
		UE_LOG(LogMOFramework, Warning, TEXT("[MODragVisual] UpdateBrush: Set texture %s"), *IconTexture->GetName());
	}
	else
	{
		// Fallback: bright yellow box so we can see SOMETHING
		IconBrush = FSlateBrush();
		IconBrush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 0.0f, 1.0f)); // Bright yellow
		IconBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		IconBrush.ImageSize = VisualSize;
		UE_LOG(LogMOFramework, Warning, TEXT("[MODragVisual] UpdateBrush: No texture, using yellow fallback"));
	}

	// Update the Slate widget if it exists
	if (SlateImage.IsValid())
	{
		SlateImage->SetImage(&IconBrush);
	}
}

void UMODragVisualWidget::SetIcon(UTexture2D* InTexture)
{
	IconTexture = InTexture;

	UE_LOG(LogMOFramework, Warning, TEXT("[MODragVisual] SetIcon: %s"),
		IsValid(InTexture) ? *InTexture->GetName() : TEXT("NULL"));

	UpdateBrush();

	// Notify Blueprint
	OnIconChanged(InTexture);
}

void UMODragVisualWidget::SetVisualSize(FVector2D InSize)
{
	VisualSize = InSize;

	UE_LOG(LogMOFramework, Warning, TEXT("[MODragVisual] SetVisualSize: %.0fx%.0f"), InSize.X, InSize.Y);

	IconBrush.ImageSize = InSize;

	if (SlateImage.IsValid())
	{
		SlateImage->SetImage(&IconBrush);
	}
}
