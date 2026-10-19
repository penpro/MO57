#include "MOCreatureController.h"
#include "MOBlackboardKeys.h"
#include "MOFramework.h"
#include "MOCreatureDefinitionRow.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BrainComponent.h"

AMOCreatureController::AMOCreatureController()
{
	// Enable ticking for continuous blackboard updates
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Create and set perception component (inherited from AAIController)
	UAIPerceptionComponent* PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SetPerceptionComponent(*PerceptionComp);
}

void AMOCreatureController::BeginPlay()
{
	Super::BeginPlay();

	// Note: Perception setup moved to OnPossess to ensure valid listener ID
}

void AMOCreatureController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// The blackboard asset is bound AFTER OnPossess (the behavior tree starts later), and a blackboard component with no asset has no keys, so the initial push that used to
	// run in OnPossess wrote nothing and logged a bogus "key does not exist" error. Push the initial values (home location, activity state, ...) the first tick it exists.
	if (!bBlackboardPrimed && BlackboardComponent && BlackboardComponent->GetBlackboardAsset())
	{
		bBlackboardPrimed = true;
		UpdateBlackboardThreatInfo();
	}

	// Throttle blackboard updates to 5Hz (was every frame)
	ThreatUpdateAccumulator += DeltaTime;
	if (ThreatUpdateAccumulator >= ThreatUpdateInterval)
	{
		ThreatUpdateAccumulator = 0.0f;
		UpdatePursuit();   // may give the chase up (and clear the threat)
		if (CurrentThreatActor.IsValid())
		{
			UpdateBlackboardThreatInfo();
		}
	}
}

void AMOCreatureController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Setup perception after possession (requires valid listener ID)
	SetupPerception();

	// Update home location when possessing
	if (InPawn)
	{
		HomeLocation = InPawn->GetActorLocation();
	}

	// (The initial blackboard push happens in Tick, once the behavior tree has bound the blackboard asset.)
}

void AMOCreatureController::OnUnPossess()
{
	// Clear threat tracking
	CurrentThreatActor.Reset();
	LastKnownThreatLocation = FVector::ZeroVector;

	Super::OnUnPossess();
}

void AMOCreatureController::SetupPerception()
{
	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();
	if (!PerceptionComp)
	{
		return;
	}

	// Create sense configs but don't call ConfigureSense yet - wait for listener registration
	SightConfig = NewObject<UAISenseConfig_Sight>(this, UAISenseConfig_Sight::StaticClass(), TEXT("SightConfig"));
	if (SightConfig)
	{
		SightConfig->SightRadius = DefaultSightRadius;
		SightConfig->LoseSightRadius = DefaultLoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = DefaultPeripheralVisionAngle;
		SightConfig->SetMaxAge(MaxStimulusAge);
		SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;
		SightConfig->DetectionByAffiliation = SenseAllAffiliations();
	}

	HearingConfig = NewObject<UAISenseConfig_Hearing>(this, UAISenseConfig_Hearing::StaticClass(), TEXT("HearingConfig"));
	if (HearingConfig)
	{
		HearingConfig->HearingRange = DefaultHearingRange;
		HearingConfig->SetMaxAge(MaxStimulusAge);
		HearingConfig->DetectionByAffiliation = SenseAllAffiliations();
	}

	// Bind to perception updates
	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMOCreatureController::OnTargetPerceptionUpdated);

	FinalizePerceptionSetup();
}

void AMOCreatureController::FinalizePerceptionSetup()
{
	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();
	if (!PerceptionComp)
	{
		return;
	}

	// A listener gets its id from UAIPerceptionSystem::UpdateListener. The component calls that by itself only in OnRegister, and only when it already HAS
	// senses then -- ours are made at possession, so nothing ever registered it: the old "wait for the id" ran out after 180 frames for every creature, each
	// ConfigureSense then logged "Listener must have a valid id to update its sense config", and the creature was blind until a later settings update
	// happened to register it (Oct 13 2026). Register the listener first, then configure the senses on it.
	if (PerceptionComp->GetListenerId() == FPerceptionListenerID::InvalidID())
	{
		PerceptionComp->RequestStimuliListenerUpdate();
	}

	bPerceptionFinalized = true;
	if (SightConfig)
	{
		PerceptionComp->ConfigureSense(*SightConfig);
	}

	if (HearingConfig)
	{
		PerceptionComp->ConfigureSense(*HearingConfig);
	}

	// Set dominant sense to sight
	PerceptionComp->SetDominantSense(UAISenseConfig_Sight::StaticClass());

	// The listener entry was created before the senses existed: refresh it so the senses' mask and ranges are the configured ones.
	PerceptionComp->RequestStimuliListenerUpdate();

	// Apply any pending perception settings that were requested before perception was ready
	ApplyPendingPerceptionSettings();
}

void AMOCreatureController::ApplyPerceptionSettings(const FMOCreatureDefinitionRow& Definition)
{
	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();

	// Perception is ready once FinalizePerceptionSetup (end of OnPossess) has registered the listener and configured the senses; a definition applied
	// before that (the creature's BeginPlay can run first) is stored and applied there.
	const bool bPerceptionReady = bPerceptionFinalized && SightConfig != nullptr && HearingConfig != nullptr;

	if (!bPerceptionReady)
	{
		// Store settings to apply later when perception is ready
		PendingSightRadius = Definition.SightRadius;
		PendingLoseSightRadius = Definition.LoseSightRadius;
		PendingPeripheralVisionAngle = Definition.PeripheralVisionAngle;
		PendingHearingRange = Definition.HearingRange;

		// Still apply combat settings immediately
		ApplyBehaviorSettings(Definition);
		return;
	}

	if (SightConfig)
	{
		SightConfig->SightRadius = Definition.SightRadius;
		SightConfig->LoseSightRadius = Definition.LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = Definition.PeripheralVisionAngle;

		if (PerceptionComp)
		{
			PerceptionComp->ConfigureSense(*SightConfig);
		}
	}

	if (HearingConfig)
	{
		HearingConfig->HearingRange = Definition.HearingRange;

		if (PerceptionComp)
		{
			PerceptionComp->ConfigureSense(*HearingConfig);
		}
	}

	// Apply combat settings from definition
	ApplyBehaviorSettings(Definition);

	// Request perception system update
	if (PerceptionComp)
	{
		PerceptionComp->RequestStimuliListenerUpdate();
	}
}

void AMOCreatureController::ApplyPendingPerceptionSettings()
{
	// Check if there are pending settings (indicated by positive values)
	if (PendingSightRadius < 0.f)
	{
		return;
	}

	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();

	if (SightConfig)
	{
		SightConfig->SightRadius = PendingSightRadius;
		SightConfig->LoseSightRadius = PendingLoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PendingPeripheralVisionAngle;

		if (PerceptionComp)
		{
			PerceptionComp->ConfigureSense(*SightConfig);
		}
	}

	if (HearingConfig && PendingHearingRange >= 0.f)
	{
		HearingConfig->HearingRange = PendingHearingRange;

		if (PerceptionComp)
		{
			PerceptionComp->ConfigureSense(*HearingConfig);
		}
	}

	// Clear pending flags
	PendingSightRadius = -1.f;
	PendingLoseSightRadius = -1.f;
	PendingPeripheralVisionAngle = -1.f;
	PendingHearingRange = -1.f;

	// Request update now that everything is configured
	if (PerceptionComp)
	{
		PerceptionComp->RequestStimuliListenerUpdate();
	}
}

void AMOCreatureController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// Check if this is a valid threat (another pawn, not same class)
	APawn* OtherPawn = Cast<APawn>(Actor);
	if (!OtherPawn)
	{
		return;
	}

	// Don't target other creatures of the same type
	if (OtherPawn->GetClass() == ControlledPawn->GetClass())
	{
		return;
	}

	// A quarry this pursuer gave up on stays ignored for a while (GiveUpChase); a wound from it ends that (NotifyDamaged).
	if (const double* Until = GivenUpUntil.Find(Actor))
	{
		if (GetWorld()->GetTimeSeconds() < *Until)
		{
			return;
		}
		GivenUpUntil.Remove(Actor);
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		// TARGET STICKINESS: Don't switch targets unless we don't have one or current is invalid
		if (CurrentThreatActor.IsValid() && CurrentThreatActor.Get() != Actor)
		{
			// We already have a valid target - only switch if new target is MUCH closer (50% closer)
			float CurrentDistance = FVector::Dist(ControlledPawn->GetActorLocation(), CurrentThreatActor->GetActorLocation());
			float NewDistance = FVector::Dist(ControlledPawn->GetActorLocation(), Actor->GetActorLocation());

			if (NewDistance >= CurrentDistance * 0.5f)
			{
				// New target isn't significantly closer - keep current target
				// DEBUG (MOCreatureController, disabled Oct 18): re-enable to trace verbose detail
				// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Ignoring %s (dist %.1f), keeping current target %s (dist %.1f)"),
				// 	*Actor->GetName(), NewDistance, *CurrentThreatActor->GetName(), CurrentDistance);
				return;
			}

			// DEBUG (MOCreatureController, disabled Oct 18): re-enable to trace verbose detail
			// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Switching from %s to closer threat %s"),
			// 	*CurrentThreatActor->GetName(), *Actor->GetName());
		}

		// New threat detected or switching to closer threat
		CurrentThreatActor = Actor;
		RememberedThreatActor = Actor;
		LastKnownThreatLocation = Actor->GetActorLocation();

		// Clear any pending "forget" timer since we can see the threat again
		GetWorld()->GetTimerManager().ClearTimer(ThreatMemoryTimerHandle);

		// DEBUG (MOCreatureController, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] THREAT DETECTED: %s at distance %.1f"),
		// 	*Actor->GetName(), FVector::Dist(ControlledPawn->GetActorLocation(), Actor->GetActorLocation()));

		// Update blackboard immediately
		UpdateBlackboardThreatInfo();
	}
	else
	{
		// Lost sight of this actor - but don't forget immediately!
		if (CurrentThreatActor.Get() == Actor)
		{
			// Update last known location
			LastKnownThreatLocation = Actor->GetActorLocation();

			// DEBUG (MOCreatureController, disabled Oct 18): re-enable to trace verbose detail
			// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Lost sight of %s, remembering for %.1fs"),
			// 	*Actor->GetName(), ThreatMemoryDuration);

			// Forget the threat after ThreatMemoryDuration (a wounded animal's flight holds it longer), keeping CurrentThreatActor valid until then.
			float Memory = ThreatMemoryDuration;
			if (IsPanicked())
			{
				Memory = FMath::Max(Memory, static_cast<float>(PanicUntilTime - GetWorld()->GetTimeSeconds()));
			}
			StartThreatForgetTimer(Actor, Memory);
		}
	}
}

void AMOCreatureController::UpdateThreatAssessment()
{
	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();
	if (!PerceptionComp)
	{
		return;
	}

	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetCurrentlyPerceivedActors(UAISense::StaticClass(), PerceivedActors);

	// Find the closest perceived threat
	AActor* ClosestThreat = nullptr;
	float ClosestDistance = MAX_FLT;

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	FVector MyLocation = ControlledPawn->GetActorLocation();

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor)
		{
			continue;
		}

		// Check if this is a valid threat (player character or hostile pawn)
		APawn* OtherPawn = Cast<APawn>(Actor);
		if (!OtherPawn)
		{
			continue;
		}

		// Don't target other creatures of the same type (simplified affiliation)
		if (OtherPawn->GetClass() == ControlledPawn->GetClass())
		{
			continue;
		}

		float Distance = FVector::Dist(MyLocation, Actor->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestThreat = Actor;
		}
	}

	// Update threat tracking
	if (ClosestThreat)
	{
		CurrentThreatActor = ClosestThreat;
		LastKnownThreatLocation = ClosestThreat->GetActorLocation();
	}
	else if (CurrentThreatActor.IsValid())
	{
		// Lost sight of threat but remember last location
		CurrentThreatActor.Reset();
	}

	// Update blackboard
	UpdateBlackboardThreatInfo();
}

void AMOCreatureController::UpdateBlackboardThreatInfo()
{
	if (!BlackboardComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreatureController] UpdateBlackboardThreatInfo - NO BLACKBOARD COMPONENT!"));
		return;
	}
	if (!BlackboardComponent->GetBlackboardAsset())
	{
		return;   // not bound yet: no keys exist to set (Tick primes the blackboard the first frame it is)
	}

	bool bHasTarget = CurrentThreatActor.IsValid();
	float DistanceToTarget = GetDistanceToThreat();
	bool bIsInAttackRange = bHasTarget && (DistanceToTarget <= AttackRange);

	// Update threat-related keys
	BlackboardComponent->SetValueAsObject(MOBlackboardKeys::TargetActor, CurrentThreatActor.Get());
	BlackboardComponent->SetValueAsVector(MOBlackboardKeys::TargetLocation, LastKnownThreatLocation);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::HasTarget, bHasTarget);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::ShouldFlee, ShouldFlee());
	BlackboardComponent->SetValueAsFloat(MOBlackboardKeys::HealthPercent, GetHealthPercent());
	BlackboardComponent->SetValueAsFloat(MOBlackboardKeys::DistanceToTarget, DistanceToTarget);

	// Check if IsInAttackRange key exists before setting
	if (BlackboardComponent->GetKeyID(MOBlackboardKeys::IsInAttackRange) == FBlackboard::InvalidKey)
	{
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOCreatureController] BLACKBOARD KEY 'IsInAttackRange' DOES NOT EXIST in blackboard '%s' (pawn '%s', controller '%s')! Add a Bool key to that blackboard, or the controller is running the wrong one."),
				*GetNameSafe(BlackboardComponent->GetBlackboardAsset()), *GetNameSafe(GetPawn()), *GetNameSafe(this));
			bLoggedOnce = true;
		}
	}
	else
	{
		BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsInAttackRange, bIsInAttackRange);
	}

	BlackboardComponent->SetValueAsVector(MOBlackboardKeys::HomeLocation, HomeLocation);

	// Update activity state keys
	BlackboardComponent->SetValueAsEnum(MOBlackboardKeys::ActivityState, static_cast<uint8>(CurrentActivityState));
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsDead, CurrentActivityState == EMOCreatureActivityState::Dead);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsResting, CurrentActivityState == EMOCreatureActivityState::Resting);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsSleeping, CurrentActivityState == EMOCreatureActivityState::Sleeping);

}

AActor* AMOCreatureController::GetCurrentThreat() const
{
	return CurrentThreatActor.Get();
}

FVector AMOCreatureController::GetLastKnownThreatLocation() const
{
	return LastKnownThreatLocation;
}

float AMOCreatureController::GetHealthPercent() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return 1.f;
	}

	// Try to get vitals component for blood volume-based health
	if (UMOVitalsComponent* Vitals = ControlledPawn->FindComponentByClass<UMOVitalsComponent>())
	{
		return FMath::Clamp(Vitals->GetBloodVolumePercent(), 0.f, 1.f);
	}

	// Fallback to anatomy component if available
	if (UMOAnatomyComponent* Anatomy = ControlledPawn->FindComponentByClass<UMOAnatomyComponent>())
	{
		// Use overall health estimate from anatomy (could be based on critical parts)
		// For now, return 1.0 as placeholder - anatomy doesn't have a simple "overall health"
		return 1.f;
	}

	return 1.f;
}

bool AMOCreatureController::ShouldFlee() const
{
	return GetHealthPercent() <= FleeHealthThreshold;
}

bool AMOCreatureController::IsInCombat() const
{
	return CurrentThreatActor.IsValid();
}

float AMOCreatureController::GetDistanceToThreat() const
{
	if (!CurrentThreatActor.IsValid())
	{
		return MAX_FLT;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return MAX_FLT;
	}

	return FVector::Dist(ControlledPawn->GetActorLocation(), CurrentThreatActor->GetActorLocation());
}

bool AMOCreatureController::IsInAttackRange() const
{
	return GetDistanceToThreat() <= AttackRange;
}

void AMOCreatureController::SetActivityState(EMOCreatureActivityState NewState)
{
	if (CurrentActivityState == NewState)
	{
		return;
	}

	// DEBUG (MOCreatureController, disabled Oct 18): re-enable to trace verbose detail (with the log below)
	// EMOCreatureActivityState OldState = CurrentActivityState;
	CurrentActivityState = NewState;

	// DEBUG (MOCreatureController, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Activity state: %d -> %d"),
	// 	static_cast<int32>(OldState), static_cast<int32>(NewState));

	// Update blackboard with new state
	if (BlackboardComponent)
	{
		BlackboardComponent->SetValueAsEnum(MOBlackboardKeys::ActivityState, static_cast<uint8>(CurrentActivityState));
		BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsDead, CurrentActivityState == EMOCreatureActivityState::Dead);
		BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsResting, CurrentActivityState == EMOCreatureActivityState::Resting);
		BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsSleeping, CurrentActivityState == EMOCreatureActivityState::Sleeping);
	}
}

void AMOCreatureController::SetDead()
{
	SetActivityState(EMOCreatureActivityState::Dead);

	// Stop all movement
	StopMovement();

	// Stop behavior tree
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Creature died"));
	}
}

void AMOCreatureController::StartThreatForgetTimer(AActor* Threat, float Seconds)
{
	const TWeakObjectPtr<AActor> WeakThreat = Threat;
	GetWorld()->GetTimerManager().SetTimer(ThreatMemoryTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, WeakThreat]()
	{
		// Only clear if this is still the remembered threat, and not while a sense still has it (a lost-sight event restarts the memory then).
		if (RememberedThreatActor != WeakThreat)
		{
			return;
		}
		TArray<AActor*> Perceived;
		if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
		{
			PerceptionComp->GetCurrentlyPerceivedActors(nullptr, Perceived);
		}
		if (WeakThreat.IsValid() && Perceived.Contains(WeakThreat.Get()))
		{
			return;
		}
		// DEBUG (MOCreatureController, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Threat forgotten: %s"), *GetNameSafe(WeakThreat.Get()));
		CurrentThreatActor.Reset();
		RememberedThreatActor.Reset();
		UpdateBlackboardThreatInfo();
	}), FMath::Max(0.1f, Seconds), false);
}

void AMOCreatureController::ApplyBehaviorSettings(const FMOCreatureDefinitionRow& Definition)
{
	FleeHealthThreshold = Definition.FleeThreshold;
	AttackRange = Definition.AttackRange;
	bPursuesThreats = Definition.bIsPredator;
	WoundedPanicSeconds = Definition.WoundedPanicSeconds;
	ChaseLeashRadius = Definition.ChaseLeashRadius;
	ChaseNoGainSeconds = Definition.ChaseNoGainSeconds;
	ChaseGiveUpCooldownSeconds = Definition.ChaseGiveUpCooldownSeconds;
}

bool AMOCreatureController::IsPanicked() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < PanicUntilTime;
}

void AMOCreatureController::NotifyDamaged(AActor* DamageInstigator)
{
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World || !DamageInstigator || DamageInstigator == ControlledPawn || WoundedPanicSeconds <= 0.0f || IsDead())
	{
		return;
	}
	GivenUpUntil.Remove(DamageInstigator);   // it hurt us: no longer ignored
	PanicUntilTime = World->GetTimeSeconds() + WoundedPanicSeconds;
	CurrentThreatActor = DamageInstigator;
	RememberedThreatActor = DamageInstigator;
	LastKnownThreatLocation = DamageInstigator->GetActorLocation();
	StartThreatForgetTimer(DamageInstigator, WoundedPanicSeconds);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCreatureController] %s hurt by %s: %s it for %.0f s"), *ControlledPawn->GetName(), *DamageInstigator->GetName(),
		bPursuesThreats ? TEXT("turns on") : TEXT("flees"), WoundedPanicSeconds);
	UpdateBlackboardThreatInfo();
}

FAISenseAffiliationFilter AMOCreatureController::SenseAllAffiliations()
{
	// Friendlies too -- for hearing that is what makes it work at all: the hearing sense compares raw team ids, the engine's default solver
	// calls NoTeam vs NoTeam FRIENDLY, and every MO57 pawn and controller is NoTeam, so with friendlies off no creature ever heard a footstep
	// (a wolf or a sprinting player came up behind a deer unheard; Oct 18 2026). Sight checks attitude per actor (a pawn without a team
	// agent is Neutral) and always worked.
	FAISenseAffiliationFilter Filter;
	Filter.bDetectEnemies = true;
	Filter.bDetectNeutrals = true;
	Filter.bDetectFriendlies = true;
	return Filter;
}

const TCHAR* AMOCreatureController::JudgeChase(float DistanceToQuarry, float DistanceFromHome, double Now, float AttackReach, float LeashRadius,
	float NoGainSeconds, float& InOutMarkDistance, double& InOutLastGainTime)
{
	static constexpr float ChaseGainStepCm = 200.f;   // closing in by this much since the last mark is progress
	if (DistanceToQuarry <= AttackReach || DistanceToQuarry < InOutMarkDistance - ChaseGainStepCm)
	{
		InOutMarkDistance = DistanceToQuarry;
		InOutLastGainTime = Now;
	}
	if (LeashRadius > 0.0f && DistanceFromHome > LeashRadius)
	{
		return TEXT("too far from home");
	}
	if (NoGainSeconds > 0.0f && Now - InOutLastGainTime > NoGainSeconds)
	{
		return TEXT("not gaining on it");
	}
	return nullptr;
}

void AMOCreatureController::UpdatePursuit()
{
	APawn* ControlledPawn = GetPawn();
	AActor* Quarry = CurrentThreatActor.Get();
	UWorld* World = GetWorld();
	if (!bPursuesThreats || !ControlledPawn || !Quarry || !World || IsDead())
	{
		ChaseQuarry.Reset();
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), Quarry->GetActorLocation());
	if (ChaseQuarry.Get() != Quarry)
	{
		ChaseQuarry = Quarry;   // a new chase starts here
		ChaseBestDistance = Distance;
		ChaseLastGainTime = Now;
	}
	if (const TCHAR* Reason = JudgeChase(Distance, FVector::Dist2D(ControlledPawn->GetActorLocation(), HomeLocation), Now, AttackRange * 1.5f,
		ChaseLeashRadius, ChaseNoGainSeconds, ChaseBestDistance, ChaseLastGainTime))
	{
		GiveUpChase(Reason);
	}
}

void AMOCreatureController::GiveUpChase(const TCHAR* Reason)
{
	AActor* Quarry = CurrentThreatActor.Get();
	UWorld* World = GetWorld();
	if (!Quarry || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (auto It = GivenUpUntil.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() <= Now)
		{
			It.RemoveCurrent();
		}
	}
	GivenUpUntil.Add(Quarry, Now + ChaseGiveUpCooldownSeconds);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCreatureController] %s gives up chasing %s (%s; %.0f m from home)"), *GetNameSafe(GetPawn()), *Quarry->GetName(), Reason,
		GetPawn() ? FVector::Dist2D(GetPawn()->GetActorLocation(), HomeLocation) / 100.0f : 0.0f);
	World->GetTimerManager().ClearTimer(ThreatMemoryTimerHandle);
	CurrentThreatActor.Reset();
	RememberedThreatActor.Reset();
	ChaseQuarry.Reset();
	PanicUntilTime = -1.0;
	UpdateBlackboardThreatInfo();
}
