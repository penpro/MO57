#include "MOPreyCreature.h"
#include "MOCreatureController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameFramework/CharacterMovementComponent.h"

AMOPreyCreature::AMOPreyCreature()
{
	// Prey creatures auto-possess for AI
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AMOPreyCreature::BeginPlay()
{
	Super::BeginPlay();

	// If no behavior tree override set, use default prey tree
	if (BehaviorTreeOverride.IsNull() && !DefaultPreyBehaviorTree.IsNull())
	{
		BehaviorTreeOverride = DefaultPreyBehaviorTree;

		// Start the behavior tree if controller is ready
		if (AMOCreatureController* CreatureAI = GetCreatureController())
		{
			if (UBehaviorTree* BTToRun = DefaultPreyBehaviorTree.LoadSynchronous())
			{
				CreatureAI->RunBehaviorTree(BTToRun);
			}
		}
	}
}

bool AMOPreyCreature::ShouldFleeFromThreat() const
{
	AMOCreatureController* CreatureAI = GetCreatureController();
	if (!CreatureAI)
	{
		return false;
	}

	// Prey always wants to flee when threat is detected within range
	AActor* Threat = CreatureAI->GetCurrentThreat();
	if (!Threat)
	{
		return false;
	}

	float DistanceToThreat = CreatureAI->GetDistanceToThreat();

	// Flee if threat is within detection range
	return DistanceToThreat <= FleeDetectionRange;
}

bool AMOPreyCreature::IsCornered() const
{
	// TODO: Implement cornered check using EQS or navmesh queries
	// For now, consider cornered if health is very low and can't find escape routes
	// This will be implemented when EQS flee query is added

	AMOCreatureController* CreatureAI = GetCreatureController();
	if (!CreatureAI)
	{
		return false;
	}

	// Simple placeholder: cornered if very low health and in attack range
	return GetHealthPercent() < CorneredHealthPercent && CreatureAI->IsInAttackRange();
}
