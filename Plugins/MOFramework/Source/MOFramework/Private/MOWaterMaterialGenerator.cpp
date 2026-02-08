#include "MOWaterMaterialGenerator.h"
#include "MOFramework.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

UMaterialInstanceDynamic* UMOWaterMaterialGenerator::CreateWaterMaterialInstance(UMaterialInterface* BaseMaterial, UObject* Outer)
{
	if (!BaseMaterial)
	{
		return nullptr;
	}

	return UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
}

#if WITH_EDITOR

UMaterial* UMOWaterMaterialGenerator::CreateSimpleWaterMaterial(const FString& PackagePath)
{
	// Create the package
	FString PackageName = PackagePath;
	FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOWaterMaterial] Failed to create package: %s"), *PackageName);
		return nullptr;
	}

	// Create the material
	UMaterial* Material = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!Material)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOWaterMaterial] Failed to create material"));
		return nullptr;
	}

	// Configure material properties
	Material->BlendMode = BLEND_Translucent;
	Material->TwoSided = false;
	Material->SetShadingModel(MSM_DefaultLit);
	Material->TranslucencyLightingMode = TLM_Surface;

	// ============================================================================
	// CREATE NODES
	// ============================================================================

	int32 NodeX = -800;
	int32 NodeY = 0;
	const int32 NodeSpacingX = 250;
	const int32 NodeSpacingY = 150;

	// --- BASE COLOR ---
	// Deep water color parameter
	UMaterialExpressionVectorParameter* DeepColorParam = NewObject<UMaterialExpressionVectorParameter>(Material);
	DeepColorParam->ParameterName = TEXT("DeepWaterColor");
	DeepColorParam->DefaultValue = FLinearColor(0.0f, 0.05f, 0.15f, 1.0f);
	DeepColorParam->MaterialExpressionEditorX = NodeX;
	DeepColorParam->MaterialExpressionEditorY = NodeY;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(DeepColorParam);

	// Shallow water color parameter
	UMaterialExpressionVectorParameter* ShallowColorParam = NewObject<UMaterialExpressionVectorParameter>(Material);
	ShallowColorParam->ParameterName = TEXT("ShallowWaterColor");
	ShallowColorParam->DefaultValue = FLinearColor(0.0f, 0.3f, 0.4f, 1.0f);
	ShallowColorParam->MaterialExpressionEditorX = NodeX;
	ShallowColorParam->MaterialExpressionEditorY = NodeY + NodeSpacingY;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(ShallowColorParam);

	// Depth fade for color lerp
	UMaterialExpressionDepthFade* DepthFade = NewObject<UMaterialExpressionDepthFade>(Material);
	DepthFade->FadeDistanceDefault = 200.0f;
	DepthFade->MaterialExpressionEditorX = NodeX + NodeSpacingX;
	DepthFade->MaterialExpressionEditorY = NodeY + NodeSpacingY / 2;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(DepthFade);

	// Lerp between deep and shallow based on depth
	UMaterialExpressionLinearInterpolate* ColorLerp = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	ColorLerp->A.Connect(0, DeepColorParam);
	ColorLerp->B.Connect(0, ShallowColorParam);
	ColorLerp->Alpha.Connect(0, DepthFade);
	ColorLerp->MaterialExpressionEditorX = NodeX + NodeSpacingX * 2;
	ColorLerp->MaterialExpressionEditorY = NodeY + NodeSpacingY / 2;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(ColorLerp);

	// --- OPACITY ---
	UMaterialExpressionScalarParameter* OpacityParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	OpacityParam->ParameterName = TEXT("Opacity");
	OpacityParam->DefaultValue = 0.8f;
	OpacityParam->MaterialExpressionEditorX = NodeX;
	OpacityParam->MaterialExpressionEditorY = NodeY + NodeSpacingY * 3;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(OpacityParam);

	// Depth-based opacity
	UMaterialExpressionDepthFade* OpacityDepthFade = NewObject<UMaterialExpressionDepthFade>(Material);
	OpacityDepthFade->FadeDistanceDefault = 50.0f;
	OpacityDepthFade->MaterialExpressionEditorX = NodeX + NodeSpacingX;
	OpacityDepthFade->MaterialExpressionEditorY = NodeY + NodeSpacingY * 3;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(OpacityDepthFade);

	// Multiply opacity by depth fade for soft edges
	UMaterialExpressionMultiply* OpacityMult = NewObject<UMaterialExpressionMultiply>(Material);
	OpacityMult->A.Connect(0, OpacityParam);
	OpacityMult->B.Connect(0, OpacityDepthFade);
	OpacityMult->MaterialExpressionEditorX = NodeX + NodeSpacingX * 2;
	OpacityMult->MaterialExpressionEditorY = NodeY + NodeSpacingY * 3;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(OpacityMult);

	// --- FRESNEL FOR REFLECTIONS ---
	UMaterialExpressionFresnel* Fresnel = NewObject<UMaterialExpressionFresnel>(Material);
	Fresnel->Exponent = 5.0f;
	Fresnel->MaterialExpressionEditorX = NodeX;
	Fresnel->MaterialExpressionEditorY = NodeY + NodeSpacingY * 5;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(Fresnel);

	// Base reflect fraction constant
	UMaterialExpressionConstant* FresnelBase = NewObject<UMaterialExpressionConstant>(Material);
	FresnelBase->R = 0.04f;
	FresnelBase->MaterialExpressionEditorX = NodeX - NodeSpacingX;
	FresnelBase->MaterialExpressionEditorY = NodeY + NodeSpacingY * 5;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(FresnelBase);
	Fresnel->BaseReflectFractionIn.Connect(0, FresnelBase);

	// --- SPECULAR ---
	UMaterialExpressionScalarParameter* SpecularParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	SpecularParam->ParameterName = TEXT("Specular");
	SpecularParam->DefaultValue = 0.5f;
	SpecularParam->MaterialExpressionEditorX = NodeX;
	SpecularParam->MaterialExpressionEditorY = NodeY + NodeSpacingY * 6;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(SpecularParam);

	// --- ROUGHNESS ---
	UMaterialExpressionScalarParameter* RoughnessParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	RoughnessParam->ParameterName = TEXT("Roughness");
	RoughnessParam->DefaultValue = 0.1f;
	RoughnessParam->MaterialExpressionEditorX = NodeX;
	RoughnessParam->MaterialExpressionEditorY = NodeY + NodeSpacingY * 7;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(RoughnessParam);

	// ============================================================================
	// WORLD POSITION OFFSET - GERSTNER WAVES (Custom HLSL)
	// ============================================================================

	// Use built-in Time node for smooth GPU-driven animation
	UMaterialExpressionTime* TimeNode = NewObject<UMaterialExpressionTime>(Material);
	TimeNode->MaterialExpressionEditorX = NodeX;
	TimeNode->MaterialExpressionEditorY = NodeY - NodeSpacingY * 3;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(TimeNode);

	// World Position
	UMaterialExpressionWorldPosition* WorldPos = NewObject<UMaterialExpressionWorldPosition>(Material);
	WorldPos->WorldPositionShaderOffset = WPT_Default;
	WorldPos->MaterialExpressionEditorX = NodeX;
	WorldPos->MaterialExpressionEditorY = NodeY - NodeSpacingY * 2;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(WorldPos);

	// Wave parameters as individual scalars (vectors come through as float3 in custom HLSL)
	UMaterialExpressionScalarParameter* WaveDirXParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	WaveDirXParam->ParameterName = TEXT("WaveDirectionX");
	WaveDirXParam->DefaultValue = 1.0f;
	WaveDirXParam->MaterialExpressionEditorX = NodeX - NodeSpacingX * 2;
	WaveDirXParam->MaterialExpressionEditorY = NodeY - NodeSpacingY * 5;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(WaveDirXParam);

	UMaterialExpressionScalarParameter* WaveDirYParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	WaveDirYParam->ParameterName = TEXT("WaveDirectionY");
	WaveDirYParam->DefaultValue = 0.3f;
	WaveDirYParam->MaterialExpressionEditorX = NodeX - NodeSpacingX * 2;
	WaveDirYParam->MaterialExpressionEditorY = NodeY - NodeSpacingY * 4;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(WaveDirYParam);

	UMaterialExpressionScalarParameter* WaveAmplitudeParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	WaveAmplitudeParam->ParameterName = TEXT("WaveAmplitude");
	WaveAmplitudeParam->DefaultValue = 80.0f;
	WaveAmplitudeParam->MaterialExpressionEditorX = NodeX - NodeSpacingX * 2;
	WaveAmplitudeParam->MaterialExpressionEditorY = NodeY - NodeSpacingY * 3;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(WaveAmplitudeParam);

	UMaterialExpressionScalarParameter* WavelengthParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	WavelengthParam->ParameterName = TEXT("Wavelength");
	WavelengthParam->DefaultValue = 3000.0f;
	WavelengthParam->MaterialExpressionEditorX = NodeX - NodeSpacingX * 2;
	WavelengthParam->MaterialExpressionEditorY = NodeY - NodeSpacingY * 2;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(WavelengthParam);

	UMaterialExpressionScalarParameter* WaveSteepnessParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	WaveSteepnessParam->ParameterName = TEXT("WaveSteepness");
	WaveSteepnessParam->DefaultValue = 0.4f;
	WaveSteepnessParam->MaterialExpressionEditorX = NodeX - NodeSpacingX;
	WaveSteepnessParam->MaterialExpressionEditorY = NodeY - NodeSpacingY * 5;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(WaveSteepnessParam);

	UMaterialExpressionScalarParameter* WaveSpeedParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	WaveSpeedParam->ParameterName = TEXT("WaveSpeed");
	WaveSpeedParam->DefaultValue = 1.0f;
	WaveSpeedParam->MaterialExpressionEditorX = NodeX - NodeSpacingX;
	WaveSpeedParam->MaterialExpressionEditorY = NodeY - NodeSpacingY * 4;
	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(WaveSpeedParam);

	// Custom HLSL for Gerstner wave calculation with multiple waves
	UMaterialExpressionCustom* GerstnerCustom = NewObject<UMaterialExpressionCustom>(Material);
	GerstnerCustom->Code = TEXT(R"(
// Multi-wave Gerstner calculation for natural water appearance
float3 TotalDisp = float3(0, 0, 0);

// Wave 1 - Main wave (parameterized)
{
    float2 Dir = normalize(float2(DirX, DirY));
    float Freq = 6.28318 / Wavelength;
    float Ph = dot(Dir, WorldPos.xy) * Freq + Time * Speed * Freq;
    TotalDisp.xy += Dir * Steepness * Amplitude * cos(Ph);
    TotalDisp.z += Amplitude * sin(Ph);
}

// Wave 2 - Secondary wave (hardcoded for detail)
{
    float2 Dir = normalize(float2(0.7, 0.7));
    float Amp = Amplitude * 0.5;
    float Freq = 6.28318 / (Wavelength * 0.6);
    float Ph = dot(Dir, WorldPos.xy) * Freq + Time * Speed * 1.2 * Freq + 1.5;
    TotalDisp.xy += Dir * Steepness * 0.8 * Amp * cos(Ph);
    TotalDisp.z += Amp * sin(Ph);
}

// Wave 3 - Cross wave (adds variety)
{
    float2 Dir = normalize(float2(-0.3, 0.95));
    float Amp = Amplitude * 0.35;
    float Freq = 6.28318 / (Wavelength * 0.4);
    float Ph = dot(Dir, WorldPos.xy) * Freq + Time * Speed * 0.9 * Freq + 3.0;
    TotalDisp.xy += Dir * Steepness * 0.6 * Amp * cos(Ph);
    TotalDisp.z += Amp * sin(Ph);
}

// Wave 4 - Small detail ripples
{
    float2 Dir = normalize(float2(0.5, -0.5));
    float Amp = Amplitude * 0.2;
    float Freq = 6.28318 / (Wavelength * 0.25);
    float Ph = dot(Dir, WorldPos.xy) * Freq + Time * Speed * 1.5 * Freq + 0.7;
    TotalDisp.xy += Dir * Steepness * 0.5 * Amp * cos(Ph);
    TotalDisp.z += Amp * sin(Ph);
}

return TotalDisp;
)");
	GerstnerCustom->OutputType = CMOT_Float3;
	GerstnerCustom->Description = TEXT("Gerstner Wave WPO");
	GerstnerCustom->MaterialExpressionEditorX = NodeX + NodeSpacingX * 2;
	GerstnerCustom->MaterialExpressionEditorY = NodeY - NodeSpacingY * 3;

	// Add inputs to custom node
	FCustomInput& WorldPosInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	WorldPosInput.InputName = TEXT("WorldPos");
	WorldPosInput.Input.Connect(0, WorldPos);

	FCustomInput& TimeInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	TimeInput.InputName = TEXT("Time");
	TimeInput.Input.Connect(0, TimeNode);

	FCustomInput& DirXInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	DirXInput.InputName = TEXT("DirX");
	DirXInput.Input.Connect(0, WaveDirXParam);

	FCustomInput& DirYInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	DirYInput.InputName = TEXT("DirY");
	DirYInput.Input.Connect(0, WaveDirYParam);

	FCustomInput& AmplitudeInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	AmplitudeInput.InputName = TEXT("Amplitude");
	AmplitudeInput.Input.Connect(0, WaveAmplitudeParam);

	FCustomInput& WavelengthInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	WavelengthInput.InputName = TEXT("Wavelength");
	WavelengthInput.Input.Connect(0, WavelengthParam);

	FCustomInput& SteepnessInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	SteepnessInput.InputName = TEXT("Steepness");
	SteepnessInput.Input.Connect(0, WaveSteepnessParam);

	FCustomInput& SpeedInput = GerstnerCustom->Inputs.AddDefaulted_GetRef();
	SpeedInput.InputName = TEXT("Speed");
	SpeedInput.Input.Connect(0, WaveSpeedParam);

	Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(GerstnerCustom);

	// ============================================================================
	// CONNECT TO MATERIAL OUTPUTS
	// ============================================================================

	Material->GetEditorOnlyData()->BaseColor.Connect(0, ColorLerp);
	Material->GetEditorOnlyData()->Specular.Connect(0, SpecularParam);
	Material->GetEditorOnlyData()->Roughness.Connect(0, RoughnessParam);
	Material->GetEditorOnlyData()->Opacity.Connect(0, OpacityMult);
	Material->GetEditorOnlyData()->WorldPositionOffset.Connect(0, GerstnerCustom);

	// ============================================================================
	// FINALIZE
	// ============================================================================

	// Let the material update
	Material->PreEditChange(nullptr);
	Material->PostEditChange();

	// Mark package dirty and save
	Package->MarkPackageDirty();

	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, Material, *PackageFileName, SaveArgs);

	// Register with asset registry
	FAssetRegistryModule::AssetCreated(Material);

	UE_LOG(LogMOFramework, Log, TEXT("[MOWaterMaterial] Created simple water material: %s"), *PackagePath);

	return Material;
}

UMaterial* UMOWaterMaterialGenerator::CreateWaterMaterial(const FString& PackagePath)
{
	// For now, use the simple version
	// A full water material would include:
	// - Normal map panning for ripples
	// - Subsurface scattering
	// - Caustics
	// - Foam at wave peaks
	// - Refraction
	return CreateSimpleWaterMaterial(PackagePath);
}

#endif // WITH_EDITOR
