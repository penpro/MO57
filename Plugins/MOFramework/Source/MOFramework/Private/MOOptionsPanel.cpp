#include "MOOptionsPanel.h"
#include "MOGameSettings.h"
#include "MOUIManagerComponent.h"
#include "MOFramework.h"
#include "CommonButtonBase.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "GameFramework/PlayerController.h"

void UMOOptionsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button clicks
	if (ApplyButton)
	{
		ApplyButton->OnClicked().RemoveAll(this);
		ApplyButton->OnClicked().AddUObject(this, &UMOOptionsPanel::HandleApplyClicked);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked().RemoveAll(this);
		ResetButton->OnClicked().AddUObject(this, &UMOOptionsPanel::HandleResetClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOOptionsPanel::HandleBackClicked);
	}

	// Bind slider value changes for real-time feedback
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleMasterVolumeChanged);
		MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UMOOptionsPanel::HandleMasterVolumeChanged);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleMusicVolumeChanged);
		MusicVolumeSlider->OnValueChanged.AddDynamic(this, &UMOOptionsPanel::HandleMusicVolumeChanged);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnValueChanged.AddDynamic(this, &UMOOptionsPanel::HandleSFXVolumeChanged);
	}
	if (AmbientVolumeSlider)
	{
		AmbientVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleAmbientVolumeChanged);
		AmbientVolumeSlider->OnValueChanged.AddDynamic(this, &UMOOptionsPanel::HandleAmbientVolumeChanged);
	}
	if (CameraSensitivitySlider)
	{
		CameraSensitivitySlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleCameraSensitivityChanged);
		CameraSensitivitySlider->OnValueChanged.AddDynamic(this, &UMOOptionsPanel::HandleCameraSensitivityChanged);
	}

	// Resolution combo box
	if (ResolutionComboBox)
	{
		ResolutionComboBox->OnSelectionChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleResolutionChanged);
		ResolutionComboBox->OnSelectionChanged.AddDynamic(this, &UMOOptionsPanel::HandleResolutionChanged);
		PopulateResolutionOptions();
	}

	RefreshUIFromSettings();
	OnRefreshSettings();
}

UWidget* UMOOptionsPanel::NativeGetDesiredFocusTarget() const
{
	if (BackButton)
	{
		return BackButton;
	}
	return nullptr;
}

void UMOOptionsPanel::RefreshUIFromSettings()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOOptionsPanel] Could not get MOGameSettings"));
		return;
	}

	// Display options
	if (ShowFPSCheckbox)
	{
		ShowFPSCheckbox->SetIsChecked(Settings->bShowFPSCounter);
	}
	if (ShowFrameTimeCheckbox)
	{
		ShowFrameTimeCheckbox->SetIsChecked(Settings->bShowFrameTime);
	}

	// Graphics options - Resolution and Fullscreen
	if (ResolutionComboBox)
	{
		const FIntPoint CurrentRes = Settings->GetScreenResolution();
		FString ResString;

		// Match to closest standard resolution
		if (CurrentRes.Y >= 2160)
		{
			ResString = TEXT("4K (3840x2160)");
		}
		else if (CurrentRes.Y >= 1440)
		{
			ResString = TEXT("1440p (2560x1440)");
		}
		else if (CurrentRes.Y >= 1080)
		{
			ResString = TEXT("1080p (1920x1080)");
		}
		else
		{
			ResString = TEXT("720p (1280x720)");
		}

		ResolutionComboBox->SetSelectedOption(ResString);
	}

	if (FullscreenCheckbox)
	{
		const EWindowMode::Type WindowMode = Settings->GetFullscreenMode();
		FullscreenCheckbox->SetIsChecked(WindowMode == EWindowMode::Fullscreen || WindowMode == EWindowMode::WindowedFullscreen);
	}

	if (FOVSlider)
	{
		// Normalize FOV from 60-120 range to 0-1
		const float NormalizedFOV = (Settings->FieldOfView - 60.0f) / 60.0f;
		FOVSlider->SetValue(FMath::Clamp(NormalizedFOV, 0.0f, 1.0f));
	}
	if (FOVValueText)
	{
		FOVValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Settings->FieldOfView)));
	}
	if (MaxFrameRateSpinBox)
	{
		MaxFrameRateSpinBox->SetValue(static_cast<float>(Settings->MaxFrameRate));
	}
	if (MotionBlurCheckbox)
	{
		MotionBlurCheckbox->SetIsChecked(Settings->bEnableMotionBlur);
	}

	// Audio options
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetValue(Settings->MasterVolume);
	}
	UpdateVolumeLabel(MasterVolumeText, Settings->MasterVolume);

	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetValue(Settings->MusicVolume);
	}
	UpdateVolumeLabel(MusicVolumeText, Settings->MusicVolume);

	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetValue(Settings->SFXVolume);
	}
	UpdateVolumeLabel(SFXVolumeText, Settings->SFXVolume);

	if (AmbientVolumeSlider)
	{
		AmbientVolumeSlider->SetValue(Settings->AmbientVolume);
	}
	UpdateVolumeLabel(AmbientVolumeText, Settings->AmbientVolume);

	// Gameplay options
	if (CameraSensitivitySlider)
	{
		// Normalize from 0.1-5.0 to 0-1
		const float NormalizedSens = (Settings->CameraSensitivity - 0.1f) / 4.9f;
		CameraSensitivitySlider->SetValue(FMath::Clamp(NormalizedSens, 0.0f, 1.0f));
	}
	if (CameraSensitivityText)
	{
		CameraSensitivityText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), Settings->CameraSensitivity)));
	}
	if (InvertYCheckbox)
	{
		InvertYCheckbox->SetIsChecked(Settings->bInvertYAxis);
	}
	if (CameraShakeCheckbox)
	{
		CameraShakeCheckbox->SetIsChecked(Settings->bEnableCameraShake);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Refreshed UI from settings"));
}

void UMOOptionsPanel::ReadUIToSettings()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		return;
	}

	// Display options
	if (ShowFPSCheckbox)
	{
		Settings->bShowFPSCounter = ShowFPSCheckbox->IsChecked();
	}
	if (ShowFrameTimeCheckbox)
	{
		Settings->bShowFrameTime = ShowFrameTimeCheckbox->IsChecked();
	}

	// Graphics options - Resolution and Fullscreen
	if (ResolutionComboBox)
	{
		const FString SelectedRes = ResolutionComboBox->GetSelectedOption();
		const FIntPoint Resolution = GetResolutionFromString(SelectedRes);
		Settings->SetScreenResolution(Resolution);
	}

	if (FullscreenCheckbox)
	{
		const EWindowMode::Type WindowMode = FullscreenCheckbox->IsChecked()
			? EWindowMode::Fullscreen
			: EWindowMode::Windowed;
		Settings->SetFullscreenMode(WindowMode);
	}

	if (FOVSlider)
	{
		// Convert 0-1 slider to 60-120 FOV
		Settings->FieldOfView = 60.0f + (FOVSlider->GetValue() * 60.0f);
	}
	if (MaxFrameRateSpinBox)
	{
		Settings->MaxFrameRate = FMath::RoundToInt(MaxFrameRateSpinBox->GetValue());
	}
	if (MotionBlurCheckbox)
	{
		Settings->bEnableMotionBlur = MotionBlurCheckbox->IsChecked();
	}

	// Audio options
	if (MasterVolumeSlider)
	{
		Settings->MasterVolume = MasterVolumeSlider->GetValue();
	}
	if (MusicVolumeSlider)
	{
		Settings->MusicVolume = MusicVolumeSlider->GetValue();
	}
	if (SFXVolumeSlider)
	{
		Settings->SFXVolume = SFXVolumeSlider->GetValue();
	}
	if (AmbientVolumeSlider)
	{
		Settings->AmbientVolume = AmbientVolumeSlider->GetValue();
	}

	// Gameplay options
	if (CameraSensitivitySlider)
	{
		// Convert 0-1 slider to 0.1-5.0 sensitivity
		Settings->CameraSensitivity = 0.1f + (CameraSensitivitySlider->GetValue() * 4.9f);
	}
	if (InvertYCheckbox)
	{
		Settings->bInvertYAxis = InvertYCheckbox->IsChecked();
	}
	if (CameraShakeCheckbox)
	{
		Settings->bEnableCameraShake = CameraShakeCheckbox->IsChecked();
	}
}

void UMOOptionsPanel::ApplySettings()
{
	ReadUIToSettings();

	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		// Apply resolution settings separately (this handles fullscreen/windowed and resolution)
		Settings->ApplyResolutionSettings(false);

		// Apply our custom MO settings
		Settings->ApplySettings(false);
		Settings->SaveSettings();

		UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Settings applied and saved - Resolution: %dx%d, Fullscreen: %s"),
			Settings->GetScreenResolution().X, Settings->GetScreenResolution().Y,
			Settings->GetFullscreenMode() == EWindowMode::Fullscreen ? TEXT("Yes") : TEXT("No"));
	}

	// Refresh FPS counter visibility on UI manager
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		if (UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>())
		{
			UIManager->RefreshFPSCounter();
		}
	}
}

void UMOOptionsPanel::ResetToDefaults()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->SetToDefaults();
		RefreshUIFromSettings();
		UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Settings reset to defaults"));
	}
	OnRefreshSettings();
}

void UMOOptionsPanel::HandleApplyClicked()
{
	ApplySettings();
}

void UMOOptionsPanel::HandleResetClicked()
{
	ResetToDefaults();
}

void UMOOptionsPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOOptionsPanel::HandleMasterVolumeChanged(float Value)
{
	UpdateVolumeLabel(MasterVolumeText, Value);
}

void UMOOptionsPanel::HandleMusicVolumeChanged(float Value)
{
	UpdateVolumeLabel(MusicVolumeText, Value);
}

void UMOOptionsPanel::HandleSFXVolumeChanged(float Value)
{
	UpdateVolumeLabel(SFXVolumeText, Value);
}

void UMOOptionsPanel::HandleAmbientVolumeChanged(float Value)
{
	UpdateVolumeLabel(AmbientVolumeText, Value);
}

void UMOOptionsPanel::HandleCameraSensitivityChanged(float Value)
{
	if (CameraSensitivityText)
	{
		// Convert 0-1 slider to 0.1-5.0 sensitivity for display
		const float Sensitivity = 0.1f + (Value * 4.9f);
		CameraSensitivityText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), Sensitivity)));
	}
}

void UMOOptionsPanel::UpdateVolumeLabel(UTextBlock* Label, float Value)
{
	if (Label)
	{
		const int32 Percentage = FMath::RoundToInt(Value * 100.0f);
		Label->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percentage)));
	}
}

void UMOOptionsPanel::PopulateResolutionOptions()
{
	if (!ResolutionComboBox)
	{
		return;
	}

	ResolutionComboBox->ClearOptions();

	// Standard 16:9 resolutions
	ResolutionComboBox->AddOption(TEXT("720p (1280x720)"));
	ResolutionComboBox->AddOption(TEXT("1080p (1920x1080)"));
	ResolutionComboBox->AddOption(TEXT("1440p (2560x1440)"));
	ResolutionComboBox->AddOption(TEXT("4K (3840x2160)"));

	// Default to 1080p if nothing selected
	if (ResolutionComboBox->GetSelectedOption().IsEmpty())
	{
		ResolutionComboBox->SetSelectedOption(TEXT("1080p (1920x1080)"));
	}
}

FIntPoint UMOOptionsPanel::GetResolutionFromString(const FString& ResString) const
{
	if (ResString.Contains(TEXT("720p")))
	{
		return FIntPoint(1280, 720);
	}
	else if (ResString.Contains(TEXT("1080p")))
	{
		return FIntPoint(1920, 1080);
	}
	else if (ResString.Contains(TEXT("1440p")))
	{
		return FIntPoint(2560, 1440);
	}
	else if (ResString.Contains(TEXT("4K")))
	{
		return FIntPoint(3840, 2160);
	}

	// Default to 1080p
	return FIntPoint(1920, 1080);
}

void UMOOptionsPanel::HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// Just log for now - actual change happens on Apply
	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Resolution selected: %s"), *SelectedItem);
}
