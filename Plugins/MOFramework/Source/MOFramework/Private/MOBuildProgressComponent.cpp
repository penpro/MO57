#include "MOBuildProgressComponent.h"
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
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

UMOBuildProgressComponent::UMOBuildProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOBuildProgressComponent::InitializeFromRecipe(const FMORecipeDefinitionRow& Recipe)
{
	RecipeId = Recipe.RecipeId;
	BuildParts = Recipe.BuildParts;
	Progress.TotalBuildTime = Recipe.TotalBuildTime;
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
	if (Progress.State == EMOBuildState::Constructing)
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

	// Start construction timer
	Progress.State = EMOBuildState::Constructing;

	// Enable tick
	SetComponentTickEnabled(true);

	// Subscribe to the builder's "I started moving" broadcast. The character will
	// call NotifyMovementInterrupt on us, which pauses the build while preserving
	// progress so the player can resume by interacting again.
	RegisterWithBuilderForInterrupts();

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
	if (ItemId.IsNone())
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
	if (Progress.State != EMOBuildState::Constructing)
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
	if (Progress.State != EMOBuildState::Constructing)
	{
		return;
	}

	Progress.State = EMOBuildState::Paused;
	SetComponentTickEnabled(false);

	// Build is no longer actively progressing; stop listening for movement.
	// ResumeConstruction will re-register.
	UnregisterFromBuilderInterrupts();

	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction paused at %.1f%%"),
		Progress.GetOverallProgress() * 100.0f);
}

void UMOBuildProgressComponent::ResumeConstruction()
{
	if (Progress.State != EMOBuildState::Paused)
	{
		return;
	}

	Progress.State = EMOBuildState::Constructing;
	SetComponentTickEnabled(true);

	// Re-subscribe to the builder's movement broadcast so the next move pauses
	// the build again.
	RegisterWithBuilderForInterrupts();

	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction resumed"));
}

void UMOBuildProgressComponent::CancelConstruction(bool bRefundMaterials)
{
	if (Progress.State == EMOBuildState::Complete)
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

	OnConstructionCancelled.Broadcast();
	OnConstructionStateChanged.Broadcast(Progress.State);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction cancelled"));
}

// ============================================================================
// MATERIAL GATHERING
// ============================================================================

bool UMOBuildProgressComponent::TryGatherNextMaterial()
{
	if (Progress.CurrentPartIndex >= BuildParts.Num())
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
// SAVE/LOAD
// ============================================================================

void UMOBuildProgressComponent::BuildSaveData(FMOBuildProgress& OutData) const
{
	OutData = Progress;
}

void UMOBuildProgressComponent::ApplySaveData(const FMOBuildProgress& InData)
{
	Progress = InData;

	// Resume tick if was constructing
	if (Progress.State == EMOBuildState::Constructing)
	{
		SetComponentTickEnabled(true);
	}
}

// ============================================================================
// TICK
// ============================================================================

void UMOBuildProgressComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Progress.State == EMOBuildState::Constructing)
	{
		ProcessConstruction(DeltaTime);
	}
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOBuildProgressComponent::ProcessConstruction(float DeltaTime)
{
	// Materials are pre-deposited, so just advance the timer
	Progress.ElapsedTime += DeltaTime;

	// Check for completion
	if (Progress.ElapsedTime >= Progress.TotalBuildTime)
	{
		FinalizeConstruction();
		return;
	}

	// Broadcast progress update
	OnConstructionProgress.Broadcast(Progress.GetOverallProgress());
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

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOBuildProgressComponent] Performed action: %s"),
			*Part.ActionId.ToString());
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

	// Construction is done; no need for further movement notifications.
	UnregisterFromBuilderInterrupts();

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
