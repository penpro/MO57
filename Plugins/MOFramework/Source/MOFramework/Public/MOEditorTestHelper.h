/**
 * =============================================================================
 * MOEditorTestHelper.h - PIE multi-world access for the autonomous test loop
 * =============================================================================
 *
 * PURPOSE:
 * The 2-client co-op harness (charter Move 2, task #165/#168) needs two things
 * editor python cannot reach on its own:
 *
 *  1. WORLD TARGETING - with N-player PIE there are N UWorlds (listen-server +
 *     clients), but UnrealEditorSubsystem.get_game_world() returns only the
 *     first. These statics enumerate GEngine's world contexts and resolve a
 *     specific PIE world by net mode, so the bridge can run console commands
 *     ON THE CLIENT WORLD:
 *         unreal.SystemLibrary.execute_console_command(client_world, "MO.Test.Craft ...")
 *     Every MO.Test.* command resolves its pawn from the world it's executed
 *     with, and the gameplay components forward non-authority calls through
 *     their Server RPCs — so client-world execution exercises the REAL
 *     client->server transport with no test-only RPCs.
 *
 *  2. PIE CONFIG - ULevelEditorPlaySettings (player count / net mode) is an
 *     editor-only class NOT exposed to python (verified 2026-07-03).
 *     ConfigurePIE() wraps it behind WITH_EDITOR.
 *
 * Dev/test tooling. Harmless in shipped builds (ConfigurePIE compiles to a
 * false return; world enumeration is plain engine API).
 *
 * RELATED: Content/Python/test_multiplayer.py (the harness), claude_seq.py
 * (the tick-driven runner it executes under), Docs/AUTONOMOUS_TOOLING.md.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOEditorTestHelper.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOEditorTestHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * One line per PIE world: name, net mode, player-controller count, pawn.
	 * Also logs each line as [MOQUERY] PIE so log-based runners can grep it.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Test|PIE")
	static FString GetPIEWorldsSummary();

	/**
	 * Resolve a PIE world by net mode name: "ListenServer", "Client",
	 * "Standalone", or "DedicatedServer". For multiple clients, ClientIndex
	 * selects among the client worlds in context order (0 = first client).
	 * Returns null when no such world exists.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Test|PIE")
	static UWorld* FindPIEWorldByNetMode(const FString& NetModeName, int32 ClientIndex = 0);

	/**
	 * World-subsystem accessor for editor-Python: this UE build exposes no
	 * SubsystemBlueprintLibrary to py, so seq gates reach live subsystems
	 * (colony, clock) through this instead. Cast the result py-side.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Test")
	static UWorldSubsystem* GetWorldSubsystem(UWorld* World, TSubclassOf<UWorldSubsystem> SubsystemClass);

	/** GameInstance-subsystem accessor for editor-Python (same gap as above). */
	UFUNCTION(BlueprintCallable, Category="MO|Test")
	static UGameInstanceSubsystem* GetGameInstanceSubsystem(UWorld* World, TSubclassOf<UGameInstanceSubsystem> SubsystemClass);

	/**
	 * Configure the NEXT PIE session: number of players and listen-server mode
	 * (net mode PIE_ListenServer under one process when bListenServer, else
	 * PIE_Standalone). Editor builds only — returns false in packaged games.
	 * Call BEFORE begin_pie; running sessions are unaffected.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Test|PIE")
	static bool ConfigurePIE(int32 NumPlayers, bool bListenServer);
};
