#include "MOItemPreviewStage.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"

namespace MOItemPreview
{
	// Live tuning (PIE): the look of the booth. Lux = illuminance the key light puts on the item; Exposure = manual EV bias.
	// Manual exposure keeps the physical-camera default (f/4, 1/60 s, ISO 100: about EV 10), where mid grey wants ~3600 lux.
	TAutoConsoleVariable<float> CVarLux(TEXT("mo.ItemPreview.Lux"), 3000.0f, TEXT("Item preview: key-light illuminance on the item (fill and rim are fractions of it)."), ECVF_Default);
	TAutoConsoleVariable<float> CVarExposure(TEXT("mo.ItemPreview.Exposure"), 0.0f, TEXT("Item preview: manual exposure bias (EV) of the capture."), ECVF_Default);

	constexpr float FovDeg = 28.0f;

	/** Stage-space light directions (camera looks along +X from -X): key upper-left-front, fill right-front low, rim behind-above. */
	const FVector KeyDir(-0.62f, -0.48f, 0.62f);
	const FVector FillDir(-0.55f, 0.80f, 0.20f);
	const FVector RimDir(0.75f, 0.15f, 0.65f);
	constexpr float FillFraction = 0.35f;
	constexpr float RimFraction = 0.8f;

	FLightingChannels StageChannel()
	{
		FLightingChannels Channels;
		Channels.bChannel0 = false;   // the world's lights (sun, campfires) stay off the preview mesh
		Channels.bChannel1 = true;
		Channels.bChannel2 = false;
		return Channels;
	}

	UPointLightComponent* MakeLight(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UPointLightComponent* Light = Owner->CreateDefaultSubobject<UPointLightComponent>(Name);
		Light->SetupAttachment(Parent);
		Light->SetCastShadows(false);
		Light->LightingChannels = StageChannel();
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->bAffectsWorld = true;
		return Light;
	}
}

AMOItemPreviewStage::AMOItemPreviewStage()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(Pivot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->CastShadow = false;
	PreviewMesh->LightingChannels = MOItemPreview::StageChannel();
	PreviewMesh->SetVisibleInRayTracing(false);
	PreviewMesh->bAffectDistanceFieldLighting = false;

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->ProjectionType = ECameraProjectionMode::Perspective;
	Capture->FOVAngle = MOItemPreview::FovDeg;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;   // temporal AA / denoisers keep their history between turntable frames
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetCloud(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetBloom(false);
	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_DynamicGlobalIlluminationMethod = true;
	PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PP.bOverride_ReflectionMethod = true;
	PP.ReflectionMethod = EReflectionMethod::None;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.0f;
	Capture->PostProcessBlendWeight = 1.0f;

	KeyLight = MOItemPreview::MakeLight(this, Root, TEXT("KeyLight"));
	FillLight = MOItemPreview::MakeLight(this, Root, TEXT("FillLight"));
	RimLight = MOItemPreview::MakeLight(this, Root, TEXT("RimLight"));
}

FVector AMOItemPreviewStage::StageLocation(int32 Slot)
{
	// 20 km above the origin, 1 km apart per stage: far outside anything the world builds or the player can reach.
	return FVector(Slot * 100000.0, 0.0, 2000000.0);
}

bool AMOItemPreviewStage::HasItem() const
{
	return PreviewMesh && PreviewMesh->GetStaticMesh() != nullptr;
}

void AMOItemPreviewStage::EnsureRenderTarget(int32 Resolution)
{
	Resolution = FMath::Clamp(Resolution, 64, 2048);
	if (RenderTarget && RenderTarget->SizeX == Resolution)
	{
		return;
	}
	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("ItemPreviewRT"));
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->InitAutoFormat(Resolution, Resolution);
	RenderTarget->UpdateResourceImmediate(true);
	Capture->TextureTarget = RenderTarget;
}

bool AMOItemPreviewStage::SetItem(FName ItemId)
{
	FMOItemDefinitionRow Def;
	if (ItemId.IsNone() || !UMOItemDatabaseSettings::GetItemDefinition(ItemId, Def))
	{
		ClearItem();
		return false;
	}
	UStaticMesh* Mesh = Def.WorldVisual.StaticMesh.LoadSynchronous();
	if (!Mesh)
	{
		ClearItem();
		return false;
	}
	PreviewMesh->SetStaticMesh(Mesh);
	PreviewMesh->EmptyOverrideMaterials();
	if (UMaterialInterface* Override = Def.WorldVisual.MaterialOverride.LoadSynchronous())
	{
		PreviewMesh->SetMaterial(0, Override);   // the dropped item applies the override to slot 0 (AMOWorldItem)
	}

	// Present by shape. A dropped item's mesh lies on the ground (its thinnest axis up), so a tool seen from a little above is a sliver: a POLE (longest axis at least
	// twice the next) is stood on end and turns about its length, the way a museum stand shows a spear or an axe; a FLAT item (a hide, a board, a coil) is looked at
	// from well above; anything else keeps its resting pose.
	const FVector Scale = Def.WorldVisual.RelativeTransform.GetScale3D();
	const FBoxSphereBounds Local = Mesh->GetBounds();
	const FVector Ext = Local.BoxExtent * Scale.GetAbs();
	const int32 Longest = Ext.X >= Ext.Y && Ext.X >= Ext.Z ? 0 : (Ext.Y >= Ext.Z ? 1 : 2);
	TArray<double> Sorted = { Ext.X, Ext.Y, Ext.Z };
	Sorted.Sort();
	FRotator Pose = FRotator::ZeroRotator;
	SuggestedPitchDeg = -25.0f;
	if (Sorted[2] >= 2.0 * Sorted[1])
	{
		Pose = Longest == 0 ? FRotator(90.0f, 0.0f, 0.0f) : Longest == 1 ? FRotator(0.0f, 0.0f, 90.0f) : FRotator::ZeroRotator;
		SuggestedPitchDeg = -12.0f;
	}
	else if (Sorted[0] <= 0.35 * Sorted[1])
	{
		SuggestedPitchDeg = -45.0f;
	}
	// Turn about the item's own centre: the mesh sits inside the pivot offset by its scaled (and posed) bounds origin.
	PreviewMesh->SetRelativeScale3D(Scale);
	PreviewMesh->SetRelativeRotation(Pose);
	PreviewMesh->SetRelativeLocation(-Pose.RotateVector(Local.Origin * Scale));
	FramedRadius = FMath::Max(Ext.Size(), 0.5);

	Capture->ClearShowOnlyComponents();
	Capture->ShowOnlyComponent(PreviewMesh);
	FrameCamera();
	return true;
}

void AMOItemPreviewStage::ClearItem()
{
	if (PreviewMesh)
	{
		PreviewMesh->SetStaticMesh(nullptr);
	}
}

void AMOItemPreviewStage::SetView(float YawDeg, float InPitchDeg)
{
	Pivot->SetRelativeRotation(FRotator(0.0f, YawDeg, 0.0f));
	const float Clamped = FMath::Clamp(InPitchDeg, -60.0f, 30.0f);
	if (!FMath::IsNearlyEqual(Clamped, PitchDeg))
	{
		PitchDeg = Clamped;
		FrameCamera();
	}
}

void AMOItemPreviewStage::FrameCamera()
{
	// Fit the item's bounding sphere in the frame (5 % margin); the camera looks at the pivot from the front, PitchDeg above it.
	const float Distance = FramedRadius / FMath::Sin(FMath::DegreesToRadians(MOItemPreview::FovDeg * 0.5f)) * 1.05f;
	const FRotator View(PitchDeg, 0.0f, 0.0f);
	Capture->SetRelativeLocation(-View.Vector() * Distance);
	Capture->SetRelativeRotation(View);

	// Lights at a fixed angle to the camera, intensity scaled by distance squared so the illuminance on the item is the same
	// for a berry and a spear.
	const float Lux = FMath::Max(MOItemPreview::CVarLux.GetValueOnGameThread(), 0.0f);
	auto Place = [this, Distance, Lux](UPointLightComponent* Light, const FVector& Dir, float Fraction)
	{
		const float D = Distance * 1.2f;
		Light->SetRelativeLocation(Dir.GetSafeNormal() * D);
		Light->SetAttenuationRadius(D * 3.0f);
		Light->SetIntensity(Lux * Fraction * D * D / 10000.0f);   // candela = lux * m^2 (D is in cm)
	};
	Place(KeyLight, MOItemPreview::KeyDir, 1.0f);
	Place(FillLight, MOItemPreview::FillDir, MOItemPreview::FillFraction);
	Place(RimLight, MOItemPreview::RimDir, MOItemPreview::RimFraction);
	Capture->PostProcessSettings.AutoExposureBias = MOItemPreview::CVarExposure.GetValueOnGameThread();
}

void AMOItemPreviewStage::CaptureNow()
{
	if (Capture && RenderTarget && HasItem())
	{
		Capture->PostProcessSettings.AutoExposureBias = MOItemPreview::CVarExposure.GetValueOnGameThread();
		Capture->CaptureScene();
	}
}
