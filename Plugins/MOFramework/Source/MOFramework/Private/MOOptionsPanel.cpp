#include "MOOptionsPanel.h"
#include "MODevMode.h"
#include "MOAudioSubsystem.h"
#include "MOGameSettings.h"
#include "MOUIManagerComponent.h"
#include "MOFramework.h"
#include "MOKeyBindingEntryWidget.h"
#include "MOKeyBindingManager.h"
#include "MOPlayerController.h"
#include "CommonButtonBase.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"

UMOOptionsPanel::UMOOptionsPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Options panel must be closed deliberately (Back button / Escape).
	// Outside-click would dismiss the entire main menu underneath it because
	// the panel sits inside the main menu's WidgetSwitcher — outside the
	// panel bounds == outside the main menu == "click outside" handler fires
	// against the main menu's surface, leaving the player with no UI.
	bClosesOnOutsideClick = false;
}

void UMOOptionsPanel::NativeDestruct()
{
	// Clean up delegate bindings to prevent dangling references
	if (ApplyButton)
	{
		ApplyButton->OnClicked().RemoveAll(this);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked().RemoveAll(this);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleMasterVolumeChanged);
	}
	if (MusicSlider)
	{
		MusicSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleMusicVolumeChanged);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleSFXVolumeChanged);
	}
	if (AmbientVolumeSlider)
	{
		AmbientVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleAmbientVolumeChanged);
	}
	if (WeatherVolumeSlider)
	{
		WeatherVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleWeatherVolumeChanged);
	}
	if (CameraSensitivitySlider)
	{
		CameraSensitivitySlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleCameraSensitivityChanged);
	}
	if (ResolutionComboBox)
	{
		ResolutionComboBox->OnSelectionChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleResolutionChanged);
	}
	if (ResetAllBindingsButton)
	{
		ResetAllBindingsButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

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
	if (MusicSlider)
	{
		MusicSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleMusicVolumeChanged);
		MusicSlider->OnValueChanged.AddDynamic(this, &UMOOptionsPanel::HandleMusicVolumeChanged);
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
	if (WeatherVolumeSlider)
	{
		WeatherVolumeSlider->OnValueChanged.RemoveDynamic(this, &UMOOptionsPanel::HandleWeatherVolumeChanged);
		WeatherVolumeSlider->OnValueChanged.AddDynamic(this, &UMOOptionsPanel::HandleWeatherVolumeChanged);
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
	PopulateQualityOptions();

	// Key bindings
	if (ResetAllBindingsButton)
	{
		ResetAllBindingsButton->OnClicked().RemoveAll(this);
		ResetAllBindingsButton->OnClicked().AddUObject(this, &UMOOptionsPanel::HandleResetAllBindingsClicked);
	}
	PopulateKeyBindings();

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
		// The exact current resolution (PopulateResolutionOptions makes sure it is in the list): matching to a "closest standard" size would change an ultrawide or 16:10 display on the next Apply.
		const FIntPoint CurrentRes = Settings->GetScreenResolution();
		ResolutionComboBox->SetSelectedOption(FString::Printf(TEXT("%d x %d"), CurrentRes.X, CurrentRes.Y));
	}

	if (QualityComboBox)
	{
		const int32 Level = Settings->GetGraphicsQualityLevel();
		if (Level < 0)
		{
			const FString Custom = UMOGameSettings::GraphicsQualityLevelName(-1).ToString();
			if (QualityComboBox->FindOptionIndex(Custom) == INDEX_NONE)
			{
				QualityComboBox->AddOption(Custom);
			}
			QualityComboBox->SetSelectedOption(Custom);
		}
		else
		{
			QualityComboBox->SetSelectedOption(UMOGameSettings::GraphicsQualityLevelName(Level).ToString());
		}
	}
	if (VSyncCheckbox)
	{
		VSyncCheckbox->SetIsChecked(Settings->IsVSyncEnabled());
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

	if (MusicSlider)
	{
		MusicSlider->SetValue(Settings->MusicVolume);
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

	if (WeatherVolumeSlider)
	{
		WeatherVolumeSlider->SetValue(Settings->WeatherVolume);
	}
	UpdateVolumeLabel(WeatherVolumeText, Settings->WeatherVolume);

	// Push current settings to the audio subsystem so the runtime mix matches
	// what the saved values say. Without this, opening the options panel for
	// the first time shows the right slider positions but volumes default to
	// the audio settings' defaults — they're out of sync until the user touches
	// a slider.
	if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this))
	{
		Audio->SetMasterVolume(Settings->MasterVolume);
		Audio->SetMusicVolume(Settings->MusicVolume);
		Audio->SetSFXVolume(Settings->SFXVolume);
		Audio->SetAmbientVolume(Settings->AmbientVolume);
		Audio->SetWeatherVolume(Settings->WeatherVolume);
	}

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
	if (DeveloperModeCheckbox)
	{
		DeveloperModeCheckbox->SetIsChecked(FMODevMode::IsEnabled());
		DeveloperModeCheckbox->SetIsEnabled(!FMODevMode::IsForcedByBuild());
	}
	if (DeveloperModeNote)
	{
		DeveloperModeNote->SetText(FMODevMode::IsForcedByBuild()
			? NSLOCTEXT("MOOptions", "DevModeForced", "Always on in development builds. ~ opens the console; Run with nothing typed opens the command menu.")
			: NSLOCTEXT("MOOptions", "DevModeHint", "Cheats. ~ opens the console; Run with nothing typed opens the command menu."));
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

	if (QualityComboBox)
	{
		const int32 Picked = QualityComboBox->GetSelectedIndex();
		// "Custom" (past the four levels) is not a choice: leave the mixed groups as they are.
		if (Picked >= 0 && Picked <= UMOGameSettings::MaxGraphicsQualityLevel && Picked != Settings->GetGraphicsQualityLevel())
		{
			Settings->SetGraphicsQualityLevel(Picked);
		}
	}
	if (VSyncCheckbox)
	{
		Settings->SetVSyncEnabled(VSyncCheckbox->IsChecked());
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
	if (MusicSlider)
	{
		Settings->MusicVolume = MusicSlider->GetValue();
	}
	if (SFXVolumeSlider)
	{
		Settings->SFXVolume = SFXVolumeSlider->GetValue();
	}
	if (AmbientVolumeSlider)
	{
		Settings->AmbientVolume = AmbientVolumeSlider->GetValue();
	}
	if (WeatherVolumeSlider)
	{
		Settings->WeatherVolume = WeatherVolumeSlider->GetValue();
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
	if (DeveloperModeCheckbox && !FMODevMode::IsForcedByBuild())
	{
		Settings->bDeveloperMode = DeveloperModeCheckbox->IsChecked();
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
		Settings->EnsureGraphicsQualityChosen();   // a real machine re-detects its quality level (no-op in the editor)
		RefreshUIFromSettings();
		UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Settings reset to defaults"));
	}
	OnRefreshSettings();
}

void UMOOptionsPanel::HandleApplyClicked()
{
	ApplySettings();

	// Restore focus to this widget after applying settings
	// Resolution changes can disrupt focus, so re-focus the back button
	if (BackButton)
	{
		BackButton->SetFocus();
	}
}

void UMOOptionsPanel::HandleResetClicked()
{
	ResetToDefaults();
}

void UMOOptionsPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

// Volume handlers push the new value INTO the audio subsystem in real-time so
// the user hears the change while dragging the slider. Persisted to disk in
// ApplySettings; this just updates the live state.

void UMOOptionsPanel::HandleMasterVolumeChanged(float Value)
{
	UpdateVolumeLabel(MasterVolumeText, Value);
	if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this))
	{
		Audio->SetMasterVolume(Value);
	}
}

void UMOOptionsPanel::HandleMusicVolumeChanged(float Value)
{
	UpdateVolumeLabel(MusicVolumeText, Value);
	if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this))
	{
		Audio->SetMusicVolume(Value);
	}
}

void UMOOptionsPanel::HandleSFXVolumeChanged(float Value)
{
	UpdateVolumeLabel(SFXVolumeText, Value);
	if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this))
	{
		Audio->SetSFXVolume(Value);
	}
}

void UMOOptionsPanel::HandleAmbientVolumeChanged(float Value)
{
	UpdateVolumeLabel(AmbientVolumeText, Value);
	if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this))
	{
		Audio->SetAmbientVolume(Value);
	}
}

void UMOOptionsPanel::HandleWeatherVolumeChanged(float Value)
{
	UpdateVolumeLabel(WeatherVolumeText, Value);
	if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this))
	{
		Audio->SetWeatherVolume(Value);
	}
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

	// What the display actually supports (ultrawide, 16:10, 1.5K ... included), smallest first; the current resolution is always in the list so Apply never silently changes it.
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	Resolutions.RemoveAll([](const FIntPoint& R) { return R.X < 1024 || R.Y < 576; });
	if (const UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Resolutions.AddUnique(Settings->GetScreenResolution());
	}
	if (Resolutions.Num() == 0)
	{
		Resolutions = { FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
	}
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	for (const FIntPoint& R : Resolutions)
	{
		ResolutionComboBox->AddOption(FString::Printf(TEXT("%d x %d"), R.X, R.Y));
	}
}

void UMOOptionsPanel::PopulateQualityOptions()
{
	if (!QualityComboBox)
	{
		return;
	}
	QualityComboBox->ClearOptions();
	for (int32 Level = 0; Level <= UMOGameSettings::MaxGraphicsQualityLevel; ++Level)
	{
		QualityComboBox->AddOption(UMOGameSettings::GraphicsQualityLevelName(Level).ToString());
	}
}

FIntPoint UMOOptionsPanel::GetResolutionFromString(const FString& ResString) const
{
	return ParseResolution(ResString);
}

FIntPoint UMOOptionsPanel::ParseResolution(const FString& ResString)
{
	// "1920 x 1080". (An older layout labelled the choices "1080p (1920x1080)": the numbers are still the last two integers in the text.)
	TArray<int32> Numbers;
	FString Digits;
	for (const TCHAR C : ResString + TEXT(" "))
	{
		if (FChar::IsDigit(C))
		{
			Digits.AppendChar(C);
		}
		else if (!Digits.IsEmpty())
		{
			Numbers.Add(FCString::Atoi(*Digits));
			Digits.Reset();
		}
	}
	if (Numbers.Num() >= 2 && Numbers[Numbers.Num() - 2] >= 640 && Numbers.Last() >= 360)
	{
		return FIntPoint(Numbers[Numbers.Num() - 2], Numbers.Last());
	}
	return FIntPoint(1920, 1080);
}

void UMOOptionsPanel::HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// Just log for now - actual change happens on Apply
	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Resolution selected: %s"), *SelectedItem);
}

// ============================================================================
// KEY BINDINGS
// ============================================================================

void UMOOptionsPanel::PopulateKeyBindings()
{
	if (!KeyBindingsScrollBox || !KeyBindingEntryClass)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Key bindings scroll box or entry class not configured"));
		return;
	}

	if (RebindableActions.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] No rebindable actions configured"));
		return;
	}

	// Clear existing entries
	KeyBindingsScrollBox->ClearChildren();

	// Load mapping contexts
	UInputMappingContext* PawnContext = PawnControlContext.LoadSynchronous();
	UInputMappingContext* BuildContext = BuildingContext.LoadSynchronous();

	if (!PawnContext)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOOptionsPanel] PawnControlContext not configured"));
		return;
	}

	int32 EntryCount = 0;

	for (const FMOKeyBindingActionConfig& ActionConfig : RebindableActions)
	{
		// Load the action
		UInputAction* Action = ActionConfig.Action.LoadSynchronous();
		if (!Action)
		{
			continue;
		}

		UMOKeyBindingEntryWidget* Entry = CreateWidget<UMOKeyBindingEntryWidget>(this, KeyBindingEntryClass);
		if (!Entry)
		{
			continue;
		}

		// Determine which context to use
		UInputMappingContext* ActionContext = ActionConfig.bUseBuildingContext ? BuildContext : PawnContext;
		if (!ActionContext)
		{
			ActionContext = PawnContext;
		}

		FMOKeyBindingVisualData Data;
		Data.DisplayName = ActionConfig.DisplayName;
		Data.ActionId = FMOKeyBindingManager::GetActionId(Action);
		Data.CurrentKey = FMOKeyBindingManager::GetCurrentBinding(Action, ActionContext);
		Data.DefaultKey = FMOKeyBindingManager::GetDefaultBinding(Action, ActionContext);
		Data.Category = ActionConfig.Category;

		Entry->SetupEntry(Data);
		Entry->OnKeyChanged.AddDynamic(this, &UMOOptionsPanel::HandleKeyBindingChanged);

		KeyBindingsScrollBox->AddChild(Entry);
		EntryCount++;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Populated %d key binding entries"), EntryCount);
}

void UMOOptionsPanel::HandleKeyBindingChanged(FName ActionId, FKey NewKey)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Key binding changed: %s -> %s"),
		*ActionId.ToString(), *NewKey.ToString());

	// Save to settings - this persists to GameUserSettings.ini
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->SetCustomBinding(ActionId, NewKey);
		Settings->SaveSettings();
	}

	// Try to apply the binding immediately if we have an MOPlayerController (in-game)
	AMOPlayerController* PC = Cast<AMOPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		// Not in gameplay - binding will be applied when game starts via MOPlayerController::BeginPlay
		UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] No MOPlayerController - binding will apply on game start"));
		return;
	}

	// Find the action config to determine context
	for (const FMOKeyBindingActionConfig& ActionConfig : RebindableActions)
	{
		UInputAction* Action = ActionConfig.Action.LoadSynchronous();
		if (!Action || FMOKeyBindingManager::GetActionId(Action) != ActionId)
		{
			continue;
		}

		// Get the appropriate context
		UInputMappingContext* Context = ActionConfig.bUseBuildingContext
			? PC->BaseBuildingContext
			: PC->PawnControlContext;

		if (Context)
		{
			FMOKeyBindingManager::ApplyBinding(Action, NewKey, Context, PC);
		}
		break;
	}
}

void UMOOptionsPanel::HandleResetAllBindingsClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] Reset all key bindings clicked"));

	// Clear all custom bindings from settings
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->ClearAllCustomBindings();
		Settings->SaveSettings();
	}

	// Restore all bindings to their cached defaults
	AMOPlayerController* PC = Cast<AMOPlayerController>(GetOwningPlayer());
	if (PC)
	{
		FMOKeyBindingManager::RestoreAllDefaults(PC);
	}

	// Repopulate the scroll box to show default keys
	PopulateKeyBindings();
}
