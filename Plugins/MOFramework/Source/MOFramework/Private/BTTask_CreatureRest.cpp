#include "BTTask_CreatureRest.h"
#include "MOCreatureController.h"
#include "MOCreatureTypes.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_CreatureRest::UBTTask_CreatureRest()
{
	NodeName = TEXT("Creature Rest");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_CreatureRest::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AMOCreatureController* Controller = Cast<AMOCreatureController>(OwnerComp.GetAIOwner());
	if (!Controller)
	{
		return EBTNodeResult::Failed;
	}

	FBTRestMemory* MyMemory = reinterpret_cast<FBTRestMemory*>(NodeMemory);
	MyMemory->ElapsedTime = 0.f;
	MyMemory->TargetDuration = bIsSleeping ? MAX_FLT : FMath::RandRange(MinDuration, MaxDuration);

	// Set the activity state
	EMOCreatureActivityState TargetState = bIsSleeping ?
		EMOCreatureActivityState::Sleeping : EMOCreatureActivityState::Resting;

	Controller->SetActivityState(TargetState);

	UE_LOG(LogTemp, Log, TEXT("BTTask_CreatureRest: Starting %s for %.1f seconds"),
		bIsSleeping ? TEXT("sleep") : TEXT("rest"),
		MyMemory->TargetDuration);

	return EBTNodeResult::InProgress;
}

void UBTTask_CreatureRest::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTRestMemory* MyMemory = reinterpret_cast<FBTRestMemory*>(NodeMemory);
	MyMemory->ElapsedTime += DeltaSeconds;

	// For sleeping, we rely on external conditions (daytime, threat) to abort
	// For resting, check duration
	if (!bIsSleeping && MyMemory->ElapsedTime >= MyMemory->TargetDuration)
	{
		AMOCreatureController* Controller = Cast<AMOCreatureController>(OwnerComp.GetAIOwner());
		if (Controller)
		{
			Controller->SetActivityState(EMOCreatureActivityState::Active);
		}

		UE_LOG(LogTemp, Log, TEXT("BTTask_CreatureRest: Finished resting after %.1f seconds"), MyMemory->ElapsedTime);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_CreatureRest::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AMOCreatureController* Controller = Cast<AMOCreatureController>(OwnerComp.GetAIOwner());
	if (Controller)
	{
		// Wake up when aborted (threat detected, etc.)
		Controller->SetActivityState(EMOCreatureActivityState::Active);
		UE_LOG(LogTemp, Log, TEXT("BTTask_CreatureRest: Aborted - waking up!"));
	}

	return EBTNodeResult::Aborted;
}

FString UBTTask_CreatureRest::GetStaticDescription() const
{
	if (bIsSleeping)
	{
		return TEXT("Sleep (until woken)");
	}
	return FString::Printf(TEXT("Rest for %.0f-%.0f seconds"), MinDuration, MaxDuration);
}

uint16 UBTTask_CreatureRest::GetInstanceMemorySize() const
{
	return sizeof(FBTRestMemory);
}
