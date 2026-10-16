#include "MOCharacterCreatorPanel.h"
#include "MOFramework.h"
#include "MOAppearanceSubsystem.h"
#include "MOCommonButton.h"
#include "MOGameInstance.h"
#include "MOIdentityComponent.h"
#include "MOMetaHumanCharacter.h"
#include "MOPlayerController.h"
#include "MOUIUtils.h"
#include "MOWorldReadinessSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UMOCharacterCreatorPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkinToneSlider)
	{
		SkinToneSlider->OnValueChanged.RemoveDynamic(this, &UMOCharacterCreatorPanel::HandleSkinToneChanged);
		SkinToneSlider->OnValueChanged.AddDynamic(this, &UMOCharacterCreatorPanel::HandleSkinToneChanged);
	}
	if (HairShadeSlider)
	{
		HairShadeSlider->OnValueChanged.RemoveDynamic(this, &UMOCharacterCreatorPanel::HandleHairShadeChanged);
		HairShadeSlider->OnValueChanged.AddDynamic(this, &UMOCharacterCreatorPanel::HandleHairShadeChanged);
	}
	if (HairRednessSlider)
	{
		HairRednessSlider->OnValueChanged.RemoveDynamic(this, &UMOCharacterCreatorPanel::HandleHairRednessChanged);
		HairRednessSlider->OnValueChanged.AddDynamic(this, &UMOCharacterCreatorPanel::HandleHairRednessChanged);
	}
	auto Bind = [this](UMOCommonButton* Button, void (UMOCharacterCreatorPanel::*Handler)())
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
			Button->OnClicked().AddUObject(this, Handler);
		}
	};
	Bind(EyePrevButton, &UMOCharacterCreatorPanel::HandleEyePrev);
	Bind(EyeNextButton, &UMOCharacterCreatorPanel::HandleEyeNext);
	Bind(RandomizeButton, &UMOCharacterCreatorPanel::HandleRandomize);
	Bind(BeginButton, &UMOCharacterCreatorPanel::HandleBegin);

	UMOUIUtils::ApplyReadableTextInputStyle(NameInputBox);
}

void UMOCharacterCreatorPanel::NativeDestruct()
{
	for (UMOCommonButton* Button : { EyePrevButton.Get(), EyeNextButton.Get(), RandomizeButton.Get(), BeginButton.Get() })
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	ReleaseCamera(0.0f);
	Super::NativeDestruct();
}

UWidget* UMOCharacterCreatorPanel::NativeGetDesiredFocusTarget() const
{
	return BeginButton;
}

AMOMetaHumanCharacter* UMOCharacterCreatorPanel::GetSurvivor() const
{
	return Cast<AMOMetaHumanCharacter>(GetOwningPlayerPawn());
}

void UMOCharacterCreatorPanel::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (AMOMetaHumanCharacter* Survivor = GetSurvivor())
	{
		Look = Survivor->GetAppearance();
		// The shipped eye has no preset of its own (it is baked): start on the first one, so the label and the eye agree.
		if (Look.EyeColor.IsNone() && UMOAppearanceSubsystem::GetEyeColorPresets().Num() > 0)
		{
			Look.EyeColor = UMOAppearanceSubsystem::GetEyeColorPresets()[0].Id;
			ApplyLookToSurvivor();
		}
		if (NameInputBox)
		{
			if (const UMOIdentityComponent* Identity = Survivor->FindComponentByClass<UMOIdentityComponent>())
			{
				NameInputBox->SetText(Identity->DisplayName);
			}
		}
	}
	FrameSurvivor();
	RefreshControls();
	RefreshWorldStatus();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(WorldStatusTimer, FTimerDelegate::CreateUObject(this, &UMOCharacterCreatorPanel::RefreshWorldStatus), 0.5f, true);
	}
}

void UMOCharacterCreatorPanel::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldStatusTimer);
	}
	ReleaseCamera(0.6f);
	Super::NativeOnDeactivated();
}

void UMOCharacterCreatorPanel::FrameSurvivor()
{
	ReleaseCamera(0.0f);
	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	AMOMetaHumanCharacter* Survivor = GetSurvivor();
	if (!World || !PC || !Survivor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacterCreator] Nothing to frame: world=%d controller=%d survivor=%s"),
			World != nullptr, PC != nullptr, *GetNameSafe(GetOwningPlayerPawn()));
		return;
	}

	// Level with the chest, in front of the survivor, offset so they stand at PreviewScreenX across the screen (right of the panel).
	const float HalfHeight = Survivor->GetCapsuleComponent() ? Survivor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Feet = Survivor->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
	const FVector Focus = Feet + FVector(0.0f, 0.0f, PreviewFocusHeightCm);
	const FVector Front = FRotator(0.0f, Survivor->GetActorRotation().Yaw, 0.0f).Vector();   // from the survivor toward the camera
	const FVector Left = FVector::CrossProduct(-Front, FVector::UpVector).GetSafeNormal();   // the camera's left (UE: Up x Forward = right)
	const float HalfWidth = PreviewCameraDistanceCm * FMath::Tan(FMath::DegreesToRadians(PreviewFieldOfView * 0.5f));
	const float Shift = HalfWidth * (2.0f * PreviewScreenX - 1.0f);
	const FVector CameraLocation = Focus + Front * PreviewCameraDistanceCm + Left * Shift + FVector(0.0f, 0.0f, 12.0f);
	const FRotator CameraRotation = (-Front + FVector(0.0f, 0.0f, -0.05f)).Rotation();
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(CameraLocation, CameraRotation);
	if (!Camera)
	{
		return;
	}
	UCameraComponent* Lens = Camera->GetCameraComponent();
	Lens->SetFieldOfView(PreviewFieldOfView);
	Lens->SetConstraintAspectRatio(false);
	// Depth of field: explicitly off unless bPreviewDepthOfField (the engine drops hair from the scene depth near water -- see the header), so no level
	// post-process volume can turn it back on behind the creator. On: a shallow focus on the survivor, the world still streaming in behind stays soft.
	Lens->PostProcessSettings.bOverride_DepthOfFieldEnabled = true;
	Lens->PostProcessSettings.DepthOfFieldEnabled = bPreviewDepthOfField;
	if (bPreviewDepthOfField)
	{
		Lens->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
		Lens->PostProcessSettings.DepthOfFieldFocalDistance = PreviewCameraDistanceCm;
		Lens->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
		Lens->PostProcessSettings.DepthOfFieldFstop = PreviewFStop;
	}
	Lens->PostProcessBlendWeight = 1.0f;
	PreviewCamera = Camera;
	PC->SetViewTarget(Camera);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacterCreator] Framing %s at %s, camera %.0f cm away"), *Survivor->GetName(), *Feet.ToCompactString(), PreviewCameraDistanceCm);
}

void UMOCharacterCreatorPanel::ReleaseCamera(float BlendSeconds)
{
	ACameraActor* Camera = PreviewCamera.Get();
	PreviewCamera.Reset();
	if (!Camera)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = GetOwningPlayerPawn();
	if (PC && Pawn && PC->GetViewTarget() == Camera)
	{
		PC->SetViewTargetWithBlend(Pawn, BlendSeconds, VTBlend_Cubic);
	}
	Camera->SetLifeSpan(BlendSeconds + 0.1f);   // outlive the blend away from it
}

void UMOCharacterCreatorPanel::ApplyLookToSurvivor()
{
	// The new game's host owns its first survivor (authority): the look is set on the pawn, replicates to co-op partners and is saved with it.
	if (AMOMetaHumanCharacter* Survivor = GetSurvivor())
	{
		Survivor->SetAppearanceColors(Look);
	}
}

void UMOCharacterCreatorPanel::RefreshControls()
{
	if (SkinToneSlider)
	{
		SkinToneSlider->SetValue(Look.SkinTone);
	}
	if (HairShadeSlider)
	{
		HairShadeSlider->SetValue(Look.HairMelanin);
	}
	if (HairRednessSlider)
	{
		HairRednessSlider->SetValue(Look.HairRedness);
	}
	if (EyeColorText)
	{
		const TArray<FMOEyeColorPreset>& Presets = UMOAppearanceSubsystem::GetEyeColorPresets();
		EyeColorText->SetText(Presets.IsValidIndex(GetEyeColorIndex()) ? Presets[GetEyeColorIndex()].DisplayName : FText::GetEmpty());
	}
}

void UMOCharacterCreatorPanel::RefreshWorldStatus()
{
	if (!WorldStatusText)
	{
		return;
	}
	const UMOWorldReadinessSubsystem* Readiness = UMOWorldReadinessSubsystem::Get(this);
	WorldStatusText->SetText(Readiness && Readiness->IsTracking()
		? FText::Format(NSLOCTEXT("MOCharacterCreator", "WorldLoading", "The world is taking shape around you ({0}%)"), FText::AsNumber(FMath::RoundToInt(100.0f * Readiness->GetProgress01())))
		: NSLOCTEXT("MOCharacterCreator", "WorldReady", "The world is ready."));
}

void UMOCharacterCreatorPanel::HandleSkinToneChanged(float Value)
{
	Look.SkinTone = Value;
	ApplyLookToSurvivor();
}

void UMOCharacterCreatorPanel::HandleHairShadeChanged(float Value)
{
	Look.HairMelanin = Value;
	ApplyLookToSurvivor();
}

void UMOCharacterCreatorPanel::HandleHairRednessChanged(float Value)
{
	Look.HairRedness = Value;
	ApplyLookToSurvivor();
}

int32 UMOCharacterCreatorPanel::GetEyeColorIndex() const
{
	// None = the iris as shipped, which is the first preset.
	const int32 Index = UMOAppearanceSubsystem::GetEyeColorPresets().IndexOfByPredicate([this](const FMOEyeColorPreset& E) { return E.Id == Look.EyeColor; });
	return FMath::Max(Index, 0);
}

void UMOCharacterCreatorPanel::StepEyeColor(int32 Delta)
{
	const TArray<FMOEyeColorPreset>& Presets = UMOAppearanceSubsystem::GetEyeColorPresets();
	if (Presets.Num() > 0)
	{
		SetEyeColor(Presets[(GetEyeColorIndex() + Delta + Presets.Num()) % Presets.Num()].Id);
	}
}

void UMOCharacterCreatorPanel::SetEyeColor(FName PresetId)
{
	Look.EyeColor = PresetId;
	ApplyLookToSurvivor();
	RefreshControls();
}

void UMOCharacterCreatorPanel::HandleRandomize()
{
	UMOAppearanceSubsystem::RandomizeColors(Look);
	ApplyLookToSurvivor();
	RefreshControls();
}

void UMOCharacterCreatorPanel::HandleBegin()
{
	AMOMetaHumanCharacter* Survivor = GetSurvivor();
	const FString Name = NameInputBox ? NameInputBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (Survivor && !Name.IsEmpty())
	{
		if (AMOPlayerController* PC = Cast<AMOPlayerController>(GetOwningPlayer()))
		{
			if (UMOIdentityComponent* Identity = Survivor->FindComponentByClass<UMOIdentityComponent>())
			{
				PC->RequestSetPawnIdentity(Identity->GetOrCreateGuid(), TEXT("CharacterName"), Name);
			}
		}
	}
	ApplyLookToSurvivor();
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacterCreator] Begin: '%s', skin %.2f, hair %.2f / %.2f, eyes %s"),
		*Name, Look.SkinTone, Look.HairMelanin, Look.HairRedness, *Look.EyeColor.ToString());

	// The world may still be filling in (the creator was quick): the loading overlay covers the rest of the readiness cycle.
	const UMOWorldReadinessSubsystem* Readiness = UMOWorldReadinessSubsystem::Get(this);
	if (Readiness && Readiness->IsTracking())
	{
		if (UMOGameInstance* GameInstance = Cast<UMOGameInstance>(GetGameInstance()))
		{
			GameInstance->ShowLoadingOverlay();
			GameInstance->ReleaseLoadingOverlayWhenWorldReady(GameInstance->LoadingScreenFadeOutSeconds);
		}
	}
	DeactivateWidget();
}
