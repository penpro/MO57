#include "MOSurvivorDialogueWidget.h"
#include "MOUITokens.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

float UMOSurvivorDialogueWidget::ReadSeconds(const FText& Text)
{
	// ~18 characters a second read aloud in the head, at least 4 s on screen, at most 16 s.
	return FMath::Clamp(2.5f + Text.ToString().Len() * 0.055f, 4.0f, 16.0f);
}

void UMOSurvivorDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;   // a Widget Blueprint subclass supplied its own tree
	}

	// Full-screen overlay (the viewport slot's default), the plate bottom-centre inside it, above the breath meter and the hotbar.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DialogueRoot"));
	WidgetTree->RootWidget = Root;

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DialogueWidth"));
	Width->SetMaxDesiredWidth(760.0f);
	if (UOverlaySlot* WidthSlot = Root->AddChildToOverlay(Width))
	{
		WidthSlot->SetHorizontalAlignment(HAlign_Center);
		WidthSlot->SetVerticalAlignment(VAlign_Bottom);
		WidthSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 270.0f));
	}

	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialoguePlate"));
	Plate->SetBrush(UMOUITokens::NineSliceBrush("hud_plate", "color.hud.plate"));
	Plate->SetPadding(FMargin(24.0f, 14.0f));
	Width->AddChild(Plate);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueColumn"));
	Plate->SetContent(Column);

	SpeakerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeakerText"));
	SpeakerText->SetFont(UMOUITokens::Font("body.strong"));
	SpeakerText->SetColorAndOpacity(FSlateColor(UMOUITokens::Color("color.accent")));
	if (UVerticalBoxSlot* SpeakerSlot = Column->AddChildToVerticalBox(SpeakerText))
	{
		SpeakerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	LinesText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LinesText"));
	LinesText->SetFont(UMOUITokens::Font("body"));
	LinesText->SetColorAndOpacity(FSlateColor(UMOUITokens::Color("color.text.primary")));
	LinesText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(LinesText);

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.0f);
}

void UMOSurvivorDialogueWidget::ShowLines(const FText& SpeakerName, const FText& Lines)
{
	if (SpeakerText)
	{
		SpeakerText->SetText(SpeakerName);
	}
	if (LinesText)
	{
		LinesText->SetText(Lines);
	}
	HoldRemaining = ReadSeconds(Lines);
	Alpha = 1.0f;   // up at once: it answers the key the player just pressed
	SetRenderOpacity(Alpha);
}

FText UMOSurvivorDialogueWidget::GetSpeakerText() const
{
	return SpeakerText ? SpeakerText->GetText() : FText::GetEmpty();
}

FText UMOSurvivorDialogueWidget::GetLinesText() const
{
	return LinesText ? LinesText->GetText() : FText::GetEmpty();
}

void UMOSurvivorDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (HoldRemaining > 0.0f)
	{
		HoldRemaining = FMath::Max(0.0f, HoldRemaining - InDeltaTime);
		return;
	}
	if (Alpha > 0.0f)
	{
		Alpha = FMath::FInterpConstantTo(Alpha, 0.0f, InDeltaTime, 1.5f);   // a slow fade: the last words linger
		SetRenderOpacity(Alpha);
	}
}
