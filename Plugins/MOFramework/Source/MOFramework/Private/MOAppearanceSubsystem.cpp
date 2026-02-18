#include "MOAppearanceSubsystem.h"
#include "MOFramework.h"
#include "MOMetaHumanCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"

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

		UE_LOG(LogMOFramework, Verbose, TEXT("  Found skeletal mesh: %s"), *Entry.AssetPath);
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

		UE_LOG(LogMOFramework, Verbose, TEXT("  Found groom asset: %s"), *Entry.AssetPath);
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
	ApplyColors(Character, Appearance.SkinTone, Appearance.HairColor, Appearance.EyeColor);

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

	UE_LOG(LogMOFramework, Verbose, TEXT("UMOAppearanceSubsystem: Body params - Fat: %.2f, Muscle: %.2f, Height: %.2f"),
		BodyFat, MuscleMass, Height);

	// Height can be applied via actor scale as a simple fallback
	Character->SetActorScale3D(FVector(1.0f, 1.0f, Height));

	return true;
}

bool UMOAppearanceSubsystem::ApplyColors(AMOMetaHumanCharacter* Character, const FLinearColor& SkinTone, const FLinearColor& HairColor, const FLinearColor& EyeColor)
{
	if (!Character)
	{
		return false;
	}

	// TODO: Apply material parameter overrides for skin, hair, and eye colors
	// This requires setting up material instances with appropriate parameters

	UE_LOG(LogMOFramework, Verbose, TEXT("UMOAppearanceSubsystem: Colors - Skin: %s, Hair: %s, Eyes: %s"),
		*SkinTone.ToString(), *HairColor.ToString(), *EyeColor.ToString());

	return true;
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

	// Randomize colors slightly from defaults
	auto RandomizeColor = [](FLinearColor Base, float Variance) -> FLinearColor
	{
		return FLinearColor(
			FMath::Clamp(Base.R + FMath::FRandRange(-Variance, Variance), 0.0f, 1.0f),
			FMath::Clamp(Base.G + FMath::FRandRange(-Variance, Variance), 0.0f, 1.0f),
			FMath::Clamp(Base.B + FMath::FRandRange(-Variance, Variance), 0.0f, 1.0f),
			1.0f
		);
	};

	Appearance.SkinTone = RandomizeColor(Appearance.SkinTone, 0.15f);
	Appearance.HairColor = RandomizeColor(Appearance.HairColor, 0.1f);
	Appearance.EyeColor = RandomizeColor(Appearance.EyeColor, 0.2f);

	return Appearance;
}
