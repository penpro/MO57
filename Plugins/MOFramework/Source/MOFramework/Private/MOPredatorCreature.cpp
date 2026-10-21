#include "MOPredatorCreature.h"
#include "MOBlackboardKeys.h"
#include "MOFramework.h"
#include "MOCreatureController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

AMOPredatorCreature::AMOPredatorCreature(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

	// Through each member's controller (NotifyPackThreat), not its blackboard: the controller rewrites the blackboard's threat keys from
	// its own state every update, so a written key was gone the next frame (and nothing called this before Oct 20).
	int32 Answered = 0;
	for (AMOPredatorCreature* Member : FindPackMembers())
	{
		if (Member == this || FVector::DistSquared(Member->GetActorLocation(), GetActorLocation()) > FMath::Square(PackAlertRange))
		{
			continue;
		}
		if (AMOCreatureController* MemberAI = Member->GetCreatureController())
		{
			MemberAI->NotifyPackThreat(Threat, this);
			++Answered;
		}
	}
	if (Answered > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s called %d packmate(s) against %s"), *GetName(), Answered, *Threat->GetName());
	}
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
	// ONE flee decision (AMOCreatureController::ShouldFlee, the species row's FleeThreshold); this class's own threshold only without a controller
	if (const AMOCreatureController* CreatureAI = GetCreatureController())
	{
		return CreatureAI->ShouldFlee();
	}
	return GetHealthPercent() <= FleeHealthThreshold;
}

TArray<AMOPredatorCreature*> AMOPredatorCreature::FindPackMembers() const
{
	TArray<AMOPredatorCreature*> PackMembers;

	// Include self
	PackMembers.Add(const_cast<AMOPredatorCreature*>(this));

	// Find other living predators of the same type within range (an actor iterator: no array of every actor of the class each call)
	FVector MyLocation = GetActorLocation();

	for (TActorIterator<AMOPredatorCreature> It(GetWorld()); It; ++It)
	{
		AMOPredatorCreature* OtherPredator = *It;
		if (!IsValid(OtherPredator) || OtherPredator == this || OtherPredator->GetClass() != GetClass() || OtherPredator->IsDead())
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
