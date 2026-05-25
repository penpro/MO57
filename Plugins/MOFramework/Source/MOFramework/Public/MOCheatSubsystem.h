/**
 * =============================================================================
 * MOCheatSubsystem.h - Dev/debug + casual cheat console commands
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Central host for MO.* console commands. Owns no gameplay state — finds the
 * local player pawn + sibling subsystems each call and acts on them.
 *
 * DESIGN DECISION (2026-05-25): Commands stay registered in ALL build
 * configurations, including Shipping. MO57 is a casual single-player game
 * with first-class mod support — cheats and runtime tooling are part of the
 * product. Players who don't want to cheat can simply not open the console.
 * Do NOT wrap registration in #if !UE_BUILD_SHIPPING; that would also hide
 * the modder-facing data-table-injection commands (task #113) the same way.
 *
 * COMMAND NAMESPACES (live in this subsystem):
 *   MO.Help           — discover commands (filterable)
 *   MO.Player.*       — local pawn (Info, Teleport, GiveItem)
 *   MO.Clock.*        — game clock (Info, SetTime, AdvanceHours, SkipTo*)
 *   MO.Weather.*      — weather state (Info, ListPresets, SetPreset, ...)
 *   MO.Audio.*        — audio bank (Info, AmbientVolume, ...)
 *   MO.AI.*           — spawn-manager freeze pipeline (DumpFreezeState,
 *                       ForceFreezeAll, ForceWakeAll, StressSpawn)
 *
 * Per-system debug commands (MO.UI.*, MO.Harvest.*) live in their respective
 * subsystems — this file is only for cross-cutting cheats that need access
 * to the player pawn + multiple component/subsystem types.
 *
 * EXTENSION:
 * To add a new command, follow the existing pattern in RegisterConsoleCommands:
 *   1. Pick a namespace (MO.Player / MO.World / MO.Mod / etc)
 *   2. RegisterConsoleCommand with a lambda
 *   3. Lambda resolves world subsystem at call time (not capture)
 *   4. Push the result into ConsoleCommands so Deinitialize can release it
 *   5. Write good help text — MO.Help displays it verbatim, that's the
 *      discovery surface for QA, players, and modders.
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
