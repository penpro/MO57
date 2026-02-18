#include "BTService_CreatureActivity.h"
#include "MOCreatureController.h"
#include "MOCreatureTypes.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_CreatureActivity::UBTService_CreatureActivity()
{
	NodeName = TEXT("Creature Activity Manager");

	// Check every second
	Interval = 1.0f;
	RandomDeviation = 0.1f;

	// Don't tick when paused
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
}

void UBTService_CreatureActivity::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AMOCreatureController* Controller = Cast<AMOCreatureController>(OwnerComp.GetAIOwner());
	if (!Controller)
	{
		return;
	}

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return;
	}

	// Don't process if dead
	if (Controller->IsDead())
	{
		return;
	}

	// Check for threats - always wake up if there's a threat
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	bool bHasThreat = BB && BB->GetValueAsBool(TEXT("HasTarget"));

	EMOCreatureActivityState CurrentState = Controller->GetActivityState();

	// If there's a threat, wake up immediately
	if (bHasThreat)
	{
		if (CurrentState == EMOCreatureActivityState::Resting ||
			CurrentState == EMOCreatureActivityState::Sleeping)
		{
			UE_LOG(LogTemp, Warning, TEXT("BTService_CreatureActivity: Waking up due to threat!"));
			Controller->SetActivityState(EMOCreatureActivityState::Active);
			CurrentStateDuration = 0.f;
		}
		return; // Don't process rest/sleep when threatened
	}

	// Check time of day
	UWorld* World = Pawn->GetWorld();
	UMOWeatherIntegrationSubsystem* WeatherSub = World ? World->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;

	bool bIsDaytime = true;
	if (WeatherSub && WeatherSub->HasWeatherProvider())
	{
		bIsDaytime = WeatherSub->IsDaytime();
	}

	// Handle state transitions based on time of day
	if (!bIsDaytime && bSleepsAtNight)
	{
		// Night time - should be sleeping
		if (CurrentState != EMOCreatureActivityState::Sleeping)
		{
			UE_LOG(LogTemp, Log, TEXT("BTService_CreatureActivity: Night time, going to sleep"));
			Controller->SetActivityState(EMOCreatureActivityState::Sleeping);
			CurrentStateDuration = 0.f;
		}
	}
	else
	{
		// Daytime behavior
		switch (CurrentState)
		{
		case EMOCreatureActivityState::Sleeping:
			// Wake up - it's daytime
			UE_LOG(LogTemp, Log, TEXT("BTService_CreatureActivity: Daytime, waking up"));
			Controller->SetActivityState(EMOCreatureActivityState::Active);
			CurrentStateDuration = 0.f;
			break;

		case EMOCreatureActivityState::Resting:
			// Check if rest duration is complete
			CurrentStateDuration += DeltaSeconds;
			if (CurrentStateDuration >= TargetRestDuration)
			{
				UE_LOG(LogTemp, Log, TEXT("BTService_CreatureActivity: Done resting (%.1f seconds)"), CurrentStateDuration);
				Controller->SetActivityState(EMOCreatureActivityState::Active);
				CurrentStateDuration = 0.f;
			}
			break;

		case EMOCreatureActivityState::Active:
		{
			// Check if we should start resting
			AccumulatedTime += DeltaSeconds;

			// Calculate chance per tick based on RestChancePerMinute
			// Convert per-minute chance to per-second: P(second) = 1 - (1 - P(minute))^(1/60)
			float ChancePerSecond = 1.f - FMath::Pow(1.f - RestChancePerMinute, 1.f / 60.f);

			if (FMath::FRand() < ChancePerSecond)
			{
				// Start resting
				TargetRestDuration = FMath::RandRange(RestDurationMin, RestDurationMax);
				UE_LOG(LogTemp, Log, TEXT("BTService_CreatureActivity: Starting rest for %.1f seconds"), TargetRestDuration);
				Controller->SetActivityState(EMOCreatureActivityState::Resting);
				CurrentStateDuration = 0.f;
			}
			break;
		}

		default:
			// Fleeing, Fighting, etc. - don't interfere
			break;
		}
	}
}

FString UBTService_CreatureActivity::GetStaticDescription() const
{
	return FString::Printf(TEXT("Rest chance: %.0f%%/min, Sleep at night: %s"),
		RestChancePerMinute * 100.f,
		bSleepsAtNight ? TEXT("Yes") : TEXT("No"));
}
