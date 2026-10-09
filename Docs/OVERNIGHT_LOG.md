# Overnight log (Wes asleep)

Standing goal from Wes: finish the multiplayer fixes, add a **Host** button to the Load game options (start that saved
world in server hosting mode), then keep testing / working the list. Nothing in this log is committed; verify the staged
column before any commit (git add fails silently on this repo).

## 2026-10-07

### Client terrain seed (Wes's report: "the second player's world seed is different")
- **Diagnosis (one layer):** the seed was applied only inside `AMOGameMode` (host-only) and never replicated, so every client
  generated its own voxel terrain from the default seed. A pawn standing on the host's ground was under/inside the client's
  different ground.
- **Fix:** `AMOGameState::WorldSeed` (replicated struct `FMOWorldSeedInfo`) + `UMOWorldSeedSubsystem` (the apply code moved
  out of `AMOGameMode`, shared by host and client; `AMOGameMode` keeps thin forwarders). The host publishes in
  `InitializeVoxelWorldWithSeed()` (the one chokepoint new-game and load both use); the client defers its voxel runtime and
  regenerates from the host's seed in `OnRep_WorldSeed`.
- **Also fixed:** `HostSession` never reset `PendingWorldSeed`, so every hosted world reused the last seed. It now resets to 0
  (random) for a fresh world.
- **Verified (real two-process run, `ue.py nettest game`):** same seed on both machines; ground height identical at three
  spots (1024 / 1138 / 984 cm, worst |dz| 0.0 cm), each a voxel-collision hit.
- **Retracted:** my first terrain check "passed" with `z=30000` on both machines. That was the trace origin, not terrain
  (the trace started inside other WorldStatic geometry), so the agreement was vacuous. The probe now only counts voxel-collision
  hits and the harness traces a window around the host pawn. Caught by reading the numbers, not the PASS.

### Negative control
- `ue.py nettest game --withhold-seed` makes the host skip the replication (reproduces the original bug).

### Host a saved world (Load panel -> Host)
- C++: `UMOSaveSlotEntry::HostButton` + `OnHostRequested`; `UMOSaveSlotListPanel::SetHostActionEnabled`; `UMOLoadPanel::OnHostRequested`
  + `StatusText`; `UMOMainMenuWidget::OnHostSavedGameRequested`; `AMOMainMenuPlayerController::HostSavedGame` sharing one
  `StartHosting` flow with `HostSession`. Console: `MO.Session.HostSave <slot>`.
- Harness: `ue.py nettest hostsave` (make a world with a recruited colonist, save, back to menu, host the save, join a client).
- Still to do: widget assets (HostButton in `WBP_MOSaveSlot`, StatusText in `WBP_LoadPanel`) via `ue.py ui`, which needs the editor.

### Control results (honest accounting, 2026-10-07)
- `nettest game --skew-seed` (host publishes seed+1): the same spots are ~147 m apart (host z=69 vs client z=14768), so the
  terrain-height comparison **can fail**. The first attempt of this control failed for my own reasons (trace window too narrow)
  and was fixed, not waved through.
- `nettest game --withhold-seed` proves the client applies nothing without a published seed. Its original expectation, that the
  client's pawn would disagree with the server's, was **wrong**: with no terrain at all the client's pawn still reported the
  server's z (4952/4952). So "client z == server z" is a vacuous check for terrain and is no longer used as a terrain test.
- Walking test (`nettest game` step): the client walked 34.7 m; its view of its pawn vs the server's: median 2 cm, max 7 cm,
  0% falling frames, with 121/121 grid points matching exactly. With matched terrain, walking is correct.
- **Unexplained, intermittent:** one run (seed 1452653648) had client ground 130-177 cm HIGHER than the host's at three spots
  (not reproduced in 5 later runs). A pawn on the server's lower ground would look ~1.5 m sunk into the client's ground, which
  matches Wes's "falling/levitating just below the ground" report. `nettest soak` repeats fresh-seed rounds to measure it.
- Old build note: the level's `AVoxelWorld` already has `bCreateRuntimeOnBeginPlay` off (the client logged "1 AVoxelWorld(s),
  deferred 0"), and only the host's GameMode created the runtime. So BEFORE this fix a client likely had **no voxel terrain at
  all**, which also looks like "falling forever, snapped back by the server". Wes's pre-fix report is explained either way.

### Load-flow bug found by the host-a-save test, fixed
- Loading a save respawned pawns at their saved spots while the voxel terrain was still regenerating; with gravity on they fell
  ~70 m (z 1019 -> -6123) before `RegroundAllPawns` ran, its +/-30 m window found no ground, and the fallback teleported the
  host's pawn ~4.6 km away. The recruited colonist stayed buried at z -6214. (Affects single-player Load too.)
- Fix at the layer that creates the bad state: `AMOCharacter::SetLoadHold(bool)` holds characters (no gravity, no rescue) from
  `WaitForVoxelAndRegroundPawns` until `FinishLoadHandoff`. Verified: after load the pawns sit at z 785/790 and the host's pawn
  is within 5 cm of its saved position.

### Tooling fixes along the way
- `claude_seq._worlds()` crashed in every `-game` process (no GEditor); it now reuses the bridge's resolution.
- New: `MO.Voxel.Seed`, `MO.Voxel.SurfaceZ`, `MO.Voxel.SurfaceGrid`, `MO.Session.HostSave`, `MO.WorldSeed.Withhold|Skew` (test only).
- New harness modes: `nettest hostsave`, `nettest soak --rounds N`, `nettest game --skew-seed|--withhold-seed`.

### Client ground collision: measured, then fixed (2026-10-07)
- **Wes's report:** "she walks on land on the server but shows falling/levitating just below the ground on the client."
- **Measured (soak, 6 fresh-seed joins):** 3 of 6 had the client's ground disagree with the host's right after "ready" by 60,
  160 and 319 cm (either direction); every time the full grid matched 0.0 cm moments later. Settle time series: host constant,
  client 372 cm off for ~7-9 s, then identical. Terrain was never the seed; it was ground collision that is not final yet.
  A pawn standing on the server's ground looks sunk into (or hovering over) the client's not-yet-final ground.
- **Second, same layer, found by the far-pawn test:** a remote player ~150 m from the host had no ground on the SERVER (the only
  invoker is the host's own camera): 20 fall-through rescues in 32 s. Both fixed by one thing: `AMOCharacter::VoxelCollisionInvoker`
  (`bWaitForVoxelWorld`, radius 30 m), enabled in `NotifyControllerChanged` (runs on the server and on the owning client) only
  while a human drives the pawn.
- **After (soak, 6 fresh-seed joins):** 5 of 6 never mismatched (0.0 cm from the first sample); 1 of 6 showed 17-50 cm for ~10 s
  and then 0. All 6 rounds pass (<= 50 cm). Honest residual: a short, small transient can still occur; not yet explained
  (the client's hit component changed between two `VoxelCollisionComponent`s when it settled, so two chunk generations overlap).
- Walking client (34 m): |dz| vs the server median 4 cm, max 10 cm, no falling frames. A walk test that turns when blocked
  replaces the first version, which failed on a seed that spawned the pawn facing a cliff (the pawn pushed in place at 250 cm/s).

### Main-menu Load button opened the OPTIONS panel (found while testing the Host button)
- The asset's switcher order is Blank, NewGame, Options(2), Load(3), Host, Join; C++ hard-coded Load=2/Options=3. Screenshot of the
  "Load" click showed the Options panel. All panels are now resolved from the widget (`UMOMainMenuWidget::ResolvePanelIndex`).
  This may be why "there's no way to load a save" earlier; it also made the Host-from-Load button unreachable until fixed.

### Host button (Load panel) — verified end to end
- Assets: `HostButton` in `WBP_MOSaveSlot`, `StatusText` in `WBP_LoadPanel` (`Content/Python/ui_specs/load_host.py`, contracts satisfied).
- A REAL mouse click on my own test save's Host button: `Host requested for saved slot` -> `Hosting requested ... resuming save` ->
  `CreateSession succeeded ... listen server` -> `Save loaded successfully: 1 pawns ... seed=199749360`. My test save was deleted;
  Wes's three saves (Harper_Wright-01, Test158, v1gate) were not touched (the click targeted the row by its name).
- A false lead on the way: `MO.Test.ClickWidget HostButton` matched the Host Game panel's button and hosted a fresh world;
  the PASS lines after it were that wrong click. Documented in Docs/UI_TOOLING.md.

### Packaged Development build (for the real two-account test)
- `Saved/StagedBuilds_DevTest/Windows/MO57.exe` (Development, so it WRITES LOGS; your existing `Saved/StagedBuilds` is untouched).
  Built from the working tree as of this log; boots to the main menu with no error-level log lines.
- Smoke-tested as the real packaged game: hosted a fresh world through its Host panel (session created, seed published, voxel
  runtime created, pawn spawned), and a second packaged copy connected to it, received the seed (439164768 on both sides), created
  its runtime and possessed a pawn. No crashes in either log.
- Fixed on the way: packaging failed twice with a file-lock error from `Tools/Update-BuildInfo.ps1` (two targets writing the same
  header at once); the script now serialises and retries. The same race could have caused your own earlier "package failed".
- Clients logged an Error per creature ("NO CONTROLLER! AutoPossessAI may have failed"): AI controllers exist only on the server, so
  that was a false error on every client. Now authority-only (`MOCreature.cpp`).

### Final test accounting (editor-binary `-game` processes, real gameplay map, Null OSS)
- `ue.py auto`: 131/131 (129 + 2 new: seed string, GameState replication).
- `nettest game`: PASS (seed handoff, ground identical from the first sample, 121/121 grid, 34 m walk within 14 cm of the server, a remote
  pawn 150 m from the host with 0 rescues, leave/rejoin, possession menu, tutorial order).
- Controls: `--skew-seed` PASS (client ground differs), `--withhold-seed` PASS (no seed, no terrain). `nettest hostsave` PASS.
- `nettest soak` (6 fresh-seed joins) with the invoker: 0 failing steps; 5/6 never mismatched, 1/6 17-50 cm for ~10 s.
- NOT verified: a real two-ACCOUNT Steam join (needs your friend); the in-game (not main-menu) Load panel hiding the Host button is by
  code path only (default off, only the main menu enables it), not by screenshot; the ~50 cm residual transient is unexplained.

### Left for you (design forks, not built blind)
1. **Ground for pawns that are NOT human-driven and far from every player** (recruited colonists doing jobs away from you, per your
   pawn-automation vision): there is no collision invoker for them, so they fall and the server's fall-through rescue teleports them
   (default `Z=200` when no terrain is found, which can bury them). Options: give working colonists a collision invoker only while a
   job is active; or hold/freeze them until ground exists. It is a cost-vs-realism call (invokers cost collision chunks per colonist).
2. **Commit chunking** (nothing committed): (a) world seed + GameState + subsystem + client terrain; (b) possession/join/pawn lifecycle
   (earlier work); (c) host-a-save (C++ + load_host spec + the two widget assets); (d) load-hold fix; (e) collision invoker;
   (f) main-menu panel indices; (g) harness/docs/memory; (h) build-info script + creature log guard. `.claude/settings.local.json`
   stays out. Verify the staged column before committing (git add fails silently on this repo).

## 2026-10-08

### Console popup (Tilde), new games at 08:00 under clear skies, `starter` — all verified with `ue.py nettest features`
- **Tilde popup:** `AMOPlayerController::InputKey` override catches `EKeys::Tilde` (Development builds only; compiled out of Shipping) and opens the
  existing `WBP_TextInputModal` via `UMOGameUIManagerSubsystem::PushModalWidget`; Run executes the line with `ConsoleCommand`, captures what it
  printed or logged (`FMOConsoleLogCapture`, needs `GLog->FlushThreadedLogs()` or the threaded log arrives after the capture is removed) and shows it as
  a notification + `[MOConsole]` log line. A stray leading `` ` ``/`~` (the opening keystroke can land in the box) is stripped
  (`SanitizeDevConsoleInput`, unit-tested).
  - `UEnhancedInputComponent` DELETES the legacy `BindKey` (compile error C2280), so the hard-coded key is an `InputKey` override, not a binding.
  - **The engine's own console owned Tilde** (`DefaultInput.ini` re-added `+ConsoleKeys=Tilde`) and consumed the key in `UGameViewportClient` before
    any controller saw it. Removed; at runtime `ConsoleKeys` is now just `'` (Quote), so the engine's own console still opens on the apostrophe key.
  - Verified: a real Slate key event (`MO.Test.PressKey Tilde`) opens the popup, a second press does not stack another, and `MO.Console.Run ~ MO.Clock.Info`
    (the popup's execution path) strips the tilde and reports `[MOClock] ... GameDateTime=...`.
  - NOT verified by me: typing into the popup's text box with a real keyboard (a harness cannot type into it).
- **08:00:** `UMOGameClockSubsystem::DefaultStartDateTime` is now 2026-06-01 08:00 (was 06:00). Measured on a fresh hosted world: `GameDateTime=2026.06.01-08.00.04`.
  Pinned by `MOFramework.Clock.FreshWorldStartsAt8AM`.
- **Clear skies:** `UMOWeatherIntegrationSubsystem::ApplyNewGameStartConditions()` (called from the NEW-GAME branch of `AMOGameMode::HandlePendingNewGame`,
  never on a load) applies the UDS `Clear_Skies` preset, queued until the weather provider registers if it has not yet. A preset is a TRANSITION:
  measured Partly Cloudy (cloud 1.73) at +2 s -> Clear Skies (cloud 0.00) at +11 s. `MO.Weather.SetPreset` now shares the preset loader
  (`LoadUdsWeatherPreset`). A hostsave step asserts that loading a save does NOT apply the new-game conditions.
  Caveat (pre-existing, not changed): the game clock is per machine (not replicated), so a co-op client's clock starts at 08:00 on its own, not at the host's current time.
- **`starter`** (also `MO.Player.Starter`): 50 x `Stick01` + 20 x `Stone01` into the local pawn; adds as many as fit and reports the shortfall; host / single player only
  (inventory mutation is server-side, a co-op client cannot cheat items in). Verified: inventory sticks/stones (20, 0) -> (70, 20). It shares one validated
  `GiveItemToLocalPawn` helper with `MO.Player.GiveItem` (which keeps its all-or-nothing behavior).
- New dev commands: `MO.Console.Run <cmd>`, `MO.Test.PressKey <Key>`. New harness mode: `ue.py nettest features`.

### Real two-account Steam test (Wes, 2026-10-08)
- Wes tested a real Steam multiplayer session: it works, and both characters walk on top of the terrain. This closes the one item every
  earlier summary listed as unverified ("a real two-account Steam join"). Reported by Wes; which build and whether it hosted a new world or a
  save was not recorded, so the host-a-save path over real Steam is still only verified locally.

## Overnight 2026-10-08 (approved by Wes before bed; commit + push at the end approved)
Approved list: (1) replicate host clock + weather to co-op clients; (2) `nettest actions` (client pickup/drop/craft/build/harvest/attack);
(3) real-window checks (type `starter` into the ~ popup, in-game Load panel hides Host); (4) the 17-50 cm post-join ground wobble;
(5) 12-round soak + hostsave + auto + refreshed Development package; (6) housekeeping + commit plan; then commit and push in chunks.
Not building: colonist ground far from players (design fork for Wes).

### Progress (overnight run)
- **Item 1 (clock + weather sync): built and verified on the positive path** -- `AMOGameState` replicates `FMOWorldClockInfo` / `FMOWorldWeatherInfo`;
  `UMOWorldSyncSubsystem` publishes (host) and applies (client). Client adopted the host's 14:01 clock + Rain after a mid-day join, followed a host
  time skip to 20:30 within ~2 s, and followed a weather change to Clear_Skies. The `--withhold-sync` control proved the CLOCK part is necessary
  (host 14:01:26, client 08:00:56 with publishing withheld).
- **Weather uses the project's bridge, not native UDS (Wes, mid-run):** the client applies the host's preset through
  `UMOWeatherIntegrationSubsystem::SetWeatherPreset` -> `IMOWeatherProviderInterface` (BP_WeatherBridge). The weather control no longer judges the
  final label (it showed 'Rain' on the client even with publishing withheld, so something else can move that label); it asserts the BRIDGE path
  ran: `[MOWorldSync] client following the host's weather preset` + `SetWeatherPreset ... dispatching to provider` on the client.
- **Spawning in trees (Wes's report): fixed.** `MOSpawnClearance` (capsule-fit check, ring search to the nearest clear ground) is used by the join
  spawn, first spawn, the fall-through rescue; `AMOCharacter::CheckEmbeddedSafety` frees a player pawn that is inside solid geometry for >= 3 s.
  Control (rescue disabled): pawn stays stuck; rescue on: moved 862 cm to clear ground. Automation 135/135.
- **Weather bridge rule vs. measured reality (Wes, mid-run: "use the bridge not the native UDS stuff").** Probe of a real host+client: `Ultra_Dynamic_Sky_C` and
  `Ultra_Dynamic_Weather_C` replicate on their own; `BP_WeatherBridge_C` does not. Tried making the bridge the only path
  (`SetReplicates(false)` on those two actors on the authority). Result: the MO sync reached the client's bridge (`client following ... Rain` then
  `SetWeatherPreset ... dispatching to provider`) but the client stayed on 'Partly Cloudy' -- UDW's Change Weather is server-only. **Reverted.**
  The CLOCK does need the MO sync (control: host 14:01 vs client 08:00 with the publish withheld). Open fork for Wes: give BP_WeatherBridge a client-side
  apply of the replicated preset (BP change), after which native replication can be switched off.
- **Harness fixes from this:** weather label is no longer used as a negative control (UDS carries it); controls assert what the MO path did (no `client
  following` line) and the SKY's own Time of Day is now compared (UDS `time_of_day`), since a clock label can agree while the sky does not.
- **Embedded-pawn test made deterministic:** `MO.Test.EmbedPawn` now pins the pawn (MOVE_None). Unpinned, character movement's depenetration freed the pawn from the
  block in one run (829 cm) and not in another (22 cm), so the control was flaky. With the pin: control stays (0 cm), rescue moves ~840 cm.
  One run (inside `--withhold-sync`) had the rescue NOT fire (moved 0 cm); its host log was overwritten by the next run -- the harness now prints the host's
  actual `is stuck inside ...` line so a repeat is diagnosable. Watching for it in the soak.
- `nettest actions` added (client pickup identity / craft / attack, each proved on the HOST with a control); first run pending below.
- **Item 1 DONE (verified):** `nettest game` PASS incl. sky checks -- after a mid-day join host sky 14.02 h / client sky 14.02 h; after a time skip to 20:30 host 20.5 h /
  client 20.5 h; weather Rain then Clear Skies agree. `--withhold-sync` control PASS (clock host 14:01 vs client 08:00; SKY host 14.03 h vs client 8.02 h; no MO weather
  sync ran on the client). New verb `MO.Weather.SkyTime` asks the BRIDGE (BP_WeatherBridge::GetDateTime -> UDS) what the sky shows, so the check is on what a player
  looks at, not the clock label. Likely source of Wes's "one window day, the other night": the control run (deliberately different clocks) or the moment right after the
  20:30 skip before the client's next sync (~2 s).
- **Item 2 DONE (verified), `nettest actions` PASS** -- all proved on the HOST with a control each: client pickup returns the SAME item GUID to the joiner's inventory (control: GUID
  not in the inventory while the stick is on the ground); client craft (control: a request for a pawn WITHOUT ingredients/skill is refused, queue 0 -> 0; after
  `MO.Test.GrantRecipe 1 KnapFlintFlakes` on the host the queue goes 0 -> 1); client attack puts the joiner's pawn in combat on the host (control: not in combat before).
  Not covered, by design of the existing verbs: `MO.Test.DropPickup` gives+drops through the LOCAL inventory (authority-only), so it is a host/PIE test; there are no
  build/harvest MO.Test verbs -- logged as a gap, not built.
  New: `GrantRecipeRequirements` (one definition of "set a pawn up for a recipe", shared by `MO.Test.Craft` and the new host verb `MO.Test.GrantRecipe`).
- **Item 4 (residual 17-50 cm post-join ground wobble): NOT reproduced.** 12-round soak on the final build: 96 settle samples and 12 terrain grids (11x11, 200 cm apart),
  every one 0.0 cm host-vs-client (max |d| 0.0, 121/121 points each round), "never mismatched" in all 12 rounds, 0 failing steps (157 PASS lines). The wobble seen
  earlier predates the collision-invoker fix reaching its final form; nothing left to fix on this evidence. (If it returns, the soak prints the signed map per round.)
- **Item 5 (in progress):** `nettest hostsave` PASS (25 steps), `nettest features` PASS (11), `ue.py auto` 135/135 passed (23 s). Development package refresh running.
- **Item 5, packaged build smoke (found a real co-op bug):** packaged Development build (97 s BuildCookRun), host started with `-ExecCmds="MO.Session.Host SmokeHost 4"`
  (the Host button's own path; a direct `...MOPCGScattering?listen` map open skips the new-game flow: no seed, no pawn), second copy `MO57.exe 127.0.0.1 -NoSteam`:
  host published seed 2099452144, the client received and applied the same seed, created its runtime, possessed a pawn, snapped its clock to the host's 08:00 and
  followed the host's Clear_Skies. **Bug found in the client log:** `Wolf_C: NO CONTROLLER` -- `UMOSpawnManagerSubsystem` (created in every game world) ran its spawn loop on the
  CLIENT, which filled its copy of the world with creatures that exist nowhere else. Diagnosis: the spawn manager is not authority-gated. Fix: `Tick` returns on `NM_Client`.
  New check in `nettest game` (`_client_spawns_nothing`): control = the host's spawn manager logs a spawn; then the client's log must hold none. **Seen failing on the unfixed
  build** (`Category 0 first spawn ...` on the client), passing after.
- **Second packaged-build bug, a crash:** the packaged host (started via the queued Host command, intro video not skipped) died 5 s after the intro began:
  `EXCEPTION_ACCESS_VIOLATION in UMediaPlayer::IsPlaying <- AMOMainMenuPlayerController::PlayIntroVideo lambda`. Diagnosis: the 5-second intro fallback timer lives in the
  GAME INSTANCE's timer manager and captured a raw `this`; travelling away from the menu before it fired left it pointing at a dead controller. (A real player reaches this
  by joining a friend's invite or loading a save while the intro is still starting up.) Fix: `FTimerDelegate::CreateWeakLambda` + `EndPlay` clears the handle.
  Re-packaged build, same smoke: host survives past the old crash point, 0 fatal lines.
- **Packaged smoke on the FINAL build:** host published seed 672839312; the client was welcomed, applied the same seed, snapped its clock to the host's 08:00:44 and followed
  the host's weather preset; the client log has NO spawn-manager lines and no `NO CONTROLLER`, the host's does (control). Both processes stopped by recorded pid.

### Final accounting (final binaries, after the last code change)
- `ue.py auto`: **135/135**. `nettest game` PASS, `nettest hostsave` PASS, `nettest features` PASS, `nettest actions` PASS, `nettest game --withhold-sync` PASS (the control).
  Earlier on the way: `nettest soak --rounds 12` PASS (0 failing steps, every ground sample 0.0 cm).
- Packaged Development build (`Saved/StagedBuilds_DevTest`, 78 s cook) refreshed from the final code; booted as host (via the queued Host command) and joined by a second packaged
  copy: same seed, clock and weather on the client, no client-side spawns, no crash.
- Pushed to origin/master in chunks: `4c12f827` Steam driver + build-info race, `e5ed6861` menu (Host a save, Join reason, intro-timer crash), `216b7260` co-op core,
  `af29c316` test harness. Docs/log committed after the battery.

### Not done / left for Wes
1. **Real-window checks (list item 3)** -- typing `starter` into the ~ popup with the REAL keyboard and confirming the in-game Load panel hides Host. Needs the screen; not done
   while Wes was at the PC. The popup/starter path IS covered by `nettest features` (real Slate key event) and the Load-panel default by code path.
2. **Weather through the bridge only** (Wes's preference): needs a client-side apply in `BP_WeatherBridge` (Blueprint change). Until then UDS's own replication carries the weather
   LABEL to clients; the clock and sky time ride the MO sync. Native replication must NOT be switched off before that (tried: client weather stops following).
3. **Colonist ground far from players** (design fork, deliberately not built).
4. **No build/harvest MO.Test verbs exist** to run from a client; `MO.Test.DropPickup` is a host/PIE test by design (local inventory). Logged, not built.
5. **The in-game (not main-menu) Load panel** has no screenshot proof of the hidden Host button.

### Real-window checks (done with the PC free; packaged Development build, real keyboard and mouse via computer-use, MO57.exe only)
- **Tilde popup + `starter`: PASS.** The real ~ key opened the Console popup (`[MOConsole] popup opened`); typed `starter`, clicked Run -> `[MOCheat] starter kit: 50/50 x Stick01 20/20 x Stone01`
  and the result showed on screen as a notification. The popup remembers the last command ("Recent: starter").
- **In-game Load panel hides Host: PASS, with its control.** Made a save through the popup (`MO.Save.SaveAs zz_realwin` -> `.sav` written). In-game (Esc -> Load) the row shows only
  **Delete / Rename**. After Main Menu -> Exit (confirmation dialog), the MAIN-menu Load panel shows **Delete / Rename / Host** for the same row.
- **Bonus, packaged: Host on that row** (real click) -> `Host requested for saved slot: zz_realwin` -> `CreateSession succeeded` -> `Save loaded successfully: 9 pawns ... seed=1915807424`
  (the saved seed) -> `Auto-possessed last-played pawn`; the resumed world rendered with the pawn.
- Cleanup: game stopped by recorded pid, my `zz_realwin.sav` deleted from the STAGED build's own SaveGames; the project's `Saved/SaveGames` (Harper_Wright-01, Test158, v1gate) was not touched.
- Note: the first click on the window landed on the desktop (Explorer had focus) and was ignored; the second focused the game. A real player would not hit that.
- **Found, not fixed:** during that save the packaged build logged a non-fatal render ensure, `FD3D12DynamicRHI::RHIReadSurfaceData: Ensure condition failed: InRHITexture` (D3D12RenderTarget.cpp:599),
  most likely the save-thumbnail capture reading a null render target. The save itself succeeded (and loaded). Worth a look at the thumbnail code path; not a gameplay blocker.

### 2026-10-09 follow-up: thumbnail ensure, save-tile overflow, unreadable text fields (UNCOMMITTED; verified in a packaged build with real input)
- **Thumbnail ensure fixed at its cause.** `FViewport::ReadPixels` from game code reads the viewport's render target, which only exists while the viewport draws: RHI ensure
  (`InRHITexture`) and a BLANK 193-byte PNG was being saved. `UMOPersistenceSubsystem` now asks the engine for a screenshot (`FScreenshotRequest` +
  `UGameViewportClient::OnScreenshotCaptured`), encodes it with the pure `EncodeThumbnailPng` (centre-crop to a square, 80x80) and re-writes the slot. Packaged: `Save thumbnail ...:
  1280x720 capture -> 19819 bytes PNG, slot re-written ok=1`, no ensure, and the Load tile shows the picture. Test `MOFramework.Persistence.ThumbnailEncoding` (with a blank-image control).
- **Long save names truncate inside their column.** Ellipsis alone was not enough: the name's text box was wider than its column (measured in PIE, 389 px vs 370; it ran 19 px under the
  buttons), so `UMOSaveSlotEntry` also clips the column and adds a right margin. PIE rects and a packaged screenshot: "ThisIsAnExtremelyLon..." ends well before Delete/Rename.
- **Text fields readable.** Typed text in every `UEditableTextBox` is now near-black on the light field via one helper, `UMOUIUtils::ApplyReadableTextInputStyle` (called by the console popup /
  text-input dialog, Host panel x2, New Game panel x2, character info entry). Test `MOFramework.UI.TextInputStyleIsReadable` (control: the engine default is a mid grey, luminance 0.285).
- **Engine bug hit on the way:** `UEditableTextBox::SetWidgetStyle` hands Slate a pointer to its PARAMETER; my first version of the helper used it and the packaged game crashed the first
  time the console popup opened (`FCachedTypefaceData`). The helper edits `WidgetStyle` in place and calls `SynchronizeProperties()` instead (see memory `ue58-ui-render-traps`).
- 137/137 automation tests. Test saves I made were deleted (staged build folder and project `Saved/SaveGames`; Harper_Wright-01 / Test158 / v1gate untouched).
- **Save tile layout (Wes: bigger thumbnail, text further left):** measured first, then changed as data (`Content/Python/ui_specs/save_slot_layout.py`, applied with `ui build`).
  Before: the tile's three columns were equal shares, so the 80 px thumbnail sat at the left of a 383 px column and the text began 260 px after it, with only ~357 px of width.
  Thumbnail 80 -> **104 px (+30%)**: the largest square that fits the tightest tile (in-game Load panel with Host hidden: row pitch 112 px, two buttons 96 px), vertically centred so it
  does not hang from the top of the taller main-menu tile. Stored thumbnail raised 80 -> 128 px so it stays crisp at that size (older saves keep their 80 px picture).
  Text column Fill 2 : buttons Fill 1 and the thumbnail column Auto: the name starts right next to the picture and gets ~669 px (was 357, +87%); buttons 389 -> 351 px.
  PIE rects confirm both tile variants; packaged real-window check: the name now reads "ThisIsAnExtremelyLongSaveGameNameThat..." (was "...Lon..."), clear of Delete/Rename.
  The thumbnail's stored PNG is 43 KB at 128 px (was ~20 KB at 80 px). Test saves deleted again; Harper_Wright-01 / Test158 / v1gate untouched. Still uncommitted.

## Overnight 2026-10-09 (approved by Wes; commit + push approved; plan: Docs/OVERNIGHT_PLAN_2026-10-09.md)
- **Item 0 done:** today's UI work pushed in two commits -- `d9b3494e` (thumbnail capture, readable text fields, tile clip + tests) and `7162de34` (bigger thumbnail + text layout asset/spec, plan, log).
  Hourly resume job `c440dad6` (:41) scheduled; keep-awake held.
- **Client trust boundary (pushed, `076e81ed`).** `nettest actions` now has a hostile-client section, each case with a control: `ServerPlaceBuilding` far from the pawn was accepted
  (audit H19) -> the server now holds it to `MaxPlacementDistance + 600 cm` (test control `MO.Building.ServerReach.Disable 1` lets it through again); a pickup 45 m away is refused (server reach 30 m);
  possessing the host's pawn is refused; terraform 3 m ahead changes the ground (control) and 100 m away is refused. **Open, logged, not fixed:** no collision/slope/rate limit on placement;
  `ServerApplyTerraform` has no server-side timer or tool requirement; the Development-only `ServerSpawnActorNearController`. Also fixed: the spawn manager ticked on clients (client-side creature
  spawning) and the main-menu intro timer fired after `EndPlay` (a startup crash in the packaged game).
- **`nettest packaged` (pushed):** two copies of the packaged Development game, judged from their logs (no crash/ensure, same seed, client follows host clock + weather, only the host spawns). It found
  the intro-timer crash, client-side spawning and the blank save thumbnail that the editor-binary `-game` runs could not.
- **Bug Report button (pushed, `36449f8d`):** opens `UMOCommunitySettings::BugReportUrl` (Discord invite, `Config/DefaultGame.ini`, QUOTED: an unquoted `//` is an ini comment and the packaged game read the URL as `https:`)
  in the default browser; clipboard + notice when no browser can be launched (test CVar `MO.BugReport.SimulateBrowserFailure`). Real click verified in the packaged build, both branches.
- **Crash archive (Wes's `mo-crashes-2026-10-09.tar.gz`, 72 uploads):** no user crashes. 57x Voxel PCG `ensure(Component)` (dev-editor PIE noise), 10x `InRHITexture` (the old thumbnail `ReadPixels`, fixed), 2x module-not-loaded
  assertion at startup, 3 single events with no recoverable stack (editor, dev game, one Shipping). Parsers promoted to `Tools/ue_crash_bundle.py` + `Tools/crash_triage.py` (below).
- **In-game bug report FORM (Wes: "add the bug report form, option A"; built, verified, committed).** The Bug Report button now opens a form (title, category, what happened, steps, optional contact, checkboxes for the
  log tail and a screenshot, Preview, Send, Open Discord). It reuses the crash endpoint, so the website needed no change: a report is the engine's own "CR1" upload bundle (`MOBugReportBundle`, the layout read from
  `CrashUpload.cpp`) marked `CrashType=BugReport` with `ReportKind=bugreport` in the query. `UMOBugReportSubsystem` collects the state through REGISTERED CONTRIBUTORS (Build, System, Session, Clock, Weather, Player; a new
  system adds rows with one `RegisterContributor` call and nothing else changes), the log tail (256 KB), a UI-less screenshot taken when the form opens, and uploads over `FHttpModule`; a failed send is kept in
  `Saved/BugReports/<id>.uecrash` and the player is told where. Privacy: user name, computer name and `C:\Users\<name>` are scrubbed from everything collected (UE writes the first two into its own log header), typed text is sent
  as typed and shown by Preview (one builder for Preview and the file), contact is opt-in, one send per 60 s, 4 MB cap (screenshot dropped first, then log).
- **Verification.** 146/146 automation tests (7 new: bundle round trip incl. the engine's own inflate, scrubber with controls, text helpers, endpoint rules incl. the user-info URL trick, report builder, screenshot encoding with an
  averaging check, multi-line text style); 16 Python tests for the parser/receiver; `Tools/ue_crash_bundle.py` parses all 72 REAL uploaded crash bundles (so the layout derivation is right). `ue.py nettest bugreport --package`
  (new, packaged game x3): accepted (valid bundle, state fields, log, JPEG, no user/computer name; control: the raw log DOES contain both), server 500 (NOT SENT, file kept, parses, player told), unreachable (same). The first run FAILED
  one phase because the harness read the previous phase's log: fixed, re-run green -- reported rather than hidden. **Real window (packaged, real clicks and typing):** form opens from the menu button, typed text is dark and readable
  in single- and multi-line boxes, Preview shows the report, Send -> "Thank you - your report was sent", the receiver holds exactly what I typed plus the screenshot (the game without any menu), and closing/reopening keeps the draft.
- **Bugs the real window found (all fixed):** (1) SizeBox overrides written by `ui build` were inert -- `set_editor_property` stores the value but not the `bOverride_*` flag, so the multi-line boxes were one line high and the preview
  ran off the screen; `mo_ui.set_props` now calls the SizeBox setters (a TOOL bug: other older specs probably carry inert sizes, see `Docs/UI_TOOLING.md`); (2) the "Show What Will Be Sent" label overflowed its button -> "Preview";
  (3) the preview showed a 32-zero report id -> "(assigned when sent)" (tested); (4) the multi-line font was half the single-line size -> 16 in the spec (asset saved; not yet re-screenshotted -- covered by the morning package).
  Also fixed in the tool: `ui preview` called a class that does not exist in 5.8 (it still cannot create the widget from Python; documented), and a native class is now allowed as the PARENT of a new Widget Blueprint.
- **Not verified, said plainly:** the panel's Open Discord button after I moved the open-link code into `UMOCommunitySettings::OpenBugReportLink` (the same code was verified by a real click in the old place; opening a browser
  tab on Wes's machine to re-check seemed worse than the risk; no clipboard test either, it would overwrite Wes's clipboard); the contact field with a value; a real send to the production endpoint (deliberately never done:
  it would put a test report into the site's real data).
- **Local-only patch:** `PCGWaitForVoxelWorld.cpp` `ensure(Component)` -> graceful return (57 of the 72 uploaded crash reports; weak execution source of a destroyed PCG component). `Plugins/Voxel/` is gitignored, so the patch is
  recorded in `Docs/Voxel_Plugin_Reference.md` ("Local patches"). Compiled and cooked into the packages above; nothing else exercised it.
- **Gotcha for next time:** computer-use `open_application` on an already-running game LAUNCHES A SECOND COPY (full-screen intro). I stopped that one by pid; use `request_access` + clicks, never `open_application`, for a
  running game.
- **Wes's report: "the pawn on the client can't see the pawn controlled by the host, but the host can see the one controlled by the client" -- two causes found, three fixes, verified (first half here, second half below).**
  *Reproduced* with two editor-binary -game processes (`Tools/pawnvis_probe.py`): the HOST's own pawn sat at z between -2900 and 200 while the voxel ground under it was z=1667 (the client's pawn stood on the
  ground at z=1774); the host log had "Fall-through detected! No valid terrain found, teleporting to default height Z=200" every few seconds. Both machines had the same seed and the same ground.
  *Why (two layers):* (1) the new-game spawn is traced against collision that exists BEFORE the pawn does; the pawn is the voxel invoker, so the collision around it is regenerated afterwards -- over 8 fresh worlds the
  final ground differed from the spawn trace's by -589..+346 cm, so a pawn spawned 200 cm above the old surface can end up under the new one and fall out of the world. (2) The rescue could not find the ground:
  `MOSpawnClearance::TraceVoxelGround` used a multi trace BY CHANNEL, which ends at the first BLOCKING hit, so a tree/roof/prop over the ground hid the terrain behind it ("no ground"); the rescue then used the
  fixed Z=200, which is under the ground on this map.
  *My first explanation was WRONG and I said so to Wes mid-run:* I blamed the map's PCGVolume brush. `MO.Test.RescueTrace` showed the brush does not block the channel trace (it only appears in the object-type probe), and a
  control that passed at an arbitrary spot proved nothing about the failing column. The real blocker class was found by hosting 8 fresh worlds (`rescue_probe.py --hunt`): the first settle prototype timed out in 5 of 8.
  *Fix:* spawn settle in `AMOGameMode` (hold the new pawn, poll the ground until it is stable for 1.5 s, place the pawn on it, THEN release queued joiners); `TraceVoxelGround` is now an object-type multi trace;
  every rescue trace goes through `AMOCharacter::TraceTerrainLine`; the rescue's fallback is the pawn's last grounded spot instead of Z=200; a failed rescue logs what blocks the sky line (so the next report's log tail says why).
  *Verified:* hunt over 8 fresh worlds: 8/8 settled in ~2 s (pawn moved -589..+196 cm), standing height 84.4-85.9 cm, 0 rescues (before: 2/8 spawned under the ground; first prototype 5/8 timeouts).
  `nettest actions` (buried pawn 15 m under the surface with a solid block over the ground): the OLD first-hit rule (`MO.Rescue.FirstHitOnly 1`, the control) says "No valid terrain found" and the new log line names the block;
  the fixed rule says "Found safe terrain" and puts both the host's pawn and the joiner's back on the ground. The control asserts on the rescue's LOG, not the pawn's final z, because the fixed-height fallback happens to be
  above the ground in worlds whose ground is below Z=200 and would have hidden the failure. 146/146 automation tests, 94 Python tests.
  *Not verified:* that these are the ONLY causes of what Wes saw (his run may differ; the new rescue log line will say); a player's view in a real window of the fixed build (the two probe windows showed both pawns standing on grass,
  but I did not walk a real client up to the host's pawn).
- **SECOND half of the same bug (found by looking at the fixed build, `pawnvis_probe` with movement mode + velocity): the host's pawn was fine on the HOST (MOVE_WALKING, z=619) but the CLIENT's simulated copy of it was
  MOVE_FALLING at vz=-4000 and z=-498790 after a minute.** A client predicts a fall for any simulated proxy with no floor under it (`UCharacterMovementComponent::SimulateMovement`: "No floor, must fall"); voxel terrain exists on a
  machine only around ITS OWN human-driven pawns (30 m invoker) and not at all before the client's runtime is created from the host's seed; a standing pawn sends no position updates to correct the prediction. That is why the host
  sees the client's pawn (the client drives it: the server mirrors its moves) and the client does not see the host's. *Fix:* `AMOCharacter::OnUpdateSimulatedPosition` never lets a proxy apply gravity (its height is the server's).
  *Verified (`nettest actions`, with a control):* the client's collision is switched off under the host's pawn; with `MO.RemotePawn.SimGravity 1` (stock engine behaviour) the copy falls (z=-22637..-23505, MOVE_FALLING, vz=-4000);
  the next position update snaps it back; with the fix and no floor it stays at the host's height (z=1510/2344). `nettest game`, `hostsave` and `churn` still pass with all three fixes.
- **`nettest churn` (new): PASS.** 3 leave/rejoin cycles keep and hand back the same pawn (no second pawn, two players each with a pawn), save-while-connected OK and the client stays connected, 4 of 4 item races
  have exactly one winner (180-356 ms apart; the joiner won every time -- the host's own console pickup is the slower path, so this proves "no duplication", not fairness), and no player/colonist rescue, crash/ensure or game
  Error on either machine. It took four runs to get a trustworthy race: harness bugs found and fixed, each reported as a FAIL when it happened -- stacks merging into the previous trial's stack, `unreal` wrapper objects never
  comparing equal (the setup deleted the item it had just dropped), `Vector` having no `.size()`, and the analyser counting the spawn manager's deer (which land off-terrain and are rescued by the same code) as a player rescue.
- **Audit/other overnight items:** `UMOGameUIManagerSubsystem::NotifyPlayerRemoved` use-after-free read fixed (`RemoveAndCopyValue`; nothing calls it yet, and the freed memory cannot be observed deterministically, so no
  automated test -- said plainly); Voxel PCG `ensure(Component)` (57 of 72 uploaded crash reports) patched in the vendored plugin (`Plugins/Voxel/` is gitignored: recorded in `Docs/Voxel_Plugin_Reference.md`); the terraform excavation
  plan was refreshed against the code (stages 1-3 had already landed; 4-7 not started) with a remaining-work breakdown and the decisions only Wes can make; weather-bridge investigation NOT done yet (probe script ready).
- **Unattended-run runner (Wes: "I have to click accept every 20 seconds"):** `Tools/ov.bat` is the one stable command; it runs `Tools/ov_task.sh` (rewritten per job, untracked) from a snapshot archived in
  `Saved/Logs/ov_history/`. Every run since then went through it.
