/**
 * =============================================================================
 * MOCheatSubsystem.h - Dev/debug console commands for player + world cheats
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Central host for MO.Player.* / MO.World.* / MO.Cheat.* console commands.
 * Owns no gameplay state — finds the local player pawn + sibling components
 * each time and acts on them.
 *
 * COMMAND CATEGORIES:
 *   MO.Player.* — operate on the locally controlled MOCharacter
 *   MO.World.*  — operate on world state (creature spawns, etc) [future]
 *
 * EXTENSION:
 * To add a new command, follow the existing pattern in RegisterConsoleCommands:
 *   1. Pick a namespace (MO.Player / MO.World / etc)
 *   2. RegisterConsoleCommand with a lambda
 *   3. Lambda resolves world subsystem at call time (not capture)
 *   4. Push the result into ConsoleCommands so Deinitialize can release it
 *
 * Per-system commands (clock, harvest, UI debug) live in their respective
 * subsystems — this subsystem is only for cross-cutting cheats that need
 * access to the player pawn + multiple component types.
 *
 * =============================================================================
 * KNOWN PITFALLS
 * =============================================================================
 * [2026-05] LIFETIME — DO NOT MAKE THIS A UWorldSubsystem. WorldSubsystems
 *   re-initialize on every PIE world change. Each re-init tries to register
 *   the same console-command names. IConsoleManager's behavior on duplicate
 *   register + reuse-after-unregister can hand back recycled pointers, and
 *   the next Deinitialize crashes on UnregisterConsoleObject of a stale ptr.
 *   GameInstanceSubsystem lives for the editor session — register/unregister
 *   happens exactly once. Existing MOUIDebugSubsystem and
 *   MOHarvestDebugSubsystem follow this pattern for the same reason.
 *
 * [2026-05] AUTHORITY: Server-authoritative gameplay state (inventory adds,
 *   damage, etc) must be triggered authoritatively. The cheat lambdas execute
 *   on the listen-server / standalone host where PIE runs in dev. If/when
 *   running as dedicated-client, these would need server RPCs — flagged as
 *   future work.
 *
 * =============================================================================
 * RELATED:
 *   - Docs/PAUSE_POLICY.md
 *   - MOGameClockSubsystem.h (MO.Clock.* commands)
 *   - MOUIDebugSubsystem.h (MO.UI.* commands)
 *   - MOHarvestDebugSubsystem.h (MO.Harvest.* commands)
 *
 * LAST UPDATED: 2026-05-23
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOCheatSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOCheatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor for the world's cheat subsystem. */
	UFUNCTION(BlueprintPure, Category="MO|Cheat", meta=(WorldContext="WorldContextObject"))
	static UMOCheatSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

private:
	/** Register MO.Player.* / MO.World.* commands. */
	void RegisterConsoleCommands();

	/** Unregister all console commands. */
	void UnregisterConsoleCommands();

	/** Registered console commands (released in Deinitialize). */
	TArray<struct IConsoleCommand*> ConsoleCommands;
};
