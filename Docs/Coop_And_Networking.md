# Co-op and networking

> **Covers:** Steam / LAN sessions (host, join, host a save), how players get and keep pawns, per-machine terrain from a replicated
> seed, spawn settle and fall-through rescue, the RPC rules every player action follows, interaction targeting, and how co-op is
> tested. General replication patterns (FastArray, COND_OwnerOnly, authority checks, timers): `Docs/TECHNICAL_REFERENCE.md`.
> **Current state (2026-10-17):** a real two-account Steam session works with both characters on the terrain (Wes, Oct 8); listen-host
> co-op (Satisfactory-style, not MMO, 1-8 players, default 4); construction, crafting, item / fire / world actions and treatment go
> through server RPCs; the host's world seed, clock and weather preset replicate through `AMOGameState`.
> **Entry points:** `UMOSessionSubsystem` (`HostSession`, `FindSessions`, `JoinSessionByIndex`, `IsLanMode`, `ReleaseStaleSession`,
> `ClampMaxPlayers`), `MOTravelUtils` (`bAsListenServer`), `AMOMainMenuPlayerController::StartHosting` / `HostSavedGame`,
> `UMOHostGamePanel`, `UMOJoinGamePanel`, `AMOPlayerController` (`SetPawn`, `PawnLeavingGame`, `ForwardUIContractRequest`,
> `ServerPossessPawnByGuid`, ...), `AMOGameMode` (`AssignPawnToJoiner`, spawn settle), `AMOGameState` (`WorldSeed`, clock / weather,
> `RemovedInstances`), `UMOWorldSeedSubsystem`, `UMOWorldSyncSubsystem`, `MOSpawnClearance`, `UMOInteractorComponent`.
> **Dev verbs:** `MO.Session.Host <name> [max]`, `.Find`, `.Join <idx>`, `.Leave`, `.Status`, `MO.Session.HostSave <slot>`,
> `MO.Possess.List|Take`, `MO.Voxel.Seed|SurfaceZ|SurfaceGrid`, `MO.Test.RescueTrace`, `MO.Test.EmbedPawn`, `MO.RemotePawn.SimGravity`,
> `MO.Rescue.FirstHitOnly`, `mo.Interact.Debug 1`. **Tests:** `python Tools/ue.py nettest lan|steam-host|game|hostsave|soak|actions|churn|features|packaged|packaged-death|packaged-removal|bugreport`
> (`Docs/AUTONOMOUS_TOOLING.md` "Multi-process / online-session testing"). **Open items:** `Docs/BACKLOG.md` § Co-op.

## 1. Sessions (Steam and LAN)

- App ID **1602810** ("Metaverse: Origins", the real public Steam title; MO57 is the repo name). `OnlineSubsystemSteam` + `SteamSockets`
  enabled; `Config/DefaultEngine.ini`: `DefaultPlatformService=Steam`, `SteamDevAppId=1602810`, `[/Script/Engine.GameEngine]`
  `!NetDriverDefinitions=ClearArray` then `/Script/SteamSockets.SteamSocketsNetDriver` (the old `OnlineSubsystemSteam.SteamNetDriver`
  does not exist in 5.8 and fell back to IpNetDriver silently: lobbies advertised a Steam P2P address while the game listened on UDP).
  `steam_appid.txt` is tracked (App IDs are public).
- **Steam is disabled inside the editor / PIE by design** (`FOnlineSubsystemSteam::IsEnabled()` is false under UE_EDITOR unless running
  as game): every editor log shows Null / offline. Test Steam in a `-game` process (`nettest steam-host`). One Steam account can't join
  its own lobby: a real join needs a second account / machine.
- The Null OSS is NOT a stub: its CreateSession / FindSessions run a LAN beacon, so host / find / join is testable across two processes
  without Steam (`nettest lan`). A host does not discover itself; single-process PIE never exercises `IOnlineSession`.
- `IsLanMode()` is the one predicate for Host + Find (the Null OSS answers only `bIsLANMatch=true`).
- Sessions outlive worlds: `ReleaseStaleSession()` on main-menu arrival, and Host / Join replace a stale session.
- UI: `HostGameButton` -> `UMOHostGamePanel`, `JoinGameButton` -> `UMOJoinGamePanel` (found BY TYPE, switcher index by
  `GetChildIndex`); session rows `WBP_SessionListEntry` (the row button must be the WBP `MOButton`, see `Docs/UI.md`). Join enables only
  with a non-full selection and logs why. Built by `ui build Content/Python/ui_specs/coop_menu.py`.
- **Hosting:** `AMOMainMenuPlayerController::StartHosting` is the ONE flow; `HostSession` = fresh world (seed 0 = random),
  `HostSavedGame(slot)` = resume a save (Load panel's Host button). Main-menu panels resolve their index from the widget
  (`ResolvePanelIndex`; the hard-coded indices were swapped).
- Not built: friend-invite / Steam-overlay join (`InviteUserToGame`). Unknown: whether hosting a SAVE was exercised over real Steam.

## 2. Pawns in co-op

- `AMOPlayerController::SetPawn` runs pawn-control setup on the OWNING CLIENT too (OnPossess is server-only: a joiner's input never
  reached his pawn). `CachePossessedPawn` / `ClearPossessedPawnCache` are shared.
- Possession goes through controller RPCs (`ServerPossessPawnByGuid`, `ServerCreateCharacter`, `ServerRequestPossessionList` /
  `ClientReceivePossessionList`) into `UMOPossessionSubsystem`; the list is the SERVER's pawns (slim `FMOPossessionListEntry`).
- **Pawns survive their player:** `PawnLeavingGame` keeps a recruited pawn (it gets an AI controller); `AMOGameMode::AssignPawnToJoiner`
  reuses the player's previous pawn, else the nearest available, else spawns.
- `CheckFallThroughSafety` is authority-only (clients were teleporting other players' pawns).
- Quests are credited to the player who acted (server-side adds / crafts / builds / level-ups report per player).

## 3. Terrain is per machine (the seed replicates)

- Every machine generates its own voxel terrain from the seed (the level's AVoxelWorld has `bCreateRuntimeOnBeginPlay` OFF).
  `AMOGameState::WorldSeed` (replicated `FMOWorldSeedInfo`) + `UMOWorldSeedSubsystem` (host publishes in `InitializeVoxelWorldWithSeed`,
  client regenerates in `OnRep_WorldSeed`). Determinism matters: `Docs/Caves_And_Geology.md` §2 (checksum) and `Docs/TRAPS.md` (/fp:precise).
- **Voxel collision exists only around each machine's own human-driven pawns:** `AMOCharacter::VoxelCollisionInvoker`
  (`bWaitForVoxelWorld`) is enabled in `NotifyControllerChanged` while a human drives the pawn (5/6 joins 0 cm, 1/6 17-50 cm for ~10 s).
- A client's simulated copy of a remote pawn must never apply gravity (`AMOCharacter::OnUpdateSimulatedPosition`): with no floor under a
  proxy the client predicts a fall (the host's pawn free-fell to z = -498,790 on clients). Control: `MO.RemotePawn.SimGravity 1`.
- PCG removals replicate (`AMOGameState::RemovedInstances`, `Docs/PCG.md` §7). Per-machine actors (lakes) don't replicate (`Docs/Water.md`).
- **A joiner enters behind the same world-building gate as the host** (Wes, Oct 17).
  * `UMOWorldSeedSubsystem::OnWorldBeginPlay` (client) calls `UMOGameInstance::HoldLoadingOverlayForWorldEntry`: overlay up, owned by
    the game instance while the joiner's player controller hasn't replicated yet.
  * The seed starts `UMOWorldReadinessSubsystem::BeginTracking`. Once the joiner's pawn is here, `DismissLoadingScreen` holds the overlay
    with the status bar until terrain, vegetation and streaming are in, the same code path as the host's load.
  * The Join menu also sets the flag and shows the overlay before travelling (`UMOSessionSubsystem::HandleJoinSessionComplete`).
  * Measured on two `-game` processes: pawn 0.2 s after the seed; READY 47.2 s later; reveal about 58 s after Join. The joiner saw
    tips, then shader preparation, then "Growing the land" with the bar.
  * No seed within 15 s, or no pawn within 60 s, lifts the overlay with an error logged, never a stuck screen.

## 4. Spawn settle and rescue

- New-game spawn: `FMOWorldGen::FindCoastalSpawn` (on a coast facing the water, never an island; spawn ground >= 0.75 climate biome,
  not bare shore sand) -> the pawn is spawned at generator height + 200 cm and held by the spawn settle in `AMOGameMode`: poll the
  ground until stable 1.5 s, place the pawn, THEN release queued joiners. `PollSpawnSettle` only accepts a trace within
  `SpawnSettleGeneratorToleranceCm` (40) of `FMOWorldGen::SampleHeightCm` (a stable COARSE-LOD collision put pawns underground).
- `AMOCharacter::SetLoadHold` (gravity off, no rescue) spans a save load until the terrain is regenerated (`WaitForVoxelAndRegroundPawns`
  -> `FinishLoadHandoff`).
- Rescue traces go through `AMOCharacter::TraceTerrainLine` -> `MOSpawnClearance::TraceVoxelGround`, which uses
  `LineTraceMultiByObjectType`: **a multi trace BY CHANNEL ends at the first blocking hit**, so a tree or roof hid the terrain. The
  rescue's fallback is the pawn's last grounded spot (not Z = 200). `MOSpawnClearance` (capsule fit + ring search) is used by join
  spawn, first spawn and fall rescue; `CheckEmbeddedSafety` frees a human pawn stuck >= 3 s. `BuriedMarginCm`: a pawn inside terrain
  below sea level is not "swimming" (`Docs/Water.md` §1).

## 5. Player actions and RPC rules

- **Every player action** = a request through a controller-owned component RPC (or `AMOPlayerController::Server*`), logic in an
  authority-gated component, never in a widget. Server bodies guard `HasAuthority()`. A new `IMOUIContractInterface` request starts
  with `ForwardUIContractRequest` (`Docs/Building.md` §1).
- Clients send intents, never results: no skill level, duration, refund policy, station type or completion (the crafting and treatment
  services derive them on the server; `Docs/Crafting_And_Progression.md`, `Docs/Medical_And_Survival.md`).
- Timed actions run on the pawn's `UMOTimedActionComponent` (server clock, replicated label / duration / start time, movement and
  damage interrupt): drinking, filling, boiling, fire lighting, eating, butchering, cover gathering, treatment.
- Dev-mode server RPCs (biome / poi / weather / time) check the SERVER's dev mode (`ServerAllowsDevCheats`, `Docs/AUTONOMOUS_TOOLING.md`).
- **Interaction targeting** (Oct 16): `UMOInteractorComponent::FindInteractionTarget` = line through cover -> 30 cm sweep -> nearest
  interactable within `AimAssistRadius` (40) of the line's impact, held to the server's reach (`IsWithinInteractReach`) + LOS;
  `mo.Interact.Debug 1` logs every pass. Dropped items (`AMOWorldItem`) have NO proxy sphere: the mesh is the target
  (`EnsureMeshAnswersSimpleQueries`), dropped item meshes are `ECC_PhysicsBody` (object-type queries for items must include it).
  Dropped items remember the drop's ground and settle from above (query collision present, physics body not yet, on a host near a joiner).
  Melee: the arc check is around the swing (camera-aimed), `BeginFacingSwing`, melee aim assist 20 deg (`mo.Combat.MeleeAimAssist`;
  Wes, Oct 16: "aim assist is good; pawns should punch in the direction of the camera").
- Authority clusters from the audit (H17 terraform rate limit fixed Oct 13; H18 combat, H19 building, H20 crafting queue guards; C7
  possession TSubclassOf host-only Oct 14): `Docs/BACKLOG.md` keeps the residuals.

## 6. How co-op is tested (rules)

```bash
python Tools/ue.py nettest lan            # host + client processes over the Null OSS / LAN beacon; PASS/FAIL + exit code
python Tools/ue.py nettest game --keep    # host + client on the gameplay map; --keep leaves both windows running afterwards
python Tools/ue.py inst list              # the running instances; drive one with: ue.py inst do <name> run "MO.<verb> ..."
```

- **PIE cannot test online sessions** (all clients share one process; PIE 2-player listen mode drags the second window along). Use real
  processes: `python Tools/ue.py inst start|stop|do|list` and `ue.py nettest <mode>` (`Docs/AUTONOMOUS_TOOLING.md`).
- **Never fire RPCs from editor Python** (or a synchronous console command it dispatches): `GAllowActorScriptExecutionInEditor` maps
  every RPC to LOCAL, so a client's "Server" call runs on the client. Test verbs that send RPCs defer a tick (RunOnNextTick:
  `MO.Possess.Take`, `MO.Test.Input`, `MO.Test.RenamePawnRPC`, `MO.Fire.Do`, `MO.Item.Do`); assert on the logs the GAME writes.
- Controls must be able to fail: terrain agreement = `MO.Voxel.SurfaceGrid` on both machines (voxel-collision hits only), proven by
  `nettest game --skew-seed` (seed + 1 -> 147 m apart); `--withhold-seed`, `--withhold-sync` are the other negative controls.
- After a fix, rerun the probe on the FIXED build and read BOTH machines (the host-pawn bug had a second cause).
- Run ALL the nettest gates after movement / spawn / water changes (they silently rotted for 4 days after swimming landed).
- Shipping writes no log: package Development for real-account tests (`Docs/Packaging.md`).
