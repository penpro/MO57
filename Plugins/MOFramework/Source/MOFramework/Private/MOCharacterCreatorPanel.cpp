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
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

UMOCharacterCreatorPanel::UMOCharacterCreatorPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A step of the new game, not a menu: a click on the world must not close it (Wes, Oct 25 2026: "if you click into the world while doing the
	// character customization it closes the character builder and just keeps whatever was there"). The world behind is OrbitArea now anyway.
	bClosesOnOutsideClick = false;
}

void UMOCharacterCreatorPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (OrbitArea)
	{
		OrbitArea->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UMOCharacterCreatorPanel, HandleOrbitAreaMouseDown));
	}

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
	Bind(HairStylePrevButton, &UMOCharacterCreatorPanel::HandleHairStylePrev);
	Bind(HairStyleNextButton, &UMOCharacterCreatorPanel::HandleHairStyleNext);
	Bind(BeardPrevButton, &UMOCharacterCreatorPanel::HandleBeardPrev);
	Bind(BeardNextButton, &UMOCharacterCreatorPanel::HandleBeardNext);
	Bind(MustachePrevButton, &UMOCharacterCreatorPanel::HandleMustachePrev);
	Bind(MustacheNextButton, &UMOCharacterCreatorPanel::HandleMustacheNext);
	Bind(RandomizeButton, &UMOCharacterCreatorPanel::HandleRandomize);
	Bind(BeginButton, &UMOCharacterCreatorPanel::HandleBegin);

	UMOUIUtils::ApplyReadableTextInputStyle(NameInputBox);
}

void UMOCharacterCreatorPanel::NativeDestruct()
{
	for (UMOCommonButton* Button : { EyePrevButton.Get(), EyeNextButton.Get(), HairStylePrevButton.Get(), HairStyleNextButton.Get(), BeardPrevButton.Get(),
		BeardNextButton.Get(), MustachePrevButton.Get(), MustacheNextButton.Get(), RandomizeButton.Get(), BeginButton.Get() })
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	if (OrbitArea)
	{
		OrbitArea->OnMouseButtonDownEvent.Unbind();
	}
	GroomPreloads.Empty();
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
	for (const EMOAppearanceCategory GroomSlot : { EMOAppearanceCategory::Hair, EMOAppearanceCategory::Beard, EMOAppearanceCategory::Mustache })
	{
		PreloadGroomNeighbours(GroomSlot);
	}
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
	GroomPreloads.Empty();   // the styles not chosen can go
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

	// Level with the chest, in front of the survivor (PlacePreviewCamera: offset so they stand at PreviewScreenX across the screen, right of the panel).
	const float HalfHeight = Survivor->GetCapsuleComponent() ? Survivor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Feet = Survivor->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
	PreviewFocus = Feet + FVector(0.0f, 0.0f, PreviewFocusHeightCm);
	PreviewBaseYawDeg = Survivor->GetActorRotation().Yaw;
	OrbitYawDeg = 0.0f;
	OrbitPitchDeg = 0.0f;
	PreviewDistanceCm = PreviewCameraDistanceCm;
	bOrbitDragging = false;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(PreviewFocus, FRotator::ZeroRotator);
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
	PlacePreviewCamera();
	PC->SetViewTarget(Camera);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacterCreator] Framing %s at %s, camera %.0f cm away"), *Survivor->GetName(), *Feet.ToCompactString(), PreviewCameraDistanceCm);
}

void UMOCharacterCreatorPanel::PlacePreviewCamera()
{
	ACameraActor* Camera = PreviewCamera.Get();
	if (!Camera)
	{
		return;
	}
	const FVector Toward = FRotator(OrbitPitchDeg, PreviewBaseYawDeg + OrbitYawDeg, 0.0f).Vector();   // from the survivor toward the camera
	const FVector Left = FVector::CrossProduct(-Toward, FVector::UpVector).GetSafeNormal();   // the camera's left (UE: Up x Forward = right)
	const float HalfWidth = PreviewDistanceCm * FMath::Tan(FMath::DegreesToRadians(PreviewFieldOfView * 0.5f));
	const float Shift = HalfWidth * (2.0f * PreviewScreenX - 1.0f);
	FVector Location = PreviewFocus + Toward * PreviewDistanceCm + Left * Shift + FVector(0.0f, 0.0f, 12.0f);
	// Turned round, the camera may be inside the slope, rock or trunk behind the survivor: bring it out in front of that.
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(MOCreatorPreviewCamera), false, GetOwningPlayerPawn());
	if (GetWorld() && GetWorld()->SweepSingleByChannel(Hit, PreviewFocus, Location, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.0f), Params))
	{
		Location = Hit.Location;
	}
	Camera->SetActorLocationAndRotation(Location, (-Toward + FVector(0.0f, 0.0f, -0.05f)).Rotation());
	if (bPreviewDepthOfField)
	{
		Camera->GetCameraComponent()->PostProcessSettings.DepthOfFieldFocalDistance = FVector::Dist(Location, PreviewFocus);
	}
}

void UMOCharacterCreatorPanel::OrbitPreview(float DeltaYawDeg, float DeltaPitchDeg)
{
	OrbitYawDeg = FMath::UnwindDegrees(OrbitYawDeg + DeltaYawDeg);
	OrbitPitchDeg = FMath::Clamp(OrbitPitchDeg + DeltaPitchDeg, OrbitPitchRange.X, OrbitPitchRange.Y);
	PlacePreviewCamera();
}

void UMOCharacterCreatorPanel::ZoomPreview(float DeltaCm)
{
	PreviewDistanceCm = FMath::Clamp(PreviewDistanceCm + DeltaCm, PreviewZoomMinCm, PreviewZoomMaxCm);
	PlacePreviewCamera();
}

FEventReply UMOCharacterCreatorPanel::HandleOrbitAreaMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton && MouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return UWidgetBlueprintLibrary::Unhandled();
	}
	bOrbitDragging = true;
	LastDragPosition = MouseEvent.GetScreenSpacePosition();
	FEventReply Reply = UWidgetBlueprintLibrary::Handled();
	return UWidgetBlueprintLibrary::CaptureMouse(Reply, this);
}

FReply UMOCharacterCreatorPanel::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bOrbitDragging)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	const FVector2D Delta = Position - LastDragPosition;
	LastDragPosition = Position;
	// The survivor turns with the drag, like a turntable: drag right, the camera goes round to their left; drag down, it looks from higher.
	OrbitPreview(Delta.X * OrbitDegreesPerPixel, Delta.Y * OrbitDegreesPerPixel);
	return FReply::Handled();
}

FReply UMOCharacterCreatorPanel::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bOrbitDragging)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	bOrbitDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UMOCharacterCreatorPanel::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	ZoomPreview(-InMouseEvent.GetWheelDelta() * 25.0f);
	return FReply::Handled();
}

void UMOCharacterCreatorPanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bOrbitDragging = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UMOCharacterCreatorPanel::PreloadGroomNeighbours(EMOAppearanceCategory GroomSlot)
{
	const TArray<FMOAppearanceAssetEntry> Choices = GetGroomChoices(GroomSlot);
	if (Choices.Num() < 2)
	{
		return;
	}
	const int32 Index = FMath::Max(GetGroomIndex(GroomSlot, Choices), 0);
	TArray<FSoftObjectPath> Paths;
	for (const int32 Step : { -1, 1 })
	{
		const FString& Path = Choices[(Index + Step + Choices.Num()) % Choices.Num()].AssetPath;
		if (Path != FMOCharacterAppearance::NoGroom)
		{
			Paths.Add(FSoftObjectPath(Path));
			Paths.Append(UMOAppearanceSubsystem::FindGroomBindingPaths(FSoftObjectPath(Path)));
		}
	}
	// Replacing the slot's handle lets the previous neighbours go (unless the survivor now wears one).
	GroomPreloads.Add(GroomSlot, Paths.IsEmpty() ? nullptr : UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths));
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
		Survivor->SetAppearanceLook(Look);
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
	if (HairStyleText)
	{
		HairStyleText->SetText(GetGroomLabel(EMOAppearanceCategory::Hair));
	}
	if (BeardText)
	{
		BeardText->SetText(GetGroomLabel(EMOAppearanceCategory::Beard));
	}
	if (MustacheText)
	{
		MustacheText->SetText(GetGroomLabel(EMOAppearanceCategory::Mustache));
	}
}

TArray<FMOAppearanceAssetEntry> UMOCharacterCreatorPanel::GetGroomChoices(EMOAppearanceCategory GroomSlot) const
{
	TArray<FMOAppearanceAssetEntry> Choices;
	FMOAppearanceAssetEntry& None = Choices.AddDefaulted_GetRef();
	None.AssetPath = FMOCharacterAppearance::NoGroom;
	None.DisplayName = GroomSlot == EMOAppearanceCategory::Hair ? NSLOCTEXT("MOCharacterCreator", "Bald", "Bald") : NSLOCTEXT("MOCharacterCreator", "NoGroom", "None");
	UGameInstance* GameInstance = GetGameInstance();
	if (UMOAppearanceSubsystem* Appearance = GameInstance ? GameInstance->GetSubsystem<UMOAppearanceSubsystem>() : nullptr)
	{
		Appearance->EnsureDiscovered();
		Choices.Append(Appearance->GetAssetsForCategory(GroomSlot));
	}
	return Choices;
}

int32 UMOCharacterCreatorPanel::GetGroomIndex(EMOAppearanceCategory GroomSlot, const TArray<FMOAppearanceAssetEntry>& Choices) const
{
	FMOCharacterAppearance Current = Look;
	const FString& Path = *UMOAppearanceSubsystem::GetGroomPath(Current, GroomSlot);
	if (!Path.IsEmpty())
	{
		return Choices.IndexOfByPredicate([&Path](const FMOAppearanceAssetEntry& E) { return E.AssetPath == Path; });
	}
	const UGroomComponent* Component = UMOAppearanceSubsystem::FindGroomComponent(GetSurvivor(), GroomSlot);
	if (!Component || !Component->GroomAsset)
	{
		return 0;   // None
	}
	const FString Worn = Component->GroomAsset->GetName();
	return Choices.IndexOfByPredicate([&Worn](const FMOAppearanceAssetEntry& E) { return FPackageName::ObjectPathToObjectName(E.AssetPath) == Worn; });
}

FText UMOCharacterCreatorPanel::GetGroomLabel(EMOAppearanceCategory GroomSlot) const
{
	const TArray<FMOAppearanceAssetEntry> Choices = GetGroomChoices(GroomSlot);
	const int32 Index = GetGroomIndex(GroomSlot, Choices);
	if (Choices.IsValidIndex(Index))
	{
		return Choices[Index].DisplayName;
	}
	const UGroomComponent* Component = UMOAppearanceSubsystem::FindGroomComponent(GetSurvivor(), GroomSlot);
	return Component && Component->GroomAsset ? UMOAppearanceSubsystem::MakeGroomDisplayName(Component->GroomAsset->GetName()) : FText::GetEmpty();
}

void UMOCharacterCreatorPanel::StepGroom(EMOAppearanceCategory GroomSlot, int32 Delta)
{
	const TArray<FMOAppearanceAssetEntry> Choices = GetGroomChoices(GroomSlot);
	const int32 Index = FMath::Max(GetGroomIndex(GroomSlot, Choices), 0);
	SetGroom(GroomSlot, Choices[(Index + Delta + Choices.Num()) % Choices.Num()].AssetPath);
}

void UMOCharacterCreatorPanel::SetGroom(EMOAppearanceCategory GroomSlot, const FString& Path)
{
	if (FString* SlotPath = UMOAppearanceSubsystem::GetGroomPath(Look, GroomSlot))
	{
		*SlotPath = Path;
		ApplyLookToSurvivor();
		RefreshControls();
		PreloadGroomNeighbours(GroomSlot);
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
	// Any hair style (never bald); the facial hair comes off -- it is a choice, and the survivor's face is a woman's MetaHuman.
	const TArray<FMOAppearanceAssetEntry> Hair = GetGroomChoices(EMOAppearanceCategory::Hair);
	if (Hair.Num() > 1)
	{
		Look.HairAssetPath = Hair[FMath::RandRange(1, Hair.Num() - 1)].AssetPath;
	}
	Look.BeardAssetPath = FMOCharacterAppearance::NoGroom;
	Look.MustacheAssetPath = FMOCharacterAppearance::NoGroom;
	ApplyLookToSurvivor();
	RefreshControls();
	PreloadGroomNeighbours(EMOAppearanceCategory::Hair);
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
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacterCreator] Begin: '%s', skin %.2f, hair %.2f / %.2f, eyes %s, hair style '%s', beard '%s', mustache '%s'"),
		*Name, Look.SkinTone, Look.HairMelanin, Look.HairRedness, *Look.EyeColor.ToString(), *GetGroomLabel(EMOAppearanceCategory::Hair).ToString(),
		*GetGroomLabel(EMOAppearanceCategory::Beard).ToString(), *GetGroomLabel(EMOAppearanceCategory::Mustache).ToString());

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
