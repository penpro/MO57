#include "MOAppearanceSubsystem.h"
#include "MOFramework.h"
#include "MOMetaHumanCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void UMOAppearanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Initialized"));
}

void UMOAppearanceSubsystem::Deinitialize()
{
	AssetCatalog.Empty();
	KnownAssetPaths.Empty();

	Super::Deinitialize();
}

// ============================================================================
// ASSET DISCOVERY
// ============================================================================

void UMOAppearanceSubsystem::DiscoverAppearanceAssets()
{
	UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Beginning asset discovery..."));

	AssetCatalog.Empty();
	KnownAssetPaths.Empty();

	// Initialize empty arrays for each category
	AssetCatalog.Add(EMOAppearanceCategory::Face, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Hair, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Eyebrows, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Eyelashes, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Beard, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Mustache, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Body, TArray<FMOAppearanceAssetEntry>());

	// Scan base content paths
	ScanDirectoryForSkeletalMeshes(FacesContentPath, EMOAppearanceCategory::Face, false);
	ScanDirectoryForSkeletalMeshes(BodiesContentPath, EMOAppearanceCategory::Body, false);

	// Scan hair paths for grooms
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Hair"), EMOAppearanceCategory::Hair, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Eyebrows"), EMOAppearanceCategory::Eyebrows, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Eyelashes"), EMOAppearanceCategory::Eyelashes, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Beards"), EMOAppearanceCategory::Beard, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Mustaches"), EMOAppearanceCategory::Mustache, false);

	// Scan mod paths
	for (const FString& ModPath : AdditionalModPaths)
	{
		ScanDirectoryForSkeletalMeshes(ModPath + TEXT("/Faces"), EMOAppearanceCategory::Face, true);
		ScanDirectoryForSkeletalMeshes(ModPath + TEXT("/Bodies"), EMOAppearanceCategory::Body, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Hair"), EMOAppearanceCategory::Hair, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Eyebrows"), EMOAppearanceCategory::Eyebrows, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Eyelashes"), EMOAppearanceCategory::Eyelashes, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Beards"), EMOAppearanceCategory::Beard, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Mustaches"), EMOAppearanceCategory::Mustache, true);
	}

	bDiscoveryComplete = true;

	// Log discovery results
	for (const auto& Pair : AssetCatalog)
	{
		UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Discovered %d assets for category %d"),
			Pair.Value.Num(), static_cast<int32>(Pair.Key));
	}
}

void UMOAppearanceSubsystem::ScanDirectoryForSkeletalMeshes(const FString& Path, EMOAppearanceCategory Category, bool bIsModded)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*Path), AssetDataList, true);

	TArray<FMOAppearanceAssetEntry>& CategoryAssets = AssetCatalog.FindOrAdd(Category);

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Filter to skeletal meshes only
		if (AssetData.AssetClassPath.GetAssetName() != TEXT("SkeletalMesh"))
		{
			continue;
		}

		FMOAppearanceAssetEntry Entry;
		Entry.AssetPath = AssetData.GetObjectPathString();
		Entry.DisplayName = GetDisplayNameFromPath(AssetData.AssetName.ToString());
		Entry.Tags = InferTagsFromAssetName(AssetData.AssetName.ToString());
		Entry.bIsModded = bIsModded;

		CategoryAssets.Add(Entry);
		KnownAssetPaths.Add(Entry.AssetPath);

		// DEBUG (MOAppearanceSubsystem, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("  Found skeletal mesh: %s"), *Entry.AssetPath);
	}
}

void UMOAppearanceSubsystem::ScanDirectoryForGrooms(const FString& Path, EMOAppearanceCategory Category, bool bIsModded)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*Path), AssetDataList, true);

	TArray<FMOAppearanceAssetEntry>& CategoryAssets = AssetCatalog.FindOrAdd(Category);

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Filter to groom assets only
		if (AssetData.AssetClassPath.GetAssetName() != TEXT("GroomAsset"))
		{
			continue;
		}

		FMOAppearanceAssetEntry Entry;
		Entry.AssetPath = AssetData.GetObjectPathString();
		Entry.DisplayName = GetDisplayNameFromPath(AssetData.AssetName.ToString());
		Entry.Tags = InferTagsFromAssetName(AssetData.AssetName.ToString());
		Entry.bIsModded = bIsModded;

		CategoryAssets.Add(Entry);
		KnownAssetPaths.Add(Entry.AssetPath);

		// DEBUG (MOAppearanceSubsystem, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("  Found groom asset: %s"), *Entry.AssetPath);
	}
}

TArray<FMOAppearanceAssetEntry> UMOAppearanceSubsystem::GetAssetsForCategory(EMOAppearanceCategory Category) const
{
	if (const TArray<FMOAppearanceAssetEntry>* Assets = AssetCatalog.Find(Category))
	{
		return *Assets;
	}
	return TArray<FMOAppearanceAssetEntry>();
}

TArray<FMOAppearanceAssetEntry> UMOAppearanceSubsystem::GetAssetsForCategoryWithTags(EMOAppearanceCategory Category, const TArray<FName>& RequiredTags) const
{
	TArray<FMOAppearanceAssetEntry> Result;

	if (const TArray<FMOAppearanceAssetEntry>* Assets = AssetCatalog.Find(Category))
	{
		for (const FMOAppearanceAssetEntry& Entry : *Assets)
		{
			bool bHasAllTags = true;
			for (const FName& Tag : RequiredTags)
			{
				if (!Entry.Tags.Contains(Tag))
				{
					bHasAllTags = false;
					break;
				}
			}

			if (bHasAllTags)
			{
				Result.Add(Entry);
			}
		}
	}

	return Result;
}

bool UMOAppearanceSubsystem::IsAssetPathValid(const FString& AssetPath) const
{
	return KnownAssetPaths.Contains(AssetPath);
}

FText UMOAppearanceSubsystem::GetDisplayNameFromPath(const FString& AssetPath) const
{
	// Extract filename and convert to display name
	// e.g., "Face_Male_01" -> "Male 01"
	FString Name = FPaths::GetBaseFilename(AssetPath);

	// Remove common prefixes
	Name.RemoveFromStart(TEXT("Face_"));
	Name.RemoveFromStart(TEXT("Body_"));
	Name.RemoveFromStart(TEXT("Hair_"));

	// Replace underscores with spaces
	Name.ReplaceInline(TEXT("_"), TEXT(" "));

	return FText::FromString(Name);
}

TArray<FName> UMOAppearanceSubsystem::InferTagsFromAssetName(const FString& AssetName) const
{
	TArray<FName> Tags;

	FString NameLower = AssetName.ToLower();

	// Gender tags
	if (NameLower.Contains(TEXT("male")) && !NameLower.Contains(TEXT("female")))
	{
		Tags.Add(FName(TEXT("Male")));
	}
	if (NameLower.Contains(TEXT("female")))
	{
		Tags.Add(FName(TEXT("Female")));
	}

	// Age tags
	if (NameLower.Contains(TEXT("young")))
	{
		Tags.Add(FName(TEXT("Young")));
	}
	if (NameLower.Contains(TEXT("old")) || NameLower.Contains(TEXT("elder")))
	{
		Tags.Add(FName(TEXT("Old")));
	}

	// Style tags
	if (NameLower.Contains(TEXT("short")))
	{
		Tags.Add(FName(TEXT("Short")));
	}
	if (NameLower.Contains(TEXT("long")))
	{
		Tags.Add(FName(TEXT("Long")));
	}
	if (NameLower.Contains(TEXT("curly")))
	{
		Tags.Add(FName(TEXT("Curly")));
	}
	if (NameLower.Contains(TEXT("bald")))
	{
		Tags.Add(FName(TEXT("Bald")));
	}

	return Tags;
}

// ============================================================================
// APPEARANCE APPLICATION
// ============================================================================

USkeletalMesh* UMOAppearanceSubsystem::LoadSkeletalMesh(const FString& AssetPath) const
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	return Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *AssetPath));
}

UGroomAsset* UMOAppearanceSubsystem::LoadGroomAsset(const FString& AssetPath) const
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	return Cast<UGroomAsset>(StaticLoadObject(UGroomAsset::StaticClass(), nullptr, *AssetPath));
}

bool UMOAppearanceSubsystem::ApplyAppearance(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance)
{
	if (!Character)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("UMOAppearanceSubsystem::ApplyAppearance: Null character"));
		return false;
	}

	bool bSuccess = true;

	// Apply body mesh
	if (!Appearance.BodyAssetPath.IsEmpty())
	{
		if (USkeletalMesh* BodyMesh = LoadSkeletalMesh(Appearance.BodyAssetPath))
		{
			Character->GetMesh()->SetSkeletalMesh(BodyMesh);
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("UMOAppearanceSubsystem: Failed to load body mesh: %s"), *Appearance.BodyAssetPath);
			bSuccess = false;
		}
	}

	// Apply face mesh
	if (!Appearance.FaceAssetPath.IsEmpty())
	{
		if (USkeletalMesh* FaceMesh = LoadSkeletalMesh(Appearance.FaceAssetPath))
		{
			if (USkeletalMeshComponent* FaceComp = Character->GetFaceMesh())
			{
				FaceComp->SetSkeletalMesh(FaceMesh);
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("UMOAppearanceSubsystem: Failed to load face mesh: %s"), *Appearance.FaceAssetPath);
			bSuccess = false;
		}
	}

	// Note: Groom components should be added and configured in Blueprint subclasses
	// The appearance system stores paths but doesn't apply grooms directly

	// Apply body parameters (Mutable integration point)
	ApplyBodyParameters(Character, Appearance.BodyFat, Appearance.MuscleMass, Appearance.Height);

	// Apply colors
	ApplyColors(Character, Appearance);

	UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Applied appearance to character %s"), *Character->GetName());

	return bSuccess;
}

bool UMOAppearanceSubsystem::ApplyFace(AMOMetaHumanCharacter* Character, const FString& FaceAssetPath)
{
	if (!Character || FaceAssetPath.IsEmpty())
	{
		return false;
	}

	if (USkeletalMesh* FaceMesh = LoadSkeletalMesh(FaceAssetPath))
	{
		if (USkeletalMeshComponent* FaceComp = Character->GetFaceMesh())
		{
			FaceComp->SetSkeletalMesh(FaceMesh);
			return true;
		}
	}

	return false;
}

bool UMOAppearanceSubsystem::ApplyHair(AMOMetaHumanCharacter* Character, const FString& HairAssetPath)
{
	// Groom components should be added and configured in Blueprint subclasses
	// This function is deprecated - manage grooms directly in your character Blueprint
	UE_LOG(LogMOFramework, Warning, TEXT("UMOAppearanceSubsystem::ApplyHair is deprecated. Add groom components in Blueprint."));
	return false;
}

bool UMOAppearanceSubsystem::ApplyBodyParameters(AMOMetaHumanCharacter* Character, float BodyFat, float MuscleMass, float Height)
{
	if (!Character)
	{
		return false;
	}

	// TODO: Integrate with Mutable plugin for actual body morphing
	// For now, we store the values but don't apply visual changes
	// This is where Mutable's CustomizableObject would be updated

	// DEBUG (MOAppearanceSubsystem, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("UMOAppearanceSubsystem: Body params - Fat: %.2f, Muscle: %.2f, Height: %.2f"),
	// 	BodyFat, MuscleMass, Height);

	// Height can be applied via actor scale as a simple fallback
	Character->SetActorScale3D(FVector(1.0f, 1.0f, Height));

	return true;
}

bool UMOAppearanceSubsystem::ApplyColors(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance)
{
	if (!Character)
	{
		return false;
	}
	ApplyColorsToActor(Character, Appearance);
	return true;
}

void UMOAppearanceSubsystem::ComputeSkinParameters(float SkinTone, float& OutValue, float& OutSaturation, FLinearColor& OutMultiply)
{
	// The baked skin sits at DefaultSkinTone. Lighter: up to +10 % value. Darker: melanin both darkens and warms skin, and deeper skin reads more
	// saturated -- value falls to 0.2 of the baked brightness at 1, saturation rises a little, and the multiply leans toward a warm brown.
	const float Tone = FMath::Clamp(SkinTone, 0.0f, 1.0f);
	const float Baked = FMOCharacterAppearance::DefaultSkinTone;
	if (Tone <= Baked)
	{
		const float Lighter = (Baked - Tone) / FMath::Max(Baked, KINDA_SMALL_NUMBER);   // 0..1
		OutValue = 1.0f + 0.1f * Lighter;
		OutSaturation = -0.05f * Lighter;
		OutMultiply = FLinearColor::White;
		return;
	}
	const float Darker = FMath::Pow((Tone - Baked) / (1.0f - Baked), 0.85f);   // 0..1
	OutValue = FMath::Lerp(1.0f, 0.2f, Darker);
	OutSaturation = 0.12f * Darker;
	OutMultiply = FMath::Lerp(FLinearColor::White, FLinearColor(1.0f, 0.88f, 0.78f, 1.0f), Darker);
}

const TArray<FMOEyeColorPreset>& UMOAppearanceSubsystem::GetEyeColorPresets()
{
	// Values for M_eye_eyeball_unified's PROCEDURAL iris (MI_MOEye_Procedural; the shipped MetaHuman eye is baked and ignores them), read off a hue
	// sweep in the game (Oct 15 2026): hue 0.0 grey-violet, 0.15 blue, 0.3 teal, 0.45 olive green, 0.75 amber, 0.9 golden brown; value = how dark.
	static const TArray<FMOEyeColorPreset> Presets = []()
	{
		auto P = [](const TCHAR* Id, FText Name, float H1, float V1, float H2, float V2, float Sat)
		{
			FMOEyeColorPreset E;
			E.Id = Id; E.DisplayName = MoveTemp(Name);
			E.PrimaryHue = H1; E.PrimaryValue = V1; E.SecondaryHue = H2; E.SecondaryValue = V2; E.Saturation = Sat;
			return E;
		};
		return TArray<FMOEyeColorPreset>{
			P(TEXT("Brown"),     NSLOCTEXT("MOAppearance", "EyeBrown", "Brown"),           0.88f, 0.30f, 0.93f, 0.20f, 1.50f),
			P(TEXT("DarkBrown"), NSLOCTEXT("MOAppearance", "EyeDarkBrown", "Dark brown"),  0.90f, 0.15f, 0.95f, 0.10f, 1.20f),
			P(TEXT("Amber"),     NSLOCTEXT("MOAppearance", "EyeAmber", "Amber"),           0.80f, 0.56f, 0.88f, 0.40f, 1.70f),
			P(TEXT("Hazel"),     NSLOCTEXT("MOAppearance", "EyeHazel", "Hazel"),           0.52f, 0.45f, 0.86f, 0.32f, 1.20f),
			P(TEXT("Green"),     NSLOCTEXT("MOAppearance", "EyeGreen", "Green"),           0.42f, 0.50f, 0.38f, 0.40f, 1.20f),
			P(TEXT("Blue"),      NSLOCTEXT("MOAppearance", "EyeBlue", "Blue"),             0.15f, 0.62f, 0.12f, 0.50f, 1.10f),
			P(TEXT("Grey"),      NSLOCTEXT("MOAppearance", "EyeGrey", "Grey"),             0.10f, 0.60f, 0.10f, 0.50f, 0.25f),
		};
	}();
	return Presets;
}

void UMOAppearanceSubsystem::RandomizeColors(FMOCharacterAppearance& Appearance)
{
	Appearance.SkinTone = FMath::FRand();
	Appearance.HairMelanin = FMath::FRandRange(0.15f, 1.0f);
	Appearance.HairRedness = FMath::FRandRange(0.0f, 0.6f);
	Appearance.EyeColor = GetEyeColorPresets()[FMath::RandRange(0, GetEyeColorPresets().Num() - 1)].Id;
}

const FMOEyeColorPreset* UMOAppearanceSubsystem::FindEyeColorPreset(FName Id)
{
	return Id.IsNone() ? nullptr : GetEyeColorPresets().FindByPredicate([Id](const FMOEyeColorPreset& E) { return E.Id == Id; });
}

void UMOAppearanceSubsystem::ApplyColorsToActor(AActor* Actor, const FMOCharacterAppearance& Appearance)
{
	if (!Actor || Actor->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	static const FName SkinValueName(TEXT("Basecolor Global Value Post-Bake"));
	static const FName SkinSaturationName(TEXT("Basecolor Global Saturation Post-Bake"));
	static const FName SkinMultiplyName(TEXT("Basecolor Global Multiply Post-Bake"));
	static const FName HairMelaninName(TEXT("hairMelanin"));
	static const FName HairRednessName(TEXT("hairRedness"));
	static const FName IrisHue1Name(TEXT("Iris Primary Color Hue"));
	static const FName IrisValue1Name(TEXT("Iris Primary Color Value"));
	static const FName IrisHue2Name(TEXT("Iris Secondary Color Hue"));
	static const FName IrisValue2Name(TEXT("Iris Secondary Color Value"));
	static const FName IrisSaturationName(TEXT("Iris Global Saturation"));

	// The procedural eye (MI_MOEye_Procedural: the MetaHuman eye with "Use Baked Material" off). /MOFramework is always cooked.
	static const FSoftObjectPath ProceduralEyePath(TEXT("/MOFramework/Characters/Eyes/MI_MOEye_Procedural.MI_MOEye_Procedural"));
	UMaterialInterface* ProceduralEye = Appearance.EyeColor.IsNone() ? nullptr : Cast<UMaterialInterface>(ProceduralEyePath.TryLoad());

	float SkinValue = 1.0f, SkinSaturation = 0.0f;
	FLinearColor SkinMultiply = FLinearColor::White;
	ComputeSkinParameters(Appearance.SkinTone, SkinValue, SkinSaturation, SkinMultiply);
	const FMOEyeColorPreset* Eye = FindEyeColorPreset(Appearance.EyeColor);

	// The dynamic instance of a slot whose material has Parameter (reusing one the MetaHuman component or ApplySkinLook already made).
	auto DynamicFor = [](UMeshComponent* Mesh, int32 Slot, const FName& Parameter) -> UMaterialInstanceDynamic*
	{
		UMaterialInterface* Material = Mesh->GetMaterial(Slot);
		float Probe = 0.0f;
		if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(Parameter), Probe))
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
		return Dynamic ? Dynamic : Mesh->CreateAndSetMaterialInstanceDynamic(Slot);
	};

	TInlineComponentArray<UMeshComponent*> Meshes(Actor);
	for (UMeshComponent* Mesh : Meshes)
	{
		// Eyelashes keep their own (near-black) melanin: blond lashes read as no lashes.
		const bool bIsHairGroom = Mesh->IsA<UGroomComponent>() && !Mesh->GetName().Contains(TEXT("Eyelash"));
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Skin = DynamicFor(Mesh, Slot, SkinValueName))
			{
				Skin->SetScalarParameterValue(SkinValueName, SkinValue);
				Skin->SetScalarParameterValue(SkinSaturationName, SkinSaturation);
				Skin->SetVectorParameterValue(SkinMultiplyName, SkinMultiply);
			}
			else if (bIsHairGroom)
			{
				if (UMaterialInstanceDynamic* Hair = DynamicFor(Mesh, Slot, HairMelaninName))
				{
					Hair->SetScalarParameterValue(HairMelaninName, Appearance.HairMelanin);
					Hair->SetScalarParameterValue(HairRednessName, Appearance.HairRedness);
				}
			}
			else if (Eye && ProceduralEye)
			{
				// The shipped eye is BAKED (static switch "Use Baked Material": the iris colour is a texture, the iris parameters are compiled out).
				// A chosen colour puts the procedural variant of the same eye on the slot; no choice (None) leaves the baked eye alone.
				UMaterialInterface* Current = Mesh->GetMaterial(Slot);
				float Probe = 0.0f;
				if (!Current || !Current->GetScalarParameterValue(FHashedMaterialParameterInfo(IrisHue1Name), Probe))
				{
					continue;
				}
				bool bProcedural = false;
				for (const UMaterialInstance* Chain = Cast<UMaterialInstance>(Current); Chain && !bProcedural; Chain = Cast<UMaterialInstance>(Chain->Parent))
				{
					bProcedural = Chain == ProceduralEye;
				}
				UMaterialInstanceDynamic* Iris = bProcedural ? Cast<UMaterialInstanceDynamic>(Current) : nullptr;
				if (!Iris)
				{
					Iris = UMaterialInstanceDynamic::Create(ProceduralEye, Mesh);
					Mesh->SetMaterial(Slot, Iris);
				}
				if (Iris)
				{
					Iris->SetScalarParameterValue(IrisHue1Name, Eye->PrimaryHue);
					Iris->SetScalarParameterValue(IrisValue1Name, Eye->PrimaryValue);
					Iris->SetScalarParameterValue(IrisHue2Name, Eye->SecondaryHue);
					Iris->SetScalarParameterValue(IrisValue2Name, Eye->SecondaryValue);
					Iris->SetScalarParameterValue(IrisSaturationName, Eye->Saturation);
				}
			}
		}
	}
	// DEBUG (MOAppearance, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOAppearance] %s: skin %.2f (value %.2f), hair melanin %.2f / red %.2f, eyes %s"),
	// 	*Actor->GetName(), Appearance.SkinTone, SkinValue, Appearance.HairMelanin, Appearance.HairRedness, *Appearance.EyeColor.ToString());
}

// ============================================================================
// RANDOM GENERATION
// ============================================================================

FMOCharacterAppearance UMOAppearanceSubsystem::GenerateRandomAppearance(bool bMale)
{
	FMOCharacterAppearance Appearance = bMale ? FMOCharacterAppearance::GetDefaultMale() : FMOCharacterAppearance::GetDefaultFemale();

	TArray<FName> GenderTags;
	GenderTags.Add(bMale ? FName(TEXT("Male")) : FName(TEXT("Female")));

	// Pick random face
	TArray<FMOAppearanceAssetEntry> Faces = GetAssetsForCategoryWithTags(EMOAppearanceCategory::Face, GenderTags);
	if (Faces.Num() > 0)
	{
		Appearance.FaceAssetPath = Faces[FMath::RandRange(0, Faces.Num() - 1)].AssetPath;
	}

	// Pick random body
	TArray<FMOAppearanceAssetEntry> Bodies = GetAssetsForCategoryWithTags(EMOAppearanceCategory::Body, GenderTags);
	if (Bodies.Num() > 0)
	{
		Appearance.BodyAssetPath = Bodies[FMath::RandRange(0, Bodies.Num() - 1)].AssetPath;
	}

	// Pick random hair (don't filter by gender for hair - many styles are unisex)
	TArray<FMOAppearanceAssetEntry> Hairs = GetAssetsForCategory(EMOAppearanceCategory::Hair);
	if (Hairs.Num() > 0)
	{
		Appearance.HairAssetPath = Hairs[FMath::RandRange(0, Hairs.Num() - 1)].AssetPath;
	}

	// Randomize body parameters
	Appearance.BodyFat = FMath::FRandRange(0.1f, 0.6f);
	Appearance.MuscleMass = FMath::FRandRange(0.2f, 0.7f);
	Appearance.Height = FMath::FRandRange(0.9f, 1.1f);

	RandomizeColors(Appearance);

	return Appearance;
}
