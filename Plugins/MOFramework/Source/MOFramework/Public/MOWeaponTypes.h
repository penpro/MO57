#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOCombatMedicalTypes.h"
#include "MOWeaponTypes.generated.h"

/**
 * =============================================================================
 * MOWeaponTypes.h - Weapon Data Structures for Combat System
 * =============================================================================
 *
 * PURPOSE:
 * Defines weapon properties, damage profiles, and attack data for the combat
 * system. DataTable-driven to allow easy tuning and modding.
 *
 * =============================================================================
 */

/**
 * Attack type enumeration for different attack styles.
 */
UENUM(BlueprintType)
enum class EMOAttackType : uint8
{
	/** Quick, low-damage attack */
	Light,

	/** Slower, high-damage attack */
	Heavy,

	/** Running/charging attack */
	Charge,

	/** Thrown weapon attack */
	Throw,

	/** Ranged projectile attack */
	Ranged,

	/** Blocking counter-attack */
	Riposte,

	MAX UMETA(Hidden)
};

/**
 * Weapon grip type affects attack animations and stamina costs.
 */
UENUM(BlueprintType)
enum class EMOWeaponGrip : uint8
{
	/** One-handed weapon */
	OneHanded,

	/** Two-handed weapon */
	TwoHanded,

	/** Dual-wielded weapons */
	DualWield,

	/** Polearm/staff grip */
	Polearm,

	/** Bow grip */
	Bow,

	/** Throwing grip */
	Thrown,

	MAX UMETA(Hidden)
};

/**
 * Weapon material affects durability and damage effectiveness.
 */
UENUM(BlueprintType)
enum class EMOWeaponMaterial : uint8
{
	/** Stone/flint weapons */
	Stone,

	/** Bone/antler weapons */
	Bone,

	/** Wooden weapons */
	Wood,

	/** Copper weapons */
	Copper,

	/** Bronze weapons */
	Bronze,

	/** Iron weapons */
	Iron,

	/** Steel weapons */
	Steel,

	MAX UMETA(Hidden)
};

/**
 * Damage profile for a specific attack type.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAttackDamageProfile
{
	GENERATED_BODY()

	/** Type of attack this profile is for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	EMOAttackType AttackType = EMOAttackType::Light;

	/** Base damage amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0"))
	float BaseDamage = 10.0f;

	/** Primary damage category. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	EMODamageCategory PrimaryDamageType = EMODamageCategory::Blunt;

	/** Secondary damage category (optional, e.g., sword does slash + blunt). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	EMODamageCategory SecondaryDamageType = EMODamageCategory::Blunt;

	/** Ratio of primary vs secondary damage (1.0 = 100% primary). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0", ClampMax="1"))
	float PrimaryDamageRatio = 1.0f;

	/** Armor penetration (0 = stopped by any armor, 1 = ignores armor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0", ClampMax="1"))
	float ArmorPenetration = 0.0f;

	/** Stamina cost to perform this attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0"))
	float StaminaCost = 5.0f;

	/** Time to wind up the attack (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0"))
	float WindUpTime = 0.2f;

	/** Recovery time after attack (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0"))
	float RecoveryTime = 0.3f;

	/** Attack range (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0"))
	float Range = 150.0f;

	/** Attack arc angle (degrees, for melee sweeps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0", ClampMax="360"))
	float ArcAngle = 90.0f;

	/** Whether this attack causes heavy bleeding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	bool bCausesHeavyBleeding = false;

	/** Whether this attack can break bones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	bool bCanBreakBones = false;

	/** Chance to apply critical hit (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0", ClampMax="1"))
	float CriticalChance = 0.05f;

	/** Critical hit damage multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="1"))
	float CriticalMultiplier = 2.0f;

	/**
	 * Generate a FMOCombatHitInfo from this profile.
	 */
	FMOCombatHitInfo ToHitInfo(EMOBodyPartType TargetPart, bool bIsCritical = false) const
	{
		FMOCombatHitInfo HitInfo;
		HitInfo.TargetBodyPart = TargetPart;
		HitInfo.BaseDamage = BaseDamage;
		HitInfo.DamageCategory = PrimaryDamageType;
		HitInfo.ArmorPenetration = ArmorPenetration;
		HitInfo.CriticalMultiplier = bIsCritical ? CriticalMultiplier : 1.0f;
		HitInfo.bCausesHeavyBleeding = bCausesHeavyBleeding;
		return HitInfo;
	}
};

/**
 * DataTable row for weapon damage profiles.
 * Each weapon type has a row defining its attack properties.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWeaponDamageProfileRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Display name of this weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon")
	FText DisplayName;

	/** Weapon grip type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon")
	EMOWeaponGrip GripType = EMOWeaponGrip::OneHanded;

	/** Weapon material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon")
	EMOWeaponMaterial Material = EMOWeaponMaterial::Stone;

	/** Weight of the weapon (kg). Affects swing speed and stamina costs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon", meta=(ClampMin="0"))
	float Weight = 1.0f;

	/** Durability when new. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon", meta=(ClampMin="1"))
	int32 MaxDurability = 100;

	/** Durability lost per attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon", meta=(ClampMin="0"))
	int32 DurabilityPerHit = 1;

	/** Base blocking effectiveness (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon", meta=(ClampMin="0", ClampMax="1"))
	float BlockEffectiveness = 0.3f;

	/** Stamina cost per second while blocking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon", meta=(ClampMin="0"))
	float BlockStaminaPerSecond = 3.0f;

	/** Stamina cost to parry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon", meta=(ClampMin="0"))
	float ParryStaminaCost = 8.0f;

	/** Parry window duration (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon", meta=(ClampMin="0"))
	float ParryWindow = 0.2f;

	/** Available attack profiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Weapon")
	TArray<FMOAttackDamageProfile> AttackProfiles;

	/**
	 * Get attack profile for a specific attack type.
	 * @return Pointer to profile or nullptr if not found.
	 */
	const FMOAttackDamageProfile* GetAttackProfile(EMOAttackType AttackType) const
	{
		for (const FMOAttackDamageProfile& Profile : AttackProfiles)
		{
			if (Profile.AttackType == AttackType)
			{
				return &Profile;
			}
		}
		return nullptr;
	}
};

/**
 * Runtime weapon state (durability, enchantments, etc.).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWeaponState
{
	GENERATED_BODY()

	/** Item ID referencing the weapon in inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	FGuid ItemGuid;

	/** Weapon profile ID (row name in DataTable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	FName WeaponProfileId;

	/** Current durability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0"))
	int32 CurrentDurability = 100;

	/** Whether this weapon is currently drawn/active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon")
	bool bIsDrawn = false;

	/** Quality modifier (1.0 = standard, affects damage/durability). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon", meta=(ClampMin="0.1"))
	float Quality = 1.0f;

	/**
	 * Calculate effective damage with quality modifier.
	 */
	float GetEffectiveDamage(float BaseDamage) const
	{
		return BaseDamage * Quality;
	}

	/**
	 * Check if weapon is broken.
	 */
	bool IsBroken() const
	{
		return CurrentDurability <= 0;
	}
};

/**
 * Projectile data for ranged weapons.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOProjectileData
{
	GENERATED_BODY()

	/** Projectile speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon|Projectile")
	float Speed = 5000.0f;

	/** Gravity scale (0 = no drop, 1 = normal gravity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon|Projectile", meta=(ClampMin="0"))
	float GravityScale = 1.0f;

	/** Damage falloff start distance (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon|Projectile")
	float DamageFalloffStart = 1000.0f;

	/** Damage falloff end distance (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon|Projectile")
	float DamageFalloffEnd = 5000.0f;

	/** Minimum damage multiplier at max range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon|Projectile", meta=(ClampMin="0", ClampMax="1"))
	float MinDamageMultiplier = 0.25f;

	/** Whether projectile can be recovered after firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon|Projectile")
	bool bRecoverable = true;

	/** Chance to break on impact (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weapon|Projectile", meta=(ClampMin="0", ClampMax="1"))
	float BreakChance = 0.2f;

	/**
	 * Calculate damage multiplier at a given distance.
	 */
	float GetDamageMultiplierAtDistance(float Distance) const
	{
		if (Distance <= DamageFalloffStart)
		{
			return 1.0f;
		}
		if (Distance >= DamageFalloffEnd)
		{
			return MinDamageMultiplier;
		}

		float FalloffRange = DamageFalloffEnd - DamageFalloffStart;
		float FalloffProgress = (Distance - DamageFalloffStart) / FalloffRange;
		return FMath::Lerp(1.0f, MinDamageMultiplier, FalloffProgress);
	}
};
