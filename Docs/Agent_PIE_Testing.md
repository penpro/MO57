# Agent PIE Testing Playbook

*How Claude (or any automation agent) runs MO57 in the Unreal Editor autonomously. Every recipe here was proven live on June 12, 2026. Companion tooling: `Content/Python/claude_bridge.py` (auto-loaded by `init_unreal.py`).*

---

## TL;DR — the no-click loop

Once the editor is running and the bridge is loaded, an entire test session is **file I/O only** (no screenshots, no UI clicks):

```
1. echo py-commands  >> %TEMP%\claude\ue_cmd.txt      (prime settings, load map, begin PIE)
2. poll Saved\Logs\MO57.log                            (wait for "possessed initial pawn")
3. echo MO.Clock.SetTime 12 0  >> ue_cmd.txt           (noon — ALWAYS, for visibility)
4. echo test commands / py:    >> ue_cmd.txt           (cheats, subsystem calls, assertions)
5. read %TEMP%\claude\ue_out.txt + grep MO57.log       (real-time results)
6. echo end_play py-command    >> ue_cmd.txt           (stop PIE)
```

Screenshots are only for *visual* verification (HUD layout, world rendering) — never for reading state.

---

## 1. Launch & attach

| Step | Command / fact |
|------|----------------|
| Is it running? | `Get-Process UnrealEditor` (window title becomes "MO57 - Unreal Editor" when loaded) |
| Launch | `Start-Process "D:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList '"D:\UEProjects\MO57\MO57.uproject"'` then poll for MainWindowTitle (≈3-4 min cold) |
| Computer-use grant | `request_access` with **"UnrealEditor.exe"** — the exe filename. Display names don't resolve. |
| Foreground | Win32 `SetForegroundWindow` via PowerShell Add-Type. **Never `open_application`** — it launches a second, project-less editor instance (kill the newer PID if it happens). Use `ShowWindow(hwnd, 3)` (SW_MAXIMIZE), **not 9/SW_RESTORE** which un-maximizes. |
| Bridge auto-load | `init_unreal.py` imports `claude_bridge` at editor startup. For an already-running editor without it: one Cmd-box click → `py "D:/UEProjects/MO57/Content/Python/claude_bridge.py"`. |

## 2. The command bridge (the workhorse)

`Content/Python/claude_bridge.py` polls `C:\Users\penum\AppData\Local\Temp\claude\ue_cmd.txt` every 0.25 s on the Slate tick and appends results to `ue_out.txt`.

- **Console command line** → executed on the **PIE world if active**, else the editor world: `MO.Clock.SetTime 12 0`
- **`py:` line** → exec'd with `unreal`, `world` (PIE world or editor world), `game`, `editor`, `out(msg)` in scope.
- Multi-statement steps: write a script file and run `py:exec(open(r"...\step.py").read())`.
- Hot-reload the bridge through itself: `py:exec(open(r"D:/UEProjects/MO57/Content/Python/claude_bridge.py").read())`.
- **Write commands with `Add-Content -Encoding ascii`** — PS 5.1's utf8 stamps BOMs that break prefix matching (bridge strips them, but ascii is cleaner).
- Dev-machine tooling only — it executes arbitrary local commands. Never ship.

## 3. Fast-start a new game (zero menu clicks)

> ⚠ **CAVEAT (from the project owner, June 12 2026): the fast-start below breaks parts of the seed flow for voxel world-gen and PCG.** PIE-ing directly into MOPCGScattering — even with pending flags primed — does not reproduce the same seed/voxel/PCG initialization as the real boot. **For any test that touches world generation, seeds, terrain, or PCG content, use the NORMAL flow:** PIE from LoadingLevel → Space to skip intro → click New Game → Start Game (menu clicks are safe — cursor is free in menus). Reserve the fast-start for tests that only need *a* world (UI, quests, inventory, subsystem logic).

The menu flow (intro → main menu → New Game → Start) exists for humans. The pending-flags path is what `StartNewGame` itself does — prime it directly:

```
# step_prime_newgame.py  (run via bridge while in the editor, PIE stopped)
gus = unreal.GameUserSettings.get_game_user_settings()   # returns MOGameSettings
gus.set_editor_property("bPendingNewGame", True)
gus.set_editor_property("bIsLoadingIntoGameplay", True)
gus.set_editor_property("PendingNewGameSlot", "ClaudeTest-01")
gus.set_editor_property("PendingWorldSeed", 13546)
gus.save_settings()
```

Then:
```
py:lvl = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); lvl.load_level("/Game/VoxelExamples/PCGScattering/MOPCGScattering")
py:unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
```
`AMOGameMode::HandlePendingNewGame` sees the flags → seeds the voxel runtime → spawns + possesses the initial pawn. Boot to possession ≈ 30-45 s (voxel gen + PSO compiles).

**Readiness signal** — poll the log, don't sleep blind:
```powershell
Select-String "D:\UEProjects\MO57\Saved\Logs\MO57.log" -Pattern "possessed initial pawn"
```

**Stop PIE:** `py:unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()` (also: `editor_play_simulate`, `is_in_play_in_editor` exist). Restore the editor map afterwards: `load_level("/Game/Penumbra/Maps/LoadingLevel")`.

> If you must PIE through the human path instead: PIE from LoadingLevel (the GameDefaultMap), skip the intro with **Space** (never Escape on the title screen — H42a soft-lock), click New Game → Start Game.

## 4. First commands after possession — always

```
MO.Clock.SetTime 12 0        # NOON. Game time may be night; screenshots are useless in the dark.
```
Other clock tools: `MO.Clock.Info`, `MO.Clock.SetTimeScale <x>`, `MO.Clock.AdvanceHours <n>`, `MO.Clock.SkipToDay`, `MO.Clock.SkipToNight`.

## 5. Reading state in real time (instead of screenshots)

| Source | What it gives you | How |
|--------|-------------------|-----|
| `ue_out.txt` | Results of every bridge command, `out()` from py steps | `Get-Content -Tail N` |
| `Saved/Logs/MO57.log` | The full engine log, live | `Select-String -Pattern ...` / `-Tail` |
| `Saved/Logs/UIDebug_*.log` | UI forensics: global focus changes, widget activations, layer pushes/pops, input-config applications, handler-FIRED lines | newest file, `-Tail` |
| `Saved/Logs/HarvestDebug_*.log` | Seed flow, harvest/terrain diagnostics | same |
| py assertions | Direct state queries (quest lists, component values) | `out(...)` from step scripts |

Marker pattern for flows: `py:out("=== TEST PHASE 2 ===")` brackets your log greps.

## 6. Live object access from `py:` (proven recipes)

```python
pc   = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)        # NOT pc.get_pawn() — not exposed
comp = pawn.get_component_by_class(unreal.MOCraftingQueueComponent)
gi   = unreal.GameplayStatics.get_game_instance(world)

# Subsystem instances — SubsystemBlueprintLibrary is NOT exposed in 5.7 python.
# unreal.find_object with the right outer works:
quest_sub = unreal.find_object(gi,    "MOQuestSubsystem_0")      # GameInstance subsystems: outer = GI
craft_sub = unreal.find_object(world, "MOCraftingSubsystem_0")   # World subsystems: outer = world
# (suffix is _0 in practice; probe _1 if None)

# Anything BlueprintCallable is callable (snake_case):
quest_sub.start_quest("Tutorial_BuildCampfire")
quest_sub.fire_game_event("TreeInspected")        # generic quest-event injector
quest_sub.is_quest_complete("Tutorial_GatherBark")
quest_sub.get_completed_quest_ids()

# Dynamic multicast delegates can be broadcast directly (struct args via set_editor_property):
r = unreal.MOCraftResult(); r.set_editor_property("success", True)
r.set_editor_property("produced_items", {"BuildCampfire": 1})
craft_sub.on_craft_completed.broadcast("BuildCampfire", r)

# UI without input: UIManager methods work directly
pc.get_component_by_class(unreal.MOUIManagerComponent).toggle_skills_panel()
```

**5.7 API traps:** `InputMappingContext.mappings` is deprecated and reads EMPTY — use `get_editor_property("default_key_mappings")`. `EditorAssetLibrary` → use `unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)`. DataTable live-refresh: `unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(dt, csv_path)` then `EditorAssetSubsystem.save_asset(path)` after PIE stops.

## 7. Cheats & key bindings reference

**Cheats:** `MO.Player.Info | Teleport | GiveItem <id> <n> | SetWet | SetStat`, `MO.Weather.*`, `MO.Clock.*`, `MO.Mod.Load*`.
`GiveItem` fires real pickup events — it advances ItemPickup quest objectives (M18), which is useful for driving tutorial chains.

**Keys (IMC_MODefault):** I inventory · K skills · Esc/Tab back-menu · C craft · B build · E interact · P possess · Y status · T terraform · R cycle tool · F1 debug · Ctrl crouch · Shift hustle · Space jump.
⚠ As of June 12, PC-level UI keys don't fire in PIE (task #144) — drive menus via UIManager calls until fixed.

## 8. Mouse/keyboard interaction rules (when you must click)

- During PIE gameplay the viewport holds `CapturePermanently + LockOnCapture`: synthetic clicks aimed at editor UI **get confined into the viewport** and keystrokes leak into the game (Ctrl→crouch, letters→movement, Ctrl+V paste is the worst). **Press Shift+F1 first, always**, before clicking any editor UI while PIE runs.
- Click the viewport once to hand input back to the game.
- In menus/intro the cursor is free — UI clicks are safe.
- The in-game `~` console doesn't open reliably under capture; `ShowDebug` is unavailable (no AHUD class). The bridge replaces both.
- Stop-PIE by hand: Shift+F1 → toolbar Stop. Or just use the bridge.

## 9. Session checklist

```
[ ] Editor running with MO57? (Get-Process) — else launch + wait
[ ] Bridge alive? (echo py:out("ping") → read ue_out.txt) — else bootstrap
[ ] Truncate/ignore stale ue_out.txt; note log byte offset for clean greps
[ ] Prime new game → load gameplay map → begin_play → poll for possession
[ ] MO.Clock.SetTime 12 0
[ ] ... test ...
[ ] end_play → restore LoadingLevel → save any dirty assets deliberately
[ ] Report: cite ue_out.txt lines + MO57.log lines as evidence
```

## 10. Known failure modes

| Symptom | Cause / fix |
|---------|-------------|
| Bridge silent | Editor restarted (re-loads via init_unreal) or exception — check `ue_out.txt` for `[bridge-error]`; hot-reload via Cmd box once |
| `[cmd-err] no world` | Nothing loaded — load a map first |
| Click lands in the wrong place | PIE capture-lock (see §8) or the editor window got restored/moved — re-maximize, fresh screenshot |
| `find_object` returns None | Wrong outer (GI vs world) or suffix — probe `_0`/`_1` |
| Direct PIE into gameplay map = void world | Missing pending flags (H48). Always prime settings first |
| Quest/pickup tests behave oddly at spawn | Starting loadout fires pickup events (M18) — GatherSticks/GatherStone complete instantly |
| Editor frontmost check fails | Desktop became frontmost — re-foreground via Win32, never `open_application` |
