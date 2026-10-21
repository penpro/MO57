#include "MOCreature.h"
#include "MOFramework.h"
#include "MOCreatureController.h"
#include "MOCreatureDefinitionRow.h"
#include "MOCreatureTypes.h"
#include "MOCarcassActor.h"
#include "MOCarcassDefinitionRow.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "MOInventoryComponent.h"
#include "MORecruitmentComponent.h"
#include "MOWorldItem.h"
#include "Components/CapsuleComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationInvokerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOFootstepComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

AMOCreature::AMOCreature(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Wildlife wades (the movement component slows it in water) but does not swim yet: swimming animals are their own unit (animations, AI that chooses to cross water).
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->NavAgentProps.bCanSwim = false;
	}

	// No foot bones in the animal skeletons: footfalls come from the ground covered (N-132). A padded paw or a light hoof, softer than a person.
	if (FootstepComponent)
	{
		FootstepComponent->BoneFreeStepLengthCm = 70.0f;
		FootstepComponent->StepVolumeScale = 0.5f;
	}

	// Set default AI controller class
	AIControllerClass = AMOCreatureController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Note: NavigationInvoker is inherited from MOCharacter
}

void AMOCreature::BeginPlay()
{
	Super::BeginPlay();

	// Creatures are not recruitable by default (use bIsRecruitable for future animal taming)
	if (UMORecruitmentComponent* RecruitComp = FindComponentByClass<UMORecruitmentComponent>())
	{
		RecruitComp->bIsRecruitable = false;
	}

	// DEBUG (MOCreature, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreature] %s BeginPlay"), *GetName());

	// Validate controller (Error only if missing). AI controllers exist ONLY on the authority: on a client (co-op joiner) every
	// replicated creature legitimately has none, and this used to log an Error per creature on every client.
	if (HasAuthority() && !GetController())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: NO CONTROLLER! AutoPossessAI may have failed."), *GetName());
	}

	// Validate navigation (Error only if missing)
	if (!FindComponentByClass<UNavigationInvokerComponent>())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: NavigationInvoker is NULL!"), *GetName());
	}

	// Apply creature definition settings
	ApplyCreatureDefinition();

	if (!HasAuthority())
	{
		TArray<FSoftObjectPath> ClientAssets;
		GatherClientAssets(ClientAssets);
		if (ClientAssets.Num() > 0)
		{
			// High priority: ahead of the world streaming in, which is what the replicated reference would otherwise wait behind
			ClientAssetsLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(ClientAssets, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s: loading %d asset(s) a cue or its carcass can name"), *GetName(), ClientAssets.Num());
	}

	// Note: OnInstantDeath binding handled by AMOCharacter base class
	// We override HandleInstantDeath to add creature-specific death behavior
	if (UMOAnatomyComponent* Anatomy = GetAnatomyComponent())
	{
		Anatomy->OnBodyPartDestroyed.RemoveDynamic(this, &AMOCreature::HandleBodyPartDestroyed);
		Anatomy->OnBodyPartDestroyed.AddDynamic(this, &AMOCreature::HandleBodyPartDestroyed);
		Anatomy->OnBodyPartDamaged.RemoveDynamic(this, &AMOCreature::HandleBodyPartDamaged);
		Anatomy->OnBodyPartDamaged.AddDynamic(this, &AMOCreature::HandleBodyPartDamaged);
	}
}

void AMOCreature::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DestroyTimerHandle);
	}

	// Note: OnInstantDeath unbinding handled by AMOCharacter base class
	if (UMOAnatomyComponent* Anatomy = GetAnatomyComponent())
	{
		Anatomy->OnBodyPartDestroyed.RemoveDynamic(this, &AMOCreature::HandleBodyPartDestroyed);
		Anatomy->OnBodyPartDamaged.RemoveDynamic(this, &AMOCreature::HandleBodyPartDamaged);
	}

	Super::EndPlay(EndPlayReason);
}

const FMOCreatureDefinitionRow* AMOCreature::GetCreatureDefinitionRow() const
{
	if (!CreatureDefinition.DataTable || CreatureDefinition.RowName.IsNone())
	{
		return nullptr;
	}

	return CreatureDefinition.DataTable->FindRow<FMOCreatureDefinitionRow>(
		CreatureDefinition.RowName, TEXT("GetCreatureDefinitionRow"));
}

void AMOCreature::GatherRuntimeAssets(TArray<FSoftObjectPath>& Out) const
{
	Super::GatherRuntimeAssets(Out);
	if (CreatureControllerClass)
	{
		GetDefault<AMOCreatureController>(CreatureControllerClass.Get())->GatherRuntimeAssets(Out);
	}
	if (!BehaviorTreeOverride.IsNull())
	{
		Out.AddUnique(BehaviorTreeOverride.ToSoftObjectPath());
	}
	if (const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow())
	{
		if (!Definition->BehaviorTree.IsNull())
		{
			Out.AddUnique(Definition->BehaviorTree.ToSoftObjectPath());
		}
	}
	GatherClientAssets(Out);
}

void AMOCreature::GatherClientAssets(TArray<FSoftObjectPath>& Out) const
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		return;
	}
	if (!Definition->CarcassDataTable.IsNull())
	{
		Out.AddUnique(Definition->CarcassDataTable.ToSoftObjectPath());
	}
	for (const TSoftObjectPtr<UAnimSequenceBase>* HitReact : { &Definition->HitReactFront, &Definition->HitReactLeft, &Definition->HitReactRight, &Definition->HitReactBack })
	{
		if (!HitReact->IsNull())
		{
			Out.AddUnique(HitReact->ToSoftObjectPath());
		}
	}
}

void AMOCreature::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

AMOCreatureController* AMOCreature::GetCreatureController() const
{
	return Cast<AMOCreatureController>(GetController());
}

float AMOCreature::GetHealthPercent() const
{
	if (UMOVitalsComponent* Vitals = GetVitalsComponent())
	{
		return FMath::Clamp(Vitals->GetBloodVolumePercent(), 0.f, 1.f);
	}
	return 1.f;
}

void AMOCreature::ApplyCreatureDefinition()
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] %s: No creature definition"), *GetName());
		return;
	}

	// Apply movement speeds
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Definition->WalkSpeed;
	}

	// Store run speed for sprint logic
	SprintSpeed = Definition->RunSpeed;
	WalkSpeed = Definition->WalkSpeed;

	// This species' footfalls (0 keeps the constructor's defaults)
	if (FootstepComponent)
	{
		if (Definition->FootstepStrideCm > 0.0f)
		{
			FootstepComponent->BoneFreeStepLengthCm = Definition->FootstepStrideCm;
		}
		if (Definition->FootstepVolume > 0.0f)
		{
			FootstepComponent->StepVolumeScale = Definition->FootstepVolume;
		}
	}

	// Check if this creature has a carcass definition
	bSpawnCarcassOnDeath = !Definition->CarcassDefinitionId.IsNone();

	// Apply perception and combat settings to controller
	if (AMOCreatureController* CreatureAI = GetCreatureController())
	{
		CreatureAI->ApplyPerceptionSettings(*Definition);

		// Start behavior tree
		UBehaviorTree* BTToRun = nullptr;

		// Check for override first
		if (!BehaviorTreeOverride.IsNull())
		{
			BTToRun = BehaviorTreeOverride.LoadSynchronous();
		}
		// Then try definition
		else if (!Definition->BehaviorTree.IsNull())
		{
			BTToRun = Definition->BehaviorTree.LoadSynchronous();
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: NO BEHAVIOR TREE SET!"), *GetName());
		}

		if (BTToRun)
		{
			bool bSuccess = CreatureAI->RunBehaviorTree(BTToRun);
			if (!bSuccess)
			{
				UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: RunBehaviorTree(%s) FAILED"),
					*GetName(), *BTToRun->GetName());
			}
		}
	}
	else if (HasAuthority()) // clients have no AI controller by design (see BeginPlay)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: GetCreatureController() returned NULL!"), *GetName());
	}

	// DEBUG (MOCreature, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreature] %s: Applied definition '%s'"),
	// 	*GetName(), *Definition->DisplayName.ToString());
}

void AMOCreature::HandleInstantDeath(EMOBodyPartType CausePart)
{
	if (bIsDead)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s died from instant death (part: %d)"),
		*GetName(), static_cast<int32>(CausePart));

	OnDeath(GetLastDamageInstigator());   // whoever wounded it last (a bleed-out too)
}

void AMOCreature::HandleBodyPartDestroyed(EMOBodyPartType Part, bool bInstantDeath)
{
	if (bIsDead)
	{
		return;
	}

	// If instant death already handled, skip
	if (bInstantDeath)
	{
		return;
	}

	// For creatures, destruction of vital body parts causes death
	// This is simpler than the full human medical simulation
	if (IsVitalPart(Part))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s died from vital part destruction (part: %d)"),
			*GetName(), static_cast<int32>(Part));
		OnDeath(GetLastDamageInstigator());
	}
}

void AMOCreature::HandleBodyPartDamaged(EMOBodyPartType Part, float Damage, float NewHP)
{
	if (bIsDead || !HasAuthority())
	{
		return;
	}
	if (AMOCreatureController* CreatureAI = GetCreatureController())
	{
		CreatureAI->NotifyDamaged(GetLastDamageInstigator());
	}
	PlayHitReactionFrom(GetLastDamageInstigator());
}

bool AMOCreature::IsVitalPart(EMOBodyPartType Part)
{
	switch (Part)
	{
	case EMOBodyPartType::Head:
	case EMOBodyPartType::Torso:
	case EMOBodyPartType::Heart:
	case EMOBodyPartType::Brain:
	case EMOBodyPartType::SpineCervical:
	case EMOBodyPartType::SpineThoracic:
	case EMOBodyPartType::Liver:
	case EMOBodyPartType::Stomach:
		return true;
	default:
		return false;
	}
}

AActor* AMOCreature::GetLastDamageInstigator() const
{
	const UMOAnatomyComponent* Anatomy = GetAnatomyComponent();
	return Anatomy ? Anatomy->GetLastDamageInstigator() : nullptr;
}

EMOCreatureHitSide AMOCreature::ClassifyHitSide(const FVector& CreatureForward, const FVector& ToAttacker)
{
	const FVector Forward = CreatureForward.GetSafeNormal2D();
	const FVector To = ToAttacker.GetSafeNormal2D();
	if (Forward.IsNearlyZero() || To.IsNearlyZero())
	{
		return EMOCreatureHitSide::Front;   // directly above or below: no side to tell
	}
	const float Dot = FVector::DotProduct(Forward, To);
	if (Dot >= UE_HALF_SQRT_2)
	{
		return EMOCreatureHitSide::Front;
	}
	if (Dot <= -UE_HALF_SQRT_2)
	{
		return EMOCreatureHitSide::Back;
	}
	// +Y is the right of a pawn facing +X: forward x (to the attacker) points up when the attacker is on its right
	return FVector::CrossProduct(Forward, To).Z > 0.0f ? EMOCreatureHitSide::Right : EMOCreatureHitSide::Left;
}

float AMOCreature::NearestFlameDistanceCm(const UWorld* World, const FVector& Location)
{
	const UMOAmbientEnvironmentRegistry* Registry = World ? World->GetSubsystem<UMOAmbientEnvironmentRegistry>() : nullptr;
	float Nearest = TNumericLimits<float>::Max();
	if (Registry)
	{
		for (const IMOLocalLightSource* Flame : Registry->GetLightSources())
		{
			if (Flame && Flame->GetLuminousIntensityCd() > 0.0f)
			{
				Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist(Flame->GetLightLocation(), Location)));
			}
		}
	}
	return Nearest;
}

bool AMOCreature::IsWaryOfFireAt(const FVector& Location) const
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	return Definition && Definition->FireFearRadiusCm > 0.0f && NearestFlameDistanceCm(GetWorld(), Location) <= Definition->FireFearRadiusCm;
}

void AMOCreature::PlayHitReactionFrom(const AActor* Attacker)
{
	if (bIsDead || !HasAuthority() || !IsValid(Attacker))
	{
		return;   // no attacker = the environment (a fall, the cold): nothing to flinch from
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastHitReactionTime < 0.3f)
	{
		return;   // one blow that damaged several parts
	}
	const USkeletalMeshComponent* MeshComp = GetMesh();
	const UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance || AnimInstance->IsAnyMontagePlaying())
	{
		return;   // mid-bite (its damage lands on the attack task's own timer, so cutting the animation would only hide the bite) or mid-flinch
	}
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		return;
	}

	const EMOCreatureHitSide Side = ClassifyHitSide(GetActorForwardVector(), Attacker->GetActorLocation() - GetActorLocation());
	const TSoftObjectPtr<UAnimSequenceBase>* Pick = &Definition->HitReactFront;
	switch (Side)
	{
	case EMOCreatureHitSide::Back:  Pick = &Definition->HitReactBack;  break;
	case EMOCreatureHitSide::Left:  Pick = &Definition->HitReactLeft;  break;
	case EMOCreatureHitSide::Right: Pick = &Definition->HitReactRight; break;
	default: break;
	}
	const TSoftObjectPtr<UAnimSequenceBase>& Chosen = Pick->IsNull() ? Definition->HitReactFront : *Pick;
	if (Chosen.IsNull())
	{
		return;   // this species has no hit reaction
	}
	UAnimSequenceBase* Animation = Chosen.Get();
	if (!Animation)
	{
		Animation = Chosen.LoadSynchronous();   // GatherRuntimeAssets preloads them with the creature; this is the fallback
	}
	LastHitReactionTime = Now;
	const float Played = PlayAnimationEverywhere(Animation);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s hit from the %s by %s: %s (%.2f s)"), *GetName(), *UEnum::GetDisplayValueAsText(Side).ToString(),
		*GetNameSafe(Attacker), *GetNameSafe(Animation), Played);
}

float AMOCreature::PlayAnimationEverywhere(UAnimSequenceBase* Animation, float PlayRate)
{
	if (!HasAuthority())
	{
		return 0.0f;
	}
	AnimCue.Animation = Animation;
	AnimCue.PlayRate = PlayRate;
	AnimCue.bStop = false;
	++AnimCue.Serial;
	MulticastAnimCue(FSoftObjectPath(Animation), PlayRate, AnimCue.Serial, false);   // the clients, each cue in order
	return PlayAnimCueLocally(AnimCue);
}

void AMOCreature::StopAnimationEverywhere(UAnimSequenceBase* Animation)
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	const UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	const UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	if (!HasAuthority() || !AnimInstance || !Montage || !AnimInstance->Montage_IsPlaying(Montage))
	{
		return;   // not playing here: nothing to stop anywhere
	}
	AnimCue.Animation = Animation;
	AnimCue.PlayRate = 1.0f;
	AnimCue.bStop = true;
	++AnimCue.Serial;
	MulticastAnimCue(FSoftObjectPath(Animation), 1.0f, AnimCue.Serial, true);
	PlayAnimCueLocally(AnimCue);
}

void AMOCreature::MulticastAnimCue_Implementation(const FSoftObjectPath& Animation, float PlayRate, int32 Serial, bool bStop)
{
	if (HasAuthority())
	{
		return;   // the server played it in PlayAnimationEverywhere / StopAnimationEverywhere
	}
	LastCueSerial = Serial;
	PlayArrivedCue(Animation, PlayRate, Serial, bStop, false);
}

void AMOCreature::PlayArrivedCue(const FSoftObjectPath& Animation, float PlayRate, int32 Serial, bool bStop, bool bAfterLoad)
{
	if (bAfterLoad && Serial != LastCueSerial)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s: anim cue %d loaded after cue %d arrived, skipped: %s"), *GetName(), Serial, LastCueSerial,
			*Animation.GetAssetName());
		return;
	}
	UAnimSequenceBase* Loaded = Cast<UAnimSequenceBase>(Animation.ResolveObject());
	if (!Loaded && !Animation.IsNull())
	{
		if (bStop)
		{
			return;   // never loaded here, so not playing
		}
		if (!bAfterLoad)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s: anim cue %d: loading %s first"), *GetName(), Serial, *Animation.GetAssetName());
			UAssetManager::GetStreamableManager().RequestAsyncLoad(Animation, FStreamableDelegate::CreateWeakLambda(this, [this, Animation, PlayRate, Serial]()
			{
				PlayArrivedCue(Animation, PlayRate, Serial, false, true);
			}), FStreamableManager::AsyncLoadHighPriority);
			return;
		}
	}
	FMOCreatureAnimCue Cue;
	Cue.Animation = Loaded;   // null (no path) stops every montage
	Cue.PlayRate = PlayRate;
	Cue.Serial = static_cast<uint8>(Serial);
	Cue.bStop = bStop;
	const float Played = PlayAnimCueLocally(Cue);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s: anim cue %d on this client: %s%s (%.2f s)"), *GetName(), Serial, bStop ? TEXT("stop ") : TEXT(""),
		Animation.IsNull() ? TEXT("None") : *Animation.GetAssetName(), Played);
}

float AMOCreature::PlayAnimCueLocally(const FMOCreatureAnimCue& Cue)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance || bIsDead)
	{
		return 0.0f;
	}
	if (!Cue.Animation)
	{
		AnimInstance->Montage_Stop(0.2f);   // every active montage
		return 0.0f;
	}
	if (UAnimMontage* Montage = Cast<UAnimMontage>(Cue.Animation))
	{
		if (Cue.bStop)
		{
			AnimInstance->Montage_Stop(0.2f, Montage);   // this one only
			return 0.0f;
		}
		return AnimInstance->Montage_Play(Montage, Cue.PlayRate);
	}
	if (Cue.bStop)
	{
		return 0.0f;   // a sequence plays as a dynamic montage that ends on its own
	}
	const UAnimMontage* Dynamic = AnimInstance->PlaySlotAnimationAsDynamicMontage(Cue.Animation, TEXT("DefaultSlot"), 0.1f, 0.2f, Cue.PlayRate);
	return Dynamic ? Dynamic->GetPlayLength() / FMath::Max(Cue.PlayRate, UE_KINDA_SMALL_NUMBER) : 0.0f;
}

void AMOCreature::OnDeath_Implementation(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	KillerActor = Killer;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s died, killer: %s"),
		*GetName(), Killer ? *Killer->GetName() : TEXT("None"));

	// Set activity state to Dead (updates blackboard for ABP, stops AI)
	if (AMOCreatureController* CreatureAI = GetCreatureController())
	{
		CreatureAI->SetDead();
	}

	// Disable capsule collision (ragdoll mesh will have its own)
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
	}

	// Enable ragdoll physics
	EnableRagdoll();

	// Broadcast death event
	OnCreatureDeath.Broadcast(this, Killer);

	// Spawn carcass actor (replaces old loot system)
	// Delay carcass spawn to allow ragdoll to settle
	if (bSpawnCarcassOnDeath)
	{
		GetWorld()->GetTimerManager().SetTimer(
			CarcassSpawnTimerHandle,
			this,
			&AMOCreature::SpawnCarcassDelayed,
			RagdollToCarassDelay,
			false
		);
	}
	else
	{
		// No carcass - use legacy loot system
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] No carcass definition, using legacy loot system"));

		// Spawn loot (legacy)
		SpawnLoot();

		// Freeze ragdoll after it settles (same delay as carcass spawn)
		// This keeps the body interactable with proper collision until destruction
		GetWorld()->GetTimerManager().SetTimer(
			RagdollFreezeTimerHandle,
			this,
			&AMOCreature::FreezeRagdoll,
			RagdollToCarassDelay,
			false
		);

		// Schedule destruction after ragdoll settles
		if (DestroyDelayAfterDeath > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				DestroyTimerHandle,
				this,
				&AMOCreature::PerformDestroy,
				DestroyDelayAfterDeath,
				false
			);
		}
		else
		{
			// At minimum, wait for ragdoll to settle before destroying
			SetLifeSpan(RagdollToCarassDelay + 1.0f);
		}
	}
}

void AMOCreature::EnableRagdoll()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// Stop any playing animation montages
	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.0f);
	}

	// Pause animation blueprint updates so it doesn't fight with physics
	// This prevents breathing/idle animations from playing during ragdoll
	MeshComp->bPauseAnims = true;

	// Enable physics simulation on the mesh (ragdoll)
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComp->WakeAllRigidBodies();

	// Apply a small impulse in the direction of last hit (if we have one)
	// This makes the ragdoll fall more naturally
	if (KillerActor.IsValid())
	{
		FVector ImpulseDir = GetActorLocation() - KillerActor.Get()->GetActorLocation();
		ImpulseDir.Z = 0.3f; // Slight upward component
		ImpulseDir.Normalize();
		MeshComp->AddImpulse(ImpulseDir * RagdollImpulseStrength, NAME_None, true);
	}

	// DEBUG (MOCreature, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreature] %s ragdoll enabled"), *GetName());
}

void AMOCreature::SpawnCarcassDelayed()
{
	AMOCarcassActor* Carcass = SpawnCarcass();

	if (Carcass)
	{
		// Position carcass at ragdoll location
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			// Get the pelvis/root bone location as the center of the ragdoll
			FVector RagdollCenter = MeshComp->GetComponentLocation();
			Carcass->SetActorLocation(RagdollCenter);
		}

		// Destroy creature actor now that carcass is spawned
		Destroy();
	}
	else
	{
		// No carcass definition - continue with legacy system
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] No carcass definition, using legacy loot system"));
		SpawnLoot();

		// Freeze the ragdoll immediately since we're already past the delay
		FreezeRagdoll();

		// Schedule final destruction
		SetLifeSpan(DestroyDelayAfterDeath > 0.f ? DestroyDelayAfterDeath : 30.0f);
	}
}

void AMOCreature::SpawnLoot()
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		return;
	}

	FVector SpawnLocation = GetActorLocation();

	for (const FMOLootEntry& LootEntry : Definition->LootEntries)
	{
		// Check drop chance
		if (FMath::FRand() > LootEntry.DropChance)
		{
			continue;
		}

		// Calculate quantity
		int32 Quantity = FMath::RandRange(LootEntry.MinQuantity, LootEntry.MaxQuantity);
		if (Quantity <= 0)
		{
			continue;
		}

		// Spawn world item
		// Note: This assumes AMOWorldItem has a static spawn method or we need to spawn and configure
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// Offset spawn location slightly to spread items
		FVector Offset = FMath::VRand() * 50.f;
		Offset.Z = 20.f; // Slight upward offset

		// For now, log what would be spawned - actual spawn requires WorldItem class setup
		UE_LOG(LogMOFramework, Log, TEXT("AMOCreature: Would spawn loot - %s x%d at %s"),
			*LootEntry.ItemDefinitionId.ToString(), Quantity, *(SpawnLocation + Offset).ToString());

		// TODO: Implement actual item spawning when MOWorldItem is available
		// AMOWorldItem* WorldItem = GetWorld()->SpawnActor<AMOWorldItem>(WorldItemClass, SpawnLocation + Offset, FRotator::ZeroRotator, SpawnParams);
		// if (WorldItem)
		// {
		//     WorldItem->SetItemDefinitionAndQuantity(LootEntry.ItemDefinitionId, Quantity);
		// }
	}
}

AMOCarcassActor* AMOCreature::SpawnCarcass()
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] SpawnCarcass: No creature definition"));
		return nullptr;
	}

	// Check if carcass definition is set
	if (Definition->CarcassDefinitionId.IsNone())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] SpawnCarcass: No carcass definition ID for %s"), *GetName());
		return nullptr;
	}

	// Load carcass DataTable
	UDataTable* CarcassTable = Definition->CarcassDataTable.LoadSynchronous();
	if (!CarcassTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] SpawnCarcass: Could not load carcass DataTable"));
		return nullptr;
	}

	// Verify the carcass definition exists
	const FMOCarcassDefinitionRow* CarcassDef = CarcassTable->FindRow<FMOCarcassDefinitionRow>(
		Definition->CarcassDefinitionId, TEXT("SpawnCarcass"));
	if (!CarcassDef)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] SpawnCarcass: Carcass definition '%s' not found in DataTable"),
			*Definition->CarcassDefinitionId.ToString());
		return nullptr;
	}

	// Spawn the carcass actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AMOCarcassActor* Carcass = GetWorld()->SpawnActor<AMOCarcassActor>(
		AMOCarcassActor::StaticClass(),
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams
	);

	if (Carcass)
	{
		// Configure carcass definition
		Carcass->CarcassDefinition.DataTable = CarcassTable;
		Carcass->CarcassDefinition.RowName = Definition->CarcassDefinitionId;

		// Initialize from this creature (copy mesh, etc.)
		Carcass->InitializeFromCreature(this);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] Spawned carcass '%s' for %s"),
			*CarcassDef->DisplayName.ToString(), *GetName());
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] Failed to spawn carcass actor for %s"), *GetName());
	}

	return Carcass;
}

void AMOCreature::PerformDestroy()
{
	Destroy();
}
