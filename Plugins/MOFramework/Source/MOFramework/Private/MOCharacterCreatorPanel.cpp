#include "MOCharacterCreatorPanel.h"
#include "MOFramework.h"
#include "MOAppearanceSubsystem.h"
#include "MOCommonButton.h"
#include "MOGameInstance.h"
#include "MOIdentityComponent.h"
#include "MOMetaHumanCharacter.h"
#include "MOPlayerController.h"
#include "MOSurvivorBase.h"
#include "MOUIUtils.h"
#include "MOWorldReadinessSubsystem.h"

#include "AnimationRuntime.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/EditableTextBox.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "UObject/UObjectIterator.h"

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
	// The build sliders: -1 (the smaller shape) .. 0 (the base as made) .. +1 (the larger).
	for (USlider* Slider : { BustSlider.Get(), HipsSlider.Get(), WaistSlider.Get(), ThighsSlider.Get() })
	{
		if (Slider)
		{
			Slider->SetMinValue(-1.0f);
			Slider->SetMaxValue(1.0f);
			Slider->SetStepSize(0.01f);
		}
	}
	if (BustSlider)
	{
		BustSlider->OnValueChanged.RemoveDynamic(this, &UMOCharacterCreatorPanel::HandleBustChanged);
		BustSlider->OnValueChanged.AddDynamic(this, &UMOCharacterCreatorPanel::HandleBustChanged);
	}
	if (HipsSlider)
	{
		HipsSlider->OnValueChanged.RemoveDynamic(this, &UMOCharacterCreatorPanel::HandleHipsChanged);
		HipsSlider->OnValueChanged.AddDynamic(this, &UMOCharacterCreatorPanel::HandleHipsChanged);
	}
	if (WaistSlider)
	{
		WaistSlider->OnValueChanged.RemoveDynamic(this, &UMOCharacterCreatorPanel::HandleWaistChanged);
		WaistSlider->OnValueChanged.AddDynamic(this, &UMOCharacterCreatorPanel::HandleWaistChanged);
	}
	if (ThighsSlider)
	{
		ThighsSlider->OnValueChanged.RemoveDynamic(this, &UMOCharacterCreatorPanel::HandleThighsChanged);
		ThighsSlider->OnValueChanged.AddDynamic(this, &UMOCharacterCreatorPanel::HandleThighsChanged);
	}
	auto Bind = [this](UMOCommonButton* Button, void (UMOCharacterCreatorPanel::*Handler)())
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
			Button->OnClicked().AddUObject(this, Handler);
		}
	};
	Bind(BasePrevButton, &UMOCharacterCreatorPanel::HandleBasePrev);
	Bind(BaseNextButton, &UMOCharacterCreatorPanel::HandleBaseNext);
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
		BeardNextButton.Get(), MustachePrevButton.Get(), MustacheNextButton.Get(), BasePrevButton.Get(), BaseNextButton.Get(), RandomizeButton.Get(),
		BeginButton.Get() })
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
		// Not in the world until Begin: no creature takes them for quarry (the host's own pawn: the server, where the creatures think).
		if (Survivor->HasAuthority())
		{
			Survivor->SetInCharacterCreator(true);
		}
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
		World->GetTimerManager().SetTimer(WorldStatusTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			RefreshWorldStatus();
			KeepFaceFramed();
		}), 0.5f, true);
	}
}

void UMOCharacterCreatorPanel::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldStatusTimer);
		World->GetTimerManager().ClearTimer(AimAtFaceTimer);
	}
	GroomPreloads.Empty();   // the styles not chosen can go
	if (AMOMetaHumanCharacter* Survivor = GetSurvivor(); Survivor && Survivor->HasAuthority())
	{
		Survivor->SetInCharacterCreator(false);   // in the world now, however the creator closed
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

	PreviewDistanceCm = PreviewCameraDistanceCm;
	bOrbitDragging = false;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(Survivor->GetActorLocation(), FRotator::ZeroRotator);
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
	AimAtFace(/*bResetOrbit=*/true);
	PC->SetViewTarget(Camera);
}

float UMOCharacterCreatorPanel::FaceYawDeg(const AMOMetaHumanCharacter* Survivor)
{
	const USkeletalMeshComponent* Body = Survivor ? Survivor->GetMesh() : nullptr;
	const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	const int32 Head = Mesh ? Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head")) : INDEX_NONE;
	if (Head != INDEX_NONE)
	{
		// A skeletal mesh faces +Y in its own space: that direction in the head bone's axes (from its reference pose, whatever the rig's bone
		// axes are), carried by the head as it is posed now.
		const FTransform RefHead = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), Head);
		const FVector FaceInHead = RefHead.InverseTransformVectorNoScale(FVector::YAxisVector);
		FVector Face = Body->GetSocketTransform(TEXT("head"), RTS_World).TransformVectorNoScale(FaceInHead);
		Face.Z = 0.0f;
		if (Face.Normalize())
		{
			return Face.Rotation().Yaw;
		}
	}
	return Survivor ? Survivor->GetActorRotation().Yaw : 0.0f;
}

void UMOCharacterCreatorPanel::AimAtFace(bool bResetOrbit)
{
	AMOMetaHumanCharacter* Survivor = GetSurvivor();
	if (!Survivor || !PreviewCamera.IsValid())
	{
		return;
	}
	// Level with the chest, in front of the face (PlacePreviewCamera: offset so they stand at PreviewScreenX across the screen, right of the panel).
	const float HalfHeight = Survivor->GetCapsuleComponent() ? Survivor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	FramedAt = Survivor->GetActorLocation();
	PreviewFocus = FramedAt - FVector(0.0f, 0.0f, HalfHeight) + FVector(0.0f, 0.0f, PreviewFocusHeightCm);
	const USkeletalMeshComponent* Body = Survivor->GetMesh();
	PreviewFace = Body && Body->DoesSocketExist(TEXT("head")) ? Body->GetSocketLocation(TEXT("head")) : PreviewFocus + FVector(0.0f, 0.0f, 20.0f);
	PreviewBaseYawDeg = FaceYawDeg(Survivor);
	GatherPreviewPlants();
	if (bResetOrbit)
	{
		OrbitYawDeg = 0.0f;
		OrbitPitchDeg = 0.0f;
	}
	PlacePreviewCamera();
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacterCreator] Framing %s's face: looks along %.1f deg (actor %.1f), at %s, orbit %.1f deg%s"), *Survivor->GetName(),
		PreviewBaseYawDeg, Survivor->GetActorRotation().Yaw, *FramedAt.ToCompactString(), OrbitYawDeg, bResetOrbit ? TEXT(" (reset)") : TEXT(""));
}

void UMOCharacterCreatorPanel::KeepFaceFramed()
{
	const AMOMetaHumanCharacter* Survivor = GetSurvivor();
	if (!Survivor || !PreviewCamera.IsValid() || bOrbitDragging)
	{
		return;
	}
	const float Turned = FMath::Abs(FMath::FindDeltaAngleDegrees(PreviewBaseYawDeg, FaceYawDeg(Survivor)));
	if (Turned > 30.0f || FVector::Dist(Survivor->GetActorLocation(), FramedAt) > 50.0f)
	{
		AimAtFace(/*bResetOrbit=*/false);
		return;
	}
	// The grass and foliage round the survivor stream in after the creator opens: gather them again, and place the camera again when they changed
	// (Oct 29: plants 60 cm from the face, the camera not pulled in -- it was framed before they existed).
	const int32 Before = PreviewPlants.Num();
	GatherPreviewPlants();
	if (PreviewPlants.Num() != Before)
	{
		PlacePreviewCamera();
	}
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
	Location = PullInFrontOfPlants(Location);
	Camera->SetActorLocationAndRotation(Location, (-Toward + FVector(0.0f, 0.0f, -0.05f)).Rotation());
	if (bPreviewDepthOfField)
	{
		Camera->GetCameraComponent()->PostProcessSettings.DepthOfFieldFocalDistance = FVector::Dist(Location, PreviewFocus);
	}
}

void UMOCharacterCreatorPanel::GatherPreviewPlants()
{
	PreviewPlants.Reset();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Reach = PreviewZoomMaxCm + 100.0f;
	const FBox Around = FBox(PreviewFocus - FVector(Reach), PreviewFocus + FVector(Reach));
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		const UInstancedStaticMeshComponent* Plant = *It;
		if (Plant->GetWorld() != World || !Plant->IsRegistered() || !Plant->IsVisible() || Plant->GetInstanceCount() == 0 || !Plant->Bounds.GetBox().Intersect(Around))
		{
			continue;
		}
		if (Plant->IsCollisionEnabled() && Plant->GetCollisionResponseToChannel(ECC_Camera) == ECR_Block)
		{
			continue;   // solid: the camera's sweep stops in front of it already
		}
		PreviewPlants.Add(Plant);
	}
}

FVector UMOCharacterCreatorPanel::PullInFrontOfPlants(const FVector& Camera) const
{
	const float Length = FVector::Dist(PreviewFace, Camera);
	if (Length <= MinFaceClearanceCm || PreviewPlants.Num() == 0)
	{
		return Camera;
	}
	const FVector Along = (Camera - PreviewFace) / Length;
	const FBox Line = FBox(PreviewFace, PreviewFace).ExpandBy(1.0f) + Camera;
	TArray<const UInstancedStaticMeshComponent*, TInlineAllocator<16>> OnLine;
	for (const TWeakObjectPtr<const UInstancedStaticMeshComponent>& Plant : PreviewPlants)
	{
		if (Plant.IsValid() && Plant->Bounds.GetBox().Intersect(Line.ExpandBy(20.0f)))
		{
			OnLine.Add(Plant.Get());
		}
	}
	constexpr float Step = 15.0f;
	constexpr float Radius = 8.0f;
	for (float Distance = MinFaceClearanceCm; Distance <= Length; Distance += Step)
	{
		const FVector Point = PreviewFace + Along * Distance;
		for (const UInstancedStaticMeshComponent* Plant : OnLine)
		{
			if (Plant->GetInstancesOverlappingSphere(Point, Radius, /*bSphereInWorldSpace=*/true).Num() > 0)
			{
				const float Stop = FMath::Max(MinFaceClearanceCm, Distance - Step);
				UE_LOG(LogMOFramework, Log, TEXT("[MOCharacterCreator] camera pulled in to %.0f cm from the face (was %.0f): %s (%s) between"), Stop, Length,
					*GetNameSafe(Plant->GetStaticMesh()), *GetNameSafe(Plant->GetOwner()));
				return PreviewFace + Along * Stop;
			}
		}
	}
	return Camera;
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
	if (BaseText)
	{
		const TArray<FMOSurvivorBase>& Bases = UMOSurvivorBaseSettings::GetBases();
		BaseText->SetText(Bases.IsValidIndex(GetBaseIndex()) ? Bases[GetBaseIndex()].DisplayName : FText::GetEmpty());
	}
	for (const FName Driver : GetBuildDrivers())
	{
		if (USlider* Slider = GetBuildSlider(Driver))
		{
			Slider->SetValue(Look.GetBuildValue(Driver));
		}
	}
}

const TArray<FName>& UMOCharacterCreatorPanel::GetBuildDrivers()
{
	static const TArray<FName> Drivers = { TEXT("Bust"), TEXT("Hips"), TEXT("Waist"), TEXT("Thighs") };
	return Drivers;
}

USlider* UMOCharacterCreatorPanel::GetBuildSlider(FName Driver) const
{
	return Driver == TEXT("Bust") ? BustSlider.Get() : Driver == TEXT("Hips") ? HipsSlider.Get() : Driver == TEXT("Waist") ? WaistSlider.Get()
		: Driver == TEXT("Thighs") ? ThighsSlider.Get() : nullptr;
}

int32 UMOCharacterCreatorPanel::GetBaseIndex() const
{
	const int32 Index = UMOSurvivorBaseSettings::GetBases().IndexOfByPredicate([this](const FMOSurvivorBase& Base) { return Base.Id == Look.BaseId; });
	return FMath::Max(Index, 0);
}

void UMOCharacterCreatorPanel::StepBase(int32 Delta)
{
	const TArray<FMOSurvivorBase>& Bases = UMOSurvivorBaseSettings::GetBases();
	if (Bases.Num() > 0)
	{
		SetBase(Bases[(GetBaseIndex() + Delta + Bases.Num()) % Bases.Num()].Id);
	}
}

void UMOCharacterCreatorPanel::SetBase(FName BaseId)
{
	const FMOSurvivorBase* Base = UMOSurvivorBaseSettings::Find(BaseId);
	if (!Base)
	{
		return;
	}
	Base->ApplyToLook(Look);
	ApplyLookToSurvivor();
	// A new game's survivor starts with the body the base was made with (the host owns it: authority).
	if (AMOMetaHumanCharacter* Survivor = GetSurvivor())
	{
		Survivor->ApplyBaseStartingBody();
	}
	RefreshControls();
	PreloadGroomNeighbours(EMOAppearanceCategory::Hair);
	// Back in front of the new face once its mesh has posed (a fraction of a second: the new body's first animation update).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AimAtFaceTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { AimAtFace(/*bResetOrbit=*/true); }), 0.2f, false);
	}
}

void UMOCharacterCreatorPanel::SetBuild(FName Driver, float Value)
{
	Look.SetBuildValue(Driver, FMath::Clamp(Value, -1.0f, 1.0f));
	ApplyLookToSurvivor();
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
		const int32 Exact = Choices.IndexOfByPredicate([&Path](const FMOAppearanceAssetEntry& E) { return E.AssetPath == Path; });
		if (Exact != INDEX_NONE)
		{
			return Exact;
		}
		// A base's own groom (<Character>/Grooms/<Groom>) is not in the library's list: its library namesake's place, so the label names it at
		// once -- not the groom still drawn while it loads (Oct 27: "Updo braids" under Lillian's updo) -- and the arrows step from there.
		const FString Name = FPackageName::ObjectPathToObjectName(Path);
		return Choices.IndexOfByPredicate([&Name](const FMOAppearanceAssetEntry& E) { return FPackageName::ObjectPathToObjectName(E.AssetPath) == Name; });
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
	FMOCharacterAppearance Current = Look;
	const FString& Path = *UMOAppearanceSubsystem::GetGroomPath(Current, GroomSlot);
	if (!Path.IsEmpty())
	{
		return UMOAppearanceSubsystem::MakeGroomDisplayName(FPackageName::ObjectPathToObjectName(Path));   // the look's groom, worn or loading
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
	if (Survivor && !Look.BaseId.IsNone())
	{
		Survivor->ApplyBaseStartingBody();   // the base's body, whatever the minutes in the creator did to the metabolism
	}
	FString Build;
	for (const FName Driver : GetBuildDrivers())
	{
		Build += FString::Printf(TEXT(" %s %+.2f"), *Driver.ToString(), Look.GetBuildValue(Driver));
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacterCreator] Begin: '%s', base '%s',%s, skin %.2f, hair %.2f / %.2f, eyes %s, hair style '%s', beard '%s', mustache '%s'"),
		*Name, *Look.BaseId.ToString(), *Build, Look.SkinTone, Look.HairMelanin, Look.HairRedness, *Look.EyeColor.ToString(),
		*GetGroomLabel(EMOAppearanceCategory::Hair).ToString(), *GetGroomLabel(EMOAppearanceCategory::Beard).ToString(),
		*GetGroomLabel(EMOAppearanceCategory::Mustache).ToString());

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
