/**
 * =============================================================================
 * MOMorphDefinitionRow.h - Character Morph Target DataTable Definitions
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * DataTable row definitions for morph targets used in character customization.
 * Defines available morphs (sliders) and presets for the customization UI.
 *
 * ROW TYPES:
 * - FMOMorphDefinitionRow: Individual morph target (slider definition)
 * - FMOMorphPresetRow: Saved combination of morph values (body type presets)
 *
 * MORPH CATEGORIES:
 * Face (Shape, Eyes, Nose, Mouth, Ears, Jaw, Brows, Cheeks, Forehead)
 * Body (Type, Torso, Arms, Hands, Legs, Feet, Breasts, Hips, Glutes)
 * Other (Age, Details, Custom)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ROW NAME = MORPH NAME: The DataTable row name must exactly match
 *   the morph target name on the skeletal mesh (e.g., "BreastsEnlarge").
 *
 * [2024-02] GENDER FILTERING: bMale/bFemale flags control which morphs appear
 *   in the UI based on character gender. Both can be true.
 *
 * [2024-02] LINKED MORPHS: Format is "MorphName:Multiplier". When parent morph
 *   is set to 1.0, linked morphs are set to their multiplier value.
 *
 * =============================================================================
 * RELATED FILES: MOCharacterAppearance.h, MOMetaHumanCharacter.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOMorphDefinitionRow.generated.h"

/**
 * Morph category for UI organization.
 */
UENUM(BlueprintType)
enum class EMOMorphCategory : uint8
{
	// Face categories
	FaceShape		UMETA(DisplayName = "Face Shape"),
	Eyes			UMETA(DisplayName = "Eyes"),
	Nose			UMETA(DisplayName = "Nose"),
	Mouth			UMETA(DisplayName = "Mouth"),
	Ears			UMETA(DisplayName = "Ears"),
	Jaw				UMETA(DisplayName = "Jaw/Chin"),
	Brows			UMETA(DisplayName = "Brows"),
	Cheeks			UMETA(DisplayName = "Cheeks"),
	Forehead		UMETA(DisplayName = "Forehead"),

	// Body categories
	BodyType		UMETA(DisplayName = "Body Type"),
	Torso			UMETA(DisplayName = "Torso"),
	Arms			UMETA(DisplayName = "Arms"),
	Hands			UMETA(DisplayName = "Hands"),
	Legs			UMETA(DisplayName = "Legs"),
	Feet			UMETA(DisplayName = "Feet"),

	// Gender-specific
	Breasts			UMETA(DisplayName = "Breasts"),
	Hips			UMETA(DisplayName = "Hips"),
	Glutes			UMETA(DisplayName = "Glutes"),

	// Age/Details
	Age				UMETA(DisplayName = "Age"),
	Details			UMETA(DisplayName = "Details"),

	// Other
	Other			UMETA(DisplayName = "Other")
};

/**
 * DataTable row for morph target definitions.
 * Equivalent to Origins MorphDataTable / S_Morphs.
 *
 * Create a DataTable with this row type to define available morphs
 * for the character customization UI.
 *
 * Row Name = Morph target name on the skeletal mesh (e.g., "BreastsEnlarge")
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMorphDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Display name shown in the UI slider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph")
	FText DisplayName;

	/** Category for UI grouping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph")
	EMOMorphCategory Category = EMOMorphCategory::Other;

	/** Custom category name (used if Category is Other). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph",
		meta=(EditCondition="Category==EMOMorphCategory::Other"))
	FName CustomCategory;

	/** Minimum slider value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph")
	float MinValue = 0.0f;

	/** Maximum slider value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph")
	float MaxValue = 1.0f;

	/** Default value for new characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph")
	float DefaultValue = 0.0f;

	/** Step size for slider increments (0 = continuous). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph", meta=(ClampMin="0.0"))
	float StepSize = 0.0f;

	/** Whether this morph is available for male characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph|Gender")
	bool bMale = true;

	/** Whether this morph is available for female characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph|Gender")
	bool bFemale = true;

	/** Sort order within category (lower = higher in list). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph|UI")
	int32 SortOrder = 0;

	/** Optional tooltip/description for the morph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph|UI")
	FText Description;

	/** Whether to hide this morph in basic mode (show only in advanced). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph|UI")
	bool bAdvancedOnly = false;

	/**
	 * Optional morph group - morphs in same group are mutually exclusive
	 * or combined (e.g., body type presets).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph|Grouping")
	FName MorphGroup;

	/**
	 * Linked morphs that should change together.
	 * Format: "MorphName:Multiplier" (e.g., "BreastsSag:0.5" means this morph
	 * at 1.0 will also set BreastsSag to 0.5).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morph|Grouping")
	TArray<FString> LinkedMorphs;

	// ============================================================================
	// HELPERS
	// ============================================================================

	/** Get the effective category name for UI grouping. */
	FName GetCategoryName() const
	{
		if (Category == EMOMorphCategory::Other && !CustomCategory.IsNone())
		{
			return CustomCategory;
		}

		// Convert enum to name
		const UEnum* EnumPtr = StaticEnum<EMOMorphCategory>();
		if (EnumPtr)
		{
			return FName(*EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Category)).ToString());
		}
		return NAME_None;
	}

	/** Check if this morph applies to the given gender. */
	bool AppliesToGender(bool bIsFemale) const
	{
		return bIsFemale ? bFemale : bMale;
	}
};

/**
 * Morph preset - a saved combination of morph values.
 * Can be used for body type presets, face presets, etc.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMorphPresetRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Display name for the preset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset")
	FText DisplayName;

	/** Category this preset belongs to (e.g., "Body Type", "Face Shape"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset")
	FName Category;

	/** Whether this preset is for male characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset|Gender")
	bool bMale = true;

	/** Whether this preset is for female characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset|Gender")
	bool bFemale = true;

	/** Optional thumbnail texture for the preset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset|UI")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/**
	 * Morph values for this preset.
	 * Map of MorphName -> Value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset")
	TMap<FName, float> MorphValues;

	/** Optional description. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset|UI")
	FText Description;

	/** Sort order for UI display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preset|UI")
	int32 SortOrder = 0;
};
