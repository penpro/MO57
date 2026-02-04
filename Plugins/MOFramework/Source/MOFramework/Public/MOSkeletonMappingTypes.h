#pragma once

#include "CoreMinimal.h"
#include "MOMedicalTypes.h"
#include "MOSkeletonMappingTypes.generated.h"

/**
 * =============================================================================
 * MOSkeletonMappingTypes.h - Skeleton Bone to Body Part Mapping
 * =============================================================================
 *
 * PURPOSE:
 * Maps skeletal mesh bone names to EMOBodyPartType for localized damage.
 * Supports UE5 Mannequin (89 bones) and MetaHuman (342 bones) skeletons.
 *
 * =============================================================================
 * PHYSICS ASSET SETUP GUIDE
 * =============================================================================
 *
 * For per-bone hit detection, you need a properly configured Physics Asset:
 *
 * 1. OPEN PHYSICS ASSET EDITOR
 *    - Double-click the Physics Asset in Content Browser
 *    - Or access via Skeletal Mesh -> Physics Asset
 *
 * 2. CREATE COLLISION BODIES PER ZONE
 *    For efficient hit detection, group bones into collision zones:
 *
 *    HEAD ZONE (1 capsule):
 *    - Attach to: head
 *    - Covers: head, neck_02
 *
 *    TORSO ZONE (2-3 capsules):
 *    - Upper Torso: spine_03, spine_04, spine_05
 *    - Lower Torso: spine_01, spine_02, pelvis
 *
 *    ARM ZONES (per arm):
 *    - Upper Arm: clavicle_l/r, upperarm_l/r
 *    - Forearm: lowerarm_l/r
 *    - Hand: hand_l/r (can include fingers or separate)
 *
 *    LEG ZONES (per leg):
 *    - Thigh: thigh_l/r
 *    - Calf: calf_l/r
 *    - Foot: foot_l/r, ball_l/r
 *
 * 3. SET COLLISION PROPERTIES
 *    - Enable "Simulation Generates Hit Events" on each body
 *    - Set Object Type to "Pawn" or custom channel
 *    - Enable collision with "Projectile" and "Weapon" traces
 *
 * 4. ASSIGN PHYSICAL MATERIALS (optional)
 *    - Create physical materials for different armor types
 *    - Assign per-body for armor simulation
 *
 * =============================================================================
 * LINE TRACE HIT DETECTION
 * =============================================================================
 *
 * When a line trace hits a character with physics collision:
 *
 * ```cpp
 * FHitResult Hit;
 * if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
 * {
 *     if (Hit.BoneName != NAME_None)
 *     {
 *         // Get body part from bone name
 *         EMOBodyPartType BodyPart = UMOSkeletonMapping::BoneToBodyPart(Hit.BoneName);
 *
 *         // Apply damage to that body part
 *         if (UMOAnatomyComponent* Anatomy = Hit.GetActor()->FindComponentByClass<UMOAnatomyComponent>())
 *         {
 *             Anatomy->InflictDamage(BodyPart, Damage, WoundType);
 *         }
 *     }
 * }
 * ```
 *
 * =============================================================================
 * SKELETON TYPES SUPPORTED
 * =============================================================================
 *
 * UE5 MANNEQUIN (89 bones):
 * - pelvis, spine_01-05, neck_01-02, head
 * - clavicle_l/r, upperarm_l/r, lowerarm_l/r, hand_l/r
 * - thigh_l/r, calf_l/r, foot_l/r, ball_l/r
 * - Finger bones: (finger)_01, (finger)_02, (finger)_03
 *
 * METAHUMAN (342 bones):
 * - All UE5 Mannequin bones plus:
 * - Twist bones: upperarm_twist_01_l/r, lowerarm_twist_01_l/r, etc.
 * - Face bones: jaw, tongue, eyes, eyelids, etc.
 * - Metacarpal bones: (finger)_metacarpal_l/r
 *
 * =============================================================================
 */

/**
 * Skeleton type presets for automatic mapping configuration.
 */
UENUM(BlueprintType)
enum class EMOSkeletonType : uint8
{
	/** Auto-detect from bone names. */
	AutoDetect,

	/** UE5 Manny/Quinn skeleton (89 bones). */
	UE5Mannequin,

	/** MetaHuman skeleton (342 bones). */
	MetaHuman,

	/** Custom skeleton with manual mapping. */
	Custom,

	MAX UMETA(Hidden)
};

/**
 * Body region for grouping body parts in hit detection.
 * Used when full body part granularity isn't needed.
 */
UENUM(BlueprintType)
enum class EMOBodyRegion : uint8
{
	Head,
	Torso,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg,

	MAX UMETA(Hidden)
};

/**
 * Single bone-to-body-part mapping entry.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBoneMapping
{
	GENERATED_BODY()

	/** Bone name in the skeleton (e.g., "upperarm_l"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skeleton")
	FName BoneName;

	/** Body part this bone maps to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skeleton")
	EMOBodyPartType BodyPart = EMOBodyPartType::None;

	/** Body region for simplified hit detection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skeleton")
	EMOBodyRegion Region = EMOBodyRegion::Torso;

	/**
	 * Priority for overlapping collision (higher = preferred).
	 * Used when multiple bones could match a hit location.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skeleton")
	int32 Priority = 0;

	FMOBoneMapping() = default;

	FMOBoneMapping(FName InBoneName, EMOBodyPartType InBodyPart, EMOBodyRegion InRegion, int32 InPriority = 0)
		: BoneName(InBoneName)
		, BodyPart(InBodyPart)
		, Region(InRegion)
		, Priority(InPriority)
	{
	}
};

/**
 * Complete skeleton mapping configuration.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSkeletonMappingConfig
{
	GENERATED_BODY()

	/** Type of skeleton this config is for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skeleton")
	EMOSkeletonType SkeletonType = EMOSkeletonType::AutoDetect;

	/** All bone-to-body-part mappings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skeleton")
	TArray<FMOBoneMapping> Mappings;

	/** Default body part if bone not found in mappings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skeleton")
	EMOBodyPartType DefaultBodyPart = EMOBodyPartType::Torso;

	/** Find mapping for a bone name. */
	const FMOBoneMapping* FindMapping(FName BoneName) const
	{
		for (const FMOBoneMapping& Mapping : Mappings)
		{
			if (Mapping.BoneName == BoneName)
			{
				return &Mapping;
			}
		}
		return nullptr;
	}

	/** Get body part for a bone name, using fuzzy matching if exact not found. */
	EMOBodyPartType GetBodyPartForBone(FName BoneName) const;
};

/**
 * Static helper class for skeleton bone mapping operations.
 */
UCLASS()
class MOFRAMEWORK_API UMOSkeletonMapping : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// MAPPING LOOKUP
	// ============================================================================

	/**
	 * Convert a bone name to body part using default UE5/MetaHuman mapping.
	 * This is the primary entry point for damage systems.
	 *
	 * @param BoneName The hit bone name from FHitResult.
	 * @return The corresponding body part, or Torso if not found.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton")
	static EMOBodyPartType BoneToBodyPart(FName BoneName);

	/**
	 * Convert a bone name to body region (simplified grouping).
	 *
	 * @param BoneName The bone name to look up.
	 * @return The body region this bone belongs to.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton")
	static EMOBodyRegion BoneToBodyRegion(FName BoneName);

	/**
	 * Get all bones that map to a specific body part.
	 *
	 * @param BodyPart The body part to find bones for.
	 * @return Array of bone names that map to this part.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton")
	static TArray<FName> GetBonesForBodyPart(EMOBodyPartType BodyPart);

	// ============================================================================
	// MAPPING CONFIGURATIONS
	// ============================================================================

	/**
	 * Get the default mapping configuration for UE5 Mannequin skeleton.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton")
	static FMOSkeletonMappingConfig GetUE5MannequinMapping();

	/**
	 * Get the default mapping configuration for MetaHuman skeleton.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton")
	static FMOSkeletonMappingConfig GetMetaHumanMapping();

	/**
	 * Detect skeleton type from a skeletal mesh's bone names.
	 *
	 * @param Skeleton The skeleton asset to analyze.
	 * @return Detected skeleton type.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton")
	static EMOSkeletonType DetectSkeletonType(const USkeleton* Skeleton);

	// ============================================================================
	// PHYSICS ASSET HELPERS
	// ============================================================================

	/**
	 * Get recommended physics body setup for a body part.
	 * Returns suggested primitive type and dimensions.
	 *
	 * @param BodyPart The body part to get setup for.
	 * @param OutPrimitiveType Output: "Capsule", "Sphere", or "Box"
	 * @param OutDimensions Output: Size parameters (radius, half-height, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton|Physics")
	static void GetRecommendedPhysicsBody(
		EMOBodyPartType BodyPart,
		FString& OutPrimitiveType,
		FVector& OutDimensions);

	/**
	 * Get the parent body part for a given part (anatomical hierarchy).
	 * E.g., Hand -> Forearm -> UpperArm -> Shoulder -> Torso
	 *
	 * @param BodyPart The body part to find parent for.
	 * @return Parent body part, or None if this is a root.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Skeleton")
	static EMOBodyPartType GetParentBodyPart(EMOBodyPartType BodyPart);

	/**
	 * Get all child body parts for a given part.
	 * E.g., Hand -> [Thumb, Index, Middle, Ring, Pinky]
	 *
	 * @param BodyPart The body part to find children for.
	 * @return Array of child body parts.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skeleton")
	static TArray<EMOBodyPartType> GetChildBodyParts(EMOBodyPartType BodyPart);

	// ============================================================================
	// VITAL ORGAN QUERIES
	// ============================================================================

	/**
	 * Check if a body part contains a vital organ.
	 * Vital organs cause rapid or instant death when destroyed.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Skeleton")
	static bool IsVitalOrgan(EMOBodyPartType BodyPart);

	/**
	 * Check if a body part is a limb (can be disabled/amputated).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Skeleton")
	static bool IsLimb(EMOBodyPartType BodyPart);

	/**
	 * Check if a body part is an extremity (finger/toe).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Skeleton")
	static bool IsExtremity(EMOBodyPartType BodyPart);

private:
	/** Initialize default mappings if not already done. */
	static void EnsureMappingsInitialized();

	/** Cached UE5 Mannequin mappings. */
	static TMap<FName, FMOBoneMapping> UE5MannequinMap;

	/** Cached MetaHuman mappings. */
	static TMap<FName, FMOBoneMapping> MetaHumanMap;

	/** Whether mappings have been initialized. */
	static bool bMappingsInitialized;
};
