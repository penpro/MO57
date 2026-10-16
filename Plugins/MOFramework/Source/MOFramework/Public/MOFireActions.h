// Tending a fuelled station (a campfire, later a hearth or kiln): add fuel, light it, put it out. Server-side and on the pawn's timed action, so a co-op client's
// fire is the server's fire and a survivor can tend one too. The station menu lists GetActions; AMOPlayerController::ServerStationAction runs Perform.
//
// Lighting is a real attempt, not a switch (Wes: "check online for actual primitive survival methods"): a friction fire into a tinder bundle, made with the best
// fire-starting tool carried (any item with the Firestarter tool capability -- a bow drill, a fire-drill kit; its effectiveness is the base chance and it wears a
// little each attempt) or, with none, an improvised hand drill twirled from one dry branch on another. It can fail; tinder is used up either way; rain and wet
// hands make it much harder; practice (Firemaking) makes it easier.

#pragma once

#include "CoreMinimal.h"
#include "MOItemActions.h"

class AMOCraftingStationActor;
class APawn;

struct MOFRAMEWORK_API FMOFireActions
{
	static const FName AddFuel;
	static const FName Light;
	static const FName PutOut;

	static void GetActions(const APawn* Pawn, const AMOCraftingStationActor* Station, TArray<FMOItemActionOption>& Out);

	/** Server: re-checks, then starts the pawn's timed action (or applies at once). False if refused. */
	static bool Perform(APawn* Pawn, AMOCraftingStationActor* Station, FName ActionId);

	/** An improvised hand drill (two dry branches, no kit): the base chance it brings. */
	static constexpr float ImprovisedDrillEffectiveness = 0.2f;

	/**
	 * The chance one lighting attempt catches (0-1): the tool's Firestarter effectiveness (improvised 0.2, a bow drill 0.7), +0.06 per Firemaking level, x0.3 in
	 * rain, x0.6 with wet hands (wetness over half). Capped at 0.95: friction fire always can fail. Pure, for the tests.
	 */
	static float LightChance(float ToolEffectiveness, int32 FiremakingLevel, bool bRaining, float Wetness01);
};
