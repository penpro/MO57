// Copyright Penumbra Group Inc. All Rights Reserved.

#include "EnvQueryContext_Threat.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UEnvQueryContext_Threat::UEnvQueryContext_Threat()
{
	ThreatBlackboardKey = TEXT("ThreatActor");
}

void UEnvQueryContext_Threat::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner)
	{
		return;
	}

	// Get the AI controller to access blackboard
	AAIController* AIController = nullptr;

	if (AAIController* DirectController = Cast<AAIController>(QueryOwner))
	{
		AIController = DirectController;
	}
	else if (APawn* Pawn = Cast<APawn>(QueryOwner))
	{
		AIController = Cast<AAIController>(Pawn->GetController());
	}

	if (!AIController)
	{
		UE_LOG(LogTemp, Verbose, TEXT("EQS ThreatContext: No AI controller found"));
		return;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
	if (!Blackboard)
	{
		UE_LOG(LogTemp, Verbose, TEXT("EQS ThreatContext: No blackboard found"));
		return;
	}

	// Get the threat actor from blackboard
	AActor* ThreatActor = Cast<AActor>(Blackboard->GetValueAsObject(ThreatBlackboardKey));
	if (!ThreatActor)
	{
		UE_LOG(LogTemp, Verbose, TEXT("EQS ThreatContext: No threat actor in blackboard key '%s'"), *ThreatBlackboardKey.ToString());
		return;
	}

	// Provide the actor to EQS
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, ThreatActor);
}
