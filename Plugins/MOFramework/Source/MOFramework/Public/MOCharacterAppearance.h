/**
 * =============================================================================
 * MOCharacterAppearance.h - Character Appearance Data Structures
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Data structures for storing character appearance (face, body, colors, morphs).
 * Supports two character systems: MetaHuman (modular meshes) and Daz Genesis 8
 * (single mesh with morph targets). Designed for save/load compatibility.
 *
 * KEY STRUCTS:
 * - FMOMorphTargetValue: Single morph name + value pair
 * - FMOMorphTargetDefinition: UI/DataTable morph definition
 * - FMOCharacterAppearance: Complete character appearance data
 * - FMOAppearanceAssetEntry: Catalog entry for discoverable assets
 *
 * SUPPORTED SYSTEMS:
 * - MetaHuman: FaceAssetPath, BodyAssetPath, TorsoAssetPath, etc.
 * - Genesis 8: BodyAssetPath + MorphTargets array
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ASSET PATHS: Stored as FString for mod support. Must be full
 *   content paths (e.g., "/Game/Characters/Faces/Face_01").
 *
 * [2024-02] MORPH VALUE RANGE: Some morphs use 0-1, others use -1 to 1.
 *   FMOMorphTargetDefinition specifies MinValue/MaxValue.
 *
 * [2024-02] VALIDITY CHECK: FMOCharacterAppearance::IsValid() only checks
 *   BodyAssetPath. Additional fields may be required for specific systems.
 *
 * =============================================================================
 * RELATED FILES: MOCustomizableCharacter.h, MOMetaHumanCharacter.h, MOAppearanceSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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

	// The grooms: a GroomAsset object path each (the MetaHuman library under /Game/MetaHumans/Common/Optional/Grooms/GroomAssets, Oct 25 2026),
	// put on the pawn by UMOAppearanceSubsystem::ApplyGroomsToCharacter. EMPTY keeps the pawn Blueprint's own groom (the shipped survivor: updo
	// braids, no beard, no mustache); NoGroom empties the slot (bald, clean-shaven).

	/** Scalp hair groom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString HairAssetPath;

	/** Eyebrows groom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString EyebrowsAssetPath;

	/** Eyelashes groom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString EyelashesAssetPath;

	/** Beard (and goatee) groom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString BeardAssetPath;

	/** Mustache groom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Hair")
	FString MustacheAssetPath;

	/** A groom path that empties its slot; an empty path keeps the Blueprint's own groom. */
	static const FString NoGroom;

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

	// BASE AND BUILD (Oct 27 2026): the survivor base -- a MetaHuman from the Creator: body and face meshes carrying the body shapes, its grooms,
	// the body fat and muscle it was made with (UMOSurvivorBaseSettings) -- and the build the character creator sets over it, the body shapes'
	// sliders (UMOBodyShapeComponent: Bust, Hips, Waist, Thighs). Body fat and muscle themselves are the metabolism's, not the look's.

	/** The survivor base (UMOSurvivorBaseSettings); None keeps the pawn Blueprint's own meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body")
	FName BaseId;

	/** The build sliders: a body-shape driver (Bust, Hips, Waist, Thighs) in MorphName and its value, -1..1 (0 = the base as made). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Body")
	TArray<FMOMorphTargetValue> BodyBuild;

	float GetBuildValue(FName Driver) const
	{
		const FMOMorphTargetValue* Entry = BodyBuild.FindByPredicate([Driver](const FMOMorphTargetValue& V) { return V.MorphName == Driver; });
		return Entry ? Entry->Value : 0.0f;
	}

	void SetBuildValue(FName Driver, float Value)
	{
		FMOMorphTargetValue* Entry = BodyBuild.FindByPredicate([Driver](const FMOMorphTargetValue& V) { return V.MorphName == Driver; });
		if (Entry)
		{
			Entry->Value = Value;
		}
		else
		{
			BodyBuild.Add(FMOMorphTargetValue(Driver, Value));
		}
	}

	// ============================================================================
	// COLORS -- the MetaHuman lookdev's own physical parameters (Oct 15 2026: the character creator), applied by
	// UMOAppearanceSubsystem::ApplyColors to the skin, groom and eye materials. The defaults are the MetaHuman as shipped.
	// (These replaced three RGB colours nothing ever applied: ApplyColors was a TODO.)
	// ============================================================================

	/** Skin tone, light to dark: DefaultSkinTone is the skin as baked; below it a little lighter, above it darker (melanin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Colors", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SkinTone = DefaultSkinTone;

	/** Hair melanin (the MetaHuman hair model): 0 = pale blond, 1 = black. Eyebrows follow the hair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Colors", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HairMelanin = 0.8f;

	/** Hair redness (pheomelanin): 0 = ash, 1 = red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Colors", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HairRedness = 0.25f;

	/** Iris colour: a preset id from UMOAppearanceSubsystem::GetEyeColorPresets (None = the iris as shipped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Colors")
	FName EyeColor;

	static constexpr float DefaultSkinTone = 0.2f;

	/** Take only the colours (skin, hair, eyes) of Other: the character creator edits colours over a pawn's own meshes. */
	void CopyColorsFrom(const FMOCharacterAppearance& Other)
	{
		SkinTone = Other.SkinTone;
		HairMelanin = Other.HairMelanin;
		HairRedness = Other.HairRedness;
		EyeColor = Other.EyeColor;
	}

	/** The look the character creator edits over a pawn's own meshes: the colours, the grooms (an empty groom path keeps the pawn's own), the base
	 *  (None keeps the pawn's own meshes) and the build. */
	void CopyLookFrom(const FMOCharacterAppearance& Other)
	{
		CopyColorsFrom(Other);
		BaseId = Other.BaseId;
		BodyBuild = Other.BodyBuild;
		HairAssetPath = Other.HairAssetPath;
		EyebrowsAssetPath = Other.EyebrowsAssetPath;
		EyelashesAssetPath = Other.EyelashesAssetPath;
		BeardAssetPath = Other.BeardAssetPath;
		MustacheAssetPath = Other.MustacheAssetPath;
	}

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

/**
 * An iris colour of the character creator: values for the MetaHuman eye material's own iris parameters (M_eye_eyeball_unified). The shipped MetaHuman
 * eye is Primary Hue 0.90 / Value 0.56, Secondary Hue 0.98 / Value 0.40, Saturation 1.70. UMOAppearanceSubsystem::GetEyeColorPresets lists them.
 */
struct FMOEyeColorPreset
{
	FName Id;
	FText DisplayName;
	float PrimaryHue = 0.9f;
	float PrimaryValue = 0.56f;
	float SecondaryHue = 0.98f;
	float SecondaryValue = 0.4f;
	float Saturation = 1.7f;
};
