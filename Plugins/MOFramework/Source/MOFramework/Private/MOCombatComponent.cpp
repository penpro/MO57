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
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UMOCombatComponent::UMOCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10Hz — sufficient for 0.15-0.6s combat timers
	SetIsReplicatedByDefault(true);

	// Default unarmed attack profile
	UnarmedAttackProfile.AttackType = EMOAttackType::Light;
	UnarmedAttackProfile.BaseDamage = 5.0f;
	UnarmedAttackProfile.PrimaryDamageType = EMODamageCategory::Blunt;
	UnarmedAttackProfile.ArmorPenetration = 1.0f;  // 1.0 = full damage against unarmored targets
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
	DOREPLIFETIME(UMOCombatComponent, CurrentAttackType);
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
	// Player-driven verb: a remote client forwards intent to the server, which runs the
	// verb authoritatively; CombatState/CurrentAttackType replicate the result back (H18).
	if (!GetOwner()->HasAuthority())
	{
		ServerStartAttack(AttackType);
		return true;
	}

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

	// Play attack animation montage
	PlayAttackAnimation();

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Started %s attack, wind-up: %.2fs"),
		AttackType == EMOAttackType::Light ? TEXT("light") : TEXT("heavy"),
		Profile->WindUpTime);

	return true;
}

bool UMOCombatComponent::CancelAttack()
{
	if (!GetOwner()->HasAuthority())
	{
		ServerCancelAttack();
		return true;
	}

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
	if (!GetOwner()->HasAuthority())
	{
		ServerStartBlock();
		return true;
	}

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
	if (!GetOwner()->HasAuthority())
	{
		ServerStopBlock();
		return;
	}

	if (CombatState != EMOCombatState::Blocking)
	{
		return;
	}

	SetCombatState(bInCombat ? EMOCombatState::Ready : EMOCombatState::Idle);
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Stopped blocking"));
}

bool UMOCombatComponent::AttemptParry()
{
	if (!GetOwner()->HasAuthority())
	{
		ServerAttemptParry();
		return true;
	}

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
	if (!GetOwner()->HasAuthority())
	{
		ServerStartDodge(Direction);
		return true;
	}

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
// SERVER RPCs + REPLICATION (H18 - client->server transport for combat verbs)
// ============================================================================

void UMOCombatComponent::ServerStartAttack_Implementation(EMOAttackType AttackType)
{
	StartAttack(AttackType);
}

void UMOCombatComponent::ServerCancelAttack_Implementation()
{
	CancelAttack();
}

void UMOCombatComponent::ServerStartBlock_Implementation()
{
	StartBlock();
}

void UMOCombatComponent::ServerStopBlock_Implementation()
{
	StopBlock();
}

void UMOCombatComponent::ServerAttemptParry_Implementation()
{
	AttemptParry();
}

void UMOCombatComponent::ServerStartDodge_Implementation(FVector Direction)
{
	StartDodge(Direction);
}

void UMOCombatComponent::OnRep_CombatState(EMOCombatState OldState)
{
	// Clients mirror the cosmetics the server already broadcast via SetCombatState.
	OnCombatStateChanged.Broadcast(OldState, CombatState);

	// Show the attack montage on remote clients (the authority played it inline).
	if (CombatState == EMOCombatState::WindingUp)
	{
		PlayAttackAnimation();
	}
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
	UMOAnatomyComponent* AnatomyToUse = CachedAnatomyComp;

	// Fallback: try to find anatomy component if not cached
	if (!AnatomyToUse)
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			AnatomyToUse = Owner->FindComponentByClass<UMOAnatomyComponent>();
			if (AnatomyToUse)
			{
				// Cache for future use
				CachedAnatomyComp = AnatomyToUse;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCombat] AnatomyComponent was not cached, found dynamically on %s"),
					*Owner->GetName());
			}
		}
	}

	if (AnatomyToUse)
	{
		UMOCombatMedicalHelpers::ApplyCombatDamage(HitInfo, AnatomyToUse, CachedVitalsComp, CachedMentalComp);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCombat] ReceiveAttack: No AnatomyComponent found, damage not applied!"));
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

	// Perform hit detection via sphere trace
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// For player-controlled pawns, use camera aim direction but start from character position
	// For AI, use traditional hand-based trace
	FVector TraceStart;
	FVector TraceDirection;

	APawn* OwnerPawn = Cast<APawn>(Owner);
	APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;

	if (PC && PC->PlayerCameraManager)
	{
		// Player-controlled: trace from character position in camera aim direction
		// This gives us the "where you're looking" targeting while keeping melee range sensible
		FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
		FVector CameraDirection = PC->PlayerCameraManager->GetCameraRotation().Vector();

		// Start from character's eye/chest height, slightly forward
		FVector CharacterCenter = Owner->GetActorLocation();
		CharacterCenter.Z += 50.f; // Raise to chest height

		// Project forward from character in the camera's aim direction
		TraceStart = CharacterCenter + CameraDirection * 30.f; // Start slightly in front
		TraceDirection = CameraDirection;

		UE_LOG(LogMOFramework, Log, TEXT("[MOCombat] Camera trace: Start=%s Dir=%s Range=%.0f"),
			*TraceStart.ToString(), *TraceDirection.ToString(), Profile->Range);
	}
	else
	{
		// AI-controlled: trace from hand/weapon socket
		TraceStart = GetAttackOrigin();
		TraceDirection = Owner->GetActorForwardVector();
	}

	const FVector TraceEnd = TraceStart + TraceDirection * Profile->Range;

	// Debug visualization (temporary - for debugging)
#if ENABLE_DRAW_DEBUG
	const float DebugDuration = 2.0f;
	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Yellow, false, DebugDuration, 0, 2.0f);
	DrawDebugSphere(GetWorld(), TraceStart, 30.0f, 8, FColor::Green, false, DebugDuration);
	DrawDebugSphere(GetWorld(), TraceEnd, 30.0f, 8, FColor::Red, false, DebugDuration);
#endif

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.bTraceComplex = true; // For accurate bone detection

	// Sphere trace for hits
	const bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ECC_Pawn, // Hit pawns and physics bodies
		FCollisionShape::MakeSphere(30.0f), // Weapon/fist radius
		QueryParams
	);

	if (bHit)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attack hit %d targets"), HitResults.Num());

		// Process each hit - draw blue sphere at impact points
		for (const FHitResult& Hit : HitResults)
		{
#if ENABLE_DRAW_DEBUG
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 20.0f, 8, FColor::Blue, false, DebugDuration);
#endif
			ProcessHit(Hit, *Profile);
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Attack missed - no targets in range"));
	}
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
		// Returned pointers are dereferenced after this call returns, so
		// they must point into the cached member row — a stack-local copy
		// here is a use-after-free. Revalidate by id; load only on change.
		if (CachedMainHandProfileRowId != MainHandWeapon.WeaponProfileId
			&& LoadWeaponProfile(MainHandWeapon.WeaponProfileId, CachedMainHandProfileRow))
		{
			CachedMainHandProfileRowId = MainHandWeapon.WeaponProfileId;
		}
		if (CachedMainHandProfileRowId == MainHandWeapon.WeaponProfileId)
		{
			return CachedMainHandProfileRow.GetAttackProfile(CurrentAttackType);
		}
		// Profile row missing from the table — fall through to unarmed.
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

void UMOCombatComponent::ProcessHit(const FHitResult& Hit, const FMOAttackDamageProfile& Profile)
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Check if hit is within attack arc
	const FVector ToTarget = (Hit.ImpactPoint - Owner->GetActorLocation()).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(Owner->GetActorForwardVector(), ToTarget);
	const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

	if (AngleToTarget > Profile.ArcAngle * 0.5f)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] Hit outside attack arc (%.1f° > %.1f°)"),
			AngleToTarget, Profile.ArcAngle * 0.5f);
		return; // Outside attack arc
	}

	// Determine body part from bone name (skeletal mesh)
	EMOBodyPartType HitBodyPart = EMOBodyPartType::Torso; // Default to torso
	if (Hit.BoneName != NAME_None)
	{
		HitBodyPart = UMOCombatMedicalHelpers::BoneNameToBodyPart(Hit.BoneName);
	}

	// Check for combat component on target
	UMOCombatComponent* TargetCombat = HitActor->FindComponentByClass<UMOCombatComponent>();
	if (TargetCombat)
	{
		// Calculate damage with distance falloff
		const float Distance = FVector::Dist(Owner->GetActorLocation(), Hit.ImpactPoint);
		const float DamageMult = 1.0f - FMath::Clamp(Distance / Profile.Range, 0.0f, 1.0f) * 0.3f;

		FMOCombatHitInfo HitInfo;
		HitInfo.BaseDamage = Profile.BaseDamage * DamageMult;
		HitInfo.DamageCategory = Profile.PrimaryDamageType;
		HitInfo.TargetBodyPart = HitBodyPart;
		HitInfo.ArmorPenetration = Profile.ArmorPenetration;
		HitInfo.bCausesHeavyBleeding = Profile.bCausesHeavyBleeding;

		EMOAttackResult Result = TargetCombat->ReceiveAttack(Owner, HitInfo);
		OnAttackHit.Broadcast(HitActor, Result, HitInfo);
	}
	else
	{
		// Non-combat actor (resources, world objects)
		OnHitNonCombatActor.Broadcast(HitActor, Hit.ImpactPoint, Hit.ImpactNormal, Hit.BoneName);
	}
}

FVector UMOCombatComponent::GetAttackOrigin() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	// If we have an equipment component with a weapon, try to get its socket
	if (CachedEquipmentComp)
	{
		// Try to get weapon actor's "AttackOrigin" socket
		// This would be set up on weapon meshes
		// For now, we fall through to character mesh check
	}

	// Try to get character's right hand socket for unarmed attacks
	if (const ACharacter* CharOwner = Cast<ACharacter>(Owner))
	{
		if (USkeletalMeshComponent* Mesh = CharOwner->GetMesh())
		{
			// Try common hand socket names
			static const FName HandSocketNames[] = {
				FName(TEXT("hand_r")),
				FName(TEXT("Hand_R")),
				FName(TEXT("RightHand")),
				FName(TEXT("hand_rSocket"))
			};

			for (const FName& SocketName : HandSocketNames)
			{
				if (Mesh->DoesSocketExist(SocketName))
				{
					return Mesh->GetSocketLocation(SocketName);
				}
			}
		}
	}

	// Final fallback: actor location + forward offset
	return Owner->GetActorLocation() + Owner->GetActorForwardVector() * 50.0f;
}

void UMOCombatComponent::PlayAttackAnimation()
{
	ACharacter* CharOwner = Cast<ACharacter>(GetOwner());
	if (!CharOwner)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = CharOwner->GetMesh();
	if (!Mesh)
	{
		return;
	}

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	// Select montage based on attack type
	UAnimMontage* MontageToPlay = nullptr;

	// TODO: If weapon is equipped, get weapon's attack montage from item definition
	// For now, use unarmed montages
	switch (CurrentAttackType)
	{
	case EMOAttackType::Light:
		MontageToPlay = UnarmedLightAttackMontage;
		break;

	case EMOAttackType::Heavy:
		MontageToPlay = UnarmedHeavyAttackMontage;
		break;

	default:
		MontageToPlay = UnarmedLightAttackMontage;
		break;
	}

	if (MontageToPlay)
	{
		AnimInstance->Montage_Play(MontageToPlay);
	}
	else
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCombat] No attack montage set for attack type %d"),
			static_cast<int32>(CurrentAttackType));
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
