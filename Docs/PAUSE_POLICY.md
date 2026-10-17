# Pause Policy

> **Covers:** why MO57 never pauses, the one chokepoint that refuses every pause, what a player does instead (back out to the main
> menu: leaving always saves), and what the policy does not govern (the editor's debugger).
> **Current state (2026-10-17):** `AMOPlayerController::SetPause` refuses every runtime pause. Every way out of a world saves first,
> with no confirm (`UMOSystemMenuUIController::SaveWorldBeforeLeaving`), so leaving is the player's pause and can't be used to undo
> anything (no save-scumming).
> **Wes, 2026-10-17:** "no pause, ever... if they want to pause they can back out to the main menu"; "get rid of the save confirm and just
> force the save... i don't want people to be able to 'save-scum'".
> **Entry points:** `AMOPlayerController::SetPause`, `UMOSystemMenuUIController::SaveWorldBeforeLeaving` / `ReturnToMainMenuImmediate` /
> `QuitGameImmediate` / `HandleViewportCloseRequested`. **Open:** manual save slots can still reopen an older save (DECISIONS F24).

**MO57 does not pause. Ever. By design.**

## Policy

| Context | Behavior |
|---------|----------|
| Single-player | World runs continuously. No menu, modal, dialog, or overlay pauses the world. |
| Co-op (Steam) | Same as single-player. Pause is conceptually meaningless when other players are connected. |
| Player needs to step away | Back out to the main menu (Wes, 2026-10-17). Leaving saves the world first, with no confirm; see "Leaving a world" below. |
| Loading screens | World load happens inside the engine's transition state — not a pause; the world simply doesn't exist yet. |
| Intro video | Plays in the main menu world (no gameplay simulation present). |
| Main menu | No gameplay world ticking — pause irrelevant. |

## Rationale

1. **Real-time survival is the design pillar.** Hunger, thirst, vitals, body temperature, wound progression, mental state, and metabolism all advance continuously. Pause would let players game these systems by opening menus to escape time-pressure decisions — undermines the entire survival simulation.

2. **Multiplayer parity from day one.** The Steam co-op vision (Satisfactory-style: drop in, drop out, shared world) doesn't survive a system where one player can freeze the world for everyone. Building the codebase around "no pause" from the start avoids retrofitting that limitation later when peers join.

3. **No two-mode complexity.** Many survival games have "pauses in solo, doesn't pause in coop" — leading to subtle bugs where save logic, AI timers, and world state diverge based on player count. Pure real-time everywhere = one code path, fewer edge cases.

4. **AI / world simulation continuity.** Creatures patrol, fires burn down, food spoils, mob aggro decays, scheduled NPC routines fire — all on continuous wall-clock ticks. Pause would require freeze + thaw logic on every component that tracks time, every BT tick, every gameplay timer. Production cost of "supports pause" across N systems is much higher than designing without it.

## Implementation

### Enforcement

`AMOPlayerController::SetPause` overrides the engine method to always return false, with a `LogMOFramework` warning when called. These
pause paths all reach it (checked in the UE 5.8.3 source, 2026-10-17):

- `UGameplayStatics::SetGamePaused` (calls the first local player controller's `SetPause`)
- Blueprint "Set Game Paused" node (the same function)
- Console `pause` command (`APlayerController::Pause` -> `ServerPause` -> `SetPause`), in a packaged game and in PIE alike
- Auto-pause behaviors that go through the player controller (gamepad disconnect, window deactivation if configured)

`AGameModeBase::SetPause(PC, ...)` called DIRECTLY does not go through the player controller (it sets the world's pauser itself), so it
is not caught by the override. No MO57 code calls it; never add a call (the header comment in `MOPlayerController.h` lists it; that
list predates this check).

```cpp
bool AMOPlayerController::SetPause(bool bPause, FCanUnpause CanUnpauseDelegate)
{
    if (bPause)
    {
        UE_LOG(LogMOFramework, Warning,
            TEXT("[MOPlayerController] SetPause(true) refused — MO57 is real-time. "
                 "If you need this, check Docs/PAUSE_POLICY.md first. Caller stack:"));
        FFrame::KismetExecutionMessage(
            TEXT("Pause attempt blocked by MO57 pause policy"),
            ELogVerbosity::Warning);
    }
    return false;
}
```

### Leaving a world (the player's pause)

Every way out of a world goes through one save, `UMOSystemMenuUIController::SaveWorldBeforeLeaving` ->
`UMOPersistenceSubsystem::SaveToCurrentSlot` (the slot the world was loaded from; a new game gets an `AutoSave_<world>_<time>` slot on its
first save). There is no confirmation and no "leave without saving":

| Way out | Path |
|---|---|
| In-game menu: Exit to Main Menu | `ReturnToMainMenuImmediate` (save, then open the main menu level) |
| In-game menu: Exit Game | `QuitGameImmediate` (save, then quit to desktop) |
| In-game menu: Load another save | `HandleLoadRequested` (save this world, then load the chosen one) |
| Death recap: Return to Main Menu | `ReturnToMainMenuImmediate` (the death is saved; deceased pawns stay deceased on load, and the load gives the player the same next life as the recap's Continue: `AMOGameMode::OnPlayerNeedsNextLife` -> `ContinueAfterDeath`) |
| Game window close (X / Alt+F4) | `HandleViewportCloseRequested` on `UGameViewportClient::OnCloseRequested` (packaged / `-game`; not PIE) |

- Only the host or a standalone game saves. A co-op client leaving writes nothing (the host owns the world save) and logs `client -- the host
  owns the world save`.
- A failed save is the only case with a dialog: "Could Not Save ... Leave Anyway / Stay", so progress is never lost silently. The window
  close can't offer one; it logs the failure.
- Log line to check: `[MOSysUI] Leaving the world (<reason>): save to slot '<slot>' ok=1`.
- Not covered: a crash or a killed process (no autosave timer exists yet: BACKLOG N-157), and the console `quit` command (developer console only).
- Verified 2026-10-17 in a `-game` instance: Exit to Main Menu, Exit Game, the death recap's Return and the window close each logged
  `save to slot ... ok=1` with no dialog. Reloading the save made at a death now respawns a new survivor 4.96 mi from the recorded death
  spot; before that fix it left a sky camera with no pawn. Not run: loading another save from the in-game menu (the same save step),
  the failed-save dialog, and the co-op client path.

### What replaces pause

Common reasons people reach for pause — handle them this way instead:

| Goal | MO57 approach |
|------|---------------|
| Player needs to step away | Back out to the main menu. The world is saved on the way out and doesn't run while no one is in it; on the next load the clock and crafting catch up by the real time away, capped at 4 h, and vitals don't fast-forward (`Docs/Persistence.md`). |
| Player needs to think / look at inventory | Open the menu; world keeps ticking. Tension is the point. |
| Watch a video / cinematic | Keep it brief; never block gameplay you care about. Acceptable to pause-equivalent ONLY in main menu / loading state where no world exists. |
| Wait on player decision (modal confirmation) | Use timeouts / sensible defaults. Don't gate gameplay-critical decisions behind modals. |
| Suspend AI for a debug session | `slomo 0.01` (time dilation, allowed). Console `pause` is refused here like everywhere else, PIE included. In the editor, use the debugger below. |

### What this policy does NOT cover

- **The editor's debugger pause** (the PIE toolbar Pause button, Blueprint / C++ breakpoints). The toolbar Pause calls
  `UEditorEngine::SetPIEWorldsPaused`, which sets `UWorld::bDebugPauseExecution`; it never calls `SetPause`, so nothing is logged, and it
  DOES halt the PIE world (`UWorld::IsPaused` honours that flag only when `IsPlayInEditor()`). It exists only in the editor; a packaged game
  can't reach it. (Read in the UE 5.8.3 source on 2026-10-17; not yet watched in a PIE run.)
- **Time dilation** (`SetGlobalTimeDilation`) is a separate concept; not blocked. Slow-motion effects are allowed.
- **Authoritative server tick rate** is unchanged; this policy is about the engine's binary paused/unpaused flag.

## How to test the enforcement

In a packaged build or PIE:
1. Open the console (`~`)
2. Type `pause`
3. Confirm the log shows: `[MOPlayerController] SetPause(true) refused — MO57 is real-time.`
4. Confirm the world keeps ticking (watch any moving creature, the time-of-day, etc).

Leaving: in a `-game` instance or packaged build, open the in-game menu and press Exit to Main Menu. No dialog appears, the log shows
`Leaving the world (exit to main menu): save to slot '...' ok=1`, and `MO.Save.List` lists that slot with a fresh timestamp.

## If you ever need to break this policy

Don't. Talk to the design lead. Then talk to them again. The policy is intentional, the enforcement is intentional, and any "just this one feature" exception cascades into the two-mode complexity problem rationale #3 is designed to prevent.

If you have a use case the policy genuinely doesn't cover, update this doc first explaining the case, then change the enforcement.

---

**Last updated:** 2026-10-17 (leaving saves with no confirm; the pause paths checked against the engine source)
**Owner:** Engineering lead
**Related:** `MOPlayerController.h` (override declaration), `MOPlayerController.cpp` (override implementation), `MOSystemMenuUIController.h/.cpp` (leaving a world)
