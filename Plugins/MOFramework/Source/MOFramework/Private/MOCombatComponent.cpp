#include "MOCombatComponent.h"
#include "MOFramework.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "MOMentalStateComponent.h"
#include "MOAdrenalineComponent.h"
#include "MOAdrenalineTypes.h"
#include "MOEquipmentComponent.h"
#include "MOSkillsComponent.h"
#include "MOCombatMedicalTypes.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UMOCombatComponent::UMOCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f; // Every frame for responsive combat
	SetIsReplicatedByDefault(true);

	// Default unarmed attack profile
	UnarmedAttackProfile.AttackType = EMOAttackType::Light;
	UnarmedAttackProfile.BaseDamage = 5.0f;
	UnarmedAttackProfile.PrimaryDamageType = EMODamageCategory::Blunt;
	UnarmedAttackProfile.ArmorPenetration = 0.0f;
	UnarmedAttackProfile.StaminaCost = 3.0f;
	UnarmedAttackProfile.WindUpTime = 0.15f;
	UnarmedAttackProfile.RecoveryTime = 0.25f;
	UnarmedAttackProfile.Range = 100.0f;
	UnarmedAttackProfile.ArcAngle = 60.0f;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UMOCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheComponents();
}

void UMOCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UMOCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only process on authority
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Track time since last combat action
	if (bInCombat)
	{
		TimeSinceLastAction += DeltaTime;

		// Exit combat after delay
		if (CombatState == EMOCombatState::Ready && TimeSinceLastAction >= CombatExitDelay)
		{
			ExitCombat();
		}
	}

	// Process current state
	switch (CombatState)
	{
	case EMOCombatState::WindingUp:
		ProcessWindUp(DeltaTime);
		break;

	case EMOCombatState::Attacking:
		ProcessAttack(DeltaTime);
		break;

	case EMOCombatState::Recovering:
		ProcessRecovery(DeltaTime);
		break;

	case EMOCombatState::Blocking:
		ProcessBlocking(DeltaTime);
		break;

	case EMOCombatState::Parrying:
		ProcessParry(DeltaTime);
		break;

	case EMOCombatState::Stunned:
		ProcessStagger(DeltaTime);
		break;

	case EMOCombatState::Dodging:
		ProcessDodge(DeltaTime);
		break;

	default:
		break;
	}
}

void UMOCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOCombatComponent, CombatState);
	DOREPLIFETIME(UMOCombatComponent, bInCombat);
	DOREPLIFETIME(UMOCombatComponent, MainHandWeapon);
	DOREPLIFETIME(UMOCombatComponent, OffHandWeapon);
}

// ============================================================================
// ATTACK API
// ============================================================================

bool UMOCombatComponent::StartLightAttack()
{
	return StartAttack(EMOAttackType::Light);
}

bool UMOCombatComponent::StartHeavyAttack()
{
	return StartAttack(EMOAttackType::Heavy);
}

bool UMOCombatComponent::StartAttack(EMOAttackType AttackType)
{
	FString Reason;
	if (!CanAttack(AttackType, Reason))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Cannot attack: %s"), *Reason);
		return false;
	}

	const FMOAttackDamageProfile* Profile = GetCurrentAttackProfile();
	if (!Profile)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCombat] No attack profile found for attack type %d"), static_cast<int32>(AttackType));
		return false;
	}

	// Consume stamina
	if (!ConsumeStamina(Profile->StaminaCost))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Not enough stamina for attack"));
		return false;
	}

	// Enter combat if not already
	if (!bInCombat)
	{
		EnterCombat();
	}

	// Start wind-up
	CurrentAttackType = AttackType;
	StateTimeRemaining = Profile->WindUpTime;
	SetCombatState(EMOCombatState::WindingUp);
	TimeSinceLastAction = 0.0f;

	OnAttackStarted.Broadcast(AttackType);

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Started %s attack, wind-up: %.2fs"),
		AttackType == EMOAttackType::Light ? TEXT("light") : TEXT("heavy"),
		Profile->WindUpTime);

	return true;
}

bool UMOCombatComponent::CancelAttack()
{
	if (CombatState != EMOCombatState::WindingUp)
	{
		return false;
	}

	SetCombatState(bInCombat ? EMOCombatState::Ready : EMOCombatState::Idle);
	return true;
}

bool UMOCombatComponent::CanAttack(EMOAttackType AttackType, FString& OutReason) const
{
	// Check combat state
	if (CombatState != EMOCombatState::Idle &&
		CombatState != EMOCombatState::Ready &&
		CombatState != EMOCombatState::Recovering)
	{
		OutReason = TEXT("Already in combat action");
		return false;
	}

	// Check weapon
	if (!MainHandWeapon.bIsDrawn && MainHandWeapon.WeaponProfileId != NAME_None)
	{
		OutReason = TEXT("Weapon not drawn");
		return false;
	}

	// Check weapon broken
	if (MainHandWeapon.WeaponProfileId != NAME_None && MainHandWeapon.IsBroken())
	{
		OutReason = TEXT("Weapon is broken");
		return false;
	}

	// Check stamina
	const FMOAttackDamageProfile* Profile = GetCurrentAttackProfile();
	if (Profile && CachedVitalsComp)
	{
		if (!UMOCombatMedicalHelpers::CanPerformCombatAction(CachedVitalsComp, Profile->StaminaCost))
		{
			OutReason = TEXT("Not enough stamina");
			return false;
		}
	}

	return true;
}

// ============================================================================
// DEFENSE API
// ============================================================================

bool UMOCombatComponent::StartBlock()
{
	if (CombatState != EMOCombatState::Idle &&
		CombatState != EMOCombatState::Ready)
	{
		return false;
	}

	if (!bInCombat)
	{
		EnterCombat();
	}

	SetCombatState(EMOCombatState::Blocking);
	TimeSinceLastAction = 0.0f;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Started blocking"));
	return true;
}

void UMOCombatComponent::StopBlock()
{
	if (CombatState != EMOCombatState::Blocking)
	{
		return;
	}

	SetCombatState(bInCombat ? EMOCombatState::Ready : EMOCombatState::Idle);
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Stopped blocking"));
}

bool UMOCombatComponent::AttemptParry()
{
	if (CombatState != EMOCombatState::Idle &&
		CombatState != EMOCombatState::Ready &&
		CombatState != EMOCombatState::Blocking)
	{
		return false;
	}

	// Get parry window from weapon profile
	float ParryWindow = 0.2f;
	FMOWeaponDamageProfileRow Profile;
	if (GetCurrentWeaponProfile(Profile))
	{
		ParryWindow = Profile.ParryWindow;

		// Check stamina for parry
		if (CachedVitalsComp && !UMOCombatMedicalHelpers::CanPerformCombatAction(CachedVitalsComp, Profile.ParryStaminaCost))
		{
			return false;
		}

		ConsumeStamina(Profile.ParryStaminaCost);
	}

	if (!bInCombat)
	{
		EnterCombat();
	}

	StateTimeRemaining = ParryWindow;
	SetCombatState(EMOCombatState::Parrying);
	TimeSinceLastAction = 0.0f;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attempting parry, window: %.2fs"), ParryWindow);
	return true;
}

bool UMOCombatComponent::StartDodge(const FVector& Direction)
{
	if (CombatState != EMOCombatState::Idle &&
		CombatState != EMOCombatState::Ready &&
		CombatState != EMOCombatState::Blocking)
	{
		return false;
	}

	// Check stamina
	if (CachedVitalsComp && !UMOCombatMedicalHelpers::CanPerformCombatAction(CachedVitalsComp, DodgeStaminaCost))
	{
		return false;
	}

	ConsumeStamina(DodgeStaminaCost);

	if (!bInCombat)
	{
		EnterCombat();
	}

	DodgeDirection = Direction.GetSafeNormal();
	StateTimeRemaining = DodgeDuration;
	bInDodgeIFrames = true;
	SetCombatState(EMOCombatState::Dodging);
	TimeSinceLastAction = 0.0f;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Started dodge"));
	return true;
}

// ============================================================================
// DAMAGE API
// ============================================================================

EMOAttackResult UMOCombatComponent::ReceiveAttack(AActor* Attacker, const FMOCombatHitInfo& HitInfo)
{
	// Check if in dodge i-frames
	if (bInDodgeIFrames)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attack dodged (i-frames)"));
		return EMOAttackResult::Dodged;
	}

	// Check for parry
	if (CombatState == EMOCombatState::Parrying)
	{
		OnParrySuccess.Broadcast(Attacker);
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attack parried!"));
		return EMOAttackResult::Parried;
	}

	// Check for block
	if (CombatState == EMOCombatState::Blocking)
	{
		float BlockEff = GetBlockEffectiveness();
		float DamageBlocked = HitInfo.BaseDamage * BlockEff;
		float DamageThrough = HitInfo.BaseDamage * (1.0f - BlockEff);

		// Apply blocked damage notification
		OnAttackBlocked.Broadcast(Attacker, DamageBlocked);

		// If some damage gets through, apply it
		if (DamageThrough > 0.0f && CachedAnatomyComp)
		{
			FMOCombatHitInfo ReducedHit = HitInfo;
			ReducedHit.BaseDamage = DamageThrough;
			UMOCombatMedicalHelpers::ApplyCombatDamage(ReducedHit, CachedAnatomyComp, CachedVitalsComp, CachedMentalComp);
		}

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attack blocked (%.0f%% effectiveness)"), BlockEff * 100.0f);
		return EMOAttackResult::Blocked;
	}

	// Full damage
	if (CachedAnatomyComp)
	{
		UMOCombatMedicalHelpers::ApplyCombatDamage(HitInfo, CachedAnatomyComp, CachedVitalsComp, CachedMentalComp);
	}

	// Enter combat if not already
	if (!bInCombat)
	{
		EnterCombat();
	}

	// Stagger if attacking
	if (CombatState == EMOCombatState::WindingUp || CombatState == EMOCombatState::Attacking)
	{
		StateTimeRemaining = StaggerDuration;
		SetCombatState(EMOCombatState::Stunned);
	}

	OnDamageReceived.Broadcast(Attacker, HitInfo);
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attack hit, damage: %.1f"), HitInfo.GetFinalDamage());

	return EMOAttackResult::Hit;
}

bool UMOCombatComponent::ApplySelfDamage(const FMOCombatHitInfo& HitInfo)
{
	if (!CachedAnatomyComp)
	{
		return false;
	}

	return UMOCombatMedicalHelpers::ApplyCombatDamage(HitInfo, CachedAnatomyComp, CachedVitalsComp, CachedMentalComp);
}

// ============================================================================
// WEAPON API
// ============================================================================

bool UMOCombatComponent::EquipMainHandWeapon(const FGuid& ItemGuid, FName WeaponProfileId)
{
	FMOWeaponDamageProfileRow Profile;
	if (!LoadWeaponProfile(WeaponProfileId, Profile))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCombat] Failed to load weapon profile: %s"), *WeaponProfileId.ToString());
		return false;
	}

	MainHandWeapon.ItemGuid = ItemGuid;
	MainHandWeapon.WeaponProfileId = WeaponProfileId;
	MainHandWeapon.CurrentDurability = Profile.MaxDurability;
	MainHandWeapon.bIsDrawn = false;
	MainHandWeapon.Quality = 1.0f;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCombat] Equipped main hand weapon: %s"), *WeaponProfileId.ToString());
	return true;
}

bool UMOCombatComponent::EquipOffHandWeapon(const FGuid& ItemGuid, FName WeaponProfileId)
{
	FMOWeaponDamageProfileRow Profile;
	if (!LoadWeaponProfile(WeaponProfileId, Profile))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCombat] Failed to load off-hand weapon profile: %s"), *WeaponProfileId.ToString());
		return false;
	}

	OffHandWeapon.ItemGuid = ItemGuid;
	OffHandWeapon.WeaponProfileId = WeaponProfileId;
	OffHandWeapon.CurrentDurability = Profile.MaxDurability;
	OffHandWeapon.bIsDrawn = false;
	OffHandWeapon.Quality = 1.0f;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCombat] Equipped off-hand weapon: %s"), *WeaponProfileId.ToString());
	return true;
}

void UMOCombatComponent::UnequipMainHandWeapon()
{
	MainHandWeapon = FMOWeaponState();
	UE_LOG(LogMOFramework, Log, TEXT("[MOCombat] Unequipped main hand weapon"));
}

void UMOCombatComponent::UnequipOffHandWeapon()
{
	OffHandWeapon = FMOWeaponState();
	UE_LOG(LogMOFramework, Log, TEXT("[MOCombat] Unequipped off-hand weapon"));
}

bool UMOCombatComponent::DrawWeapon()
{
	if (MainHandWeapon.WeaponProfileId == NAME_None)
	{
		return false;
	}

	MainHandWeapon.bIsDrawn = true;
	OffHandWeapon.bIsDrawn = (OffHandWeapon.WeaponProfileId != NAME_None);

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Weapon drawn"));
	return true;
}

void UMOCombatComponent::SheatheWeapon()
{
	MainHandWeapon.bIsDrawn = false;
	OffHandWeapon.bIsDrawn = false;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Weapon sheathed"));
}

bool UMOCombatComponent::GetCurrentWeaponProfile(FMOWeaponDamageProfileRow& OutProfile) const
{
	if (MainHandWeapon.WeaponProfileId == NAME_None)
	{
		return false;
	}

	return LoadWeaponProfile(MainHandWeapon.WeaponProfileId, OutProfile);
}

// ============================================================================
// QUERY API
// ============================================================================

bool UMOCombatComponent::IsAttacking() const
{
	return CombatState == EMOCombatState::WindingUp ||
		   CombatState == EMOCombatState::Attacking ||
		   CombatState == EMOCombatState::Recovering;
}

bool UMOCombatComponent::CanBeInterrupted() const
{
	return CombatState == EMOCombatState::WindingUp ||
		   CombatState == EMOCombatState::Attacking;
}

float UMOCombatComponent::GetBlockEffectiveness() const
{
	float BaseEffectiveness = 0.3f; // Unarmed block

	FMOWeaponDamageProfileRow Profile;
	if (GetCurrentWeaponProfile(Profile))
	{
		BaseEffectiveness = Profile.BlockEffectiveness;
	}

	// TODO: Factor in skill level
	return BaseEffectiveness;
}

float UMOCombatComponent::GetSkillDamageBonus() const
{
	if (!CachedSkillsComp)
	{
		return 0.0f;
	}

	// TODO: Look up combat skill and calculate bonus
	// For now return 0
	return 0.0f;
}

float UMOCombatComponent::GetAttackCooldownRemaining() const
{
	if (CombatState == EMOCombatState::Recovering)
	{
		return StateTimeRemaining;
	}
	return 0.0f;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void UMOCombatComponent::BuildSaveData(FMOCombatSaveData& OutSaveData) const
{
	OutSaveData.MainHandWeapon = MainHandWeapon;
	OutSaveData.OffHandWeapon = OffHandWeapon;
	OutSaveData.TimeSinceLastCombat = TimeSinceLastAction;
}

bool UMOCombatComponent::ApplySaveDataAuthority(const FMOCombatSaveData& InSaveData)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	MainHandWeapon = InSaveData.MainHandWeapon;
	OffHandWeapon = InSaveData.OffHandWeapon;
	TimeSinceLastAction = InSaveData.TimeSinceLastCombat;

	return true;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UMOCombatComponent::CacheComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	CachedAnatomyComp = Owner->FindComponentByClass<UMOAnatomyComponent>();
	CachedVitalsComp = Owner->FindComponentByClass<UMOVitalsComponent>();
	CachedMentalComp = Owner->FindComponentByClass<UMOMentalStateComponent>();
	CachedAdrenalineComp = Owner->FindComponentByClass<UMOAdrenalineComponent>();
	CachedEquipmentComp = Owner->FindComponentByClass<UMOEquipmentComponent>();
	CachedSkillsComp = Owner->FindComponentByClass<UMOSkillsComponent>();
}

void UMOCombatComponent::SetCombatState(EMOCombatState NewState)
{
	if (CombatState == NewState)
	{
		return;
	}

	EMOCombatState OldState = CombatState;
	CombatState = NewState;

	OnCombatStateChanged.Broadcast(OldState, NewState);
}

void UMOCombatComponent::EnterCombat()
{
	EnterCombatWithThreat(nullptr, 50.0f);
}

void UMOCombatComponent::EnterCombatWithThreat(AActor* ThreatActor, float ThreatPower)
{
	if (bInCombat)
	{
		// Already in combat - just refresh adrenaline with new threat
		if (CachedAdrenalineComp && ThreatActor)
		{
			FMOThreatInfo ThreatInfo;
			ThreatInfo.ThreatActor = ThreatActor;
			ThreatInfo.ThreatPower = ThreatPower;
			ThreatInfo.ThreatCount = 1;
			ThreatInfo.bIsAttacking = true;
			if (ThreatActor)
			{
				ThreatInfo.Distance = FVector::Dist(GetOwner()->GetActorLocation(), ThreatActor->GetActorLocation());
			}
			CachedAdrenalineComp->RegisterThreat(ThreatInfo);
		}
		return;
	}

	bInCombat = true;
	SetCombatState(EMOCombatState::Ready);

	// Set activity level to combat
	if (CachedVitalsComp)
	{
		CachedVitalsComp->SetActivityLevel(EMOActivityLevel::Combat);
	}

	// Trigger adrenaline response with threat info
	if (CachedAdrenalineComp)
	{
		FMOThreatInfo ThreatInfo;
		ThreatInfo.ThreatActor = ThreatActor;
		ThreatInfo.ThreatPower = ThreatPower;
		ThreatInfo.ThreatCount = 1;
		ThreatInfo.bIsAttacking = true;
		if (ThreatActor)
		{
			ThreatInfo.Distance = FVector::Dist(GetOwner()->GetActorLocation(), ThreatActor->GetActorLocation());
		}
		CachedAdrenalineComp->EnterCombat(ThreatInfo);
	}

	OnCombatEngaged.Broadcast(true);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCombat] Entered combat"));
}

void UMOCombatComponent::ExitCombat()
{
	if (!bInCombat)
	{
		return;
	}

	bInCombat = false;
	SetCombatState(EMOCombatState::Idle);

	// Reset activity level
	if (CachedVitalsComp)
	{
		CachedVitalsComp->SetActivityLevel(EMOActivityLevel::Idle);
	}

	// Notify adrenaline system
	if (CachedAdrenalineComp)
	{
		CachedAdrenalineComp->ExitCombat();
	}

	OnCombatEngaged.Broadcast(false);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCombat] Exited combat"));
}

void UMOCombatComponent::ProcessWindUp(float DeltaTime)
{
	StateTimeRemaining -= DeltaTime;

	if (StateTimeRemaining <= 0.0f)
	{
		// Transition to active attack phase
		ExecuteAttack();
	}
}

void UMOCombatComponent::ProcessAttack(float DeltaTime)
{
	StateTimeRemaining -= DeltaTime;

	if (StateTimeRemaining <= 0.0f)
	{
		// Transition to recovery
		const FMOAttackDamageProfile* Profile = GetCurrentAttackProfile();
		StateTimeRemaining = Profile ? Profile->RecoveryTime : 0.3f;
		SetCombatState(EMOCombatState::Recovering);
	}
}

void UMOCombatComponent::ProcessRecovery(float DeltaTime)
{
	StateTimeRemaining -= DeltaTime;

	if (StateTimeRemaining <= 0.0f)
	{
		SetCombatState(bInCombat ? EMOCombatState::Ready : EMOCombatState::Idle);
	}
}

void UMOCombatComponent::ProcessBlocking(float DeltaTime)
{
	// Drain stamina while blocking
	FMOWeaponDamageProfileRow Profile;
	float DrainRate = 3.0f;
	if (GetCurrentWeaponProfile(Profile))
	{
		DrainRate = Profile.BlockStaminaPerSecond;
	}

	if (!ConsumeStamina(DrainRate * DeltaTime))
	{
		// Out of stamina, stop blocking
		StopBlock();
	}
}

void UMOCombatComponent::ProcessParry(float DeltaTime)
{
	StateTimeRemaining -= DeltaTime;

	if (StateTimeRemaining <= 0.0f)
	{
		// Parry window closed, transition to block or ready
		SetCombatState(EMOCombatState::Blocking);
	}
}

void UMOCombatComponent::ProcessStagger(float DeltaTime)
{
	StateTimeRemaining -= DeltaTime;

	if (StateTimeRemaining <= 0.0f)
	{
		SetCombatState(bInCombat ? EMOCombatState::Ready : EMOCombatState::Idle);
	}
}

void UMOCombatComponent::ProcessDodge(float DeltaTime)
{
	StateTimeRemaining -= DeltaTime;

	// End i-frames partway through dodge
	if (bInDodgeIFrames && (DodgeDuration - StateTimeRemaining) >= DodgeIFrames)
	{
		bInDodgeIFrames = false;
	}

	if (StateTimeRemaining <= 0.0f)
	{
		bInDodgeIFrames = false;
		SetCombatState(bInCombat ? EMOCombatState::Ready : EMOCombatState::Idle);
	}
}

void UMOCombatComponent::ExecuteAttack()
{
	const FMOAttackDamageProfile* Profile = GetCurrentAttackProfile();
	if (!Profile)
	{
		SetCombatState(bInCombat ? EMOCombatState::Ready : EMOCombatState::Idle);
		return;
	}

	// Transition to attacking state
	StateTimeRemaining = 0.1f; // Brief active phase
	SetCombatState(EMOCombatState::Attacking);

	OnAttackExecuted.Broadcast(CurrentAttackType);

	// Apply durability loss
	ApplyWeaponDurabilityLoss(1);

	// TODO: Actual hit detection would go here
	// For now this is scaffolding - the actual hit detection would be:
	// 1. Trace or overlap in attack arc
	// 2. For each hit actor with CombatComponent, call their ReceiveAttack()
	// 3. Handle results

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attack executed"));
}

EMOAttackResult UMOCombatComponent::ApplyHitToTarget(AActor* Target, const FMOCombatHitInfo& HitInfo)
{
	if (!Target)
	{
		return EMOAttackResult::Missed;
	}

	UMOCombatComponent* TargetCombat = Target->FindComponentByClass<UMOCombatComponent>();
	if (TargetCombat)
	{
		EMOAttackResult Result = TargetCombat->ReceiveAttack(GetOwner(), HitInfo);
		OnAttackHit.Broadcast(Target, Result, HitInfo);
		return Result;
	}

	// Target has no combat component - apply damage directly to anatomy if available
	UMOAnatomyComponent* TargetAnatomy = Target->FindComponentByClass<UMOAnatomyComponent>();
	if (TargetAnatomy)
	{
		UMOCombatMedicalHelpers::ApplyCombatDamage(HitInfo, TargetAnatomy, nullptr, nullptr);
		OnAttackHit.Broadcast(Target, EMOAttackResult::Hit, HitInfo);
		return EMOAttackResult::Hit;
	}

	return EMOAttackResult::Missed;
}

const FMOAttackDamageProfile* UMOCombatComponent::GetCurrentAttackProfile() const
{
	// If we have a weapon, use its profile
	if (MainHandWeapon.WeaponProfileId != NAME_None)
	{
		FMOWeaponDamageProfileRow Profile;
		if (LoadWeaponProfile(MainHandWeapon.WeaponProfileId, Profile))
		{
			return Profile.GetAttackProfile(CurrentAttackType);
		}
	}

	// Fall back to unarmed
	if (UnarmedAttackProfile.AttackType == CurrentAttackType)
	{
		return &UnarmedAttackProfile;
	}

	return nullptr;
}

bool UMOCombatComponent::ConsumeStamina(float Amount)
{
	if (!CachedVitalsComp)
	{
		return true; // No vitals = no stamina check
	}

	if (CachedVitalsComp->Activity.CurrentStamina < Amount)
	{
		return false;
	}

	CachedVitalsComp->ModifyStamina(-Amount);
	return true;
}

void UMOCombatComponent::ApplyWeaponDurabilityLoss(int32 Amount)
{
	if (MainHandWeapon.WeaponProfileId == NAME_None)
	{
		return;
	}

	MainHandWeapon.CurrentDurability = FMath::Max(0, MainHandWeapon.CurrentDurability - Amount);

	if (MainHandWeapon.IsBroken())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCombat] Main hand weapon broke!"));
		// TODO: Broadcast weapon broke event
	}
}

void UMOCombatComponent::GrantCombatXP(float Amount)
{
	if (!CachedSkillsComp)
	{
		return;
	}

	// TODO: Grant XP to appropriate combat skill
}

bool UMOCombatComponent::LoadWeaponProfile(FName ProfileId, FMOWeaponDamageProfileRow& OutProfile) const
{
	if (ProfileId == NAME_None)
	{
		return false;
	}

	UDataTable* DataTable = WeaponProfilesDataTable.LoadSynchronous();
	if (!DataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCombat] Weapon profiles DataTable not set"));
		return false;
	}

	const FMOWeaponDamageProfileRow* Row = DataTable->FindRow<FMOWeaponDamageProfileRow>(ProfileId, TEXT("LoadWeaponProfile"));
	if (!Row)
	{
		return false;
	}

	OutProfile = *Row;
	return true;
}
