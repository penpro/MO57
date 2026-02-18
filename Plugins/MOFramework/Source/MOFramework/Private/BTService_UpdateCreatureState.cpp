#include "BTService_UpdateCreatureState.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MOCreatureController.h"
#include "MOCreature.h"

UBTService_UpdateCreatureState::UBTService_UpdateCreatureState()
{
	NodeName = TEXT("Update Creature State");

	// Default tick interval
	Interval = 0.5f;
	RandomDeviation = 0.1f;

	// Setup key filters
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCreatureState, TargetActorKey), AActor::StaticClass());
	HealthPercentKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCreatureState, HealthPercentKey));
	ShouldFleeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCreatureState, ShouldFleeKey));
	HasTargetKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCreatureState, HasTargetKey));
	DistanceToTargetKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCreatureState, DistanceToTargetKey));
	IsInAttackRangeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCreatureState, IsInAttackRangeKey));
}

void UBTService_UpdateCreatureState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	// Get health from creature
	float HealthPercent = 1.f;
	if (AMOCreature* Creature = Cast<AMOCreature>(OwnerPawn))
	{
		HealthPercent = Creature->GetHealthPercent();
	}
	else if (AMOCreatureController* CreatureAI = Cast<AMOCreatureController>(AIController))
	{
		HealthPercent = CreatureAI->GetHealthPercent();
	}

	// Update health
	BlackboardComp->SetValueAsFloat(HealthPercentKey.SelectedKeyName, HealthPercent);

	// Update should flee
	bool bShouldFlee = HealthPercent <= FleeHealthThreshold;
	BlackboardComp->SetValueAsBool(ShouldFleeKey.SelectedKeyName, bShouldFlee);

	// Get target
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
	bool bHasTarget = TargetActor != nullptr;
	BlackboardComp->SetValueAsBool(HasTargetKey.SelectedKeyName, bHasTarget);

	// Update distance and attack range
	if (bHasTarget)
	{
		float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());
		BlackboardComp->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, Distance);
		BlackboardComp->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, Distance <= AttackRange);
	}
	else
	{
		BlackboardComp->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, MAX_FLT);
		BlackboardComp->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, false);
	}
}

FString UBTService_UpdateCreatureState::GetStaticDescription() const
{
	return FString::Printf(TEXT("Update creature state (flee at %.0f%% health)"), FleeHealthThreshold * 100.f);
}
