#include "MOCreatureController.h"
#include "MOFramework.h"
#include "MOCreatureDefinitionRow.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"

AMOCreatureController::AMOCreatureController()
{
	// Create and set perception component (inherited from AAIController)
	UAIPerceptionComponent* PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SetPerceptionComponent(*PerceptionComp);
}

void AMOCreatureController::BeginPlay()
{
	Super::BeginPlay();

	// Note: Perception setup moved to OnPossess to ensure valid listener ID
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

	// Initialize blackboard threat info
	UpdateBlackboardThreatInfo();
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
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	}

	HearingConfig = NewObject<UAISenseConfig_Hearing>(this, UAISenseConfig_Hearing::StaticClass(), TEXT("HearingConfig"));
	if (HearingConfig)
	{
		HearingConfig->HearingRange = DefaultHearingRange;
		HearingConfig->SetMaxAge(MaxStimulusAge);
		HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
		HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
		HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	}

	// Bind to perception updates
	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMOCreatureController::OnTargetPerceptionUpdated);

	// Defer ConfigureSense to next frame when listener ID will be valid
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			FinalizePerceptionSetup();
		}));
	}
}

void AMOCreatureController::FinalizePerceptionSetup()
{
	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();
	if (!PerceptionComp)
	{
		return;
	}

	// Now that listener ID is valid, configure the senses
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

	// Apply any pending perception settings that were requested before perception was ready
	ApplyPendingPerceptionSettings();
}

void AMOCreatureController::ApplyPerceptionSettings(const FMOCreatureDefinitionRow& Definition)
{
	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();

	// Check if perception is ready (sense configs created in SetupPerception during OnPossess)
	bool bPerceptionReady = SightConfig != nullptr && HearingConfig != nullptr;

	if (!bPerceptionReady)
	{
		// Store settings to apply later when perception is ready
		PendingSightRadius = Definition.SightRadius;
		PendingLoseSightRadius = Definition.LoseSightRadius;
		PendingPeripheralVisionAngle = Definition.PeripheralVisionAngle;
		PendingHearingRange = Definition.HearingRange;

		// Still apply combat settings immediately
		FleeHealthThreshold = Definition.FleeThreshold;
		AttackRange = Definition.AttackRange;
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
	FleeHealthThreshold = Definition.FleeThreshold;
	AttackRange = Definition.AttackRange;

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

	if (Stimulus.WasSuccessfullySensed())
	{
		// New threat detected - track it directly
		CurrentThreatActor = Actor;
		RememberedThreatActor = Actor;
		LastKnownThreatLocation = Actor->GetActorLocation();

		// Clear any pending "forget" timer since we can see the threat again
		GetWorld()->GetTimerManager().ClearTimer(ThreatMemoryTimerHandle);

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Threat detected: %s at distance %.1f"),
			*Actor->GetName(), FVector::Dist(ControlledPawn->GetActorLocation(), Actor->GetActorLocation()));

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

			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Lost sight of %s, remembering for %.1fs"),
				*Actor->GetName(), ThreatMemoryDuration);

			// Start timer to forget the threat after ThreatMemoryDuration
			// But keep CurrentThreatActor valid until then!
			GetWorld()->GetTimerManager().SetTimer(
				ThreatMemoryTimerHandle,
				[this, Actor]()
				{
					// Only clear if this is still the remembered threat
					if (RememberedThreatActor.Get() == Actor)
					{
						UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Threat forgotten: %s"),
							Actor ? *Actor->GetName() : TEXT("Unknown"));
						CurrentThreatActor.Reset();
						RememberedThreatActor.Reset();
						UpdateBlackboardThreatInfo();
					}
				},
				ThreatMemoryDuration,
				false
			);
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
		return;
	}

	bool bHasTarget = CurrentThreatActor.IsValid();

	// Update threat-related keys
	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), CurrentThreatActor.Get());
	BlackboardComponent->SetValueAsVector(TEXT("TargetLocation"), LastKnownThreatLocation);
	BlackboardComponent->SetValueAsBool(TEXT("HasTarget"), bHasTarget);
	BlackboardComponent->SetValueAsBool(TEXT("ShouldFlee"), ShouldFlee());
	BlackboardComponent->SetValueAsFloat(TEXT("HealthPercent"), GetHealthPercent());
	BlackboardComponent->SetValueAsFloat(TEXT("DistanceToTarget"), GetDistanceToThreat());
	BlackboardComponent->SetValueAsVector(TEXT("HomeLocation"), HomeLocation);

	// Update activity state keys
	BlackboardComponent->SetValueAsEnum(TEXT("ActivityState"), static_cast<uint8>(CurrentActivityState));
	BlackboardComponent->SetValueAsBool(TEXT("IsDead"), CurrentActivityState == EMOCreatureActivityState::Dead);
	BlackboardComponent->SetValueAsBool(TEXT("IsResting"), CurrentActivityState == EMOCreatureActivityState::Resting);
	BlackboardComponent->SetValueAsBool(TEXT("IsSleeping"), CurrentActivityState == EMOCreatureActivityState::Sleeping);

	// Commented out - fires constantly during gameplay
	// UE_LOG(LogMOFramework, Warning, TEXT("MOCreatureController: Updated blackboard - HasTarget=%d, Target=%s, Distance=%.1f"),
	// 	bHasTarget ? 1 : 0,
	// 	CurrentThreatActor.IsValid() ? *CurrentThreatActor->GetName() : TEXT("None"),
	// 	GetDistanceToThreat());
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

	EMOCreatureActivityState OldState = CurrentActivityState;
	CurrentActivityState = NewState;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreatureController] Activity state: %d -> %d"),
		static_cast<int32>(OldState), static_cast<int32>(NewState));

	// Update blackboard with new state
	if (BlackboardComponent)
	{
		BlackboardComponent->SetValueAsEnum(TEXT("ActivityState"), static_cast<uint8>(CurrentActivityState));
		BlackboardComponent->SetValueAsBool(TEXT("IsDead"), CurrentActivityState == EMOCreatureActivityState::Dead);
		BlackboardComponent->SetValueAsBool(TEXT("IsResting"), CurrentActivityState == EMOCreatureActivityState::Resting);
		BlackboardComponent->SetValueAsBool(TEXT("IsSleeping"), CurrentActivityState == EMOCreatureActivityState::Sleeping);
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
