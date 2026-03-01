#include "MOCarcassActor.h"
#include "MOFramework.h"
#include "MOCarcassDefinitionRow.h"
#include "MOInteractableComponent.h"
#include "MOIdentityComponent.h"
#include "MOInventoryComponent.h"
#include "MOUIContractInterface.h"
#include "MOCreature.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

AMOCarcassActor::AMOCarcassActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10 Hz for harvest progress
	bReplicates = true;

	// Root component - interaction sphere (handles interaction traces)
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->InitSphereRadius(100.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // Block interaction traces
	RootComponent = InteractionSphere;

	// Poseable mesh for carcass body (allows manual bone positioning from ragdoll)
	CarcassMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("CarcassMesh"));
	CarcassMesh->SetupAttachment(RootComponent);
	// Enable collision for interaction traces (Visibility channel)
	CarcassMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CarcassMesh->SetCollisionObjectType(ECC_WorldDynamic);
	CarcassMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CarcassMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // Block interaction traces

	// Static mesh for bones (hidden initially)
	BonesMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BonesMesh"));
	BonesMesh->SetupAttachment(RootComponent);
	BonesMesh->SetCollisionProfileName(TEXT("NoCollision"));
	BonesMesh->SetVisibility(false);

	// Interactable component
	InteractableComponent = CreateDefaultSubobject<UMOInteractableComponent>(TEXT("InteractableComponent"));

	// Identity component for persistence
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("IdentityComponent"));
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void AMOCarcassActor::BeginPlay()
{
	Super::BeginPlay();

	// Bind interaction handlers
	if (InteractableComponent)
	{
		InteractableComponent->OnHandleInteract.BindUObject(this, &AMOCarcassActor::HandleInteract);
		InteractableComponent->OnHandleSecondaryInteract.BindUObject(this, &AMOCarcassActor::HandleSecondaryInteract);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] %s spawned, decay stage: %d"),
		*GetName(), static_cast<int32>(DecayStage));
}

void AMOCarcassActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only process on authority
	if (!HasAuthority())
	{
		return;
	}

	// Update decay
	TimeSinceDeath += DeltaTime;
	UpdateDecayStage();

	// Update harvest if in progress
	if (!CurrentHarvestPart.IsNone())
	{
		TickHarvest(DeltaTime);
	}
}

void AMOCarcassActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMOCarcassActor, DecayStage);
	DOREPLIFETIME(AMOCarcassActor, HarvestedParts);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

const FMOCarcassDefinitionRow* AMOCarcassActor::GetCarcassDefinitionRow() const
{
	if (!CarcassDefinition.DataTable || CarcassDefinition.RowName.IsNone())
	{
		return nullptr;
	}

	return CarcassDefinition.DataTable->FindRow<FMOCarcassDefinitionRow>(
		CarcassDefinition.RowName, TEXT("GetCarcassDefinitionRow"));
}

// ============================================================================
// STATE QUERIES
// ============================================================================

float AMOCarcassActor::GetHarvestProgress() const
{
	if (HarvestTotalTime <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(HarvestProgress / HarvestTotalTime, 0.0f, 1.0f);
}

bool AMOCarcassActor::IsPartHarvested(FName PartId) const
{
	return HarvestedParts.Contains(PartId);
}

// ============================================================================
// API
// ============================================================================

TArray<FMOCarcassPartEntry> AMOCarcassActor::GetAvailableParts() const
{
	TArray<FMOCarcassPartEntry> Available;

	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	if (!Def)
	{
		return Available;
	}

	for (const FMOCarcassPartEntry& Part : Def->Parts)
	{
		// Check if already harvested
		if (HarvestedParts.Contains(Part.PartId))
		{
			continue;
		}

		// Check if available at current decay stage
		if (!Part.IsAvailableAtStage(DecayStage))
		{
			continue;
		}

		Available.Add(Part);
	}

	return Available;
}

bool AMOCarcassActor::CanHarvestPart(FName PartId, UMOInventoryComponent* Inventory, FString& OutReason) const
{
	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	if (!Def)
	{
		OutReason = TEXT("No carcass definition");
		return false;
	}

	const FMOCarcassPartEntry* Part = Def->FindPart(PartId);
	if (!Part)
	{
		OutReason = TEXT("Part not found");
		return false;
	}

	// Already harvested?
	if (HarvestedParts.Contains(PartId))
	{
		OutReason = TEXT("Already harvested");
		return false;
	}

	// Available at decay stage?
	if (!Part->IsAvailableAtStage(DecayStage))
	{
		OutReason = TEXT("Part has decayed");
		return false;
	}

	// Currently harvesting something else?
	if (!CurrentHarvestPart.IsNone())
	{
		OutReason = TEXT("Already harvesting");
		return false;
	}

	// Tool requirement?
	if (!Part->RequiredToolType.IsNone() && Inventory)
	{
		// TODO: Check if player has tool of required type
		// For now, allow without tool check
		// if (!Inventory->HasToolOfType(Part->RequiredToolType))
		// {
		//     OutReason = FString::Printf(TEXT("Requires %s"), *Part->RequiredToolType.ToString());
		//     return false;
		// }
	}

	// Container requirement?
	if (Part->bRequiresContainer && Inventory)
	{
		// TODO: Check if player has empty container
		// if (!Inventory->HasItem(Part->ContainerItemId))
		// {
		//     OutReason = TEXT("Requires container");
		//     return false;
		// }
	}

	return true;
}

bool AMOCarcassActor::StartHarvestPart(FName PartId, AController* Harvester)
{
	if (!Harvester)
	{
		return false;
	}

	// Get inventory for validation
	UMOInventoryComponent* Inventory = nullptr;
	if (APawn* Pawn = Harvester->GetPawn())
	{
		Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>();
	}

	FString Reason;
	if (!CanHarvestPart(PartId, Inventory, Reason))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCarcass] Cannot harvest %s: %s"), *PartId.ToString(), *Reason);
		return false;
	}

	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	const FMOCarcassPartEntry* Part = Def->FindPart(PartId);

	// Start harvest
	CurrentHarvestPart = PartId;
	HarvestProgress = 0.0f;
	HarvestTotalTime = Part->HarvestTime;
	HarvesterController = Harvester;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] Started harvesting %s (%.1fs)"), *PartId.ToString(), HarvestTotalTime);

	return true;
}

void AMOCarcassActor::CancelHarvest()
{
	if (CurrentHarvestPart.IsNone())
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] Harvest cancelled for %s"), *CurrentHarvestPart.ToString());

	CurrentHarvestPart = NAME_None;
	HarvestProgress = 0.0f;
	HarvestTotalTime = 0.0f;
	HarvesterController.Reset();

	OnHarvestCancelled.Broadcast();
}

void AMOCarcassActor::InitializeFromCreature(AMOCreature* DeadCreature)
{
	if (!DeadCreature)
	{
		return;
	}

	// Copy skeletal mesh configuration and ragdoll pose
	if (USkeletalMeshComponent* CreatureMesh = DeadCreature->GetMesh())
	{
		// Set the same skeletal mesh asset
		USkeletalMesh* MeshAsset = CreatureMesh->GetSkeletalMeshAsset();
		CarcassMesh->SetSkinnedAssetAndUpdate(MeshAsset, false); // false = don't reset to ref pose

		// Copy all materials
		for (int32 i = 0; i < CreatureMesh->GetNumMaterials(); ++i)
		{
			CarcassMesh->SetMaterial(i, CreatureMesh->GetMaterial(i));
		}

		// Position carcass actor at the ragdoll's root bone location (where it settled)
		FName RootBoneName = CreatureMesh->GetBoneName(0);
		FVector RagdollPosition = CreatureMesh->GetBoneTransform(RootBoneName, RTS_World).GetLocation();
		SetActorLocation(RagdollPosition);

		// Copy bone transforms from the ragdolled creature using UPoseableMeshComponent
		// This component allows us to manually set each bone's transform
		const int32 NumBones = CreatureMesh->GetNumBones();
		for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
		{
			FName BoneName = CreatureMesh->GetBoneName(BoneIdx);
			FTransform BoneWorldTransform = CreatureMesh->GetBoneTransform(BoneName, RTS_World);

			// SetBoneTransformByName sets the bone in world space when using EBoneSpaces::WorldSpace
			CarcassMesh->SetBoneTransformByName(BoneName, BoneWorldTransform, EBoneSpaces::WorldSpace);
		}

		// Set up collision for interaction (query only, no physics simulation)
		CarcassMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CarcassMesh->SetCollisionObjectType(ECC_WorldDynamic);
		CarcassMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		CarcassMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] Copied %d bone transforms from ragdoll to poseable mesh"), NumBones);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] Initialized from creature %s"), *DeadCreature->GetName());
}

// ============================================================================
// INTERACTION HANDLERS
// ============================================================================

bool AMOCarcassActor::HandleInteract(AController* InteractorController)
{
	// Get available parts
	TArray<FMOCarcassPartEntry> Available = GetAvailableParts();
	if (Available.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] No parts available to harvest"));
		return false;
	}

	// Route through UI system for progress bar and input lockout
	if (InteractorController && InteractorController->Implements<UMOUIContractInterface>())
	{
		IMOUIContractInterface::Execute_RequestStartCarcassButchering(InteractorController, this, Available[0].PartId);
		return true;
	}

	// Fallback: direct harvest (no UI, for AI or non-player controllers)
	return StartHarvestPart(Available[0].PartId, InteractorController);
}

bool AMOCarcassActor::HandleSecondaryInteract(AController* InteractorController)
{
	// TODO: Show context menu with all available parts
	// For now, same as primary (start harvesting first available part)
	return HandleInteract(InteractorController);
}

// ============================================================================
// DECAY
// ============================================================================

void AMOCarcassActor::UpdateDecayStage()
{
	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	if (!Def)
	{
		return;
	}

	EMOCarcassDecayStage NewStage = DecayStage;

	if (DecayStage == EMOCarcassDecayStage::Fresh && TimeSinceDeath >= Def->TimeToRotting)
	{
		NewStage = EMOCarcassDecayStage::Rotting;
	}
	else if (DecayStage == EMOCarcassDecayStage::Rotting && TimeSinceDeath >= Def->TimeToRotting + Def->TimeToSkeletal)
	{
		NewStage = EMOCarcassDecayStage::Skeletal;
	}
	else if (DecayStage == EMOCarcassDecayStage::Skeletal && Def->TimeToDestroyed > 0.0f &&
		TimeSinceDeath >= Def->TimeToRotting + Def->TimeToSkeletal + Def->TimeToDestroyed)
	{
		NewStage = EMOCarcassDecayStage::Destroyed;
	}

	if (NewStage != DecayStage)
	{
		EMOCarcassDecayStage OldStage = DecayStage;
		DecayStage = NewStage;

		UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] %s decay: %d -> %d"),
			*GetName(), static_cast<int32>(OldStage), static_cast<int32>(NewStage));

		UpdateVisualForDecayStage();
		OnDecayChanged.Broadcast(NewStage);

		// Destroy if fully decayed
		if (NewStage == EMOCarcassDecayStage::Destroyed)
		{
			Destroy();
		}
	}
}

void AMOCarcassActor::UpdateVisualForDecayStage()
{
	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	if (!Def)
	{
		return;
	}

	switch (DecayStage)
	{
	case EMOCarcassDecayStage::Fresh:
		// Normal appearance
		CarcassMesh->SetVisibility(true);
		BonesMesh->SetVisibility(false);
		break;

	case EMOCarcassDecayStage::Rotting:
		// Apply rotting material if available
		CarcassMesh->SetVisibility(true);
		BonesMesh->SetVisibility(false);
		if (!Def->RottingMaterial.IsNull())
		{
			if (UMaterialInterface* RottingMat = Def->RottingMaterial.LoadSynchronous())
			{
				CarcassMesh->SetMaterial(0, RottingMat);
			}
		}
		break;

	case EMOCarcassDecayStage::Skeletal:
		// Switch to bones mesh
		CarcassMesh->SetVisibility(false);
		if (!Def->SkeletalMesh.IsNull())
		{
			if (UStaticMesh* BonesMeshAsset = Def->SkeletalMesh.LoadSynchronous())
			{
				BonesMesh->SetStaticMesh(BonesMeshAsset);
				BonesMesh->SetVisibility(true);
			}
		}
		break;

	default:
		break;
	}
}

// ============================================================================
// HARVEST
// ============================================================================

void AMOCarcassActor::TickHarvest(float DeltaTime)
{
	if (CurrentHarvestPart.IsNone())
	{
		return;
	}

	// Check if harvester is still in range
	if (!IsHarvesterInRange())
	{
		CancelHarvest();
		return;
	}

	// Update progress
	HarvestProgress += DeltaTime;

	// Broadcast progress
	OnHarvestProgress.Broadcast(HarvestProgress, HarvestTotalTime);

	// Check completion
	if (HarvestProgress >= HarvestTotalTime)
	{
		CompleteHarvest();
	}
}

void AMOCarcassActor::CompleteHarvest()
{
	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	const FMOCarcassPartEntry* Part = Def ? Def->FindPart(CurrentHarvestPart) : nullptr;

	if (!Part)
	{
		CancelHarvest();
		return;
	}

	// Give items to harvester
	if (HarvesterController.IsValid())
	{
		if (APawn* Pawn = HarvesterController->GetPawn())
		{
			if (UMOInventoryComponent* Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>())
			{
				// Get correct item based on decay stage
				FName ItemToGive = Part->GetItemForStage(DecayStage);
				int32 Quantity = Part->Quantity;

				// TODO: Consume container if required
				// if (Part->bRequiresContainer)
				// {
				//     Inventory->RemoveItemByDefinition(Part->ContainerItemId, 1);
				// }

				// Add harvested item
				FGuid NewItemGuid = FGuid::NewGuid();
				bool bAdded = Inventory->AddItemByGuid(NewItemGuid, ItemToGive, Quantity);

				UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] Harvested %s: gave %d x %s (success: %s)"),
					*CurrentHarvestPart.ToString(), Quantity, *ItemToGive.ToString(),
					bAdded ? TEXT("true") : TEXT("false"));
			}
		}
	}

	// Mark part as harvested
	FName HarvestedPart = CurrentHarvestPart;
	HarvestedParts.Add(HarvestedPart);

	// Reset state
	CurrentHarvestPart = NAME_None;
	HarvestProgress = 0.0f;
	HarvestTotalTime = 0.0f;
	HarvesterController.Reset();

	// Broadcast completion
	OnPartHarvested.Broadcast(HarvestedPart, true);

	// Check if all parts harvested
	TArray<FMOCarcassPartEntry> Remaining = GetAvailableParts();
	if (Remaining.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] All parts harvested, destroying %s"), *GetName());
		Destroy();
	}
}

bool AMOCarcassActor::IsHarvesterInRange() const
{
	if (!HarvesterController.IsValid())
	{
		return false;
	}

	APawn* Pawn = HarvesterController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	float Distance = FVector::Dist(GetActorLocation(), Pawn->GetActorLocation());
	return Distance <= MaxHarvestRange;
}
