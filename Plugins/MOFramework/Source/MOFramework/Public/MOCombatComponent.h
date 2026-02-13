#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOWeaponTypes.h"
#include "MOCombatMedicalTypes.h"
#include "MOCombatComponent.generated.h"

// Forward declarations
class UMOAnatomyComponent;
class UMOVitalsComponent;
class UMOMentalStateComponent;
class UMOAdrenalineComponent;
class UMOEquipmentComponent;
class UMOSkillsComponent;
struct FMOThreatInfo;

/**
 * =============================================================================
 * MOCombatComponent - Combat System Orchestration
 * =============================================================================
 *
 * PURPOSE:
 * Manages combat state, attack execution, and damage application. Acts as the
 * central hub for all combat-related functionality, integrating with:
 * - Equipment (weapon state)
 * - Anatomy (damage application)
 * - Vitals (stamina consumption)
 * - Adrenaline (combat state modifiers)
 * - Skills (accuracy, damage bonuses)
 *
 * COMBAT FLOW:
 * 1. Player initiates attack via input
 * 2. CombatComponent validates (stamina, weapon state, cooldowns)
 * 3. Attack executes (animation, hitbox check)
 * 4. On hit: Generate FMOCombatHitInfo from weapon profile
 * 5. Apply damage via UMOCombatMedicalHelpers::ApplyCombatDamage()
 * 6. Handle stamina drain, weapon durability, skill XP
 *
 * =============================================================================
 */

/**
 * Combat state enumeration.
 */
UENUM(BlueprintType)
enum class EMOCombatState : uint8
{
	/** Not in combat, normal state. */
	Idle,

	/** In combat but not actively attacking. */
	Ready,

	/** Winding up an attack. */
	WindingUp,

	/** Active attack swing/thrust. */
	Attacking,

	/** Recovery after attack. */
	Recovering,

	/** Actively blocking. */
	Blocking,

	/** In parry window. */
	Parrying,

	/** Stunned/staggered. */
	Stunned,

	/** Dodging/rolling. */
	Dodging,

	MAX UMETA(Hidden)
};

/**
 * Result of an attack attempt.
 */
UENUM(BlueprintType)
enum class EMOAttackResult : uint8
{
	/** Attack successfully landed. */
	Hit,

	/** Attack was blocked. */
	Blocked,

	/** Attack was parried. */
	Parried,

	/** Attack missed. */
	Missed,

	/** Attack was dodged. */
	Dodged,

	/** Attack glanced off armor. */
	Glanced,

	MAX UMETA(Hidden)
};

// ============================================================================
// DELEGATES
// ============================================================================

/** Fired when combat state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCombatStateChanged, EMOCombatState, OldState, EMOCombatState, NewState);

/** Fired when an attack starts winding up. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnAttackStarted, EMOAttackType, AttackType);

/** Fired when an attack is executed (enters active phase). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnAttackExecuted, EMOAttackType, AttackType);

/** Fired when an attack connects with a target. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnAttackHit, AActor*, Target, EMOAttackResult, Result, const FMOCombatHitInfo&, HitInfo);

/** Fired when we receive damage from an attacker. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnDamageReceived, AActor*, Attacker, const FMOCombatHitInfo&, HitInfo);

/** Fired when blocking an attack. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnAttackBlocked, AActor*, Attacker, float, DamageBlocked);

/** Fired when successfully parrying. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnParrySuccess, AActor*, Attacker);

/** Fired when entering/exiting combat. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnCombatEngaged, bool, bInCombat);

// ============================================================================
// SAVE DATA
// ============================================================================

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCombatSaveData
{
	GENERATED_BODY()

	/** Main hand weapon state. */
	UPROPERTY()
	FMOWeaponState MainHandWeapon;

	/** Off-hand weapon state. */
	UPROPERTY()
	FMOWeaponState OffHandWeapon;

	/** Time since last combat (for decay/recovery). */
	UPROPERTY()
	float TimeSinceLastCombat = 0.0f;
};

// ============================================================================
// COMPONENT
// ============================================================================

/**
 * Component managing combat state and attack execution.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOCombatComponent();

	// ============================================================================
	// STATE
	// ============================================================================

	/** Current combat state. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Combat")
	EMOCombatState CombatState = EMOCombatState::Idle;

	/** Whether we are considered "in combat" (for activity level, UI, etc). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Combat")
	bool bInCombat = false;

	/** Main hand weapon state. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Combat")
	FMOWeaponState MainHandWeapon;

	/** Off-hand weapon state (shield, secondary weapon). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Combat")
	FMOWeaponState OffHandWeapon;

	/** Current attack type being executed. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Combat")
	EMOAttackType CurrentAttackType = EMOAttackType::Light;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnCombatStateChanged OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnAttackStarted OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnAttackExecuted OnAttackExecuted;

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnAttackHit OnAttackHit;

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnDamageReceived OnDamageReceived;

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnAttackBlocked OnAttackBlocked;

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnParrySuccess OnParrySuccess;

	UPROPERTY(BlueprintAssignable, Category="MO|Combat|Events")
	FMOOnCombatEngaged OnCombatEngaged;

	// ============================================================================
	// ATTACK API
	// ============================================================================

	/**
	 * Start a light attack.
	 * @return True if attack was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Attack")
	bool StartLightAttack();

	/**
	 * Start a heavy attack.
	 * @return True if attack was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Attack")
	bool StartHeavyAttack();

	/**
	 * Start a specific attack type.
	 * @param AttackType Type of attack to perform.
	 * @return True if attack was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Attack")
	bool StartAttack(EMOAttackType AttackType);

	/**
	 * Cancel the current attack (if in wind-up phase).
	 * @return True if attack was cancelled.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Attack")
	bool CancelAttack();

	/**
	 * Check if an attack of the given type can be performed.
	 * @param AttackType Type of attack to check.
	 * @param OutReason Reason why attack cannot be performed (if returns false).
	 * @return True if attack can be performed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Attack")
	bool CanAttack(EMOAttackType AttackType, FString& OutReason) const;

	// ============================================================================
	// DEFENSE API
	// ============================================================================

	/**
	 * Start blocking.
	 * @return True if block was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Defense")
	bool StartBlock();

	/**
	 * Stop blocking.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Defense")
	void StopBlock();

	/**
	 * Attempt a parry (timed block).
	 * @return True if parry was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Defense")
	bool AttemptParry();

	/**
	 * Start a dodge/roll.
	 * @param Direction World direction to dodge.
	 * @return True if dodge was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Defense")
	bool StartDodge(const FVector& Direction);

	/**
	 * Check if we are currently blocking.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Defense")
	bool IsBlocking() const { return CombatState == EMOCombatState::Blocking; }

	/**
	 * Check if we are in the parry window.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Defense")
	bool IsParrying() const { return CombatState == EMOCombatState::Parrying; }

	// ============================================================================
	// DAMAGE API
	// ============================================================================

	/**
	 * Receive an attack from an attacker.
	 * Called by the attacker's combat component when their attack hits us.
	 * @param Attacker The attacking actor.
	 * @param HitInfo The hit information.
	 * @return Result of the attack (blocked, hit, etc).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Damage")
	EMOAttackResult ReceiveAttack(AActor* Attacker, const FMOCombatHitInfo& HitInfo);

	/**
	 * Apply combat damage to our own pawn (from environment, fall, etc).
	 * @param HitInfo The hit information.
	 * @return True if damage was applied.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Damage")
	bool ApplySelfDamage(const FMOCombatHitInfo& HitInfo);

	// ============================================================================
	// WEAPON API
	// ============================================================================

	/**
	 * Equip a weapon to main hand.
	 * @param ItemGuid GUID of the weapon item.
	 * @param WeaponProfileId Row name in weapon DataTable.
	 * @return True if weapon was equipped.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Weapon")
	bool EquipMainHandWeapon(const FGuid& ItemGuid, FName WeaponProfileId);

	/**
	 * Equip a weapon to off-hand.
	 * @param ItemGuid GUID of the weapon item.
	 * @param WeaponProfileId Row name in weapon DataTable.
	 * @return True if weapon was equipped.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Weapon")
	bool EquipOffHandWeapon(const FGuid& ItemGuid, FName WeaponProfileId);

	/**
	 * Unequip main hand weapon.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Weapon")
	void UnequipMainHandWeapon();

	/**
	 * Unequip off-hand weapon.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Weapon")
	void UnequipOffHandWeapon();

	/**
	 * Draw/ready the equipped weapon.
	 * @return True if weapon was drawn.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Weapon")
	bool DrawWeapon();

	/**
	 * Sheathe the equipped weapon.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Weapon")
	void SheatheWeapon();

	/**
	 * Check if a weapon is drawn.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Weapon")
	bool IsWeaponDrawn() const { return MainHandWeapon.bIsDrawn; }

	/**
	 * Get the current weapon's damage profile.
	 * @param OutProfile The weapon profile.
	 * @return True if a weapon is equipped and profile was found.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat|Weapon")
	bool GetCurrentWeaponProfile(FMOWeaponDamageProfileRow& OutProfile) const;

	// ============================================================================
	// QUERY API
	// ============================================================================

	/**
	 * Check if we are in any combat state (not idle).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Query")
	bool IsInCombatState() const { return CombatState != EMOCombatState::Idle; }

	/**
	 * Check if we are currently attacking.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Query")
	bool IsAttacking() const;

	/**
	 * Check if we can be interrupted (for stagger).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Query")
	bool CanBeInterrupted() const;

	/**
	 * Get current block effectiveness (0-1).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Query")
	float GetBlockEffectiveness() const;

	/**
	 * Get attack damage bonus from skills.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Query")
	float GetSkillDamageBonus() const;

	/**
	 * Get time until we can attack again.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat|Query")
	float GetAttackCooldownRemaining() const;

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category="MO|Combat|Save")
	void BuildSaveData(FMOCombatSaveData& OutSaveData) const;

	UFUNCTION(BlueprintCallable, Category="MO|Combat|Save")
	bool ApplySaveDataAuthority(const FMOCombatSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** DataTable containing weapon damage profiles. */
	UPROPERTY(EditAnywhere, Category="MO|Combat|Config")
	TSoftObjectPtr<UDataTable> WeaponProfilesDataTable;

	/** Time after last combat action before exiting combat state (seconds). */
	UPROPERTY(EditAnywhere, Category="MO|Combat|Config", meta=(ClampMin="1"))
	float CombatExitDelay = 5.0f;

	/** Stagger duration when hit while attacking (seconds). */
	UPROPERTY(EditAnywhere, Category="MO|Combat|Config", meta=(ClampMin="0"))
	float StaggerDuration = 0.5f;

	/** Dodge invincibility frames duration (seconds). */
	UPROPERTY(EditAnywhere, Category="MO|Combat|Config", meta=(ClampMin="0"))
	float DodgeIFrames = 0.3f;

	/** Dodge total duration (seconds). */
	UPROPERTY(EditAnywhere, Category="MO|Combat|Config", meta=(ClampMin="0"))
	float DodgeDuration = 0.6f;

	/** Stamina cost for dodging. */
	UPROPERTY(EditAnywhere, Category="MO|Combat|Config", meta=(ClampMin="0"))
	float DodgeStaminaCost = 15.0f;

	/** Unarmed attack damage profile. */
	UPROPERTY(EditAnywhere, Category="MO|Combat|Config")
	FMOAttackDamageProfile UnarmedAttackProfile;

	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Time remaining in current state. */
	float StateTimeRemaining = 0.0f;

	/** Time since last combat action. */
	float TimeSinceLastAction = 0.0f;

	/** Whether we are in dodge i-frames. */
	bool bInDodgeIFrames = false;

	/** Current dodge direction. */
	FVector DodgeDirection = FVector::ZeroVector;

	/** Cached reference to anatomy component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOAnatomyComponent> CachedAnatomyComp;

	/** Cached reference to vitals component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOVitalsComponent> CachedVitalsComp;

	/** Cached reference to mental state component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOMentalStateComponent> CachedMentalComp;

	/** Cached reference to adrenaline component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOAdrenalineComponent> CachedAdrenalineComp;

	/** Cached reference to equipment component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOEquipmentComponent> CachedEquipmentComp;

	/** Cached reference to skills component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOSkillsComponent> CachedSkillsComp;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Cache component references. */
	void CacheComponents();

	/** Set combat state with delegate broadcast. */
	void SetCombatState(EMOCombatState NewState);

	/** Enter combat mode. */
	void EnterCombat();

	/** Enter combat mode with a specific threat. */
	void EnterCombatWithThreat(AActor* ThreatActor, float ThreatPower = 50.0f);

	/** Exit combat mode. */
	void ExitCombat();

	/** Process attack wind-up phase. */
	void ProcessWindUp(float DeltaTime);

	/** Process attack active phase. */
	void ProcessAttack(float DeltaTime);

	/** Process attack recovery phase. */
	void ProcessRecovery(float DeltaTime);

	/** Process blocking state. */
	void ProcessBlocking(float DeltaTime);

	/** Process parry window. */
	void ProcessParry(float DeltaTime);

	/** Process stagger state. */
	void ProcessStagger(float DeltaTime);

	/** Process dodge. */
	void ProcessDodge(float DeltaTime);

	/** Execute the current attack (do the hit detection). */
	void ExecuteAttack();

	/** Apply hit to target. */
	EMOAttackResult ApplyHitToTarget(AActor* Target, const FMOCombatHitInfo& HitInfo);

	/** Get the current attack profile (weapon or unarmed). */
	const FMOAttackDamageProfile* GetCurrentAttackProfile() const;

	/** Consume stamina for an action. */
	bool ConsumeStamina(float Amount);

	/** Apply weapon durability loss. */
	void ApplyWeaponDurabilityLoss(int32 Amount);

	/** Grant combat skill XP. */
	void GrantCombatXP(float Amount);

	/** Load weapon profile from DataTable. */
	bool LoadWeaponProfile(FName ProfileId, FMOWeaponDamageProfileRow& OutProfile) const;
};
