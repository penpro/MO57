# Pause Policy

**MO57 does not pause. Ever. By design.**

## Policy

| Context | Behavior |
|---------|----------|
| Single-player | World runs continuously. No menu, modal, dialog, or overlay pauses the world. |
| Co-op (Steam) | Same as single-player. Pause is conceptually meaningless when other players are connected. |
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

`AMOPlayerController::SetPause` overrides the engine method to always return false, with a `LogMOFramework` warning when called. This catches every pause path in UE:

- `UGameplayStatics::SetGamePaused`
- `AGameMode::SetPause`
- Blueprint "Set Game Paused" node
- Console `pause` command
- Auto-pause behaviors (gamepad disconnect, window deactivation if configured)

All of them ultimately call `APlayerController::SetPause` — single chokepoint, single point of refusal.

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

### What replaces pause

Common reasons people reach for pause — handle them this way instead:

| Goal | MO57 approach |
|------|---------------|
| Player needs to think / look at inventory | Open the menu; world keeps ticking. Tension is the point. |
| Watch a video / cinematic | Keep it brief; never block gameplay you care about. Acceptable to pause-equivalent ONLY in main menu / loading state where no world exists. |
| Wait on player decision (modal confirmation) | Use timeouts / sensible defaults. Don't gate gameplay-critical decisions behind modals. |
| Suspend AI for a debug session | Console command `slomo 0.01` or `pause` will both log warnings — for editor work use `pause` (the engine handles editor pause separately from the player-controller path; this policy applies to runtime only). |

### What this policy does NOT cover

- **Editor PIE pause** (the play-in-editor toolbar Pause button) goes through a different path and still works for development. It logs a warning but doesn't actually halt the world (the override returns false). If you need to debug-pause, use breakpoints or `slomo`.
- **Time dilation** (`SetGlobalTimeDilation`) is a separate concept; not blocked. Slow-motion effects are allowed.
- **Authoritative server tick rate** is unchanged; this policy is about the engine's binary paused/unpaused flag.

## How to test the enforcement

In a packaged build or PIE:
1. Open the console (`~`)
2. Type `pause`
3. Confirm the log shows: `[MOPlayerController] SetPause(true) refused — MO57 is real-time.`
4. Confirm the world keeps ticking (watch any moving creature, the time-of-day, etc).

## If you ever need to break this policy

Don't. Talk to the design lead. Then talk to them again. The policy is intentional, the enforcement is intentional, and any "just this one feature" exception cascades into the two-mode complexity problem rationale #3 is designed to prevent.

If you have a use case the policy genuinely doesn't cover, update this doc first explaining the case, then change the enforcement.

---

**Last updated:** 2026-05-23
**Owner:** Engineering lead
**Related:** `MOPlayerController.h` (override declaration), `MOPlayerController.cpp` (override implementation)
