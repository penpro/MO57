/**
 * =============================================================================
 * MOTravelUtils.h - Net-mode-aware level travel
 * =============================================================================
 *
 * PURPOSE:
 * ONE chokepoint for "travel into a gameplay level" so single-player and
 * co-op behave correctly from the same call sites.
 *
 * WHY (evidence, 2026-07-03 `ue.py mptest`): the new-game/load flows called
 * UGameplayStatics::OpenLevel directly. OpenLevel is a LOCAL travel — on a
 * listen server it silently drops the server role and disconnects every
 * client: pre-travel the session was `ListenServer PCs=2`, post-travel
 * `Standalone PCs=1`. Co-op could not survive starting or loading a game.
 * (Charter Pillar 1A; found by the first 2-client harness boot.)
 *
 * BEHAVIOR:
 *  - NM_Standalone       -> UGameplayStatics::OpenLevel (single-player path,
 *                           byte-identical to the old behavior)
 *  - NM_ListenServer /
 *    NM_DedicatedServer  -> UWorld::ServerTravel(Path + "?listen") — keeps
 *                           the server role and brings connected clients
 *  - NM_Client           -> refused + logged (clients never initiate travel;
 *                           the server brings them along)
 *
 * USE THIS instead of OpenLevel for any menu->game or game->game travel.
 * Deliberate exception: host "exit to main menu" stays OpenLevel — tearing
 * the session down IS the intent there (proper co-op session teardown is the
 * session layer's job, charter Move 3).
 *
 * [2026-09] bAsListenServer PARAM: the one legitimate Standalone -> hosting
 * transition (MOSessionSubsystem::HostSession, right after CreateSession
 * succeeds) needs OpenLevel's "listen" URL option to spin up a net driver —
 * plain OpenLevel (the default NM_Standalone branch) never does this, so
 * hosting would silently stay single-player without it. Kept as an opt-in
 * param on the SAME chokepoint rather than a second travel function, so
 * there's still exactly one place that decides how travel behaves per net
 * mode.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOTravelUtils.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOTravelUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Travel into a gameplay level, preserving the listen-server role (and
	 * connected clients) when hosting. See file header for the full contract.
	 * @param bAsListenServer  Only meaningful from NM_Standalone: if true, opens
	 *        the level with the "listen" URL option to become a listen server
	 *        instead of a plain single-player OpenLevel. Ignored (a no-op) once
	 *        already networked — ServerTravel is used instead in that case.
	 * @return true if a travel was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Travel", meta=(WorldContext="WorldContextObject"))
	static bool TravelToGameplayLevel(UObject* WorldContextObject, const FString& LevelPath, bool bAsListenServer = false);
};
