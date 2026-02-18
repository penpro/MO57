#include "BTTask_CreatureAttack.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"

UBTTask_CreatureAttack::UBTTask_CreatureAttack()
{
	NodeName = TEXT("Creature Attack");

	// Setup target key filter
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_CreatureAttack, TargetActorKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_CreatureAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		return EBTNodeResult::Failed;
	}

	// Get target from blackboard
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	// Check range
	float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());
	if (Distance > AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	// Face the target
	FVector Direction = (TargetActor->GetActorLocation() - OwnerPawn->GetActorLocation()).GetSafeNormal();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		OwnerPawn->SetActorRotation(Direction.Rotation());
	}

	// TODO: Use MOCombatComponent when available
	// For now, apply damage directly
	// UMOCombatComponent* Combat = OwnerPawn->FindComponentByClass<UMOCombatComponent>();
	// if (Combat && Combat->CanAttack())
	// {
	//     Combat->StartAttack(EMOAttackType::Light);
	// }

	// Simple damage application for now
	UE_LOG(LogTemp, Log, TEXT("BTTask_CreatureAttack: %s attacking %s for %.1f damage"),
		*OwnerPawn->GetName(), *TargetActor->GetName(), BaseDamage);

	// Apply damage to target (if it has a damage interface)
	// For full implementation, this should go through the combat/medical system

	return EBTNodeResult::Succeeded;
}

FString UBTTask_CreatureAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("Attack target with %.1f damage"), BaseDamage);
}
