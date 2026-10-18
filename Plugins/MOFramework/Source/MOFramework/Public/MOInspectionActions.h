// Inspecting an item to learn from it (knowledge, skill XP, recipe discoveries): the action every player uses, host and joiner alike, through
// AMOPlayerController::ServerInspectItem. Server side.
//
// Until Oct 18 2026 the inspection ran wherever it was clicked: UMOInspectionProgressWidget's UI timer called UMOKnowledgeComponent::InspectItem on that
// machine. On a joining player's machine that changed only the joiner's own copy of their knowledge and skills -- the host never had it and never
// saved it, so when the joiner left, the pawn they had played had "lost" everything they learned (Wes's Steam test: "client leaving game resets all
// skills/knowledge on pawn"). Now the SERVER checks the item is really at hand, times the study on the pawn (UMOTimedActionComponent) and inspects on
// its own copy; knowledge, skills and discoveries replicate to the owner, and the result reaches the player's UI through ClientInspectionResult.

#pragma once

#include "CoreMinimal.h"
#include "MOInstanceRef.h"

class APawn;

struct MOFRAMEWORK_API FMOInspectionActions
{
	/** The timed action's id on the pawn (UMOTimedActionComponent). */
	static const FName TimedActionId;

	/** Game seconds an inspection takes (console variable MO.Inspect.Seconds). */
	static float GetSeconds();

	/**
	 * Server: study ItemId as Inspector. The item must be at hand: the carried item ItemGuid, or a world item of that kind within reach, or the instance
	 * Instance names within reach when it gives that item (a stick on the ground, the tree its bark comes from). After the study time on the pawn,
	 * UMOKnowledgeComponent::InspectItem runs on the server's copy and the owning player is sent the result. Returns false with OutReason (what to tell
	 * the player) if it did not start.
	 */
	static bool Begin(APawn* Inspector, FName ItemId, const FGuid& ItemGuid, const FMOInstanceRef& Instance, FText& OutReason);
};
