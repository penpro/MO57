# Co-op and networking

> **Covers:** Steam / LAN sessions (host, join, host a save), how players get and keep pawns, per-machine terrain from a replicated
> seed, spawn settle and fall-through rescue, the RPC rules every player action follows, interaction targeting, how co-op is
> tested, and how its SMOOTHNESS is measured (rubber banding, remote pawns: section 7). General replication patterns (FastArray, COND_OwnerOnly, authority checks, timers): `Docs/TECHNICAL_REFERENCE.md`.
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
> `MO.Rescue.FirstHitOnly`, `mo.Interact.Debug 1`; smoothness: `MO.Net.Stats [0|1]`, `MO.Net.Show 1`, `MO.Net.Trace 1`, `MO.Net.AutoStats`,
> `MO.Net.TestRateCap`, `MO.Test.NetPath` (section 7). **Tests:** `python Tools/ue.py nettest lan|steam-host|game|hostsave|soak|actions|churn|features|packaged|packaged-death|packaged-removal|bugreport|netdiag`
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
  damage interrupt): drinking, filling, boiling, fire lighting, eating, butchering, cover gathering, treatment, **harvesting a resource
  node** (`FMOHarvestActions`, `ServerBeginHarvest`) and **inspecting an item** (`FMOInspectionActions`, `ServerInspectItem`, length
  `MO.Inspect.Seconds`). Both used to run in a widget's UI timer on the clicking machine: a joiner's harvest said "Inventory Full!" and
  what a joiner learned lived only in its own copy of the pawn, unsaved (Oct 18 Steam test, `Docs/BugReports_2026-10-18.md`).
- **PCG instances have no network identity.** Partition actors and their instanced-mesh components are spawned at runtime under names
  that differ per machine (two processes on one machine too), and indices shift as instances go. Never put a PCG component pointer or an
  instance index in an RPC: it arrives as null ("Unable to resolve default guid from client: PCGPartitionActor_..."). Send
  `FMOInstanceRef` (MOInstanceRef.h: static mesh + world location; `Resolve` finds each machine's own copy within 25 cm), as E on a
  ground item (`ServerRequestInstanceInteract`), the harvest and the inspection do. The other direction is world state: removals
  (`FMORemovedInstanceArray`) and worked-ground zones (`FMOReplicatedTerrainZoneArray`, MOTerrainModificationTypes.h) replicate on
  AMOGameState and each machine applies them to its own PCG copy.
- **Notifications for a joiner**: `UMONotificationComponent` on the server's copy of a joiner's controller forwards to that player
  (`ClientShowNotification`, `ClientShowSkillPopup`); it used to queue them where no one could see them.
- Dev-mode server RPCs (biome / poi / weather / time) check the SERVER's dev mode (`ServerAllowsDevCheats`, `Docs/AUTONOMOUS_TOOLING.md`).
- **Interaction targeting** (Oct 16): `UMOInteractorComponent::FindInteractionTarget` = line through cover -> 30 cm sweep -> nearest
  interactable within `AimAssistRadius` (40) of the line's impact -- an actor, or (Oct 18) a harvestable instance by its pivot, so a flint
  flush with the ground is a target -- held to the server's reach (`IsWithinInteractReach`) + LOS;
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
- **PCG cases need the joiner's PCG.** A joiner's loading overlay lifts at the readiness cap (60 s) while its PCG still fills in the small
  things for minutes on a slow machine: `_worlds_built` waits for both READY lines OF THE GAME MAP (the menu world logs its own READY first)
  and then for ground items around the joiner's pawn; a joiner on bare ground is stood by the host's pawn after 2 min, and a world that
  stays empty logs its WorldBuild fields (`MO.BugReport.Preview`: PCG components generated vs generating) -- once on Oct 18 a joiner's
  PCG made nothing at all (BACKLOG N-031).
  Instances are compared across machines by mesh + place (`MO.Test.GroundItems`, `MO.Test.InstanceAt`, `MO.Test.ResourceNodes`,
  `MO.Test.Sweepable`; MOCoopInstanceCheats.cpp). The Oct 18 cases: `_client_e_on_ground_item`, `_client_e_beside_ground_item` (the crosshair 20 cm beside it), `_trust_instance_far`, `_client_harvest`,
  `_client_inspect`, `_host_foliage_reaches_client` (`MO_ACTIONS_ONLY=<names> python Tools/ue.py nettest actions` runs a subset).
- `nettest netdiag` checks the SMOOTHNESS instruments, not smoothness: two processes on one PC talk over loopback, so it cannot show an
  internet session's rubber banding (Wes, Oct 17). Its controls (a 4 KB/s rate cap must saturate, a joiner walking 1.6x faster than the
  server allows must be corrected, +100 ms of `NetEmulation.PktLag` must show as delay) prove each measure can fail. Real tests: section 7.

## 7. Smoothness: rubber banding and remote pawns (measured, Oct 17)

Wes (chat, Oct 17): "it might be performance related of the client, but we saw really really bad rubber banding and animation latancy
and such in the host to client play" / "ping itself only showed like 9ms, but it was acting like the connection was dogshit". That was the
Oct 16 Steam playtest on **Shipping `0573ab31`** ("it was the last time i submitted a ton of bug reports"), built BEFORE `68d999b0` moved
the joiner's gait into the network-predicted moves: there every jog / sprint was a local speed change the server corrected back, a
rubber band by itself. Its mechanism, measured: the netdiag speed-hack control (a joiner moving faster than the server allows) had 26% of
its moves corrected; real walk / jog / sprint on the current build, 0.8% (all ~2 cm height corrections, invisible). Smoothness over Steam
on a current build is not confirmed yet (N-164). Nothing measured smoothness then; now every co-op session does
(`UMONetDiagnosticsSubsystem`, `MONetDiagnostics.h`):

- **One `[MONet]` line a second on every machine, without being asked** (`MO.Net.AutoStats 1`; `MO.Net.Stats 0|1` overrides): fps and the
  worst frame, bytes in / out against the connection's rate cap (`CurrentNetSpeed`, 100 KB/s), saturation (the engine's own per-connection
  count of replication passes cut short), loss, round trip, ping, jitter, replication passes (a listen server replicates every frame:
  `NetServerMaxTickRate` 30 does not apply to it), the joiner's moves the host corrected and by how much (xy / z), what the joiner saw
  of it (corrections, the visible snap after replaying its moves), how often each machine got the other player's position and the
  longest gap, and the actor classes with the most bytes (`UMONetStatsActorChannel`, the actor channel of every net driver via
  `[/Script/Engine.NetDriver]` in `Config/DefaultEngine.ini`). Every line has `utc=` (wall clock, ms), which lines up the host's and a
  joiner's logs across two PCs (sync both clocks first: Windows Settings > Time > Sync now), `play=` (0 while the world still builds
  behind the loading screen: not counted) and `st=` (the server clock; NOT for lining up: it is world time, which stops at 0.4 s a frame,
  and drifted 3-6 s apart between a hitching host and joiner).
- **`[MONetEvent]` lines, always:** a host correction or joiner snap of 30 cm or more, a frame of 250 ms or more while a player moves, a saturated second, a moving
  remote pawn silent for 500 ms. `[MONetCorr]` lines (6 a second at most while stats log) detail corrections: the error, both modes, speeds,
  the position, the FLOOR each machine stands on (actor class and mesh, the floor's height, the gap) and the movement BASE with the engine's
  own choice of `RELATIVE` or `absolute` positions over it (on the terrain the floor is a per-chunk voxel collision component, the base
  is `VoxelLandscape_0`'s root: a level actor both machines have under one name); the correction / rubber-band events carry the position and the floors too (host: its floor; joiner: before and
  after the snap), so a pair of reports names the floor the machines disagree about.
- **Bug reports** carry `Net.*`: the last 60 s, `Net.Events`, `Net.Clock` (UTC: match the host's and the joiner's reports by it) and the
  last line. That is the only record a Shipping build keeps.
- **On screen:** `MO.Net.Show 1` (development builds). **Positions:** `MO.Net.Trace 1` logs every player pawn 20 times a second.

**Steam test recipe (Wes + a remote joiner):**
1. Both sync their PC clocks (Windows Settings > Time > Sync now: the two logs are lined up by wall clock) and play the **Development** package (Shipping writes no log and has no screen messages): `Docs/Packaging.md`.
2. Host, join, play. Nothing to type: the stats log by themselves. To watch live, either player opens the console (Tilde) and runs
   `MO.Net.Show 1`. For the picture-delay numbers, both also run `MO.Net.Trace 1` (about 20 MB of log an hour).
3. When it rubber-bands, a bug report from either side records the event and the last minute (with the UTC time).
4. Afterwards each player sends `Saved/Logs/MO57.log` (rename them `host.log` / `joiner.log`) and:
   ```bash
   python -I Tools/ue_netdiag.py logs host.log joiner.log
   ```
   prints the session, every minute, every event in time order and, for each visible rubber band, what came just before it
   (a joiner frame of 250+ ms, a host frame, a saturated channel, a stalled remote pawn, or nothing).
5. **A/B toggles** for the causes found below (host console unless said; each is a test seam, not a fix):
   - `set GameNetworkManager MAXCLIENTUPDATEINTERVAL 0` -- the host stops extrapolating a joiner that stops sending moves (`0.25` = stock;
     `get GameNetworkManager MAXCLIENTUPDATEINTERVAL` reads it back).
   - `p.NetServerMaxMoveDeltaTimeScalar 1` -- the host takes no longer a move than the joiner may send (1.75 = stock).
   - `MO.RemotePawn.SimGravity 1` on the JOINER -- remote pawns keep their velocity (stock engine; see the second finding).

**Findings on two local processes (Development editor binary, both on one PC; commit of this section):**
- **Bandwidth is not it here:** the host sent 3.6-4.8 KB/s to the joiner of a 100 KB/s cap, never saturated in 63 s (22 replicated objects;
  the remote pawn ~1.2 KB/s, a deer ~0.8 KB/s). The control (a 4 KB/s cap) saturated 34 of 34 s, so the measure can see it.
- **A joiner frame hitch becomes a rubber band through the host's forced update.** When a joiner sends no move for 0.25 s
  (`AGameNetworkManager::MAXCLIENTUPDATEINTERVAL`), the host simulates its pawn on with the last input; the joiner, back, is corrected to
  where the host put it. A 500 ms `debug hitch` on the jogging joiner (450 cm/s): **124 cm snap in 3 of 3 trials** (stock), **42 cm** with
  `MAXCLIENTUPDATEINTERVAL 0` (what remains is `p.NetServerMaxMoveDeltaTimeScalar` 1.75: the host takes 0.22 s of a long move, the joiner
  0.125 s: 0.094 s x 450 = 42 cm), **0 cm** without the hitch. In the first run a 449 ms joiner frame (14 meadow-flower materials
  recompiling, an editor-binary hitch; Shipping has its own: shader / PSO compiles on first sight, streaming) at a turn gave a 230 cm snap.
  Decision for Wes: F25.
- **Remote players' pawns have velocity 0 on every other machine** (N-160): `AMOCharacter::OnUpdateSimulatedPosition` sets
  `bSimGravityDisabled` on every simulated proxy (the fall-through fix in section 3), and the engine's `SimulateMovement` reads that flag as
  "stuck": it zeroes the proxy's velocity every tick. The proxy is never extrapolated between updates (it moves in steps of the host's
  update rate, smoothed only by the mesh) and anything reading its velocity sees a standing pawn. Control: `MO.RemotePawn.SimGravity 1` on
  the joiner -> the host's walking pawn reads 250 cm/s instead of 0. (One screenshot still showed the walking pose, so the animation
  blueprint does not read only that velocity: the Steam A/B decides how much this matters.)
- **Small height corrections:** about 2 cm (dz +1.8..2.4, just over the engine's 1.73 cm tolerance) on 0.8% of the joiner's moves, 14%
  with +100 ms emulated lag; never visible (snap <= 2 cm) (N-162).
- **Replication references per-machine PCG components:** 84 `FNetGUIDCache::SupportsObject ... InstancedStaticMeshComponent
  PCGPartitionActor... NOT Supported` warnings on the host in 25 min (rocks, stones, scrub tiers): most likely a pawn standing on a PCG rock
  (its movement base) (N-163).
- Pawns are relevant within 150 m (`NetCullDistanceSquared` 225,000,000): further apart, players do not see each other.

**First Steam session with the tools (Oct 18, 06:22-07:46 UTC, build `afb9aabc`, 27 bug reports; read with `Tools/ue_netdiag.py`).**
Wes: "it looks like we did fix the weird laggy no running stuff, and it no longer kicks the client out of the pawn when you interact".
- Bandwidth: 1.9 KB/s on average of the 100 KB/s cap, 9 of 2,923 s saturated: not the problem over Steam either.
- Visible corrections (30 cm or more): 49, and 48 had no hitch, saturation or stall just before them. They are FLOOR-HEIGHT
  disagreements. Of about 2,200 `[MONetCorr]` lines:
  - 1,064 are height only with the host HIGHER (dz median +3 to +17 cm, 90th percentile up to +38 cm), mostly a sprinting joiner
    (700 cm/s) in the mountains with the joiner at 11-30 fps. That fits N-112: the voxel collision around a fast-moving pawn is still coarse;
  - 412 are a joiner standing still with the host a constant 4.7 cm higher (one report: 1,529 of 2,429 moves corrected; invisible, but constant);
  - 451 are "walking on the host, falling on the joiner" while standing at the camp. The host was looking at a `BP_BuildableFloorBase`:
    probably a building floor with collision on the host only;
  - sideways-only lines, which a bush present on one machine only would give, are about 4%.
- Two host frames of 5-6 s, and a 1.7 km correction at 07:22:44 (the joiner's pawn teleported).
- Since then the correction lines and events name the position and the floor on both machines (above), so the next session says which
  floor disagrees. PCG rocks and logs as movement bases: PCG partition actors keep their root Static and their instanced meshes inherit it,
  so positions over them travel absolute and the per-machine names that cannot resolve ("Unable to resolve default guid", "SupportsObject
  ... NOT Supported") cost a warning, not a position error (measured on two processes: 4,833 and 5,810 of as many PCG instanced meshes Static, and the
  terrain's movement base `VoxelLandscape_0.RootComponent absolute` on both machines; the lines say `RELATIVE` if a base ever is). A joiner's
  `Session.WorldSeed` showed its own old worlds (384, 21354), not the host's: the row now reads the replicated seed.
- **First floor named (Oct 18, nettest actions `_joiner_long_run`):** the joiner, at rest 1 km from the host after a 150 s sprint, was
  corrected ~10x/s by 69 cm with no end: `[MONetCorr] server ... floor VoxelWorld/VoxelCollisionComponent z=5865.3` against the joiner's
  `floor before VoxelWorld/VoxelCollisionComponent z=5796.2`; `MO.WorldGen.Sample` there gives 5796.6 on both machines. The joiner's
  collision is right; the HOST's terrain collision around a far remote pawn is ~0.7 m high (a coarser mesh on a convex slope). BACKLOG N-162.
- **Why (measured Oct 18 on two -game processes, CL-0081):** the voxel world has TWO collision layers, both `WorldStatic` and both
  blocking `Pawn`: the render chunks' visibility collision, whose detail follows THIS machine's camera, and the invoker collision (fine
  16 m chunks around every human-driven pawn, on every machine, the same on each). Under a joiner 1 km from the host, the host has a coarse
  256 m visibility chunk AND the fine invoker chunk, and a pawn stands on the TOPMOST. Where the coarse surface is above the true ground
  the host's floor is too high (+59.3 cm, 121 of 121 moves corrected; the joiner's own floor 1.1 cm off); where it is below, nothing shows
  (-0.1 cm, 0 corrections). That is why the Steam data's error is one-signed: the host is always HIGHER. The host's surface error against
  the generator grows with distance from its camera: 0.4 cm at 20 m, up to 18 cm at 300 m, ~100 cm at 1 km, ~120 cm at 2 km.
  Fix not chosen yet: a 30 m `UVoxelLODVolumeComponent` on the joiner's pawn (host side) did not change it; a 200 m one still has to be
  tried at a spot that reproduces. The other way is to keep player pawns off the coarse layer (its own object channel, which
  human-driven capsules ignore and AI keeps blocking), which needs an audit of every object-type terrain trace.
