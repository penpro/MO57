#include "MOHarvestActions.h"
#include "MOHarvestSubsystem.h"
#include "MOCraftingSubsystem.h"
#include "MOInteractionSubsystem.h"
#include "MOInteractorComponent.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOTimedActionComponent.h"
#include "MOPlayerController.h"
#include "MOResourceDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "MOHarvestActions"

const FName FMOHarvestActions::TimedActionId(TEXT("Harvest"));

FMOHarvestOutcome FMOHarvestOutcome::FromResult(FName InActionId, const FMOCraftResult& Result)
{
	FMOHarvestOutcome Out;
	Out.ActionId = InActionId;
	for (const TPair<FName, int32>& Pair : Result.ProducedItems)
	{
		Out.ProducedIds.Add(Pair.Key);
		Out.ProducedCounts.Add(Pair.Value);
	}
	Out.bFailed = Result.HasFailedItems();
	Out.bDropped = Result.DroppedItems.Num() > 0;
	Out.bDepleted = Result.HasDepletedItems();
	Out.bOutOfSeason = Result.HasOutOfSeasonItems();
	return Out;
}

bool FMOHarvestActions::Begin(APawn* Harvester, const FMOInstanceRef& Instance, FName ActionId, FText& OutReason)
{
	UWorld* World = Harvester ? Harvester->GetWorld() : nullptr;
	UMOTimedActionComponent* Timed = Harvester ? Harvester->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	UMOInventoryComponent* Inventory = Harvester ? Harvester->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	UMOHarvestSubsystem* Harvest = World ? World->GetSubsystem<UMOHarvestSubsystem>() : nullptr;
	OutReason = LOCTEXT("CannotHarvest", "You can't do that here.");
	if (!Harvester || !Harvester->HasAuthority() || !Timed || !Inventory || !Harvest)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] %s: harvest refused: needs the server, a timed-action component, an inventory and the harvest subsystem"),
			*GetNameSafe(Harvester));
		return false;
	}

	// The server's own copy of the plant / tree / rock the player chose.
	UInstancedStaticMeshComponent* ISM = nullptr;
	int32 Index = INDEX_NONE;
	if (!Instance.Resolve(World, ISM, Index))
	{
		OutReason = LOCTEXT("NothingThere", "Nothing to harvest there.");
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] %s: '%s' refused: no %s here (taken already?)"), *Harvester->GetName(), *ActionId.ToString(), *Instance.ToString());
		return false;
	}

	// Reach and sight: the rule every instance request shares (UMOInteractorComponent::ServerCheckInstanceRequest; the pickup RPC's too).
	FString Why;
	const UMOInteractorComponent::EInstanceRequestCheck Check = UMOInteractorComponent::ServerCheckInstanceRequest(Harvester, ISM, Index, Why);
	if (Check != UMOInteractorComponent::EInstanceRequestCheck::Ok)
	{
		OutReason = Check == UMOInteractorComponent::EInstanceRequestCheck::TooFar ? LOCTEXT("TooFar", "Too far away: step closer.")
			: Check == UMOInteractorComponent::EInstanceRequestCheck::OutOfSight ? LOCTEXT("OutOfSight", "You can't see that from here.")
			: LOCTEXT("NothingThere", "Nothing to harvest there.");
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] %s: '%s' refused: %s -- %s"), *Harvester->GetName(), *ActionId.ToString(), *Instance.ToString(), *Why);
		return false;
	}

	// Knowledge: the server's own copy of the harvester (the tool is checked by BeginHarvest).
	const FMOResourceNodeDefinitionRow* Resource = Harvest->GetResourceDefinitionForComponent(ISM);
	const FMOResourceHarvestAction* Action = Resource ? Resource->FindAction(ActionId) : nullptr;
	if (Action && !UMOHarvestSubsystem::HasKnowledgeForAction(*Action, Harvester))
	{
		OutReason = LOCTEXT("DontKnowHow", "You don't know how to do that yet.");
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] %s: '%s' refused: needs knowledge '%s'"), *Harvester->GetName(), *ActionId.ToString(),
			*Action->RequiredKnowledgeId.ToString());
		return false;
	}
	if (!Harvest->BeginHarvest(ISM, Index, ActionId, Inventory))
	{
		OutReason = Action && !UMOHarvestSubsystem::HasToolForAction(*Action, Inventory)
			? UMOItemDatabaseSettings::DescribeToolNeed(Action->RequiredToolType, Action->MinToolQuality) : OutReason;
		return false;   // BeginHarvest logged why
	}
	const FMOHarvestContext* Ctx = Harvest->GetHarvestContext(Harvester);
	const float Seconds = Ctx ? Ctx->TotalTime : 1.0f;
	const FText Label = Action ? FText::Format(LOCTEXT("HarvestingFormat", "{0}..."), Action->DisplayName) : LOCTEXT("Harvesting", "Harvesting...");

	TWeakObjectPtr<APawn> WeakPawn(Harvester);
	TWeakObjectPtr<UMOHarvestSubsystem> WeakHarvest(Harvest);
	const bool bBegun = Timed->Begin(TimedActionId, Label, Seconds,
		[WeakPawn, WeakHarvest, ActionId]()
		{
			APawn* Pawn = WeakPawn.Get();
			UMOHarvestSubsystem* H = WeakHarvest.Get();
			if (!Pawn || !H)
			{
				return;
			}
			// The pawn waited the action's time on the game clock: the subsystem's own wall-clock gate is for callers that did not.
			const FMOCraftResult Result = H->CompleteHarvest(Pawn->FindComponentByClass<UMOInventoryComponent>(), Pawn->FindComponentByClass<UMOSkillsComponent>(),
				/*bAllowSkipTimer=*/true);
			if (AMOPlayerController* PC = Cast<AMOPlayerController>(Pawn->GetController()))
			{
				PC->ClientHarvestOutcome(FMOHarvestOutcome::FromResult(ActionId, Result));
			}
		},
		[WeakPawn, WeakHarvest]()
		{
			if (UMOHarvestSubsystem* H = WeakHarvest.Get())
			{
				H->CancelHarvest(WeakPawn.Get());
			}
		},
		/*MaxDriftCm=*/75.0f,
		[WeakPawn, WeakHarvest]()
		{
			// The harvest subsystem cancels on its own interrupts (movement, damage): then the wait ends too.
			const UMOHarvestSubsystem* H = WeakHarvest.Get();
			return H && WeakPawn.IsValid() && H->IsHarvestInProgress(WeakPawn.Get());
		});
	if (!bBegun)
	{
		Harvest->CancelHarvest(Harvester);
		return false;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] %s: '%s' on %s (%.1f s, server-timed)"), *Harvester->GetName(), *ActionId.ToString(), *Instance.ToString(), Seconds);
	return true;
}

#undef LOCTEXT_NAMESPACE
