#include "MOPredatorCreature.h"
#include "MOBlackboardKeys.h"
#include "MOFramework.h"
#include "MOCreatureController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

AMOPredatorCreature::AMOPredatorCreature()
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AMOPredatorCreature::BeginPlay()
{
	Super::BeginPlay();

	// Apply flee threshold to controller
	if (AMOCreatureController* CreatureAI = GetCreatureController())
	{
		CreatureAI->SetFleeHealthThreshold(FleeHealthThreshold);
	}

	// If no behavior tree override set, use default predator tree
	if (BehaviorTreeOverride.IsNull() && !DefaultPredatorBehaviorTree.IsNull())
	{
		BehaviorTreeOverride = DefaultPredatorBehaviorTree;

		// Start the behavior tree if controller is ready
		if (AMOCreatureController* CreatureAI = GetCreatureController())
		{
			if (UBehaviorTree* BTToRun = DefaultPredatorBehaviorTree.LoadSynchronous())
			{
				CreatureAI->RunBehaviorTree(BTToRun);
			}
		}
	}
}

void AMOPredatorCreature::AlertPack(AActor* Threat)
{
	if (!Threat)
	{
		return;
	}

	TArray<AMOPredatorCreature*> PackMembers = FindPackMembers();

	for (AMOPredatorCreature* Member : PackMembers)
	{
		if (Member == this)
		{
			continue;
		}

		// Alert each pack member through their controller
		if (AMOCreatureController* MemberAI = Member->GetCreatureController())
		{
			// Set threat on blackboard (this will trigger their behavior tree)
			if (UBlackboardComponent* BB = MemberAI->GetBlackboardComponent())
			{
				BB->SetValueAsObject(MOBlackboardKeys::TargetActor, Threat);
				BB->SetValueAsVector(MOBlackboardKeys::TargetLocation, Threat->GetActorLocation());
				BB->SetValueAsBool(MOBlackboardKeys::HasTarget, true);
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("AMOPredatorCreature: %s alerted %d pack members of threat %s"),
		*GetName(), PackMembers.Num() - 1, *Threat->GetName());
}

bool AMOPredatorCreature::IsInPack() const
{
	return GetPackSize() > 1;
}

int32 AMOPredatorCreature::GetPackSize() const
{
	return FindPackMembers().Num();
}

bool AMOPredatorCreature::ShouldAttack() const
{
	AMOCreatureController* CreatureAI = GetCreatureController();
	if (!CreatureAI)
	{
		return false;
	}

	// Don't attack if we should be fleeing
	if (ShouldFlee())
	{
		return false;
	}

	// Attack if we have a target
	return CreatureAI->IsInCombat();
}

bool AMOPredatorCreature::ShouldFlee() const
{
	return GetHealthPercent() <= FleeHealthThreshold;
}

TArray<AMOPredatorCreature*> AMOPredatorCreature::FindPackMembers() const
{
	TArray<AMOPredatorCreature*> PackMembers;

	// Include self
	PackMembers.Add(const_cast<AMOPredatorCreature*>(this));

	// Find other predators of the same type within range
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), GetClass(), FoundActors);

	FVector MyLocation = GetActorLocation();

	for (AActor* Actor : FoundActors)
	{
		AMOPredatorCreature* OtherPredator = Cast<AMOPredatorCreature>(Actor);
		if (!OtherPredator || OtherPredator == this)
		{
			continue;
		}

		// Check if within pack detection range
		float Distance = FVector::Dist(MyLocation, OtherPredator->GetActorLocation());
		if (Distance <= PackDetectionRange)
		{
			PackMembers.Add(OtherPredator);
		}
	}

	return PackMembers;
}
