#include "MOCreature.h"
#include "MOFramework.h"
#include "MOCreatureController.h"
#include "MOCreatureDefinitionRow.h"
#include "MOCreatureTypes.h"
#include "MOCarcassActor.h"
#include "MOCarcassDefinitionRow.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "MOInventoryComponent.h"
#include "MORecruitmentComponent.h"
#include "MOWorldItem.h"
#include "Components/CapsuleComponent.h"
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

	// Creatures are not recruitable by default (use bIsRecruitable for future animal taming)
	if (UMORecruitmentComponent* RecruitComp = FindComponentByClass<UMORecruitmentComponent>())
	{
		RecruitComp->bIsRecruitable = false;
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreature] %s BeginPlay"), *GetName());

	// Validate controller (Error only if missing)
	if (!GetController())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: NO CONTROLLER! AutoPossessAI may have failed."), *GetName());
	}

	// Validate navigation (Error only if missing)
	if (!FindComponentByClass<UNavigationInvokerComponent>())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: NavigationInvoker is NULL!"), *GetName());
	}

	// Apply creature definition settings
	ApplyCreatureDefinition();

	// Bind to anatomy death events
	if (UMOAnatomyComponent* Anatomy = GetAnatomyComponent())
	{
		Anatomy->OnInstantDeath.AddDynamic(this, &AMOCreature::HandleInstantDeath);
		Anatomy->OnBodyPartDestroyed.AddDynamic(this, &AMOCreature::HandleBodyPartDestroyed);
	}
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
		Anatomy->OnBodyPartDestroyed.RemoveDynamic(this, &AMOCreature::HandleBodyPartDestroyed);
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] %s: No creature definition"), *GetName());
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
		CreatureAI->ApplyPerceptionSettings(*Definition);

		// Start behavior tree
		UBehaviorTree* BTToRun = nullptr;

		// Check for override first
		if (!BehaviorTreeOverride.IsNull())
		{
			BTToRun = BehaviorTreeOverride.LoadSynchronous();
		}
		// Then try definition
		else if (!Definition->BehaviorTree.IsNull())
		{
			BTToRun = Definition->BehaviorTree.LoadSynchronous();
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: NO BEHAVIOR TREE SET!"), *GetName());
		}

		if (BTToRun)
		{
			bool bSuccess = CreatureAI->RunBehaviorTree(BTToRun);
			if (!bSuccess)
			{
				UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: RunBehaviorTree(%s) FAILED"),
					*GetName(), *BTToRun->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] %s: GetCreatureController() returned NULL!"), *GetName());
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCreature] %s: Applied definition '%s'"),
		*GetName(), *Definition->DisplayName.ToString());
}

void AMOCreature::HandleInstantDeath(EMOBodyPartType CausePart)
{
	if (bIsDead)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s died from instant death (part: %d)"),
		*GetName(), static_cast<int32>(CausePart));

	// Find the killer (could be from combat component or last damage source)
	// For now, we don't have a direct reference, so pass nullptr
	OnDeath(nullptr);
}

void AMOCreature::HandleBodyPartDestroyed(EMOBodyPartType Part, bool bInstantDeath)
{
	if (bIsDead)
	{
		return;
	}

	// If instant death already handled, skip
	if (bInstantDeath)
	{
		return;
	}

	// For creatures, destruction of vital body parts causes death
	// This is simpler than the full human medical simulation
	bool bIsVitalPart = false;
	switch (Part)
	{
	case EMOBodyPartType::Head:
	case EMOBodyPartType::Torso:
	case EMOBodyPartType::Heart:
	case EMOBodyPartType::Brain:
	case EMOBodyPartType::SpineCervical:
	case EMOBodyPartType::SpineThoracic:
	case EMOBodyPartType::Liver:
	case EMOBodyPartType::Stomach:
		bIsVitalPart = true;
		break;
	default:
		break;
	}

	if (bIsVitalPart)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s died from vital part destruction (part: %d)"),
			*GetName(), static_cast<int32>(Part));
		OnDeath(nullptr);
	}
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
			UE_LOG(LogMOFramework, Log, TEXT("AMOCreature: %s playing hit reaction"), *GetName());
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] %s died, killer: %s"),
		*GetName(), Killer ? *Killer->GetName() : TEXT("None"));

	// Set activity state to Dead (updates blackboard for ABP, stops AI)
	if (AMOCreatureController* CreatureAI = GetCreatureController())
	{
		CreatureAI->SetDead();
	}

	// Disable collision and movement
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
	}

	// Broadcast death event
	OnCreatureDeath.Broadcast(this, Killer);

	// Spawn carcass actor (replaces old loot system)
	AMOCarcassActor* Carcass = SpawnCarcass();

	// If carcass spawned successfully, destroy creature actor
	// If not (no carcass definition), fall back to old loot system
	if (Carcass)
	{
		// Play death montage briefly before transitioning to carcass
		if (DeathMontage)
		{
			if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Play(DeathMontage);
			}
		}

		// Hide this actor after a short delay, let carcass take over
		SetLifeSpan(0.5f);
	}
	else
	{
		// No carcass definition - use old loot system
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] No carcass definition, using legacy loot system"));

		// Play death montage if available
		if (DeathMontage)
		{
			if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Play(DeathMontage);
			}
		}

		// Spawn loot (legacy)
		SpawnLoot();

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
		UE_LOG(LogMOFramework, Log, TEXT("AMOCreature: Would spawn loot - %s x%d at %s"),
			*LootEntry.ItemDefinitionId.ToString(), Quantity, *(SpawnLocation + Offset).ToString());

		// TODO: Implement actual item spawning when MOWorldItem is available
		// AMOWorldItem* WorldItem = GetWorld()->SpawnActor<AMOWorldItem>(WorldItemClass, SpawnLocation + Offset, FRotator::ZeroRotator, SpawnParams);
		// if (WorldItem)
		// {
		//     WorldItem->SetItemDefinitionAndQuantity(LootEntry.ItemDefinitionId, Quantity);
		// }
	}
}

AMOCarcassActor* AMOCreature::SpawnCarcass()
{
	const FMOCreatureDefinitionRow* Definition = GetCreatureDefinitionRow();
	if (!Definition)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] SpawnCarcass: No creature definition"));
		return nullptr;
	}

	// Check if carcass definition is set
	if (Definition->CarcassDefinitionId.IsNone())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] SpawnCarcass: No carcass definition ID for %s"), *GetName());
		return nullptr;
	}

	// Load carcass DataTable
	UDataTable* CarcassTable = Definition->CarcassDataTable.LoadSynchronous();
	if (!CarcassTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] SpawnCarcass: Could not load carcass DataTable"));
		return nullptr;
	}

	// Verify the carcass definition exists
	const FMOCarcassDefinitionRow* CarcassDef = CarcassTable->FindRow<FMOCarcassDefinitionRow>(
		Definition->CarcassDefinitionId, TEXT("SpawnCarcass"));
	if (!CarcassDef)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCreature] SpawnCarcass: Carcass definition '%s' not found in DataTable"),
			*Definition->CarcassDefinitionId.ToString());
		return nullptr;
	}

	// Spawn the carcass actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AMOCarcassActor* Carcass = GetWorld()->SpawnActor<AMOCarcassActor>(
		AMOCarcassActor::StaticClass(),
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams
	);

	if (Carcass)
	{
		// Configure carcass definition
		Carcass->CarcassDefinition.DataTable = CarcassTable;
		Carcass->CarcassDefinition.RowName = Definition->CarcassDefinitionId;

		// Initialize from this creature (copy mesh, etc.)
		Carcass->InitializeFromCreature(this);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCreature] Spawned carcass '%s' for %s"),
			*CarcassDef->DisplayName.ToString(), *GetName());
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCreature] Failed to spawn carcass actor for %s"), *GetName());
	}

	return Carcass;
}

void AMOCreature::PerformDestroy()
{
	Destroy();
}
