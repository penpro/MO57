#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOCharacterAppearance.h"
#include "MOAppearanceSubsystem.generated.h"

class AMOMetaHumanCharacter;
class USkeletalMesh;
class UGroomAsset;

/**
 * Subsystem for managing character appearance assets.
 * Discovers available faces, hair, bodies from content folders (including mods).
 * Handles applying appearances to MetaHuman characters.
 */
UCLASS()
class MOFRAMEWORK_API UMOAppearanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============================================================================
	// ASSET DISCOVERY
	// ============================================================================

	/**
	 * Scan content folders for available appearance assets.
	 * Call this at game start or when mods are loaded.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void DiscoverAppearanceAssets();

	/**
	 * Get all discovered assets for a category.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	TArray<FMOAppearanceAssetEntry> GetAssetsForCategory(EMOAppearanceCategory Category) const;

	/**
	 * Get assets filtered by tags (e.g., "Male", "Female").
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	TArray<FMOAppearanceAssetEntry> GetAssetsForCategoryWithTags(EMOAppearanceCategory Category, const TArray<FName>& RequiredTags) const;

	/**
	 * Check if a specific asset path exists in the catalog.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Appearance")
	bool IsAssetPathValid(const FString& AssetPath) const;

	// ============================================================================
	// APPEARANCE APPLICATION
	// ============================================================================

	/**
	 * Apply a full appearance to a MetaHuman character.
	 * Loads and sets all meshes, grooms, and parameters.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyAppearance(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance);

	/**
	 * Apply only the face mesh.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyFace(AMOMetaHumanCharacter* Character, const FString& FaceAssetPath);

	/**
	 * Apply only hair groom.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyHair(AMOMetaHumanCharacter* Character, const FString& HairAssetPath);

	/**
	 * Apply body parameters (for Mutable integration).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyBodyParameters(AMOMetaHumanCharacter* Character, float BodyFat, float MuscleMass, float Height);

	/**
	 * Apply color parameters (skin, hair, eyes).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyColors(AMOMetaHumanCharacter* Character, const FLinearColor& SkinTone, const FLinearColor& HairColor, const FLinearColor& EyeColor);

	// ============================================================================
	// RANDOM GENERATION
	// ============================================================================

	/**
	 * Generate a random appearance from available assets.
	 * @param bMale If true, filter to male-tagged assets; if false, female.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	FMOCharacterAppearance GenerateRandomAppearance(bool bMale = true);

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Base content path for faces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	FString FacesContentPath = TEXT("/Game/Characters/Faces");

	/** Base content path for hair grooms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	FString HairContentPath = TEXT("/Game/Characters/Hair");

	/** Base content path for body meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	FString BodiesContentPath = TEXT("/Game/Characters/Bodies");

	/** Additional mod paths to scan (e.g., "/Game/Mods/Characters"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	TArray<FString> AdditionalModPaths;

protected:
	/** Scan a directory for skeletal mesh assets. */
	void ScanDirectoryForSkeletalMeshes(const FString& Path, EMOAppearanceCategory Category, bool bIsModded);

	/** Scan a directory for groom assets. */
	void ScanDirectoryForGrooms(const FString& Path, EMOAppearanceCategory Category, bool bIsModded);

	/** Extract display name from asset path. */
	FText GetDisplayNameFromPath(const FString& AssetPath) const;

	/** Infer tags from asset name (e.g., "Male_Face_01" -> ["Male"]). */
	TArray<FName> InferTagsFromAssetName(const FString& AssetName) const;

	/** Load a skeletal mesh from path (async-friendly). */
	USkeletalMesh* LoadSkeletalMesh(const FString& AssetPath) const;

	/** Load a groom asset from path. */
	UGroomAsset* LoadGroomAsset(const FString& AssetPath) const;

private:
	/** Discovered assets by category (runtime cache, not serialized). */
	TMap<EMOAppearanceCategory, TArray<FMOAppearanceAssetEntry>> AssetCatalog;

	/** Set of all known asset paths for quick validation. */
	TSet<FString> KnownAssetPaths;

	/** Whether discovery has been run. */
	bool bDiscoveryComplete = false;
};
