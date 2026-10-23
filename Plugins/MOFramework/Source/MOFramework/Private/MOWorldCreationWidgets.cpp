#include "MOWorldCreationWidgets.h"
#include "MOCommonButton.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "MOWorldCreation"

// ---------------------------------------------------------------------------------------------------------------------------- knob row

void UMOWorldKnobRow::NativeConstruct()
{
	Super::NativeConstruct();
	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.RemoveDynamic(this, &UMOWorldKnobRow::HandleSliderChanged);
		ValueSlider->OnValueChanged.AddDynamic(this, &UMOWorldKnobRow::HandleSliderChanged);
	}
}

void UMOWorldKnobRow::NativeDestruct()
{
	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.RemoveDynamic(this, &UMOWorldKnobRow::HandleSliderChanged);
	}
	OnKnobChanged.Clear();
	Super::NativeDestruct();
}

FText UMOWorldKnobRow::FormatValue(float Value)
{
	FNumberFormattingOptions Two;
	Two.MinimumFractionalDigits = 2;
	Two.MaximumFractionalDigits = 2;
	return FText::Format(LOCTEXT("KnobValue", "x{0}"), FText::AsNumber(Value, &Two));
}

void UMOWorldKnobRow::Configure(const FMOWorldKnob& InKnob, float Value)
{
	Knob = InKnob;
	if (LabelText)
	{
		LabelText->SetText(Knob.Label);
	}
	if (HintText)
	{
		HintText->SetText(Knob.Hint);
	}
	SetToolTipText(Knob.Hint);
	if (ValueSlider)
	{
		ValueSlider->SetMinValue(Knob.Min);
		ValueSlider->SetMaxValue(Knob.Max);
		ValueSlider->SetStepSize(Knob.Step);
	}
	SetValue(Value);
}

void UMOWorldKnobRow::SetValue(float Value)
{
	TGuardValue<bool> Guard(bSettingValue, true);
	if (ValueSlider)
	{
		ValueSlider->SetValue(Value);
	}
	if (ValueText)
	{
		ValueText->SetText(FormatValue(Value));
	}
}

void UMOWorldKnobRow::HandleSliderChanged(float Value)
{
	if (bSettingValue)
	{
		return;
	}
	const float Snapped = FMath::Clamp(Knob.Step > 0.0f ? FMath::GridSnap(Value, Knob.Step) : Value, Knob.Min, Knob.Max);
	if (ValueText)
	{
		ValueText->SetText(FormatValue(Snapped));
	}
	OnKnobChanged.Broadcast(Knob.Id, Snapped);
}

// ---------------------------------------------------------------------------------------------------------------------------- biome card

void UMOBiomeCard::NativeConstruct()
{
	Super::NativeConstruct();
	if (SelectButton)
	{
		SelectButton->OnClicked().RemoveAll(this);
		SelectButton->OnClicked().AddUObject(this, &UMOBiomeCard::HandleClicked);
	}
}

void UMOBiomeCard::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked().RemoveAll(this);
	}
	OnPicked.Clear();
	Super::NativeDestruct();
}

void UMOBiomeCard::Configure(FName InBiomeId, const FText& Name, UTexture2D* Image)
{
	BiomeId = InBiomeId;
	if (SelectButton)
	{
		SelectButton->SetButtonText(Name);
	}
	if (PreviewImage)
	{
		if (Image)
		{
			PreviewImage->SetBrushFromTexture(Image, /*bMatchSize=*/false);
			PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			PreviewImage->SetVisibility(ESlateVisibility::Hidden);   // keep the card's footprint
		}
	}
	SetSelected(false);
}

void UMOBiomeCard::SetSelected(bool bSelected)
{
	if (SelectButton)
	{
		SelectButton->SetSelectedState(bSelected);
	}
	if (SelectedFrame)
	{
		SelectedFrame->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UMOBiomeCard::HandleClicked()
{
	OnPicked.Broadcast(BiomeId);
}

#undef LOCTEXT_NAMESPACE
