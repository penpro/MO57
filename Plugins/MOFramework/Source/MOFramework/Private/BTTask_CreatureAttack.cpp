#include "BTTask_CreatureAttack.h"
#include "MOBlackboardKeys.h"
#include "MOFramework.h"
#include "MOAnatomyComponent.h"
#include "MOBodyPartTypes.h"
#include "MOWoundTypes.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UBTTask_CreatureAttack::UBTTask_CreatureAttack()
{
	NodeName = TEXT("Creature Attack");
	bNotifyTick = true;

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

	// Initialize attack memory
	FBTAttackMemory* Memory = reinterpret_cast<FBTAttackMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.f;
	Memory->bDamageApplied = false;
	Memory->bMontageStarted = false;

	UE_LOG(LogMOFramework, Verbose, TEXT("[BTCreatureAttack] %s starting attack on %s"),
		*OwnerPawn->GetName(), *TargetActor->GetName());

	// Stop movement during attack
	AIController->StopMovement();

	// Set IsAttacking blackboard key (for AnimBP to prevent idle override)
	BlackboardComp->SetValueAsBool(MOBlackboardKeys::IsAttacking, true);

	// Play attack animation montage
	if (AttackMontage)
	{
		if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
		{
			if (USkeletalMeshComponent* Mesh = Character->GetMesh())
			{
				if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
				{
					float Duration = AnimInstance->Montage_Play(AttackMontage, MontagePlayRate);
					if (Duration > 0.f)
					{
						Memory->bMontageStarted = true;
						UE_LOG(LogMOFramework, Verbose, TEXT("[BTCreatureAttack] Playing attack montage: %s (duration: %.2fs)"),
							*AttackMontage->GetName(), Duration);
					}
					else
					{
						UE_LOG(LogMOFramework, Verbose, TEXT("[BTCreatureAttack] Failed to play attack montage: %s"),
							*AttackMontage->GetName());
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[BTCreatureAttack] No attack montage set"));
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_CreatureAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTAttackMemory* Memory = reinterpret_cast<FBTAttackMemory*>(NodeMemory);

	// Clear IsAttacking blackboard key
	if (UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsBool(MOBlackboardKeys::IsAttacking, false);
	}

	// Stop attack montage if it was playing
	if (Memory->bMontageStarted && AttackMontage)
	{
		AAIController* AIController = OwnerComp.GetAIOwner();
		if (AIController)
		{
			if (APawn* OwnerPawn = AIController->GetPawn())
			{
				if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
				{
					if (USkeletalMeshComponent* Mesh = Character->GetMesh())
					{
						if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
						{
							AnimInstance->Montage_Stop(0.2f, AttackMontage);
							UE_LOG(LogMOFramework, Verbose, TEXT("[BTCreatureAttack] Stopped attack montage on abort"));
						}
					}
				}
			}
		}
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_CreatureAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTAttackMemory* Memory = reinterpret_cast<FBTAttackMemory*>(NodeMemory);
	Memory->ElapsedTime += DeltaSeconds;

	// Apply damage after wind-up
	if (!Memory->bDamageApplied && Memory->ElapsedTime >= WindUpTime)
	{
		Memory->bDamageApplied = true;

		AAIController* AIController = OwnerComp.GetAIOwner();
		APawn* OwnerPawn = AIController ? AIController->GetPawn() : nullptr;
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		AActor* TargetActor = BlackboardComp ? Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;

		if (OwnerPawn && TargetActor)
		{
			// Check target still in range
			float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());
			if (Distance <= AttackRange * 1.5f) // Slightly forgiving range for mid-attack
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[BTCreatureAttack] %s hits %s for %.1f damage"),
					*OwnerPawn->GetName(), *TargetActor->GetName(), BaseDamage);

				if (UMOAnatomyComponent* TargetAnatomy = TargetActor->FindComponentByClass<UMOAnatomyComponent>())
				{
					// Pick a random body part to damage
					TArray<EMOBodyPartType> TargetParts = {
						EMOBodyPartType::Torso,
						EMOBodyPartType::UpperArmLeft,
						EMOBodyPartType::UpperArmRight,
						EMOBodyPartType::ThighLeft,
						EMOBodyPartType::ThighRight
					};
					EMOBodyPartType TargetPart = TargetParts[FMath::RandRange(0, TargetParts.Num() - 1)];

					TargetAnatomy->InflictDamage(TargetPart, BaseDamage, EMOWoundType::Laceration);
				}
			}
			else
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[BTCreatureAttack] %s missed - target moved out of range"),
					*OwnerPawn->GetName());
			}
		}
	}

	// Complete attack after full duration
	if (Memory->ElapsedTime >= AttackDuration)
	{
		// Clear IsAttacking blackboard key
		if (UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent())
		{
			BlackboardComp->SetValueAsBool(MOBlackboardKeys::IsAttacking, false);
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

FString UBTTask_CreatureAttack::GetStaticDescription() const
{
	FString MontageInfo = AttackMontage ? AttackMontage->GetName() : TEXT("None");
	return FString::Printf(TEXT("Attack: %.1f dmg, %.1fs windup\nMontage: %s"),
		BaseDamage, WindUpTime, *MontageInfo);
}
