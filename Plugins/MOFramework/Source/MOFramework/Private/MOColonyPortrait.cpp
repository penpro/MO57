/**
 * MOColonyPortrait.cpp - Colony Character Portrait Widget Implementation
 */

#include "MOColonyPortrait.h"
#include "MOCommonButton.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

UMOColonyPortrait::UMOColonyPortrait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOColonyPortrait::NativeConstruct()
{
	Super::NativeConstruct();

	if (ClickButton)
	{
		ClickButton->OnClicked().RemoveAll(this);
		ClickButton->OnClicked().AddUObject(this, &UMOColonyPortrait::HandleButtonClicked);
	}

	UpdateVisuals();
}

void UMOColonyPortrait::NativeDestruct()
{
	if (ClickButton)
	{
		ClickButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMOColonyPortrait::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Handle critical alert pulsing
	if (AlertState == EAlertState::Critical && AlertBorder)
	{
		PulseTime += InDeltaTime * 3.0f; // 3 Hz pulse
		const float PulseAlpha = (FMath::Sin(PulseTime) + 1.0f) * 0.5f;
		FLinearColor PulseColor = FMath::Lerp(CriticalBorderColor * 0.5f, CriticalBorderColor, PulseAlpha);
		AlertBorder->SetBrushColor(PulseColor);
	}
}

// ============================================================================
// DATA BINDING
// ============================================================================

void UMOColonyPortrait::SetCharacterGuid(const FGuid& InCharacterGuid)
{
	CharacterGuid = InCharacterGuid;
}

// ============================================================================
// PORTRAIT DISPLAY
// ============================================================================

void UMOColonyPortrait::SetPortraitImage(UTexture2D* Portrait)
{
	if (PortraitImage)
	{
		if (Portrait)
		{
			PortraitImage->SetBrushFromTexture(Portrait);
			PortraitImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			PortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UMOColonyPortrait::SetPortraitFromSoftPtr(TSoftObjectPtr<UTexture2D> PortraitPtr)
{
	if (PortraitPtr.IsNull())
	{
		SetPortraitImage(nullptr);
	}
	else
	{
		UTexture2D* LoadedPortrait = PortraitPtr.LoadSynchronous();
		SetPortraitImage(LoadedPortrait);
	}
}

// ============================================================================
// STATE DISPLAY
// ============================================================================

void UMOColonyPortrait::SetAlertState(EAlertState InAlertState)
{
	AlertState = InAlertState;
	PulseTime = 0.0f; // Reset pulse animation
	UpdateVisuals();
}

void UMOColonyPortrait::SetActivityState(EColonyActivityState InActivityState)
{
	ActivityState = InActivityState;
	UpdateVisuals();
}

void UMOColonyPortrait::SetCharacterName(const FText& InName)
{
	CharacterName = InName;
	if (NameLabel)
	{
		NameLabel->SetText(CharacterName);
	}
}

// ============================================================================
// SELECTION
// ============================================================================

void UMOColonyPortrait::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		UpdateVisuals();
	}
}

// ============================================================================
// VISUALS
// ============================================================================

void UMOColonyPortrait::UpdateVisuals_Implementation()
{
	// Update border color based on selection and alert state
	if (AlertBorder)
	{
		FLinearColor BorderColor;

		if (bIsSelected)
		{
			BorderColor = SelectedBorderColor;
		}
		else
		{
			switch (AlertState)
			{
			case EAlertState::Critical:
				BorderColor = CriticalBorderColor;
				break;
			case EAlertState::Urgent:
				BorderColor = UrgentBorderColor;
				break;
			case EAlertState::Notable:
				BorderColor = NotableBorderColor;
				break;
			default:
				BorderColor = NormalBorderColor;
				break;
			}
		}

		// Only set static color here - pulsing handled in Tick
		if (AlertState != EAlertState::Critical || bIsSelected)
		{
			AlertBorder->SetBrushColor(BorderColor);
		}
	}

	// Update activity indicator visibility/icon
	// (Subclasses should override to set appropriate icons)
	if (ActivityIndicator)
	{
		bool bShowActivity = (ActivityState != EColonyActivityState::Idle &&
							  ActivityState != EColonyActivityState::Possessed);
		ActivityIndicator->SetVisibility(bShowActivity ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

// ============================================================================
// CLICK HANDLING
// ============================================================================

void UMOColonyPortrait::HandleButtonClicked()
{
	if (CharacterGuid.IsValid())
	{
		OnPortraitClicked.Broadcast(CharacterGuid);
	}
}
