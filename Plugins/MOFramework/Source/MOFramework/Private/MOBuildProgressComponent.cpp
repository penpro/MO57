#include "MOBuildProgressComponent.h"
#include "MOWorldCreationTypes.h"
#include "MOInventoryComponent.h"
#include "MORecipeDefinitionRow.h"
#include "MOFramework.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOContainerActor.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOWorldItemFactory.h"
#include "MOCharacter.h"
#include "MOSkillsComponent.h"
#include "MOMaterialSourceInterface.h"
#include "MORecipeDatabaseSettings.h"
#include "MOPlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"

const FName UMOBuildProgressComponent::ConstructionSkillId = FName("Construction");

UMOBuildProgressComponent::UMOBuildProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UMOBuildProgressComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMOBuildProgressComponent, RepState);
}

bool UMOBuildProgressComponent::CheckAuthority(const TCHAR* Caller) const
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		return true;
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] %s refused on %s: not the server (players go through UMOBuildingComponent::RequestBuildAction)"),
		Caller, *GetNameSafe(Owner));
	return false;
}

// ============================================================================
// REPLICATION
// ============================================================================

void UMOBuildProgressComponent::PublishRepState(bool bForce)
{
	if (!bForce && FMath::Abs(Progress.ElapsedTime - LastPublishedElapsed) < 1.0f)
	{
		return;
	}
	RepState.State = Progress.State;
	RepState.ElapsedTime = Progress.ElapsedTime;
	RepState.TotalBuildTime = Progress.SkillTimeScale > 0.0f ? Progress.TotalBuildTime : 0.0f;
	LastPublishedElapsed = Progress.ElapsedTime;
	RepState.Deposits.Reset(Progress.DepositedMaterials.Num());
	for (const TPair<FName, int32>& Pair : Progress.DepositedMaterials)
	{
		if (Pair.Value > 0)
		{
			RepState.Deposits.Add(FMOBuildDeposit{ Pair.Key, Pair.Value });
		}
	}
	RepState.Deposits.Sort([](const FMOBuildDeposit& A, const FMOBuildDeposit& B) { return A.ItemId.LexicalLess(B.ItemId); });
}

void UMOBuildProgressComponent::OnRep_RepState(const FMOBuildRepState& Previous)
{
	bHasRepState = true;
	ApplyRepState(Previous, /*bBroadcast=*/true);
}

void UMOBuildProgressComponent::ApplyRepState(const FMOBuildRepState& Previous, bool bBroadcast)
{
	const EMOBuildState OldState = Progress.State;
	Progress.State = RepState.State;
	Progress.ElapsedTime = RepState.ElapsedTime;
	if (RepState.TotalBuildTime > 0.0f)
	{
		Progress.TotalBuildTime = RepState.TotalBuildTime;   // the first builder's skill scaled it on the server (F31)
	}
	Progress.DepositedMaterials.Reset();
	for (const FMOBuildDeposit& Deposit : RepState.Deposits)
	{
		Progress.DepositedMaterials.Add(Deposit.ItemId, Deposit.Count);
	}
	if (Progress.State == EMOBuildState::Complete)
	{
		Progress.ElapsedTime = Progress.TotalBuildTime;
		Progress.CurrentPartProgress = 1.0f;
	}
	// Between updates a client advances the elapsed time itself (display only: completion comes from the server).
	SetComponentTickEnabled(Progress.State == EMOBuildState::Constructing);

	if (!bBroadcast)
	{
		return;
	}
	if (Previous.Deposits != RepState.Deposits)
	{
		OnDepositsChanged.Broadcast();
	}
	if (OldState != Progress.State)
	{
		// The server's own broadcasts, in its order (FinalizeConstruction: completed, then state changed).
		if (Progress.State == EMOBuildState::Constructing && OldState == EMOBuildState::Ghost)
		{
			OnConstructionStarted.Broadcast();
		}
		else if (Progress.State == EMOBuildState::Complete)
		{
			OnConstructionCompleted.Broadcast();
		}
		else if (Progress.State == EMOBuildState::Ghost)
		{
			OnConstructionCancelled.Broadcast();
		}
		OnConstructionStateChanged.Broadcast(Progress.State);
	}
	OnConstructionProgress.Broadcast(Progress.GetOverallProgress());
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOBuildProgressComponent::InitializeFromRecipe(const FMORecipeDefinitionRow& Recipe)
{
	RecipeId = Recipe.RecipeId;
	BuildParts = Recipe.BuildParts;
	Progress.TotalBuildTime = FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Building, Recipe.TotalBuildTime);   // this world's building pace (MOWorldCreationTypes.h)
	Progress.GatherRange = Recipe.BuildRange;
	Progress.State = EMOBuildState::Ghost;
	Progress.ElapsedTime = 0.0f;
	Progress.CurrentPartIndex = 0;
	Progress.CurrentPartProgress = 0.0f;

	// Initialize consumed tracking
	Progress.ConsumedPerPart.SetNum(BuildParts.Num());
	for (int32& Count : Progress.ConsumedPerPart)
	{
		Count = 0;
	}

	CalculateTotalWeight();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PublishRepState();
	}
	else if (bHasRepState)
	{
		// The recipe (the building's RecipeId) replicated after the progress did: the reset above wiped it, so take the server's again.
		ApplyRepState(RepState, /*bBroadcast=*/false);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Initialized with %d parts, total time: %.1fs"),
		BuildParts.Num(), Progress.TotalBuildTime);
}

void UMOBuildProgressComponent::CalculateTotalWeight()
{
	TotalWeight = 0;
	for (const FMOBuildPart& Part : BuildParts)
	{
		TotalWeight += Part.Quantity * Part.Weight;
	}

	if (TotalWeight > 0 && Progress.TotalBuildTime > 0.0f)
	{
		TimePerWeight = Progress.TotalBuildTime / static_cast<float>(TotalWeight);
	}
	else
	{
		TimePerWeight = 0.0f;
	}
}

// ============================================================================
// CONSTRUCTION CONTROL
// ============================================================================

void UMOBuildProgressComponent::StartConstruction(const FMOBuildProgress& Options, UMOInventoryComponent* InBuilderInventory)
{
	if (!CheckAuthority(TEXT("StartConstruction")) || Progress.State == EMOBuildState::Constructing || Progress.State == EMOBuildState::Complete)
	{
		return;
	}

	// Check if all materials are deposited
	if (!AreAllMaterialsDeposited())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] Cannot start construction - not all materials deposited"));
		return;
	}

	// Apply material source options (stored for reference)
	Progress.bDrawFromInventory = Options.bDrawFromInventory;
	Progress.bDrawFromNearbyContainers = Options.bDrawFromNearbyContainers;
	Progress.bDrawFromSurroundingArea = Options.bDrawFromSurroundingArea;
	Progress.GatherRange = Options.GatherRange;

	// Set builder's inventory
	if (InBuilderInventory)
	{
		BuilderInventory = InBuilderInventory;
	}

	// How well this builder does the work (F31): decided once, at the first start.
	if (Progress.SkillTimeScale <= 0.0f)
	{
		ApplyBuilderSkill();
	}

	// Start construction timer
	Progress.State = EMOBuildState::Constructing;

	// Enable tick
	SetComponentTickEnabled(true);

	// Subscribe to the builder's "I started moving" broadcast. The character will
	// call NotifyMovementInterrupt on us, which pauses the build while preserving
	// progress so the player can resume by interacting again.
	RegisterWithBuilderForInterrupts();

	PublishRepState();
	OnConstructionStarted.Broadcast();
	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction started! Build time: %.1fs, Progress: %.1f%%"),
		Progress.TotalBuildTime, Progress.GetOverallProgress() * 100.0f);
}

void UMOBuildProgressComponent::SetBuilderInventory(UMOInventoryComponent* InInventory)
{
	BuilderInventory = InInventory;
}

// ============================================================================
// MATERIAL DEPOSIT SYSTEM
// ============================================================================

bool UMOBuildProgressComponent::DepositMaterial(FName ItemId)
{
	if (ItemId.IsNone() || !CheckAuthority(TEXT("DepositMaterial")))
	{
		return false;
	}

	// Check if this material is needed
	int32 Required = GetRequiredCount(ItemId);
	int32 Deposited = GetDepositedCount(ItemId);

	if (Deposited >= Required)
	{
		// Already have enough of this material
		return false;
	}

	// Accept the deposit
	Progress.DepositedMaterials.FindOrAdd(ItemId)++;
	PublishRepState();
	OnDepositsChanged.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Deposited %s (%d/%d)"),
		*ItemId.ToString(), Deposited + 1, Required);

	return true;
}

int32 UMOBuildProgressComponent::GetDepositedCount(FName ItemId) const
{
	const int32* Count = Progress.DepositedMaterials.Find(ItemId);
	return Count ? *Count : 0;
}

int32 UMOBuildProgressComponent::GetRequiredCount(FName ItemId) const
{
	int32 Total = 0;
	for (const FMOBuildPart& Part : BuildParts)
	{
		if (Part.IsItemPart() && Part.ItemDefinitionId == ItemId)
		{
			Total += Part.Quantity;
		}
	}
	return Total;
}

bool UMOBuildProgressComponent::AreAllMaterialsDeposited() const
{
	for (const FMOBuildPart& Part : BuildParts)
	{
		if (Part.IsItemPart())
		{
			int32 Required = Part.Quantity;
			int32 Deposited = GetDepositedCount(Part.ItemDefinitionId);
			if (Deposited < Required)
			{
				return false;
			}
		}
	}
	return true;
}

void UMOBuildProgressComponent::GetRequiredMaterials(TArray<FName>& OutItemIds) const
{
	OutItemIds.Empty();

	for (const FMOBuildPart& Part : BuildParts)
	{
		if (Part.IsItemPart() && !OutItemIds.Contains(Part.ItemDefinitionId))
		{
			OutItemIds.Add(Part.ItemDefinitionId);
		}
	}
}

void UMOBuildProgressComponent::InterruptBuild(float PenaltyPercent)
{
	if (Progress.State != EMOBuildState::Constructing || !CheckAuthority(TEXT("InterruptBuild")))
	{
		return;
	}

	// Apply penalty - lose percentage of total build time
	float PenaltyTime = Progress.TotalBuildTime * FMath::Clamp(PenaltyPercent, 0.0f, 1.0f);
	Progress.ElapsedTime = FMath::Max(0.0f, Progress.ElapsedTime - PenaltyTime);

	// Pause construction
	Progress.State = EMOBuildState::Paused;
	SetComponentTickEnabled(false);

	// Build is no longer actively progressing; stop listening for movement.
	// ResumeConstruction will re-register if it's later resumed.
	UnregisterFromBuilderInterrupts();

	PublishRepState();
	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Build interrupted! Lost %.1f%% (%.1fs). Progress now at %.1f%%"),
		PenaltyPercent * 100.0f, PenaltyTime, Progress.GetOverallProgress() * 100.0f);
}

void UMOBuildProgressComponent::GetDepositedMaterials(TMap<FName, int32>& OutMaterials) const
{
	OutMaterials = Progress.DepositedMaterials;
}

void UMOBuildProgressComponent::PauseConstruction()
{
	if (Progress.State != EMOBuildState::Constructing || !CheckAuthority(TEXT("PauseConstruction")))
	{
		return;
	}

	Progress.State = EMOBuildState::Paused;
	SetComponentTickEnabled(false);

	// Build is no longer actively progressing; stop listening for movement.
	// ResumeConstruction will re-register.
	UnregisterFromBuilderInterrupts();

	PublishRepState();
	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction paused at %.1f%%"),
		Progress.GetOverallProgress() * 100.0f);
}

void UMOBuildProgressComponent::ResumeConstruction()
{
	if (Progress.State != EMOBuildState::Paused || !CheckAuthority(TEXT("ResumeConstruction")))
	{
		return;
	}

	Progress.State = EMOBuildState::Constructing;
	SetComponentTickEnabled(true);

	// Re-subscribe to the builder's movement broadcast so the next move pauses
	// the build again.
	RegisterWithBuilderForInterrupts();

	PublishRepState();
	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction resumed"));
}

void UMOBuildProgressComponent::CancelConstruction(bool bRefundMaterials)
{
	if (Progress.State == EMOBuildState::Complete || !CheckAuthority(TEXT("CancelConstruction")))
	{
		return;
	}

	// Refund deposited materials by spawning them as world items through the
	// canonical factory. Item definitions carry meshes, not per-item actor
	// classes, so spawning WorldVisual.WorldActorClass directly refunds
	// nothing for any item in the database.
	if (bRefundMaterials && Progress.DepositedMaterials.Num() > 0)
	{
		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		UMOWorldItemFactory* Factory = World ? World->GetSubsystem<UMOWorldItemFactory>() : nullptr;

		if (Factory && Owner)
		{
			const FVector DropLocation = Owner->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
			int32 RefundedStacks = 0;

			for (const auto& Pair : Progress.DepositedMaterials)
			{
				const FName& ItemId = Pair.Key;
				const int32 Count = Pair.Value;

				if (Count <= 0)
				{
					continue;
				}

				const FVector SpawnLocation = DropLocation + FVector(
					FMath::RandRange(-50.0f, 50.0f),
					FMath::RandRange(-50.0f, 50.0f),
					FMath::RandRange(0.0f, 30.0f)
				);

				if (Factory->SpawnDroppedItem(ItemId, Count, FGuid::NewGuid(), SpawnLocation, FRotator::ZeroRotator, Owner))
				{
					++RefundedStacks;
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] Failed to refund %d x %s"), Count, *ItemId.ToString());
				}
			}

			UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Refunded %d of %d material stacks"),
				RefundedStacks, Progress.DepositedMaterials.Num());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] Cannot refund materials - no world item factory"));
		}
	}

	// Clear deposited materials
	Progress.DepositedMaterials.Empty();

	Progress.State = EMOBuildState::Ghost;
	Progress.ElapsedTime = 0.0f;
	Progress.CurrentPartIndex = 0;
	Progress.CurrentPartProgress = 0.0f;

	for (int32& Count : Progress.ConsumedPerPart)
	{
		Count = 0;
	}

	SetComponentTickEnabled(false);

	// Build is gone; stop listening for movement on the (former) builder.
	UnregisterFromBuilderInterrupts();

	PublishRepState();
	OnDepositsChanged.Broadcast();
	OnConstructionCancelled.Broadcast();
	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction cancelled"));
}

// ============================================================================
// MATERIAL GATHERING
// ============================================================================

bool UMOBuildProgressComponent::TryGatherNextMaterial()
{
	if (Progress.CurrentPartIndex >= BuildParts.Num() || !CheckAuthority(TEXT("TryGatherNextMaterial")))
	{
		return false;
	}

	const FMOBuildPart& CurrentPart = BuildParts[Progress.CurrentPartIndex];
	return TryConsumeMaterial(CurrentPart);
}

TArray<AActor*> UMOBuildProgressComponent::FindMaterialSources(float Range) const
{
	TArray<AActor*> Sources;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return Sources;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Sources;
	}

	FVector Origin = Owner->GetActorLocation();

	// Find nearby actors that could provide materials
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	// A DROPPED world item's mesh is a PhysicsBody (AMOWorldItem::EnableDropPhysics). Until Oct 16 2026 only the item's interaction sphere
	// (WorldDynamic) made dropped materials findable here; that sphere is gone (MOWorldItem.h pitfall 3).
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	// Use a sphere overlap to find potential sources
	UKismetSystemLibrary::SphereOverlapActors(
		World,
		Origin,
		Range,
		ObjectTypes,
		nullptr,
		TArray<AActor*>{Owner}, // Ignore self
		OverlappingActors
	);

	for (AActor* Actor : OverlappingActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		// World items
		if (Actor->IsA<AMOWorldItem>())
		{
			Sources.Add(Actor);
			continue;
		}

		// Containers with inventory
		if (Actor->IsA<AMOContainerActor>())
		{
			Sources.Add(Actor);
			continue;
		}

		// Any actor with inventory component
		if (Actor->FindComponentByClass<UMOInventoryComponent>())
		{
			Sources.Add(Actor);
		}
	}

	return Sources;
}

// ============================================================================
// PLAYER REQUESTS (the ghost menu's actions, run on the server)
// ============================================================================

int32 UMOBuildProgressComponent::GatherMissingMaterials(UMOInventoryComponent* InBuilderInventory, uint8 SourceFlags)
{
	if (!CheckAuthority(TEXT("GatherMissingMaterials")) || Progress.State == EMOBuildState::Complete)
	{
		return 0;
	}
	AActor* BuilderActor = InBuilderInventory ? InBuilderInventory->GetOwner() : nullptr;
	const float Range = Progress.GatherRange > 0.0f ? Progress.GatherRange : 500.0f;
	TArray<AActor*> Sources = FindMaterialSources(Range);
	if ((SourceFlags & MOMaterialSourceBit(EMOMaterialSource::Inventory)) && BuilderActor)
	{
		Sources.AddUnique(BuilderActor);
	}
	// Priority 100 = a pawn's own inventory, 50 = a container or station, 25 = a loose item (IMOMaterialSourceInterface implementers). A pawn
	// other than the builder is never a source: in co-op that would empty another player's (or a survivor's) pockets into this building.
	Sources.RemoveAll([&](AActor* Source)
	{
		if (!IsValid(Source) || !Source->Implements<UMOMaterialSourceInterface>() || (Source->IsA<APawn>() && Source != BuilderActor))
		{
			return true;
		}
		const int32 Priority = IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(Source);
		const EMOMaterialSource Kind = Priority >= 100 ? EMOMaterialSource::Inventory : Priority >= 50 ? EMOMaterialSource::NearbyContainers : EMOMaterialSource::SurroundingArea;
		return (SourceFlags & MOMaterialSourceBit(Kind)) == 0;
	});
	Sources.StableSort([](AActor& A, AActor& B)
	{
		return IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(&A) > IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(&B);
	});

	TArray<FName> Required;
	GetRequiredMaterials(Required);
	int32 Deposited = 0;
	for (const FName& ItemId : Required)
	{
		for (AActor* Source : Sources)
		{
			while (GetDepositedCount(ItemId) < GetRequiredCount(ItemId) && IMOMaterialSourceInterface::Execute_CanProvideMaterial(Source, ItemId, 1))
			{
				if (IMOMaterialSourceInterface::Execute_GatherMaterial(Source, ItemId, 1) <= 0)
				{
					break;
				}
				Progress.DepositedMaterials.FindOrAdd(ItemId)++;
				++Deposited;
				UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Gathered %s from %s"), *ItemId.ToString(), *Source->GetName());
			}
		}
	}
	if (Deposited > 0)
	{
		PublishRepState();
		OnDepositsChanged.Broadcast();
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] GatherMissingMaterials: %d unit(s) deposited into %s (sources 0x%x)"), Deposited, *GetNameSafe(GetOwner()), SourceFlags);
	return Deposited;
}

bool UMOBuildProgressComponent::StartOrResume(UMOInventoryComponent* InBuilderInventory, uint8 SourceFlags)
{
	if (!CheckAuthority(TEXT("StartOrResume")))
	{
		return false;
	}
	FMOBuildProgress Options;
	Options.bDrawFromInventory = (SourceFlags & MOMaterialSourceBit(EMOMaterialSource::Inventory)) != 0;
	Options.bDrawFromNearbyContainers = (SourceFlags & MOMaterialSourceBit(EMOMaterialSource::NearbyContainers)) != 0;
	Options.bDrawFromSurroundingArea = (SourceFlags & MOMaterialSourceBit(EMOMaterialSource::SurroundingArea)) != 0;
	Options.GatherRange = Progress.GatherRange;   // the recipe's build range (a default-constructed option set would shrink it to 150)
	StartConstruction(Options, InBuilderInventory);   // also resumes a paused build: the elapsed time is kept
	return Progress.State == EMOBuildState::Constructing;
}

int32 UMOBuildProgressComponent::CalculateRefundAmount(int32 TotalDeposited, bool bBuildStarted, int32 ConstructionSkillLevel)
{
	if (TotalDeposited <= 0)
	{
		return 0;
	}
	if (!bBuildStarted)
	{
		return TotalDeposited;   // nothing was worked yet: everything comes back
	}
	constexpr float BaseRefund = 0.50f;
	constexpr float MaxSkillBonus = 0.45f;   // skill 0..100
	const float SkillBonus = FMath::Clamp(static_cast<float>(ConstructionSkillLevel), 0.0f, 100.0f) / 100.0f * MaxSkillBonus;
	return FMath::FloorToInt(static_cast<float>(TotalDeposited) * (BaseRefund + SkillBonus));
}

int32 UMOBuildProgressComponent::CancelAndRemove(UMOInventoryComponent* InBuilderInventory)
{
	if (!CheckAuthority(TEXT("CancelAndRemove")) || Progress.State == EMOBuildState::Complete)
	{
		return 0;
	}
	const bool bStarted = Progress.State == EMOBuildState::Constructing || Progress.State == EMOBuildState::Paused;

	// The ledger as single units, least rare first: what a partial refund loses goes first, the rarest come back.
	struct FUnit
	{
		FName ItemId;
		uint8 Rarity;
	};
	TArray<FUnit> Units;
	for (const TPair<FName, int32>& Pair : Progress.DepositedMaterials)
	{
		FMOItemDefinitionRow ItemDef;
		const uint8 Rarity = UMOItemDatabaseSettings::GetItemDefinition(Pair.Key, ItemDef) ? static_cast<uint8>(ItemDef.Rarity) : 0;
		for (int32 I = 0; I < Pair.Value; ++I)
		{
			Units.Add(FUnit{ Pair.Key, Rarity });
		}
	}
	Units.StableSort([](const FUnit& A, const FUnit& B) { return A.Rarity < B.Rarity; });

	AActor* BuilderActor = InBuilderInventory ? InBuilderInventory->GetOwner() : nullptr;
	const UMOSkillsComponent* Skills = BuilderActor ? BuilderActor->FindComponentByClass<UMOSkillsComponent>() : nullptr;
	const int32 RefundCount = CalculateRefundAmount(Units.Num(), bStarted, Skills ? Skills->GetSkillLevel(ConstructionSkillId) : 0);
	TMap<FName, int32> Refund;
	for (int32 I = Units.Num() - RefundCount; I < Units.Num(); ++I)
	{
		Refund.FindOrAdd(Units[I].ItemId)++;
	}

	// Into the builder's inventory; what it cannot take goes on the ground at the builder's feet (or beside the building) -- never destroyed.
	UMOWorldItemFactory* Factory = GetWorld() ? GetWorld()->GetSubsystem<UMOWorldItemFactory>() : nullptr;
	const AActor* DropAt = BuilderActor ? BuilderActor : GetOwner();
	int32 Returned = 0;
	for (const TPair<FName, int32>& Pair : Refund)
	{
		if (InBuilderInventory && InBuilderInventory->AddItemByGuid(FGuid::NewGuid(), Pair.Key, Pair.Value))
		{
			Returned += Pair.Value;
			continue;
		}
		if (Factory && DropAt && Factory->SpawnDroppedItem(Pair.Key, Pair.Value, FGuid::NewGuid(), DropAt->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator, BuilderActor))
		{
			Returned += Pair.Value;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] CancelAndRemove: could not return %d x %s"), Pair.Value, *Pair.Key.ToString());
		}
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] CancelAndRemove %s: %d of %d deposited unit(s) returned (started=%d)"),
		*GetNameSafe(GetOwner()), Returned, Units.Num(), bStarted ? 1 : 0);

	CancelConstruction(/*bRefundMaterials=*/false);   // the refund above replaces the full drop
	if (AActor* Owner = GetOwner())
	{
		Owner->Destroy();
	}
	return Returned;
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOBuildProgressComponent::BuildSaveData(FMOBuildProgress& OutData) const
{
	OutData = Progress;
}

void UMOBuildProgressComponent::ApplySaveData(const FMOBuildProgress& InData)
{
	if (!CheckAuthority(TEXT("ApplySaveData")))
	{
		return;
	}
	Progress = InData;

	// Resume tick if was constructing
	if (Progress.State == EMOBuildState::Constructing)
	{
		SetComponentTickEnabled(true);
	}
	PublishRepState();
}

// ============================================================================
// TICK
// ============================================================================

void UMOBuildProgressComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Progress.State != EMOBuildState::Constructing)
	{
		return;
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ProcessConstruction(DeltaTime);
		PublishRepState(/*bForce=*/false);   // the elapsed time about once a second; state changes publish themselves
	}
	else
	{
		// A client between server updates: the bar keeps moving; completion comes from the server.
		Progress.ElapsedTime = FMath::Min(Progress.ElapsedTime + DeltaTime, Progress.TotalBuildTime);
		OnConstructionProgress.Broadcast(Progress.GetOverallProgress());
	}
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOBuildProgressComponent::ProcessConstruction(float DeltaTime)
{
	// Materials are pre-deposited, so just advance the timer
	Progress.ElapsedTime += DeltaTime;

	// Unskilled work spoils material at evenly spread points (F31): the k-th of N at k / (N + 1) of the way.
	if (Progress.SpoilsLeft > 0 && Progress.TotalBuildTime > 0.0f)
	{
		const int32 Next = Progress.SpoilsPlanned - Progress.SpoilsLeft + 1;
		if (Progress.ElapsedTime / Progress.TotalBuildTime >= static_cast<float>(Next) / static_cast<float>(Progress.SpoilsPlanned + 1))
		{
			SpoilOneUnit();
			if (Progress.State != EMOBuildState::Constructing)
			{
				return;   // paused: nothing to replace the spoiled unit with
			}
		}
	}

	// Check for completion
	if (Progress.ElapsedTime >= Progress.TotalBuildTime)
	{
		FinalizeConstruction();
		return;
	}

	// Broadcast progress update
	OnConstructionProgress.Broadcast(Progress.GetOverallProgress());
}

void UMOBuildProgressComponent::ApplyBuilderSkill()
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	const AActor* Builder = BuilderInventory.IsValid() ? BuilderInventory->GetOwner() : nullptr;
	const int32 Required = (Recipe && !Recipe->RequiredSkillId.IsNone()) ? Recipe->RequiredSkillLevel : 0;
	const UMOSkillsComponent* Skills = Builder ? Builder->FindComponentByClass<UMOSkillsComponent>() : nullptr;
	// No known builder (a path without one): judged as skilled, never punished for a missing link.
	const int32 Current = !Builder ? Required : (Skills && Required > 0 ? Skills->GetSkillLevel(Recipe->RequiredSkillId) : 0);
	const FMOBuildSkillPenalty Penalty = FMOBuildSkillPenalty::For(Current, Required);

	Progress.SkillTimeScale = Penalty.TimeScale;
	Progress.TotalBuildTime *= Penalty.TimeScale;
	CalculateTotalWeight();
	int32 Units = 0;
	for (const FMOBuildPart& Part : BuildParts)
	{
		Units += Part.IsItemPart() ? Part.Quantity : 0;
	}
	Progress.SpoilsPlanned = FMath::RoundToInt(static_cast<float>(Units) * Penalty.WasteFraction);
	Progress.SpoilsLeft = Progress.SpoilsPlanned;

	if (Current < Required)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] %s builds %s at %s %d / %d: time x%.2f (%.0f s), %d of %d unit(s) will be spoiled"),
			*GetNameSafe(Builder), *RecipeId.ToString(), *Recipe->RequiredSkillId.ToString(), Current, Required, Penalty.TimeScale,
			Progress.TotalBuildTime, Progress.SpoilsPlanned, Units);
		NotifyBuilder(FText::Format(NSLOCTEXT("MOBuild", "Unskilled", "You are unskilled at this ({0} {1} / {2}): the work will take longer and waste some material."),
			FText::FromName(Recipe->RequiredSkillId), FText::AsNumber(Current), FText::AsNumber(Required)));
	}
}

void UMOBuildProgressComponent::SpoilOneUnit()
{
	if (!CheckAuthority(TEXT("SpoilOneUnit")))
	{
		return;
	}
	--Progress.SpoilsLeft;
	// The material the work uses most is the one it wastes (ties by id: the same ledger always spoils the same thing).
	FName Spoiled = NAME_None;
	int32 Most = 0;
	for (const TPair<FName, int32>& Pair : Progress.DepositedMaterials)
	{
		if (Pair.Value > Most || (Pair.Value == Most && Pair.Value > 0 && Pair.Key.LexicalLess(Spoiled)))
		{
			Spoiled = Pair.Key;
			Most = Pair.Value;
		}
	}
	if (Spoiled.IsNone())
	{
		return;
	}
	if (--Progress.DepositedMaterials[Spoiled] <= 0)
	{
		Progress.DepositedMaterials.Remove(Spoiled);
	}

	uint8 Sources = 0;
	Sources |= Progress.bDrawFromInventory ? MOMaterialSourceBit(EMOMaterialSource::Inventory) : 0;
	Sources |= Progress.bDrawFromNearbyContainers ? MOMaterialSourceBit(EMOMaterialSource::NearbyContainers) : 0;
	Sources |= Progress.bDrawFromSurroundingArea ? MOMaterialSourceBit(EMOMaterialSource::SurroundingArea) : 0;
	GatherMissingMaterials(BuilderInventory.Get(), Sources);
	const bool bReplaced = AreAllMaterialsDeposited();
	const FText ItemName = UMOItemDatabaseSettings::GetItemDisplayName(Spoiled);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] %s: unskilled work spoiled 1 %s (%d of %d) -- %s"), *GetNameSafe(GetOwner()),
		*Spoiled.ToString(), Progress.SpoilsPlanned - Progress.SpoilsLeft, Progress.SpoilsPlanned,
		bReplaced ? TEXT("replaced from the builder's sources") : TEXT("nothing to replace it: paused"));
	if (bReplaced)
	{
		NotifyBuilder(FText::Format(NSLOCTEXT("MOBuild", "SpoiledReplaced", "Your clumsy work spoiled a {0}; you used another."), ItemName));
	}
	else
	{
		NotifyBuilder(FText::Format(NSLOCTEXT("MOBuild", "SpoiledPaused", "Your clumsy work spoiled a {0}. Add another to go on."), ItemName));
		OnMaterialNeeded.Broadcast(Spoiled, 1);
		PauseConstruction();
	}
	PublishRepState(/*bForce=*/true);
	OnDepositsChanged.Broadcast();
}

void UMOBuildProgressComponent::NotifyBuilder(const FText& Message) const
{
	const APawn* Builder = BuilderInventory.IsValid() ? Cast<APawn>(BuilderInventory->GetOwner()) : nullptr;
	if (AMOPlayerController* PC = Builder ? Cast<AMOPlayerController>(Builder->GetController()) : nullptr)
	{
		PC->ClientShowNotification(Message, 4.0f, EMONotificationType::Info);
	}
}

void UMOBuildProgressComponent::CheckMaterialConsumption()
{
	if (Progress.CurrentPartIndex >= BuildParts.Num())
	{
		return;
	}

	const FMOBuildPart& CurrentPart = BuildParts[Progress.CurrentPartIndex];

	// Calculate how many items should have been consumed at current progress
	int32 RequiredConsumed = FMath::FloorToInt(Progress.CurrentPartProgress * CurrentPart.Quantity);
	int32 CurrentConsumed = Progress.ConsumedPerPart[Progress.CurrentPartIndex];

	if (RequiredConsumed > CurrentConsumed)
	{
		// Need to consume more
		int32 ToConsume = RequiredConsumed - CurrentConsumed;
		for (int32 i = 0; i < ToConsume; ++i)
		{
			if (!TryConsumeMaterial(CurrentPart))
			{
				// Failed to consume - pause construction
				OnMaterialNeeded.Broadcast(
					CurrentPart.IsItemPart() ? CurrentPart.ItemDefinitionId : CurrentPart.ActionId,
					1
				);
				PauseConstruction();
				return;
			}
		}
	}
}

bool UMOBuildProgressComponent::TryConsumeMaterial(const FMOBuildPart& Part)
{
	if (Part.IsItemPart())
	{
		FName ItemId = Part.ItemDefinitionId;
		bool bConsumed = false;

		// 1. Try builder's inventory first (if enabled)
		if (Progress.bDrawFromInventory)
		{
			if (UMOInventoryComponent* Inventory = BuilderInventory.Get())
			{
				if (Inventory->HasItem(ItemId, 1))
				{
					if (Inventory->RemoveItemByDefinitionId(ItemId, 1))
					{
						bConsumed = true;
						UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Consumed %s from player inventory"), *ItemId.ToString());
					}
				}
			}
		}

		// Cache material sources once if either nearby option is enabled
		TArray<AActor*> CachedSources;
		const bool bNeedsSources = (!bConsumed && Progress.bDrawFromNearbyContainers) ||
		                           (!bConsumed && Progress.bDrawFromSurroundingArea);
		if (bNeedsSources)
		{
			CachedSources = FindMaterialSources(Progress.GatherRange);
		}

		// 2. Try nearby containers (if enabled and not yet consumed)
		if (!bConsumed && Progress.bDrawFromNearbyContainers)
		{
			for (AActor* Source : CachedSources)
			{
				AMOContainerActor* Container = Cast<AMOContainerActor>(Source);
				if (Container)
				{
					UMOInventoryComponent* ContainerInv = Container->GetContainerInventory();
					if (ContainerInv && ContainerInv->HasItem(ItemId, 1))
					{
						if (ContainerInv->RemoveItemByDefinitionId(ItemId, 1))
						{
							bConsumed = true;
							UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Consumed %s from container: %s"),
								*ItemId.ToString(), *Container->GetName());
							break;
						}
					}
				}
			}
		}

		// 3. Try surrounding world items (if enabled and not yet consumed)
		if (!bConsumed && Progress.bDrawFromSurroundingArea)
		{
			for (AActor* Source : CachedSources)
			{
				AMOWorldItem* WorldItem = Cast<AMOWorldItem>(Source);
				if (WorldItem)
				{
					UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
					if (ItemComp && ItemComp->ItemDefinitionId == ItemId && ItemComp->bWorldItemActive)
					{
						int32 Available = ItemComp->Quantity;
						if (Available > 0)
						{
							// Take one from the world item
							if (Available == 1)
							{
								// Deactivate or destroy the world item
								ItemComp->SetWorldItemActive(false);
								WorldItem->Destroy();
							}
							else
							{
								ItemComp->Quantity = Available - 1;
							}
							bConsumed = true;
							UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Consumed %s from world item: %s"),
								*ItemId.ToString(), *WorldItem->GetName());
							break;
						}
					}
				}
			}
		}

		if (bConsumed)
		{
			if (Progress.CurrentPartIndex < Progress.ConsumedPerPart.Num())
			{
				Progress.ConsumedPerPart[Progress.CurrentPartIndex]++;
			}
			return true;
		}

		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildProgressComponent] Could not find item: %s"), *ItemId.ToString());
		return false;
	}
	else if (Part.IsActionPart())
	{
		// Action parts don't require material consumption
		// They represent labor like digging, hammering, etc.
		if (Progress.CurrentPartIndex < Progress.ConsumedPerPart.Num())
		{
			Progress.ConsumedPerPart[Progress.CurrentPartIndex]++;
		}

		// DEBUG (MOBuildProgressComponent, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOBuildProgressComponent] Performed action: %s"),
		// 	*Part.ActionId.ToString());
		return true;
	}

	return false;
}

void UMOBuildProgressComponent::CompleteCurrentPart()
{
	if (Progress.CurrentPartIndex < BuildParts.Num())
	{
		OnPartCompleted.Broadcast(Progress.CurrentPartIndex, BuildParts[Progress.CurrentPartIndex]);

		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Completed part %d"),
			Progress.CurrentPartIndex);
	}
}

void UMOBuildProgressComponent::FinalizeConstruction()
{
	// Complete any remaining parts
	for (int32 i = Progress.CurrentPartIndex; i < BuildParts.Num(); ++i)
	{
		Progress.CurrentPartIndex = i;
		CompleteCurrentPart();
	}

	Progress.State = EMOBuildState::Complete;
	Progress.ElapsedTime = Progress.TotalBuildTime;
	Progress.CurrentPartProgress = 1.0f;

	SetComponentTickEnabled(false);

	// Who finished it, before the builder references go (quests credit this pawn).
	CompletedBy = BuilderInventory.IsValid() ? BuilderInventory->GetOwner() : static_cast<AActor*>(RegisteredBuilderCharacter.Get());

	// Construction is done; no need for further movement notifications.
	UnregisterFromBuilderInterrupts();

	PublishRepState();
	OnConstructionCompleted.Broadcast();
	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction complete!"));
}

// ============================================================================
// INTERRUPT HANDLING (IMOInterruptibleInterface)
// ============================================================================

void UMOBuildProgressComponent::NotifyInterrupt_Implementation(const FMOInterruptContext& Context)
{
	// Only meaningful while actually constructing. Defensive — the unregister
	// calls in PauseConstruction/CancelConstruction/FinalizeConstruction should
	// have already taken us off the listener list in other states.
	if (Progress.State != EMOBuildState::Constructing)
	{
		return;
	}

	// Per-reason policy. The CLAUDE.md "Engineering Principles" rule applies
	// here: this listener is the right place to encode "what does interrupt
	// X mean for building?" — not the broadcaster, not the source.
	switch (Context.Reason)
	{
	case EMOInterruptReason::Movement:
	case EMOInterruptReason::Knockdown:
	case EMOInterruptReason::Unconscious:
	case EMOInterruptReason::EnteredCombat:
	case EMOInterruptReason::LostControl:
	case EMOInterruptReason::External:
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOBuildProgressComponent] Interrupt reason=%d — pausing construction (progress preserved at %.1f%%)"),
			(int32)Context.Reason, Progress.GetOverallProgress() * 100.0f);
		PauseConstruction();
		break;

	case EMOInterruptReason::Damage:
		// Only pause on substantial damage. Trivial hits (a stubbed toe)
		// shouldn't break a 30-minute build. Threshold can be tuned.
		if (Context.Severity >= 0.5f)
		{
			UE_LOG(LogMOFramework, Log,
				TEXT("[MOBuildProgressComponent] Severe damage (severity=%.2f) — pausing construction"),
				Context.Severity);
			PauseConstruction();
		}
		break;

	case EMOInterruptReason::Death:
		// Builder is gone. Don't try to refund — that's the death pipeline's
		// concern. Just stop the timer and unregister.
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOBuildProgressComponent] Builder died — cancelling construction (no refund here; death pipeline owns cleanup)"));
		CancelConstruction(/*bRefundMaterials=*/false);
		break;

	case EMOInterruptReason::UserCancel:
		// UI paths call CancelConstruction directly; do nothing here to avoid
		// double-firing the cancel.
		break;

	case EMOInterruptReason::None:
	default:
		break;
	}
}

void UMOBuildProgressComponent::RegisterWithBuilderForInterrupts()
{
	// BuilderInventory must be set first; resolve the builder character from it.
	UMOInventoryComponent* Inv = BuilderInventory.Get();
	if (!IsValid(Inv))
	{
		return;
	}

	AMOCharacter* Builder = Cast<AMOCharacter>(Inv->GetOwner());
	if (!IsValid(Builder))
	{
		return;
	}

	Builder->RegisterInterruptListener(this);
	RegisteredBuilderCharacter = Builder;
}

void UMOBuildProgressComponent::UnregisterFromBuilderInterrupts()
{
	if (AMOCharacter* Builder = RegisteredBuilderCharacter.Get())
	{
		Builder->UnregisterInterruptListener(this);
	}
	RegisteredBuilderCharacter.Reset();
}
