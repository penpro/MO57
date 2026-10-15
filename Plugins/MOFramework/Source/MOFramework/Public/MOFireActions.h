// Tending a fuelled station (a campfire, later a hearth or kiln): add fuel, light it, put it out. Server-side and on the pawn's timed action, so a co-op client's
// fire is the server's fire and a survivor can tend one too. The station menu lists GetActions; AMOPlayerController::ServerStationAction runs Perform.
//
// Lighting is a real attempt, not a switch (Wes: "check online for actual primitive survival methods"): a friction fire -- a hand drill twirled from a dry stick on
// a second one, or a bow drill once there is one -- into a tinder bundle. It can fail; tinder is used up either way; rain and wet hands make it much harder;
// practice (Firemaking) makes it easier.

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

	/**
	 * The chance one lighting attempt catches (0-1): the method's base (hand drill 0.3, bow drill 0.6), +0.06 per Firemaking level, x0.3 in rain, x0.6 with wet
	 * hands (wetness over half). Capped at 0.95: friction fire always can fail. Pure, for the tests.
	 */
	static float LightChance(bool bBowDrill, int32 FiremakingLevel, bool bRaining, float Wetness01);
};
