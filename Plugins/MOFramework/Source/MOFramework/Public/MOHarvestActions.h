// A player's harvest of a resource node (strip bark, gather twigs, chop down a tree): the action every player uses, host and joiner alike, through
// AMOPlayerController::ServerBeginHarvest. Server side.
//
// Until Oct 18 2026 the harvest ran wherever the menu was clicked: the KeepOnHarvest menu started UMOHarvestProgressWidget, whose UI timer called
// UMOHarvestSubsystem::BeginHarvest / CompleteHarvest on that machine. On the host that was the authority; on a joining player's machine it worked on
// the client's own copy of the bush and its non-authoritative inventory, refused every item, and showed "Inventory Full!" (Wes's Steam test). Now the
// SERVER resolves the instance (FMOInstanceRef), runs the harvest on its own pawn and times it on the pawn (UMOTimedActionComponent: the simulation owns
// the wait, the owner sees its progress bar); the result reaches the player's UI through AMOPlayerController::ClientHarvestOutcome.

#pragma once

#include "CoreMinimal.h"
#include "MOInstanceRef.h"
#include "MOHarvestActions.generated.h"

class APawn;
struct FMOCraftResult;

/** What a finished harvest gave, for the player's notifications (an RPC parameter: no maps). */
USTRUCT()
struct MOFRAMEWORK_API FMOHarvestOutcome
{
	GENERATED_BODY()

	UPROPERTY()
	FName ActionId;

	UPROPERTY()
	TArray<FName> ProducedIds;

	UPROPERTY()
	TArray<int32> ProducedCounts;

	/** The pack had no room for some of it (the inventory's own refusal). */
	UPROPERTY()
	bool bFailed = false;

	/** Everything it gives is used up at this node. */
	UPROPERTY()
	bool bDepleted = false;

	/** Everything it gives is out of season. */
	UPROPERTY()
	bool bOutOfSeason = false;

	static FMOHarvestOutcome FromResult(FName ActionId, const FMOCraftResult& Result);
};

struct MOFRAMEWORK_API FMOHarvestActions
{
	/** The timed action's id on the pawn (UMOTimedActionComponent). */
	static const FName TimedActionId;

	/**
	 * Server: begin ActionId (a FMOResourceHarvestAction of the instance's resource node) on the instance Instance names, as Harvester. Checks reach, the
	 * action's knowledge and tool (UMOHarvestSubsystem::BeginHarvest), then waits the action's time on the pawn. On completion the yields go to the
	 * pawn's inventory (refused when it is full), a destroying action removes the instance through the removal record, and the owning player is sent
	 * the outcome. Returns false with OutReason (what to tell the player) if it did not start.
	 */
	static bool Begin(APawn* Harvester, const FMOInstanceRef& Instance, FName ActionId, FText& OutReason);
};
