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
| Launch | `python Tools/ue.py editor start` resolves the UE 5.8 installation, starts MCP explicitly, and waits for the bridge |
| Computer-use grant | `request_access` with **"UnrealEditor.exe"** — the exe filename. Display names don't resolve. |
| Foreground | Win32 `SetForegroundWindow` via PowerShell Add-Type. **Never `open_application`** — it launches a second, project-less editor instance (kill the newer PID if it happens). Use `ShowWindow(hwnd, 3)` (SW_MAXIMIZE), **not 9/SW_RESTORE** which un-maximizes. |
| Bridge auto-load | `init_unreal.py` imports `claude_bridge` at editor startup. For an already-running editor without it: one Cmd-box click → `py "D:/UEProjects/MO57/Content/Python/claude_bridge.py"`. |

## 2. The command bridge (the workhorse)

`Content/Python/claude_bridge.py` polls `%TEMP%\claude\ue_cmd.txt` every 0.25 s on the Slate tick and appends results to `ue_out.txt`. Set `MO57_BRIDGE_DIR` when separate editor/agent instances need isolated bridge files.

- **Console command line** → executed on the **PIE world if active**, else the editor world: `MO.Clock.SetTime 12 0`
- **`py:` line** → exec'd with `unreal`, `world` (PIE world or editor world), `game`, `editor`, `out(msg)` in scope.
- Multi-statement steps: write a script file and run `py:exec(open(r"...\step.py").read())`.
- Hot-reload the bridge through itself: `py:exec(open(r"D:/UEProjects/MO57/Content/Python/claude_bridge.py").read())`.
- **Write commands with `Add-Content -Encoding ascii`** — PS 5.1's utf8 stamps BOMs that break prefix matching (bridge strips them, but ascii is cleaner).
- Dev-machine tooling only — it executes arbitrary local commands. Never ship.

## 3. Boot a new game — ONE COMMAND, seed-safe, zero screenshots

```powershell
powershell -ExecutionPolicy Bypass -File Tools\agent_boot_newgame.ps1 -Seed 4242
```

That's the whole thing. The script drives the **normal** boot flow entirely in-process (proven June 12, ~10 s warm / ~45 s cold):
LoadingLevel PIE → intro → `SkipIntroVideo()` → seed into settings → `StartNewGame()` → poll log for possession → `MO.Clock.SetTime 12 0`. It ends any running PIE first and restores LoadingLevel as the origin. Stage transitions come from tailing `MO57.log` — no screenshots, no clicks, no coordinates.

Why this is seed-correct: `SkipIntroVideo` and `StartNewGame` are the **same BlueprintCallable functions the menu buttons call** — the game performs its own normal OpenLevel transition into the gameplay map, so the voxel/PCG seed pipeline initializes exactly as a human run.

> ⚠ **Never PIE directly into MOPCGScattering** (even with pending flags primed) — it skips the fresh OpenLevel transition and breaks parts of the seed flow for voxel world-gen and PCG (project owner, June 12 2026). Tests touching world generation, seeds, terrain, or PCG must go through this boot.

**Stop PIE:** `py:import agent_test_lib as atl; atl.end_pie(out)` — or `editor_request_end_play()` directly.

**The library (`Content/Python/agent_test_lib.py`)** is the toolbox to GROW — every new test need becomes a named function there (driving real code paths), never a click sequence:
`pc / pawn / component / ui_manager` · `gi_subsystem(world,"MOQuestSubsystem")` / `world_subsystem(world,"MOCraftingSubsystem")` · boot ladder (`ensure_loading_level / begin_pie / end_pie / skip_intro / start_new_game / set_noon`) · `give(world,"Stick01",10,out)` · `quest_status(world,out)`.
Note: `import agent_test_lib` caches — after editing the lib mid-session, send `py:import importlib, agent_test_lib; importlib.reload(agent_test_lib)`.

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

⚠ **Synthetic-input limitation (resolved #144, June 12):** keys injected by the computer-use tools do NOT reliably reach the PC's Enhanced Input actions in PIE gameplay (they do reach Slate widgets and some pawn paths). A human's physical keyboard works perfectly — verified live, all handlers FIRED. So: **never test gameplay keybinds with injected keys**; drive the same code paths in-process (`ui_manager(world).toggle_skills_panel()` etc.) and reserve keybind verification for a human press. This also rules out AHK-style tools — same injected-input class, same blind spot.

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

## 11. Slate key-injection testing (UI widget tests)

`MOUITestSubsystem` (world subsystem) exposes `simulate_escape()` / `simulate_tab()` /
`simulate_key_press(key)` — these call `FSlateApplication::ProcessKeyDownEvent`, which
routes along the **keyboard focus path** and runs handlers synchronously. Proven for
testing preview-key handling (the #138 close-key suite lives in
`%TEMP%\claude\step_138_*.py`). Hard-won rules:

1. **An UNCONSUMED Escape ends PIE** (editor-level binding). Always claim focus into
   the widget under test first, and GUARD every injection on a verified
   `has_keyboard_focus()` — if the claim failed, skip the injection or you kill the
   session.
2. **Push/open and test in SEPARATE bridge commands.** CommonUI defers widget
   construction/activation a tick; `set_keyboard_focus()` in the same exec as
   `push_modal_widget`/`open_in_game_menu` silently no-ops (no cached SWidget yet).
3. **Programmatic focus claiming works at the main menu, is flaky in the in-game
   world** (viewport capture / input-mode interplay). Prefer state-query tests
   in-game (open menu via controller method → assert `GetActiveMenuCount`/cursor)
   over key-injection; injection tests are most reliable at the main menu. NOTE:
   #137 is now fixed, so in-game cursor/menu-count behavior is correct — but the
   *harness's* SetAllUserFocus into a fresh in-game widget can still lose to
   viewport capture, so guard injections regardless.
4. **`get_editor_property` only sees `Edit*` or `Blueprint*` UPROPERTYs.** Plain
   `UPROPERTY(meta=(BindWidget))` members (e.g. `OptionsButton`, `SeedInputBox`) are
   invisible to python. Workaround: `unreal.WidgetLibrary.get_all_widgets_of_class`
   (note: py name is `WidgetLibrary`, not `WidgetBlueprintLibrary`) + filter by
   `w.get_typed_outer(unreal.MOWidgetClass)`.
5. Persist state across bridge commands by stashing on the test-lib module:
   `atl.T138_state = {...}` — each `py:` exec gets a fresh env, the module survives.
6. Dynamic delegates accept python callables: `dlg.on_cancelled.add_callable(fn)`;
   `is_bound()` checks survive into asserts (used to prove one-shot delegate clears).

## 12. The sequence runner (claude_seq) — multi-frame tests in ONE file

`Content/Python/claude_seq.py` (auto-loaded by init_unreal, independent slate
tick hook) drives **generator-based test sequences one step per frame**. This
replaces the old "Add-Content; Start-Sleep; Add-Content" external orchestration
for anything that needs frame separation (CommonUI defers construction/activation
a tick, so push→focus→inject→assert MUST span frames).

Write a file defining `sequence(ctx)` as a generator; each `yield` lets a frame
pass (`yield N` waits N frames). Submit it through the core bridge:

    py:import claude_seq; claude_seq.run_file(r"C:\path\to\seq_mytest.py")

Results stream to ue_out.txt prefixed `[seq:<name>]`, ending in
`DONE pass=<n> fail=<n>` (or `FAILED <traceback>` / `ABORTED`). Read them the
usual way (`Get-Content ue_out.txt -Tail N`). A failing sequence is contained —
it logs and drops, never crashing the tick (proven: an intentional exception and
an intentional assert-FAIL both reported cleanly without touching the core bridge).

`ctx` essentials (full list in the file header):
- `ctx.world/.pc/.pawn` — live-resolved each access (valid across travel/PIE start)
- `ctx.atl` — agent_test_lib; `ctx.test_subsystem()` — MOUITestSubsystem
- `ctx.find_class("MOFoo")`, `ctx.all_widgets(cls)`, `ctx.push_modal(cls)`
- `ctx.simulate_escape()/_tab()/_key(FKey)`
- `ctx.assert_true(label, cond)` / `ctx.assert_eq(label, a, b)` — record PASS/FAIL
- `ctx.guard(label, cond)` — like assert but ABORTS the sequence if false; use
  before injecting Escape so a failed focus precondition can't end PIE

Proven examples (the #137 suite): `%TEMP%\claude\seq_h43.py` (menu-count + cursor
state, in-game, no injection) and `seq_h41.py` (recipe-query regression). Both run
green start-to-finish from a single submit. This is the preferred harness for all
future UI/state verification — reach for it over hand-sequenced bridge commands.

**Hard-won menu-cycling rules (from a #143 false-alarm investigation):**
- The test subsystem's `OpenMenu(name)` is a TOGGLE (`ToggleInventoryMenu` etc.),
  while `CloseAllMenus()` is a one-way close. Mixing toggle-open with CloseAll-close
  in a tight loop polled at ~2 ticks DESYNCS parity: a deferred deactivation hasn't
  completed when the next toggle reads "still open" and flips it back, eventually
  leaving one menu stuck activated. That stuck state then poisons every later run in
  the session (baseline count != 0). This looks exactly like a regression but isn't.
- Robust pattern (see `seq_regress2.py`, runs 9/0): ONE fully-settled cycle per menu
  — open, `yield 10`, assert count==1, CloseAll, `yield 14`, assert count==0 — and
  gate each open on `count==0` first. Generous settles (10-14 ticks) because
  deactivation → stub reactivation → focus reclaim is multi-frame, especially when
  the editor is backgrounded and tick-throttled.
- To recover a session with a stuck menu without rebooting: directly
  `widget.deactivate_widget()` the activated widget (find it via
  `all_widgets("MOActivatableWidget")` + `is_activated()`). That clears it when
  CloseAllMenus can't (CloseAllMenus closes the *controller-tracked* instance, not a
  toggle-opened orphan).
- The runner itself never broke through all of this — every failing sequence logged
  its FAIL/traceback and the core bridge kept polling. Trust it.

**Foregrounding to un-throttle:** a backgrounded editor throttles slate ticks hard,
stretching `yield N` wall-time. The `Get-Process | where MainWindowHandle -ne 0` +
`SetForegroundWindow` dance (memory: ue_pie_computer_testing) speeds runs up; the
window handle is sometimes 0 right after launch — poll for it.
