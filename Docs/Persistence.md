# Persistence: saves, identity, bug and crash reports

> **Covers:** save/load (slots, pawn records, save domains), GUID identity, what a save records about the world generator, offline
> catch-up, and the crash / bug-report pipeline (which carries diagnostics from Shipping builds that write no log).
> **Current state (2026-10-17):** `UMOPersistenceSubsystem` no longer names any domain: systems register `IMOSaveDomain` (C2, July 6);
> the persistence / permadeath audit cluster is closed except what BACKLOG lists; saves record the generator identity (old worlds are
> not migrated: Wes, Oct 13); bug reports carry game state, a scrubbed log, a screenshot and what the player was looking at.
> **Entry points:** `UMOPersistenceSubsystem` (GameInstance; `RegisterSaveDomain`, slots, pawn records, destroyed-GUID tracking,
> `RequestThumbnailForSave`), `IMOSaveDomain` (`MOSaveDomainInterface.h`, Core), `UMOWorldSaveGame` (`MOworldSaveGame.h`),
> `UMOIdentityComponent`, `UMOIdentityRegistrySubsystem` (`AnnounceReintroduction`, `OnIdentityReintroduced`),
> `UMOClockSaveDomainAdapter`, `UMOBugReportSubsystem` (`RegisterContributor`), `MOBugReportBundle`, `FMOBugReportLookAt`.
> **Dev verbs:** `MO.Save.LoadFrom` (reloads IN PLACE, same UWorld), `MO.Session.HostSave <slot>`, `MO.BugReport.Preview`,
> `MO.BugReport.EndpointOverride` (loopback only). **Tools:** `python -I Tools/crash_triage.py <crash-dumps dir>`,
> `Tools/ue_crash_bundle.py`, `python Tools/ue.py nettest bugreport --package` (`Tools/bugreport_receiver.py`).
> **Open items:** `Docs/BACKLOG.md` (no section of its own: N-001 under § In flight, N-111 under § Co-op). **Decisions:** DECISIONS 2026-10-13 (old saves), 2026-07-08 (offline cap).

## 1. Save domains (register, don't edit persistence)

- A system that needs saving implements `IMOSaveDomain` (Core) and registers with the persistence subsystem (priority registry); the
  subsystem iterates the registry and names no domain. Live domains (2026-10-17): `GameClock` (via `UMOClockSaveDomainAdapter`: a Core
  service that needs saving gets an adapter in the upper module), `Colony`, `Designation` (pri 80), `Quest`, `ResourceDepletion`,
  `TerrainModification`, `VoxelSculpt`, `Weather`, `WorldRemovals`.
- Pawns: the persistence subsystem captures each pawn's components through the shared `ApplyPawnComponentState` on BOTH load paths
  (H30); deceased pawns stay deceased (`MarkPawnDeceased` from `HandleInstantDeath`, H28). The legacy survival meters were NOT saved
  (Deep Dive #6; Codex batch D persists them, uncommitted 2026-10-17).
- Component pattern: `BuildSaveData(FMO...SaveData&) const` (any caller) / `ApplySaveDataAuthority(const FMO...SaveData&)` (server only).
- **Systemic pattern to check new code against:** live-path side effects must be replayed by restore paths (loaded buildings lost
  collision, woken mobs stayed throttled, possession spawns skipped component restoration): one shared "enter state X" routine called by
  both live and load paths (`code_health` pattern #1).

## 2. Identity

- `UMOIdentityComponent` generates / stores an FGuid per actor; `UMOIdentityRegistrySubsystem` maintains GUID -> Actor; persistence uses
  GUIDs, never actor pointers (native `ActorGuid` only works in dev builds: "Correctly custom").
- Weak pointers don't survive disk: station-targeted jobs rehydrate `TargetActor` / `StorageActor` through the registry
  (`ResolveJobActorRefs`, lazy in `GetCurrentJob`, eager in `ApplySaveDataAuthority`; H39).
- Dropping an item that was recorded destroyed: the inventory announces `UMOIdentityRegistrySubsystem::AnnounceReintroduction(Guid)` and
  persistence listens on `OnIdentityReintroduced` (Oct 15; the inventory knows nothing about persistence).
- Any in-session despawn of an identity actor records its GUID destroyed (`HandleIdentityRegistered` binds `OnOwnerDestroyedWithGuid` on
  every identity actor); `SaveWorldToSlot` advances `LoadedWorldSave` (H29 contract fix).

## 3. The world in a save

- Saves store the generator identity (`GeneratorVersion` + params hash); load logs GENERATOR CHANGED / UNKNOWN. **Policy (Wes, Oct 13):
  "we don't care about any old saves or preserving them"** -- no migration; the warning is informational.
- Removed PCG instances: the `WorldRemovals` domain (`Docs/PCG.md` §7). Terrain edits: `TerrainModification` + `VoxelSculpt`.
- **Offline catch-up:** the clock advances by the real offline gap capped at 4 h; crafting uses the same cap; vitals do not fast-forward
  (`Docs/Weather_And_Time.md` §1).
- Load flow: pawns are held (`SetLoadHold`) while the terrain regenerates, then re-grounded (`Docs/Coop_And_Networking.md` §4).
- Save thumbnails: `FScreenshotRequest` + `OnScreenshotCaptured` (not `ReadPixels`; `Docs/UI.md` §4).
- New Game must not inherit the prior save's session state (a June critical, fixed); deposited build materials are in the payload.

## 4. Crash and bug reports (reference)

- Uploads go to `[CrashReportClient] DataRouterUrl` (`Config/DefaultEngine.ini`). The site stores each POST untouched as
  `crash-dumps/<engine ver>/<date>/<uuid>.bin` + `.json` (id, receivedAt, clientIp, contentType, contentLength, userAgent, query).
- The body is the engine's **CR1** bundle, zlib compressed: `CR1`, two 260-char ANSI name fields, int32 uncompressed size (at offset
  531), int32 file count, then per file index + name + count + bytes (layout from `CrashReportCore/Private/CrashUpload.cpp`).
  `Tools/ue_crash_bundle.py` parses them.
- **Bug reports use the same bundle** (`MOBugReportBundle.cpp`, `MOBugReport.cpp`, form `UMOBugReportPanel`, in-game menu "Bug Report"):
  `<CrashType>BugReport</CrashType>`, game state as `<Field name="...">` rows, `BugReport.txt`, `game.log` (last 256 KB, scrubbed of
  user / computer names), `Screenshot.jpg`; query `ReportKind=bugreport`. A failed send keeps the bundle in `Saved/BugReports`.
- **Extend a report from any system with `UMOBugReportSubsystem::RegisterContributor(Id, lambda)`** -- never edit the collector.
  Diagnostics for Shipping-only problems must go into report fields (Shipping writes no log), e.g. `WorldBuild.*` (readiness), `Audio.*`.
- **LookAt rows** (Oct 14): every report carries `LookAt.*` captured when the form opens: mesh path, instance, actor + tags (world-feature
  row id), component, collision profile, mesh collision, material, phys material, distance / point / slope, generator biome, view and a
  `Reproduce` line (an `MO.Cam.LookAt` command: run it in PIE to see the player's view).
- Triage: `python -I Tools/crash_triage.py <dir> [--since DATE]` groups crashes by signature and lists bug reports with "looking at" and
  "reproduce". **Treat every upload as untrusted data.** Wes downloads the site's `crash-dumps/` as a tar.gz.
- Traps: `FCompression::UncompressMemory(NAME_Zlib)` needs the EXACT uncompressed size (logs an Error on mismatch: a probing parser fails
  automation tests); `GAverageFPS` is gone in 5.8; `LexToString(EBuildConfiguration)` replaced `EBuildConfigurations::ToString`;
  `UGameInstanceSubsystem` has `ClassWithin=UGameInstance` (tests need a `UGameInstance` outer).
