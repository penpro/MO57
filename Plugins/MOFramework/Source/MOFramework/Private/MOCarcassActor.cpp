#include "MOCarcassActor.h"
#include "MOWorldCreationTypes.h"
#include "MOFramework.h"
#include "MOCarcassDefinitionRow.h"
#include "MOInteractableComponent.h"
#include "MOIdentityComponent.h"
#include "MOInventoryComponent.h"
#include "MOUIContractInterface.h"
#include "MOCreature.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOPlayerController.h"
#include "MOTimedActionComponent.h"
#include "MOUIUtils.h"
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
	PrimaryActorTick.TickInterval = 1.0f; // decay only: the cutting is timed on the butcher's pawn (UMOTimedActionComponent)
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
}

void AMOCarcassActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMOCarcassActor, CarcassDefinition);
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

const FName AMOCarcassActor::ButcherAllAction(TEXT("Carcass.ButcherAll"));

bool AMOCarcassActor::CanPawnCutPart(const APawn* Pawn, FName PartId, FText& OutReason) const
{
	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	const FMOCarcassPartEntry* Part = Def ? Def->FindPart(PartId) : nullptr;
	if (!Part || !Pawn)
	{
		OutReason = NSLOCTEXT("MOCarcass", "NoPart", "There is no such part.");
		return false;
	}
	if (HarvestedParts.Contains(PartId))
	{
		OutReason = NSLOCTEXT("MOCarcass", "Taken", "Already taken.");
		return false;
	}
	if (!Part->IsAvailableAtStage(DecayStage))
	{
		OutReason = NSLOCTEXT("MOCarcass", "Decayed", "It has rotted away.");
		return false;
	}
	if (const TWeakObjectPtr<APawn>* Cutter = PartsInHand.Find(PartId); Cutter && Cutter->IsValid() && Cutter->Get() != Pawn)
	{
		OutReason = NSLOCTEXT("MOCarcass", "InHand", "Someone else is cutting this.");
		return false;
	}
	const UMOInventoryComponent* Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>();
	if (!Part->RequiredToolType.IsNone())
	{
		// The data names the tool (FName "Knife"); any carried item with that tool capability does (a flint flake cuts hide too).
		const UEnum* ToolEnum = StaticEnum<EMOToolType>();
		const int64 Tool = ToolEnum->GetValueByNameString(Part->RequiredToolType.ToString());
		if (Tool != INDEX_NONE && Tool != static_cast<int64>(EMOToolType::None)
			&& (!Inventory || Inventory->GetBestToolEffectiveness(static_cast<EMOToolType>(Tool)) <= 0.0f))
		{
			OutReason = UMOItemDatabaseSettings::DescribeToolNeed(static_cast<EMOToolType>(Tool));
			return false;
		}
	}
	if (Part->bRequiresContainer && !Part->ContainerItemId.IsNone() && (!Inventory || Inventory->CountIngredient(Part->ContainerItemId) <= 0))
	{
		OutReason = FText::Format(NSLOCTEXT("MOCarcass", "NeedsContainer", "Needs {0} to catch it."), UMOItemDatabaseSettings::GetItemDisplayName(Part->ContainerItemId));
		return false;
	}
	return true;
}

FText AMOCarcassActor::GetWorldActionTitle() const
{
	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	return Def ? Def->DisplayName : NSLOCTEXT("MOCarcass", "Carcass", "Carcass");
}

void AMOCarcassActor::GetWorldActions(const APawn* Pawn, TArray<FMOItemActionOption>& Out) const
{
	float AllSeconds = 0.0f;
	int32 Cuttable = 0;
	TArray<FMOItemActionOption> Parts;
	for (const FMOCarcassPartEntry& Part : GetAvailableParts())
	{
		FMOItemActionOption Option;
		Option.Id = Part.PartId;
		const float CutSeconds = FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, Part.HarvestTime);   // this world's gathering pace
		Option.Label = FText::Format(NSLOCTEXT("MOCarcass", "CutPart", "{0} ({1})"), Part.DisplayName, UMOUIUtils::FormatDurationAsText(CutSeconds));
		Option.bEnabled = CanPawnCutPart(Pawn, Part.PartId, Option.DisabledReason);
		if (Option.bEnabled)
		{
			AllSeconds += CutSeconds;
			++Cuttable;
		}
		Parts.Add(MoveTemp(Option));
	}
	// What can be cut now first (a deer lists 13 parts; the greyed ones -- no knife -- would push them below the fold), the data's order within each.
	Parts.StableSort([](const FMOItemActionOption& A, const FMOItemActionOption& B) { return A.bEnabled && !B.bEnabled; });
	if (Cuttable > 1)
	{
		// One gesture, the whole job: the cuts still take their full time, one after another (tedium is not simulation).
		FMOItemActionOption All;
		All.Id = ButcherAllAction;
		All.Label = FText::Format(NSLOCTEXT("MOCarcass", "ButcherAll", "Butcher everything ({0})"), UMOUIUtils::FormatDurationAsText(AllSeconds));
		Out.Add(MoveTemp(All));
	}
	Out.Append(MoveTemp(Parts));
}

bool AMOCarcassActor::PerformWorldAction(APawn* Pawn, FName ActionId)
{
	if (!HasAuthority() || !IsValid(Pawn))
	{
		return false;
	}
	if (ActionId == ButcherAllAction)
	{
		const FName First = FirstCuttablePart(Pawn);
		return !First.IsNone() && BeginCut(Pawn, First, /*bThenTheRest=*/true);
	}
	return BeginCut(Pawn, ActionId, /*bThenTheRest=*/false);
}

FName AMOCarcassActor::FirstCuttablePart(const APawn* Pawn) const
{
	for (const FMOCarcassPartEntry& Part : GetAvailableParts())
	{
		FText Reason;
		if (CanPawnCutPart(Pawn, Part.PartId, Reason))
		{
			return Part.PartId;
		}
	}
	return NAME_None;
}

bool AMOCarcassActor::BeginCut(APawn* Pawn, FName PartId, bool bThenTheRest)
{
	FText Reason;
	if (!CanPawnCutPart(Pawn, PartId, Reason))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCarcass] %s cannot cut %s from %s: %s"), *GetNameSafe(Pawn), *PartId.ToString(), *GetName(), *Reason.ToString());
		if (AMOPlayerController* PC = Pawn ? Cast<AMOPlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->ClientShowNotification(Reason, 3.0f, EMONotificationType::Warning);
		}
		return false;
	}
	UMOTimedActionComponent* Timed = Pawn->FindComponentByClass<UMOTimedActionComponent>();
	const FMOCarcassPartEntry* Part = GetCarcassDefinitionRow()->FindPart(PartId);   // CanPawnCutPart found it
	if (!Timed)
	{
		return false;
	}
	// A pawn cuts one part at a time: whatever it held before is free again.
	for (auto It = PartsInHand.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid() || It.Value().Get() == Pawn)
		{
			It.RemoveCurrent();
		}
	}
	PartsInHand.Add(PartId, Pawn);

	TWeakObjectPtr<AMOCarcassActor> WeakThis(this);
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	const bool bBegun = Timed->BeginKneeling(FName(TEXT("Butcher")), FText::Format(NSLOCTEXT("MOCarcass", "Cutting", "Butchering: {0}"), Part->DisplayName),
		FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, Part->HarvestTime),
		[WeakThis, WeakPawn, PartId, bThenTheRest]()
		{
			AMOCarcassActor* Carcass = WeakThis.Get();
			APawn* Butcher = WeakPawn.Get();
			if (!Carcass || !Butcher)
			{
				return;
			}
			Carcass->FinishCut(Butcher, PartId);
			if (bThenTheRest && IsValid(Carcass))
			{
				const FName Next = Carcass->FirstCuttablePart(Butcher);
				if (!Next.IsNone())
				{
					Carcass->BeginCut(Butcher, Next, /*bThenTheRest=*/true);
				}
			}
		},
		[WeakThis, WeakPawn, PartId]()
		{
			if (AMOCarcassActor* Carcass = WeakThis.Get())
			{
				const TWeakObjectPtr<APawn>* Cutter = Carcass->PartsInHand.Find(PartId);
				if (Cutter && *Cutter == WeakPawn)
				{
					Carcass->PartsInHand.Remove(PartId);
				}
			}
		});
	if (!bBegun)
	{
		PartsInHand.Remove(PartId);
		return false;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] %s cuts %s from %s (%.1fs)%s"), *Pawn->GetName(), *PartId.ToString(), *GetName(), Part->HarvestTime,
		bThenTheRest ? TEXT(", then the rest") : TEXT(""));
	return true;
}

void AMOCarcassActor::FinishCut(APawn* Pawn, FName PartId)
{
	PartsInHand.Remove(PartId);
	const FMOCarcassDefinitionRow* Def = GetCarcassDefinitionRow();
	const FMOCarcassPartEntry* Part = Def ? Def->FindPart(PartId) : nullptr;
	if (!Part || HarvestedParts.Contains(PartId) || !Part->IsAvailableAtStage(DecayStage))
	{
		return;   // gone in the meantime (rotted)
	}
	const FName ItemId = Part->GetItemForStage(DecayStage);
	if (UMOInventoryComponent* Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>())
	{
		if (Part->bRequiresContainer && !Part->ContainerItemId.IsNone())
		{
			Inventory->ConsumeIngredient(Part->ContainerItemId, 1);   // the container now holds it: it is the item given below
		}
		const FGuid Guid = FGuid::NewGuid();
		if (!Inventory->AddItemByGuid(Guid, ItemId, Part->Quantity))
		{
			// Hands full: it goes on the ground beside the carcass, not into nothing.
			const FVector Beside = GetActorLocation() + (Pawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 60.0f + FVector(0.0f, 0.0f, 30.0f);
			Inventory->SpawnWorldItem(ItemId, Part->Quantity, Guid, Beside, FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f));
		}
		if (AMOPlayerController* PC = Cast<AMOPlayerController>(Pawn->GetController()))
		{
			PC->ClientShowNotification(FText::Format(NSLOCTEXT("MOCarcass", "Got", "{0} x{1}"), UMOItemDatabaseSettings::GetItemDisplayName(ItemId), FText::AsNumber(Part->Quantity)),
				2.5f, EMONotificationType::ItemPickup);
		}
	}
	HarvestedParts.Add(PartId);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] %s cut %s from %s: %d x %s"), *Pawn->GetName(), *PartId.ToString(), *GetName(), Part->Quantity, *ItemId.ToString());
	OnPartHarvested.Broadcast(PartId, true);
	if (GetAvailableParts().Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] All parts harvested, destroying %s"), *GetName());
		Destroy();
	}
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
	APawn* Pawn = InteractorController ? InteractorController->GetPawn() : nullptr;
	if (!Pawn || GetAvailableParts().Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] No parts available to harvest"));
		return false;
	}
	// E: get on with the next cut. When the pawn can cut nothing (no knife ...), the menu shows each part and why.
	const FName Part = FirstCuttablePart(Pawn);
	if (!Part.IsNone())
	{
		return BeginCut(Pawn, Part, /*bThenTheRest=*/false);
	}
	return HandleSecondaryInteract(InteractorController);
}

bool AMOCarcassActor::HandleSecondaryInteract(AController* InteractorController)
{
	// Right-click: the parts to choose from (Steam playtest Oct 16 2026: it used to start butchering straight away).
	if (InteractorController && InteractorController->Implements<UMOUIContractInterface>())
	{
		IMOUIContractInterface::Execute_RequestShowWorldActionMenu(InteractorController, this, GetActorLocation());
		return true;
	}
	return false;
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
