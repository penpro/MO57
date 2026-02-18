#pragma once

#include "CoreMinimal.h"
#include "MOCharacterAppearance.generated.h"

/**
 * Single morph target value entry.
 * Equivalent to Origins S_MorphSaves - stores a morph name and its value.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMorphTargetValue
{
	GENERATED_BODY()

	FMOMorphTargetValue() = default;

	FMOMorphTargetValue(FName InMorphName, float InValue)
		: MorphName(InMorphName), Value(InValue) {}

	/** Name of the morph target on the skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	FName MorphName;

	/** Value of the morph target (-1.0 to 1.0 or 0.0 to 1.0 depending on morph). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	float Value = 0.0f;

	bool operator==(const FMOMorphTargetValue& Other) const
	{
		return MorphName == Other.MorphName && FMath::IsNearlyEqual(Value, Other.Value);
	}
};

/**
 * Morph target definition for UI/data tables.
 * Equivalent to Origins S_Morphs / MorphDataTable row.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMorphTargetDefinition
{
	GENERATED_BODY()

	/** Unique ID for this morph (matches morph target name on mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	FName MorphId;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	FText DisplayName;

	/** Category for UI grouping (e.g., "Face", "Body", "Details"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	FName Category;

	/** Minimum allowed value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	float MinValue = 0.0f;

	/** Maximum allowed value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	float MaxValue = 1.0f;

	/** Default value for new characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	float DefaultValue = 0.0f;

	/** Whether this morph applies to male characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	bool bAppliesToMale = true;

	/** Whether this morph applies to female characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	bool bAppliesToFemale = true;
};

/**
 * Character appearance data - stores all customization parameters.
 * Designed to be saved/loaded with pawn records and supports modded assets.
 *
 * Supports two character systems:
 * 1. MetaHuman (modular meshes: Body, Face, Torso, Legs, Feet + Grooms)
 * 2. Daz Genesis 8 style (single mesh with morph targets)
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCharacterAppearance
{
	GENERATED_BODY()

	// ============================================================================
	// FACE / HEAD
	// ============================================================================

	/** Asset path to face skeletal mesh (supports mod paths). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Face")
	FString FaceAssetPath;

	/** Asset path to hair groom asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString HairAssetPath;

	/** Asset path to eyebrows groom asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString EyebrowsAssetPath;

	/** Asset path to eyelashes groom asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString EyelashesAssetPath;

	/** Asset path to beard groom asset (empty = no beard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString BeardAssetPath;

	/** Asset path to mustache groom asset (empty = no mustache). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString MustacheAssetPath;

	// ============================================================================
	// BODY (Mutable Parameters)
	// ============================================================================

	/** Asset path to body skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body")
	FString BodyAssetPath;

	/** Body fat percentage (0.0 = lean, 1.0 = heavy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BodyFat = 0.3f;

	/** Muscle mass (0.0 = none, 1.0 = bodybuilder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MuscleMass = 0.4f;

	/** Height multiplier (0.8 = short, 1.0 = average, 1.2 = tall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body", meta=(ClampMin="0.8", ClampMax="1.2"))
	float Height = 1.0f;

	/** Shoulder width multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body", meta=(ClampMin="0.8", ClampMax="1.2"))
	float ShoulderWidth = 1.0f;

	/** Hip width multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body", meta=(ClampMin="0.8", ClampMax="1.2"))
	float HipWidth = 1.0f;

	// ============================================================================
	// COLORS
	// ============================================================================

	/** Skin tone color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Colors")
	FLinearColor SkinTone = FLinearColor(0.8f, 0.6f, 0.5f, 1.0f);

	/** Hair color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Colors")
	FLinearColor HairColor = FLinearColor(0.1f, 0.08f, 0.05f, 1.0f);

	/** Eye color (iris). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Colors")
	FLinearColor EyeColor = FLinearColor(0.2f, 0.4f, 0.3f, 1.0f);

	// ============================================================================
	// MORPH TARGETS (Daz Genesis 8 / Custom Mesh Support)
	// ============================================================================

	/** All morph target values for this character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Morphs")
	TArray<FMOMorphTargetValue> MorphTargets;

	// ============================================================================
	// CHARACTER INFO
	// ============================================================================

	/** Whether this is a female character (affects mesh selection and available morphs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Info")
	bool bIsFemale = false;

	/** Character name (for display). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Info")
	FString CharacterName;

	// ============================================================================
	// CLOTHING / EQUIPMENT MESH PATHS (MetaHuman modular system)
	// ============================================================================

	/** Asset path to torso/clothing mesh (MetaHuman style). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Clothing")
	FString TorsoAssetPath;

	/** Asset path to legs mesh (MetaHuman style). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Clothing")
	FString LegsAssetPath;

	/** Asset path to feet mesh (MetaHuman style). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Clothing")
	FString FeetAssetPath;

	// ============================================================================
	// UTILITIES
	// ============================================================================

	/** Check if this appearance has valid required data. */
	bool IsValid() const
	{
		// Valid if we have either a body mesh (Genesis 8 style) or face+body (MetaHuman style)
		return !BodyAssetPath.IsEmpty();
	}

	/** Get a default male appearance. */
	static FMOCharacterAppearance GetDefaultMale();

	/** Get a default female appearance. */
	static FMOCharacterAppearance GetDefaultFemale();

	// ============================================================================
	// MORPH TARGET HELPERS
	// ============================================================================

	/** Set a morph target value. Creates entry if it doesn't exist. */
	void SetMorphValue(FName MorphName, float Value)
	{
		for (FMOMorphTargetValue& Morph : MorphTargets)
		{
			if (Morph.MorphName == MorphName)
			{
				Morph.Value = Value;
				return;
			}
		}
		// Not found, add new entry
		MorphTargets.Add(FMOMorphTargetValue(MorphName, Value));
	}

	/** Get a morph target value. Returns 0 if not found. */
	float GetMorphValue(FName MorphName) const
	{
		for (const FMOMorphTargetValue& Morph : MorphTargets)
		{
			if (Morph.MorphName == MorphName)
			{
				return Morph.Value;
			}
		}
		return 0.0f;
	}

	/** Check if a morph target exists in our data. */
	bool HasMorph(FName MorphName) const
	{
		for (const FMOMorphTargetValue& Morph : MorphTargets)
		{
			if (Morph.MorphName == MorphName)
			{
				return true;
			}
		}
		return false;
	}

	/** Remove a morph target entry. */
	void RemoveMorph(FName MorphName)
	{
		MorphTargets.RemoveAll([MorphName](const FMOMorphTargetValue& Morph)
		{
			return Morph.MorphName == MorphName;
		});
	}

	/** Clear all morph targets. */
	void ClearMorphs()
	{
		MorphTargets.Empty();
	}
};

/**
 * Catalog entry for a discoverable appearance asset (face, hair, etc.).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAppearanceAssetEntry
{
	GENERATED_BODY()

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FText DisplayName;

	/** Asset path for loading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FString AssetPath;

	/** Optional thumbnail texture path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FString ThumbnailPath;

	/** Tags for filtering (e.g., "Male", "Female", "Young", "Old"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	TArray<FName> Tags;

	/** Whether this is from a mod (discovered at runtime). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	bool bIsModded = false;
};

/**
 * Category of appearance assets (faces, hair, etc.).
 */
UENUM(BlueprintType)
enum class EMOAppearanceCategory : uint8
{
	Face,
	Hair,
	Eyebrows,
	Eyelashes,
	Beard,
	Mustache,
	Body
};
