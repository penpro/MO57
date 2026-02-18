#include "MOCreature.h"
#include "MOCreatureController.h"
#include "MOCreatureDefinitionRow.h"
#include "MOCreatureTypes.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "MOInventoryComponent.h"
#include "MOWorldItem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationInvokerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Animation/AnimInstance.h"

AMOCreature::AMOCreature()
{
	// Set default AI controller class
	AIControllerClass = AMOCreatureController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Note: NavigationInvoker is inherited from MOCharacter
}

void AMOCreature::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("=== MOCreature::BeginPlay START for %s ==="), *GetName());

	// Check controller status
	AController* MyController = GetController();
	if (MyController)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOCreature: Controller = %s"), *MyController->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MOCreature: NO CONTROLLER! AutoPossessAI may have failed."));
	}

	// Check navigation invoker (inherited from MOCharacter)
	if (FindComponentByClass<UNavigationInvokerComponent>())
	{
		UE_LOG(LogTemp, Warning, TEXT("MOCreature: NavigationInvoker is valid (inherited from MOCharacter)"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MOCreature: NavigationInvoker is NULL!"));
	}

	// Check if NavMesh exists at our location
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation NavLoc;
		bool bOnNavMesh = NavSys->ProjectPointToNavigation(GetActorLocation(), NavLoc, FVector(500.f, 500.f, 500.f));
		UE_LOG(LogTemp, Warning, TEXT("MOCreature: NavSystem exists. On NavMesh = %s"), bOnNavMesh ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MOCreature: NO NAVIGATION SYSTEM!"));
	}

	// Apply creature definition settings
	ApplyCreatureDefinition();

	// Bind to anatomy death events
	if (UMOAnatomyComponent* Anatomy = GetAnatomyComponent())
	{
		Anatomy->OnInstantDeath.AddDynamic(this, &AMOCreature::HandleInstantDeath);
	}

	UE_LOG(LogTemp, Warning, TEXT("=== MOCreature::BeginPlay END for %s ==="), *GetName());
}

void AMOCreature::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DestroyTimerHandle);
	}

	// Unbind from anatomy
	if (UMOAnatomyComponent* Anatomy = GetAnatomyComponent())
	{
		Anatomy->OnInstantDeath.RemoveDynamic(this, &AMOCreature::HandleInstantDeath);
	}

	Super::EndPlay(EndPlayReason);
}

const FMOCreatureDefinitionRow* AMOCreature::GetCreatureDefinitionRow() const
{
	if (!CreatureDefinition.DataTable || CreatureDefinition.RowName.IsNone())
	{
		return nullptr;
	}

	return CreatureDefinition.DataTable->FindRow<FMOCreatureDefinitionRow>(
		CreatureDefinition.RowName, TEXT("GetCreatureDefinitionRow"));
}

AMOCreatureController* AMOCreature::GetCreatureController() const
{
	return Cast<AMOCreatureController>(GetController());
}

float AMOCreature::GetHealthPercent() const
{
	if (UMOVitalsComponent* Vitals = GetVitalsComponent())
	{
		return FMath::Clamp(Vitals->GetBloodVolumePercent(), 0.f, 1.f);
	}
	return 1.f;
}

void AMOCreature::ApplyCreatureDefinition()
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOCreature::ApplyCreatureDefinition - No valid creature definition for %s"), *GetName());
		return;
	}

	// Apply movement speeds
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Definition->WalkSpeed;
	}

	// Store run speed for sprint logic
	SprintSpeed = Definition->RunSpeed;
	WalkSpeed = Definition->WalkSpeed;

	// Apply perception and combat settings to controller
	if (AMOCreatureController* CreatureAI = GetCreatureController())
	{
		UE_LOG(LogTemp, Warning, TEXT("MOCreature: Found CreatureController, applying settings..."));
		CreatureAI->ApplyPerceptionSettings(*Definition);

		// Start behavior tree
		UBehaviorTree* BTToRun = nullptr;

		// Check for override first
		if (!BehaviorTreeOverride.IsNull())
		{
			BTToRun = BehaviorTreeOverride.LoadSynchronous();
			UE_LOG(LogTemp, Warning, TEXT("MOCreature: Using BehaviorTreeOverride"));
		}
		// Then try definition
		else if (!Definition->BehaviorTree.IsNull())
		{
			BTToRun = Definition->BehaviorTree.LoadSynchronous();
			UE_LOG(LogTemp, Warning, TEXT("MOCreature: Using BehaviorTree from definition"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MOCreature: NO BEHAVIOR TREE SET! Check definition or override."));
		}

		if (BTToRun)
		{
			bool bSuccess = CreatureAI->RunBehaviorTree(BTToRun);
			UE_LOG(LogTemp, Warning, TEXT("MOCreature: RunBehaviorTree(%s) = %s"),
				*BTToRun->GetName(), bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MOCreature: GetCreatureController() returned NULL!"));
	}

	UE_LOG(LogTemp, Log, TEXT("AMOCreature: Applied definition '%s' to %s"),
		*Definition->DisplayName.ToString(), *GetName());
}

void AMOCreature::HandleInstantDeath(EMOBodyPartType CausePart)
{
	if (bIsDead)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("AMOCreature: %s died from %d"),
		*GetName(), static_cast<int32>(CausePart));

	// Find the killer (could be from combat component or last damage source)
	// For now, we don't have a direct reference, so pass nullptr
	OnDeath(nullptr);
}

void AMOCreature::PlayHitReaction()
{
	if (bIsDead)
	{
		return;
	}

	if (HitReactionMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(HitReactionMontage);
			UE_LOG(LogTemp, Log, TEXT("AMOCreature: %s playing hit reaction"), *GetName());
		}
	}
}

void AMOCreature::OnDeath_Implementation(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	KillerActor = Killer;

	// Set activity state to Dead (updates blackboard for ABP, stops AI)
	if (AMOCreatureController* CreatureAI = GetCreatureController())
	{
		CreatureAI->SetDead();
	}

	// Play death montage if available
	if (DeathMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(DeathMontage);
		}
	}

	// Disable collision and movement
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
	}

	// Spawn loot
	SpawnLoot();

	// Broadcast death event
	OnCreatureDeath.Broadcast(this, Killer);

	// Schedule destruction
	if (DestroyDelayAfterDeath > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			DestroyTimerHandle,
			this,
			&AMOCreature::PerformDestroy,
			DestroyDelayAfterDeath,
			false
		);
	}
	else
	{
		PerformDestroy();
	}
}

void AMOCreature::SpawnLoot()
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		return;
	}

	FVector SpawnLocation = GetActorLocation();

	for (const FMOLootEntry& LootEntry : Definition->LootEntries)
	{
		// Check drop chance
		if (FMath::FRand() > LootEntry.DropChance)
		{
			continue;
		}

		// Calculate quantity
		int32 Quantity = FMath::RandRange(LootEntry.MinQuantity, LootEntry.MaxQuantity);
		if (Quantity <= 0)
		{
			continue;
		}

		// Spawn world item
		// Note: This assumes AMOWorldItem has a static spawn method or we need to spawn and configure
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// Offset spawn location slightly to spread items
		FVector Offset = FMath::VRand() * 50.f;
		Offset.Z = 20.f; // Slight upward offset

		// For now, log what would be spawned - actual spawn requires WorldItem class setup
		UE_LOG(LogTemp, Log, TEXT("AMOCreature: Would spawn loot - %s x%d at %s"),
			*LootEntry.ItemDefinitionId.ToString(), Quantity, *(SpawnLocation + Offset).ToString());

		// TODO: Implement actual item spawning when MOWorldItem is available
		// AMOWorldItem* WorldItem = GetWorld()->SpawnActor<AMOWorldItem>(WorldItemClass, SpawnLocation + Offset, FRotator::ZeroRotator, SpawnParams);
		// if (WorldItem)
		// {
		//     WorldItem->SetItemDefinitionAndQuantity(LootEntry.ItemDefinitionId, Quantity);
		// }
	}
}

void AMOCreature::PerformDestroy()
{
	Destroy();
}
