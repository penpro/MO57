# MO57 Project Status

*Last updated: June 11, 2026 (full re-audit). Single source of truth for metrics, progress, and tracked issues.*

---

## Codebase Metrics

| Metric | Value (Jun 2026) | Notes |
|--------|-------|-------|
| Plugin C++ files | 511 (505 MOFramework + 6 MOFrameworkEditor) | Editor module split exists; runtime monolith remains (C1) |
| Plugin LOC | ~156,500 | Up from ~114K at May 17 audit (+25.4K insertions since) |
| Subsystems | 25 | +10 since May: Audio, GameClock, WeatherIntegration, VoxelReadiness, TerrainModification, Cheat, UIDebug, HarvestDebug, ResourceDepletion, UITest |
| Components | 29+ | 18+ on AMOCharacter alone |
| UI Widgets | ~69 widget classes | Menus now CommonUI layer-stack + pooled (L5 fixed) |
| BT Tasks/Services | 14 (11 tasks, 3 services) | |
| DataTable Row types | 15+ | |
| Test files | 5 (~5,000 LOC) + UI test subsystem | ~4% coverage; medical-heavy |
| UENUMs | 58+ | All properly marked BlueprintType |
| Interfaces | 9+ | Good decoupling |

### Art Debt Baseline (pipeline A1 — `MO.Test.ValidateArt`, 2026-07-04)

The burn-down number for the A-track. Re-run `python Tools/ue.py pie begin`
+ `ue.py run "MO.Test.ValidateArt"` and compare against this table; the
`[MOTEST] ART` log lines are the per-slot offender list.

| Category | Slots | Missing | Placeholder | Notes |
|----------|-------|---------|-------------|-------|
| Recipes (icon; +preview mesh & actor class for buildings) | 144 | 135 | 0 | 122 recipes, 11 buildings |
| Items (small+large icon, world visual) | 642 | 600 | 0 | 214 items |
| Skills (icon) | 22 | 22 | 0 | 22 skills |
| **Total** | **808** | **757** | **0** | 51 slots have real art |

Zero placeholders = art slots are simply unset, not stubbed. A5 graybox
generation flips building-mesh MISSING → PLACEHOLDER (`Graybox` paths are
classified as placeholder by design). `MO.Test.RunAll` carries the totals as
an informational `Data:Art` line without failing the standing gate.

---

## Master Plan Progress

### Foundation (COMPLETE)

| Stage | Name | Status |
|-------|------|--------|
| 0 | CLAUDE.md + Python Plugin Setup | COMPLETE |
| 1 | Generic C++ Widget Base Classes (7 classes) | COMPLETE |
| 2 | Python WBP Batch Generator | COMPLETE |
| 3A | CommonUI Layer Stack + Validation Tests | COMPLETE |

### UI Refactor Track (NOT STARTED)

| Stage | Name | Key Work |
|-------|------|----------|
| 4A | Scroll List + Detail Panel Migration | Reparent to generic bases |
| 5A | Context Menu + Progress Widget Migration | UCommonUserWidget for ephemeral popups |
| 6A | Inventory + Crafting UI to Layer Push/Pop | Replace AddToViewport with layer stacks |
| 7A | System Menus + Legacy Delegate Cleanup | Remove OnLegacyRequestClose |

### Colony Feature Track (NOT STARTED)

| Stage | Name | Key Work |
|-------|------|----------|
| 3B | Character Data + History Log | UMOCharacterHistoryComponent, UMOColonyManagerSubsystem |
| 4B | Colony Bar (Persistent HUD) | UMOColonyBarWidget on HUD layer |
| 5B | Colony Overview Screen | Replaces/extends possession menu |
| 6B | Task Assignment Without Possession | Highest risk stage |
| 7B | Alert Tier System | 4-tier alerts (Critical/Urgent/Notable/Log) |

### Integration (NOT STARTED)

| Stage | Name |
|-------|------|
| 8 | Possession Transition Experience |
| 9 | Relationship Simulation |
| 10 | Full Integration Test |

---

## Separate Feature Tracks

### Creature AI (MOSTLY COMPLETE)

C++ infrastructure done. Blueprint setup pending.

| Done | Pending |
|------|---------|
| AMOCreature, AMOPreyCreature, AMOPredatorCreature | Deer ABP state machine |
| AMOCreatureController with perception | Blackboard keys for activity states |
| 11 BT tasks, 3 BT services | Animation montages for hit/death |
| Wolf predator with full animation set | BT_Prey rest/sleep branches |
| Carcass system and loot tables | |
| Foraging AI with actual item navigation | |

### EQS (C++ COMPLETE, BLUEPRINTS PENDING)

| C++ Class | Blueprint Asset Needed |
|-----------|----------------------|
| EnvQueryGenerator_HarvestableItems | EQ_FindHarvestableItems |
| EnvQueryGenerator_HarvestTargets | EQ_FindHarvestTargets |
| EnvQueryTest_EscapeRoute | EQ_FindEscapeRoute |
| EnvQueryContext_Threat | (used by above) |

### Crafting System Phases

| Phase | Status |
|-------|--------|
| 1: Core Types & Discovery | DONE |
| 2: Crafting Queue | DONE |
| 3: Crafting Stations | NOT STARTED |
| 4: Crafting UI | DONE |
| 5: Recipe Discovery Methods | PARTIAL |
| 6: Polish | NOT STARTED |

### PCG World Items (PLANNED)

See `PCG_Integration_Plan.md`. Hybrid ISM + Actor approach.

---

## Audit Issue Tracker

*Full audit May 17, 2026 (6 agents, 467 files). **Re-audit June 11, 2026** (15 agents + adversarial verification, 511 files): all 27 issues re-verified against master with file:line evidence; statuses below are current.*

### CRITICAL — Architectural

| Code | Issue | Location | Status (Jun 11) |
|------|-------|----------|--------|
| C1 | **Monolithic runtime module** — all ~60 runtime systems in one MOFramework module. | `MOFramework.Build.cs` | **PARTIAL** — MOFrameworkEditor module split exists now (6 files), but runtime monolith grew 467→505 files and Build.cs:116-128 still pulls editor deps under bBuildEditor. No Core/Inventory/Medical/UI split. |
| C2 | **MOPersistenceSubsystem god-class** — single orchestration point for every save domain; each new domain edits this class (violates extension-without-modification). | `MOPersistenceSubsystem.cpp` | **OPEN, GREW** — 2415→2550 LOC, ~14 responsibilities (added since May: voxel sculpt, quest, weather/clock, terrain zones, spectator cam). Mitigating: new domains are thin ~20-35-line delegators to their subsystems — right shape, wrong home. |
| C3 | **GameInstance subsystem binding to UWorld** — manually tracks/re-binds per-world objects across level transitions. | `MOPersistenceSubsystem.h:349-353` | **OPEN** — unchanged (members moved to ~349 only because header grew). Header's own pitfall #3 documents the hazard. |

### HIGH — Performance / Correctness

| Code | Issue | Location | Status (Jun 11) |
|------|-------|----------|--------|
| H1 | LoadSynchronous in AI tick paths. | `MOAIController.cpp:32-35` | **FIXED, verified** — cache intact; no Tick override exists; all other BT loads are one-shot (BeginPlay/OnPossess). |
| H2 | Combat component ticked every frame. | `MOCombatComponent.cpp:30` | **FIXED, verified** — 10Hz throttle intact, no other writer touches the interval. |
| H3 | UI icons loaded synchronously. | various widgets | **ACCEPTED, re-confirmed** — every NativeTick in the module enumerated; zero per-frame loads; all sites are event-driven data-bind paths. No new violations since May. |
| H4 | O(n) inventory GUID lookup. | `MOInventoryComponent.cpp:532-536` | **FIXED, verified** — index rebuilt on ALL 8 mutation paths incl. cross-inventory transfer + client replication callbacks; lookup self-validates with linear fallback (missed rebuild degrades to O(N), never wrong-item). |
| H5 | Creature blackboard updated every frame. | `MOCreatureController.cpp:36-45` | **FIXED, verified** — 5Hz accumulator intact + IsValid gate skips idle creatures. |
| H6 | GetAllActorsOfClass in FindPackMembers. | `MOPredatorCreature.cpp:115` | **OPEN, dormant** — zero C++ callers now; AlertPack/GetPackSize/IsInPack are BP-exposed only. Hazard: GetPackSize is BlueprintPure — a future BT decorator binding re-hot-paths it silently. |
| **H7** | **NEW (Jun 11): Use-after-free in armed combat** — `GetCurrentAttackProfile()` returns pointer into stack-local row copy's `AttackProfiles` TArray; freed at return, dereferenced at 4 sites (StartAttack:159, CanAttack:229, :740, ProcessWindUp:812-817 @10Hz). Unarmed path returns safe member pointer, masking the bug in fist-only testing. | `MOCombatComponent.cpp:941-951`, `MOWeaponTypes.h:430-439` | **OPEN — verified by hand. Task #124.** Fix: return stable FindRow pointer or cache row at weapon equip (also removes per-tick sync load during wind-up). |

### MEDIUM — Design Debt

| Code | Issue | Location | Status (Jun 11) |
|------|-------|----------|--------|
| M1 | Hardcoded blackboard key strings. | `MOPredatorCreature.cpp:62-64` | **OPEN** — unchanged. |
| M2 | Magic numbers in creature logic (0.15f bare literal; 2× independent 500.0f). | `MOPreyCreature.cpp:66`, `MOSurvivorController.h:328`, `BTTask_SurvivorGather.h:75` | **OPEN** — unchanged. |
| M3 | EQS escape-route test authored but IsCornered() still uses health+range placeholder. | `MOPreyCreature.cpp:55-66` | **OPEN** — TODO comment verbatim unchanged. |
| M4 | Concrete subsystem coupling (Harvest→Crafting, Quest→Crafting, Inventory→Persistence). | `MOHarvestSubsystem.cpp:275,349,443`; `MOQuestSubsystem.cpp:51-60`; `MOInventoryComponent.cpp:1374,1453` | **OPEN** — no interface layer introduced. |
| M5 | Mixed authority idioms — 4 medical components use GetOwnerRole(), MOAdrenalineComponent uses HasAuthority() (7 sites). | medical components | **OPEN** — the newer component extended the split. |
| M6 | Two activity taxonomies (EMOActivityLevel vs EMOCreatureActivityState). | `MOActivityTypes.h:115`, `MOCreatureTypes.h:56` | **OPEN** — unchanged. |
| M7 | Raw UButton in list-entry widgets (5: BuildingEntry, BuildingQueueEntry, CraftingQueueEntry, InventorySlot, SkillEntry). | entry widget headers | **OPEN** — same 5; non-activatable entry rows the menu overhaul never touched. |
| M8 | ECommonInputMode::All leaked WASD during menus. | (UMOMenuWidgetBase deleted) | **FIXED** (May-June menu rework) — class gone; all menu/modal bases return Menu mode; zero ::All constructions remain. Residual: ~10 stale comments still claim "All" (Principle 3 cleanup). |
| M9 | IsAnyMenuOpen() checked one layer. | `MOGameUIManagerSubsystem.cpp:574-587` | **FIXED** — counts Game+GameOverlay+Menu+Modal, excludes input stubs; all wrapper paths delegate to the same count. |
| M10 | No dedicated-server UI guards. | UI entry points | **FIXED** — 3 guard depths: IsRunningDedicatedServer in Push templates, IsLocalController root-layout gate, IsLocalOwningPlayerController across all controllers (documented as convention in MOUIControllerBase.h:56). |
| M11 | Persistence↔Inventory circular dep (DropItemByGuid + SpawnWorldItem both touch destroyed-GUID ledger). | `MOInventoryComponent.cpp:1374-1378, 1453-1457` | **OPEN** — no IMOPersistenceProvider interface exists. |

### LOW — Housekeeping

| Code | Issue | Location | Status (Jun 11) |
|------|-------|----------|--------|
| L1 | Unguarded on-screen debug messages. | `MOCharacter.cpp:808,1866` etc. | **FIXED** — all 4 AddOnScreenDebugMessage sites guarded; DrawDebug behind ENABLE_DRAW_DEBUG. Nit: combat debug draws fire unconditionally in dev builds, no CVar. |
| L2 | GetFirstPlayerController multi-player hazards. | 8 sites | **OPEN, GREW 3→8** — 2 gameplay-affecting (PCG culling + spawn manager anchor to host only in co-op = the real hazard), 4 dev-tools (acceptable), 2 new documented UI fallbacks inheriting the pattern. |
| L3 | Documented deprecations with no compiler enforcement. | 16 files, ~18 distinct APIs | **OPEN, improved 28→18** — only 1 uses UE_DEPRECATED; 8 of 18 are the L4 family. |
| L4 | OnLegacyRequestClose delegates. | 8 widget headers | **OPEN, now mechanical** — zero C++ subscribers remain after menu migration; deletion needs only a WBP-binding check (they're BlueprintAssignable), then remove 8 declarations + 8 broadcasts. |
| L5 | Widget re-creation GC pressure. | layer stacks | **FIXED** — menus now acquired via CommonUI FUserWidgetPool (engine-pooled); controller context menus cached behind weak ptrs. Per-open CreateWidget only for transient right-click popups (negligible). RemoveAll-before-bind convention is load-bearing for pooled reuse. |
| L6 | Click-outside-to-dismiss unspecified. | `MOActivatableWidget.h:109`, `MOContextMenuBase.cpp:35-107` | **FIXED** — implemented twice: subsystem focus-watcher for stack menus + Slate IInputProcessor for context menus; default ON (Principle 7), verified opt-outs for confirmations/modals/HUD. Nit: [DIAG] Warning log on every outside-click decision should drop to Verbose. |
| L7 | ConfirmationDialog → CommonUI modal layer. | `MOConfirmationBase.h:61` | **FIXED** — UMOConfirmationBase : UMOActivatableWidget pushed to Layer_Modal; old class is a documented shim; UMOTextInputDialog reuses the base (extension without modification, Principle 9). Vestiges: unused ConfirmationDialogZOrder property, stale "consider migrating" comment. |

### June 11, 2026 Re-Audit — New Findings

*Delta audit of +25.4K LOC since May 17 (8 subsystem clusters) + 4 never-run dimension sweeps (server-authority #63, realism-pillar/Principle 11, save/load integrity matrix, data validation #65). All 12 units complete. Every Critical/High below passed adversarial verification (40 workflow verdicts, 0 refuted) or independent hand-verification (grep/read of the cited code). Totals: 9 new criticals (C4–C12), 48 new highs (H8–H55), 13 medium groups (M12–M24, ~75 items), 9 low groups (L8–L16, ~60 items). H7 (combat use-after-free) was found by this audit and lives in the main HIGH table above (task #124).*

#### New CRITICAL (C4–C9)

| Code | Issue | Location | Fix direction |
|------|-------|----------|---------------|
| C4 | **Audio async-load use-after-free** — FStreamableManager continuations capture raw `this` (GameInstance subsystem) and the returned FStreamableHandle is discarded, so loads in flight at PIE-end/exit fire into a destroyed subsystem and nothing can cancel them. | `MOAudioSubsystem.cpp:689` (+342, 369, 887, 938, 1200) | CreateWeakLambda + store handles per request; CancelHandle in Deinitialize. |
| C5 | **Mod recipe overlay is GC-invisible** — mod rows (containing hard `TSubclassOf<AMOBuildableActor>`) copied into a plain static TMap that is neither UPROPERTY nor FGCObject; GC collects the Blueprint class → dangling UClass* crash on next build-menu open. | `MORecipeDatabaseSettings.cpp:315` (map at `.h:175`) | Root merged tables via `TStrongObjectPtr<UDataTable>` cleared by ClearModRecipes, or FGCObject::AddReferencedObjects. |
| C6 | **TickAnatomy reads `Wounds[i]` after ProcessWound may remove it** — loop re-indexes the array after a callee that can `RemoveWound` on heal completion → out-of-bounds read the tick a wound finishes healing. | `MOAnatomyComponent.cpp:781` | Accumulate BleedRate from the reference *before* possible removal; have ProcessWound signal completion instead of removing in place. |
| C7 | **Client spawn-anything RPCs** — `ServerSpawnActorNearController` / `ServerSpawnAndPossessPawn` forward a client-supplied `TSubclassOf<AActor>` to SpawnActor with no class whitelist, distance clamp, or rate limit. | `MOPossessionSubsystem.cpp:164`, `MOPossessionComponent.h:91-95` | Config whitelist + distance/rate validation at the subsystem layer, or move behind UCheatManager for shipping. |
| C8 | **ISM/HISM harvest RPCs unvalidated** — server accepts any component + instance index with no distance, LOS, or rate check; a client can harvest the entire world remotely. The parallel actor-interaction path (`ServerExecuteInteract`) implements all of these checks. | `MOInteractorComponent.cpp:485-546` | Mirror ServerExecuteInteract's validation against InstanceTransform location. |
| C9 | **New Game never resets GameInstance persistence state** — LoadedWorldSave, CurrentSlotName, SessionDestroyedGuids, SessionPlayTimeSeconds survive "play save A → main menu → New Game": cross-save contamination, and a careless save overwrites slot A. Same family: quest/tutorial state also survives (H33). | `MOGameMode.cpp:139`, `MOPersistenceSubsystem.cpp` | `ResetForNewWorld()` protocol on the persistence subsystem; new-game flow calls it + `ResetAllQuests()`. |

#### New HIGH (H8–H41)

**Audio lifecycle (subsystem added 2026-05-25):**

| Code | Issue | Location |
|------|-------|----------|
| H8 | Stream-lifecycle races: ambient continuations guarded by a single re-armable bool (false→true within one SetAmbientState call), music continuations have **no** staleness guard, StartMusic overwrites MusicComponent without stopping it, and delayed-restart timer handles are uncancelable locals → stale async loads orphan immortal looping components / two music tracks at once. | `MOAudioSubsystem.cpp:806, 897, 1002, 369, 739` |
| H9 | Saved user volumes never applied at boot — ApplyAudioSettings left the per-category push as commented-out example code; every session runs on developer defaults until the Options panel is opened. | `MOGameSettings.cpp:77-106` |
| H10 | Base-layer "breathe" drift computes its target from config BaseLayerVolume alone, erasing authored per-layer Volume × per-asset VolumeMultiplier within seconds of ambient start. | `MOAudioSubsystem.cpp:1347` |

**Medical simulation:**

| Code | Issue | Location |
|------|-------|----------|
| H11 | Blood-loss death threshold 100× off — compares the 0–100 percent API against `0.20f`, so "die at <20% blood" actually fires at 0.2%; bleeding out to death is effectively unreachable via this path. | `MOVitalsComponent.cpp:93` |
| H12 | Entire weather→body exposure pipeline unwired — `ApplyEnvironmentalTemperature`, `GetColdStress`, `GetHeatStress`, `GetFeelsLikeTemperature` have **zero callers** (verified incl. binary grep of all .uassets). Cold/heat/wind/wet affect nothing physiological. | `MOVitalsComponent.cpp:303` |
| H13 | Exercise heat is a one-way ratchet — movement writes BodyTemperature up toward 40°C; the only dissipation code lives in the never-called ApplyEnvironmentalTemperature → permanent fever + stuck Hot moodle. | `MOCharacter.cpp:1020` |
| H14 | Condition progression use-after-realloc — ProcessCondition holds `FMOCondition&` into the array that nested AddCondition (Infection→Sepsis) can reallocate; heap write via MarkItemDirty on the stale reference. | `MOAnatomyComponent.cpp:924` |
| H15 | Adrenaline is decorative — HR modifier, bleed reduction, stamina drain, accuracy penalty all computed but have zero consumers; `ApplyVitalsModifiers` is an empty function under a comment claiming the integration exists (Principle 3). | `MOAdrenalineComponent.cpp:581` |
| H16 | No clotting model — `HealFactor` is computed and discarded under a "bleed rate naturally decreases as wound closes" comment; every untreated wound bleeds at full rate until 100% healed, making any cut eventually lethal (once H11 is fixed). | `MOAnatomyComponent.cpp:865` |

**Server authority (sweep — task #63):**

| Code | Issue | Location |
|------|-------|----------|
| H17 | Terraforming pipeline fully client-side — zero authority checks, no RPC transport; voxel edits and RegisterModifiedZone persistence records happen on the calling machine only. | `MOTerraformingComponent.cpp:767` |
| H18 | Combat verbs (attack/block/parry/dodge) mutate replicated state with no authority guard and no client→server transport — combat is functionally listen-host-only. | `MOCombatComponent.cpp:142` |
| H19 | Building placement finalizes a locally spawned ghost into a permanent building entirely client-side — in MP the building exists only on the placing client and never persists. | `MOBuildingComponent.cpp:437` |
| H20 | Crafting queue's replicated FastArray + bIsCraftingActive mutated and ticked with no authority guards (sibling UMOSurvivorJobQueueComponent guards every mutator). | `MOCraftingQueueComponent.cpp:80` |
| H21 | Four parallel UI pickup reimplementations bypass the canonical `MOItemComponent::GiveToInteractorInventory` — they mint fresh FGuids (severing world-item ↔ inventory identity) and destroy/hide actors with non-replicated calls (Principle 2). | `MOInventorySlot.cpp:518` + 3 siblings |

**Realism pillar / simulation ownership (sweep — Principle 11):**

| Code | Issue | Location |
|------|-------|----------|
| H22 | Player foraging (Dig for Supplies / Search Nearby) is a zero-duration world action with free XP — UI click handlers invoke the foraging primitives directly, skipping the timed-action pattern. | `MOGroundContextMenu.cpp:100` |
| H23 | Harvest simulation clock owned by the UI — completion decided by widget wall-clock NativeTick; `CompleteHarvest` performs zero duration validation; the subsystem's own timer is dead code; and a single world-global FMOHarvestContext means concurrent harvests (co-op, player+pawn) cancel each other. | `MOProgressWidgetBase.cpp:62`, `MOHarvestSubsystem.h:415` |
| H24 | Pawn-delegated harvest replaces per-action simulation duration with a flat 4-second constant ("instant for survivors — skip timer"); never reads the resource definition's BaseActionTime. | `MOSurvivorController.cpp:890` |
| H25 | Crafting pause/resume silently destroys accumulated craft progress — StartCrafting unconditionally resets CurrentCraftStartTime and the tick overwrites the preserved Entry.Progress. | `MOCraftingQueueComponent.cpp:271` |
| H26 | Passive Game-mode overlays (tutorial hint, progress widgets) force-hide the mouse cursor when they activate while a menu is open — NativeOnActivated applies cursor policy without consulting the active-widget registry (its own deactivate path does). | `MOActivatableWidget.cpp:175` |

**Persistence & quests (save/load matrix sweep + cluster):**

| Code | Issue | Location |
|------|-------|----------|
| H27 | CheckQuestCompletion reads a dangling `FMOQuestState&` after `ActiveQuests.Remove(State.QuestId)` — three reads + two broadcasts through freed map memory. | `MOQuestSubsystem.cpp:649` |
| H28 | Character death never reaches persistence — `MarkPawnDeceased` has zero callers; dead pawns resurrect on load. Breaks the permadeath pillar. | `MOCharacter.cpp:1124` |
| H29 | Save erases unspawned pawns — capture Resets PersistedPawns and rebuilds only from live world actors, so records for pawns that failed to spawn (load explicitly anticipates "PAWN LOST") are destroyed by the next save. | `MOPersistenceSubsystem.cpp:936` |
| H30 | SpawnPawnFromRecord skips all 7 component restorations (vitals/anatomy/metabolism/mental/skills/equipment/recruitment) that the full-load path applies — possession-menu spawn full-heals + skill-wipes the character, and the next save overwrites the good record. | `MOPersistenceSubsystem.cpp:658` |
| H31 | TransferItem re-rolls tool durability from the definition — any chest round-trip repairs a tool to full. | `MOInventoryComponent.cpp:1512` |
| H32 | RecoverStuckSpawn's pawn-destroyed path clears the timer but never dismisses the loading screen or clears bIsLoadingIntoGameplay — player stranded on the loading screen forever. | `MOGameMode.cpp:1127` |
| H33 | Quest/tutorial state leaks across worlds — `ResetAllQuests` has zero callers (hand-verified); completing the tutorial in world A marks it complete in every later world this session. Pairs with C9. | `MOQuestSubsystem.cpp:347` |
| H34 | Game clock never saved — `FMOGameClockSaveData` referenced only by its own files (hand-verified); date/time/TimeScale/accumulators reset every load while the header claims "persisted with the save slot" (Principle 3). | `MOGameClockSubsystem.cpp:150` |
| H35 | Spawn manager never adopts persistence-restored creatures — SpawnedEntities is Transient and only its own spawn paths add to it, so restored creatures are invisible to population caps, FIFO despawn, and AI-freeze → compounding double-population each save/load cycle. | `MOSpawnManagerSubsystem.h:223` |
| H36 | Crafting queue grants up to **7 days of real-wall-clock "offline progress"** on every load — save/quit/reload skips crafting time entirely (Principle 11 violation at the persistence boundary). | `MOCraftingQueueComponent.cpp:575` |
| H37 | Resource node depletion has no save path — DepletionMap is Transient with no UMOWorldSaveGame field; every harvested node reads fresh after load. | `MOResourceDepletionSubsystem.h:126` |
| H38 | Combat weapon state never persisted **and** equip resets durability from the profile — `FMOCombatSaveData` orphaned (hand-verified); weapon wear cannot survive a reload or even a re-equip. Root fix: make the inventory item the single owner of durability. | `MOCombatComponent.cpp:587` |
| H39 | Survivor job queues lost on save/load — BuildSaveData/ApplySaveDataAuthority exist but the orchestrator never calls them and FMOPersistedPawnRecord has no field; every recruited pawn's job stack evaporates. | `MOSurvivorJobQueueComponent.cpp:398` |

**Data integrity (sweep — task #65):**

| Code | Issue | Location |
|------|-------|----------|
| H40 | Four broken content chains (all hand- or uasset-verified): (a) Tutorial_BuildCampfire targets nonexistent recipe id `Campfire01` — the build tutorial **can never complete** (live recipe is `BuildCampfire`); (b) knowledge grant typo `SnareSettin` → MakeSnare permanently undiscoverable; (c) harvest/MineRock XP awarded to phantom skills (Woodcutting/Mining/Knapping/Metalworking — none in DT_Skills); (d) six harvest actions gated on RequiredKnowledgeId values that nothing grants (item ids used as knowledge ids). | `Quests.csv:15`, `Items_JSON.json:11787`, `Resources_JSON.json:57, 112` |
| H41 | Recipe query caches permanently stale in editor — only GetBuildingRecipes has the WITH_EDITOR fresh-scan branch; station/category/craftable queries serve first-PIE-session data for the rest of the editor process after any reimport. | `MORecipeDatabaseSettings.cpp:112` |

#### New MEDIUM (M12–M21, grouped by system)

| Code | System | Issues (compact) |
|------|--------|------------------|
| M12 | Audio | Master volume applied twice (device × per-class = squared); DefaultWeatherVolume never read (plays at 1.0 not 0.5); volume buses mutate the shared USoundClass asset instead of sound-mix overrides; bAutoSwapDayNightAmbient gates the bootstrap, not the day/night subscription it documents; duration heuristics hard-stop ≥30s events at ~5.1s and restart ≥60s base layers at ~5s; one-shot API has no loop guard while the bank's waves are bulk-flagged bLooping; full pipeline (state machines, MB loads) runs on dedicated servers (M10-family); FMOAudioBankRow::Category never consumed → bus routing depends on manual per-asset SoundClass; state-change delegates swallowed on failed loads (observers desync); stinger states (Death/Discovery) are terminal in a loop-oriented machine; four conflicting default-volume sources across three layers. `MOAudioSubsystem.cpp` passim |
| M13 | Medical | Function-local `static TMap` deficiency debounce shared across all pawns + PIE sessions; OnCardiacArrest/OnRespiratoryFailure re-broadcast at 2Hz with no edge detection (and the HR<10 trigger is unreachable); wetness never decays while raining even fully sheltered; hemorrhage stage names off-by-one vs the ATLS classes the docs cite; dehydration cascade documented in CLAUDE.md unimplemented + UI estimate 3× off actual rate; death timer (destroyed lungs) not persisted and death conditions not re-evaluated on load — saving during the countdown cancels death; both-lungs death rule only exists in the no-DataTable fallback branch; replication notify gaps (no OnRep on plain structs, per-tick MarkItemDirty churn, digestion progress never marked dirty); three different time bases across the physiology stack diverge when TimeScale ≠ 1; Today calorie counters never reset. `MOMetabolismComponent.cpp:793`, `MOVitalsComponent.cpp:835, 1096`, `MOAnatomyComponent.cpp:953-976`, `MOMentalStateComponent.cpp:672` |
| M14 | Authority (component-level) | Equipment mutators unguarded + add-before-remove ordering can duplicate the displaced item; Skills XP/level mutators unguarded; Knowledge + RecipeDiscovery mutators and save-apply unguarded; ApplySaveDataAuthority convention violated by 9 implementors; TerrainModificationSubsystem skips its authority check on a false "no clean check exists" comment; cheat-command authority inconsistent (only GiveItem checks). `MOEquipmentComponent.cpp:136`, `MOSkillsComponent.cpp:45`, `MORecipeDiscoveryComponent.cpp:233`, `MOTerrainModificationSubsystem.cpp:223`, `MOCheatSubsystem.cpp:1264` |
| M15 | Realism QoL | Ghost "Add Materials" deposits the entire bill of materials in one frame (design doc mandates combined-duration timer); all bulk/cross-container inventory transfers instant — no timed-batch mechanism exists anywhere; crafting menu silently falls back to instant zero-duration crafting when no queue component; harvest never consumes tool durability though the header documents it (Principle 3); hydration loss flat-rate with a false "handled by vitals" comment. `MOGhostContextMenu.cpp:162`, `MOInventoryComponent.cpp:1528`, `MOCraftingMenu.cpp:248`, `MOHarvestSubsystem.h:46`, `MOMetabolismComponent.cpp:658` |
| M16 | HUD/moodles | Moodle tooltips can never display (strip sets HitTestInvisible, blocking hover for all children); thermal comfort has no hysteresis → moodle add/remove flicker at boundary temps; initial moodle state dropped if possession precedes HUD-root creation (push-only pipeline, no replay); GetStatPercent returns 0.0 for missing stat — indistinguishable from depleted; close paths orphan cached-but-not-activated panels on the layer stack; bStatusPanelVisible never written (dead rebind path). `MOStatusEffectStripWidget.cpp:121`, `MOVitalsComponent.cpp:543`, `MOCharacterUIController.cpp:230, 422, 949`, `MOSurvivalStatsComponent.cpp:200` |
| M17 | Mod/settings/test | GetRecipeDefinition/GetSkillDefinition return raw pointers into static TMaps invalidated by the next mod-merge rehash; MO.Mod.Load* ingestion copy-pasted 3× (task #114 will multiply it); UI test suite produces false confidence (tautological Escape/Tab tests, placeholders report PASS, focus probe is a stub); transient new-game flow flags persisted to GameUserSettings.ini via Config specifier; SetToDefaults resets audio 4-5× louder than field defaults; "thread-safe lazy init" header claim is false; BuildInfo PreBuildStep reflection hook fails silently and its documented fallback doesn't exist; random world seed limited to 15 bits by FMath::Rand(). `MORecipeDatabaseSettings.cpp:30`, `MOCheatSubsystem.cpp:1066`, `MOUITestSubsystem.cpp:600`, `MOGameSettings.h:148` / `.cpp:47, 210`, `MOFramework.Build.cs:36` |
| M18 | Persistence/quest (secondary) | Every inventory add fires the quest ItemPickup event (transfers, craft refunds, craft outputs advance pickup objectives); LoadQuestDefinitions failure paths never signal readiness — quest UI waits forever; AddItemByGuid silently overflows slot capacity (entries invisible to slot UI); weapon durability dual-tracked unsynced (combat copy vs inventory entry); quest + crafting-queue ApplySaveData unguarded and unsuffixed (convention); personality traits not persisted — colony characters reroll every load; **no version field in the save game or any component save struct** — old saves break silently on schema change; destroyed-GUID set grows unboundedly (runtime-spawned actors get random GUIDs forever); save-slot playtime frozen (AddSessionPlayTime zero callers, game clock not consulted); crafting progress on UtcNow wall clock bypasses the centralized game clock (SetTimeScale has no effect); synchronous save hitch — GPU readback + PNG compress + voxel sync + full actor scans + disk write all on the game thread. `MOInventoryComponent.cpp:182, 239`, `MOQuestSubsystem.cpp:420`, `MOCombatComponent.cpp:978`, `MOPersonalityComponent.h:157`, `MOWorldSaveGame.h:286`, `MOPersistenceSubsystem.cpp:363, 900`, `MOCraftingQueueComponent.cpp:569, 639` |
| M19 | Creatures/character | Creature blackboard never learns its threat was destroyed (valid→invalid transition never pushed, HasTarget stuck true); repossessing a creature duplicates perception bindings (AddDynamic without Remove) and orphans sense configs; MOCharacter::BeginPlay binds five dynamic delegates bare while its own header prescribes RemoveAll-first, and EndPlay unbinds one of five; foraging query iterates every actor + every ISM instance in the world per click; two parallel stamina systems (vitals CurrentStamina vs string-keyed SurvivalStats "Stamina"). `MOCreatureController.cpp:41, 107`, `MOCharacter.cpp:196, 1043`, `MOForagingSubsystem.cpp:103` |
| M20 | Clock/weather/build | Weather subsystem binds clock + UI delegates in Initialize without InitializeDependency (init-order fragile); UDS sync throttle wedges after backward time jumps (negative elapsed suppresses syncs); BuildProgress header promises server-only tick + replicated progress — neither implemented, algorithm doc obsolete; movement physiology writes another component's replicated vitals field directly from a timer started with no authority gate. `MOWeatherIntegrationSubsystem.cpp:19, 131`, `MOBuildProgressComponent.h:22`, `MOCharacter.cpp:1020` |
| M21 | Data (secondary) | All 16 medical treatments require items that do not exist (11 dangling ids — medical loop unplayable as authored); 22 of 63 body-part enum values have no BodyParts row (fingers/toes/kidneys on fabricated defaults); Items authoring pipeline drift — Items_JSON.json older than DT_Items.uasset, next import reverts two days of in-editor edits. `Treatments.csv:2`, `BodyParts.csv:1`, `Items_JSON.json:1` |

#### New LOW (L8–L13, grouped)

| Code | Theme | Issues (compact) |
|------|-------|------------------|
| L8 | Audio | Warning-level diagnostic spam (always-on 10s status dump, per-event bearing logs); one-shot paths LoadSynchronous on the game thread (H3-family, but audio ≫ icons); timer continuations capture raw `this` (safe only by lifetime ordering); event-group cooldown burned when the load fails (group starved); breathe timer's empty-array early-out never reschedules; attenuation comment promises air absorption + spread the code doesn't implement; bPersistAcrossLevelTransition=true contradicts the PreLoadMap hard-stop design; AudioIdLookup caches raw row pointers into DataTable row maps; MasterSoundClass is dead config; StopAllAudio mutates states without broadcasting. `MOAudioSubsystem.cpp` passim |
| L9 | Medical | Vitals broadcasts Anatomy's OnInstantDeath from outside the owning component; feels-like temperature has hard step discontinuities (up to 20° jumps) at wind-chill/wet-cold boundaries; consciousness-level docs contradict the threshold ordering; WetnessLevel neither saved nor replicated though its API doc claims save-restore uses it; EnterCombat appends duplicate threats + adrenaline State replicates to all clients unlike siblings; mid-digestion micronutrients dropped on save/load; flat 100%/day decay for all vitamins/minerals (game-y vs biological); fatigue fully clears in ~3 min of standing despite "very slow, mainly rest/sleep" comment; dead code + stale TODOs across medical tick paths. `MOVitalsComponent.cpp:103, 828`, `MOWeatherIntegrationSubsystem.cpp:410`, `MOAdrenalineComponent.cpp:100`, `MOMetabolismComponent.h:112` / `.cpp:689` |
| L10 | Authority/cheat/test | Stale comments describe the superseded widget-owned terraform timer; four queue widgets bind in NativeConstruct without RemoveAll (convention); cheat commands registered ECVF_Default not ECVF_Cheat; Error/Warning logs on routine interaction/equip flows; MergeMod*Table rejects derived row structs (exact-pointer struct compare); HarvestDebug log handle checked outside its own mutex (TOCTOU); file-casing mismatches tracked in git (`MOItemDataBaseSettings.cpp`, `MOworldSaveGame.h`); 22 test methods declared but undefined; UI test results written into Content/ (read-only when packaged) with unchecked save result; impossible `/Script/` DataTable fallback path; unused ConstructorHelpers include; test console logs to LogTemp; MOSpawnSettings diverges from settings-class conventions; UMOGameSettings::ApplySettings skips Super (engine resolution/scalability path disabled). various |
| L11 | HUD/widgets | Wind-direction localization keys swapped vs display text; ShowHintWithIcon silently ignores its Icon parameter; moodle Id literals duplicated between push and remove sites; moodle entry keeps stale icon/tooltip when an update clears fields; harvest cancellation triple-broadcasts across two parallel delegate families; CancelHarvest dereferences GetWorld() unguarded on the destruct path; routine lifecycle diagnostics at Warning; Skills/Status toggles share one debounce counter (same-frame presses of different keys swallowed); OnPawnChanged binds pawn delegates without a locality guard (M10-family). `MOWindDirectionWidget.cpp:94`, `MOToolHintWidget.cpp:65`, `MOStatusMoodleWidget.cpp:60`, `MOHarvestProgressWidget.cpp:105-112`, `MOCharacterUIController.cpp:122, 419, 974` |
| L12 | Persistence/inventory | StackAndOrganize + FillStacksFrom skip the HasAuthority early-out every sibling performs; RagdollFreezeTimerHandle never cleared in EndPlay; quest log panel binds three subsystem delegates without Remove-first; ClearQueue bypasses PauseCrafting (interrupt listener left registered); quest world-filter relies on && / \|\| precedence without parentheses; combat hit processing has no per-actor dedupe within a single swing; equipment ApplySaveData convention violations; new-game branch leaves PendingNewGameSlot persisted to config; per-actor string-matching diagnostics run on every save; voxel sculpt records matched by actor-name string instead of GUID identity; adrenaline save struct orphaned (4th orphan with combat/personality/job-queue); CraftTime ≤ 0 silently produces instant crafts (no floor/validation). `MOInventoryComponent.cpp:1567`, `MOCharacter.cpp:1192`, `MOQuestLogPanel.cpp:86`, `MOCraftingQueueComponent.cpp:607, 631`, `MOQuestSubsystem.cpp:45`, `MOCombatComponent.cpp:900`, `MOPersistenceSubsystem.cpp:1549, 2240` |
| L13 | Clock/config/data | Daytime window can't wrap midnight; clock + weather subsystems tick in editor (non-PIE) worlds; dead weather types (EMOWeatherEventType unused; FMOTimeOfDay contradictory season conventions); save-pattern naming drift (clock/weather/adrenaline Apply* unguarded or unsuffixed); config loads Skills + all 4 medical tables through stale /Game/Data redirector stubs; packaged-build fallback table paths designed but never set (dead mechanism); verbose spawn logging force-enabled in default config for all builds; Ambient spawn category enabled with empty SpawnableClasses; only 16 of 203 items have inventory icons (92% blank tiles); stray pre-move Recipes.csv + JSON sources living apart from their tables. `MOGameClockSubsystem.cpp:119` / `.h:150, 296`, `MOWeatherTypes.h:233`, `DefaultGame.ini:124-154`, `Items_JSON.json` |

#### CommonUI core cluster (C10, H42–H45, M22, L14) — hand-verified June 11

| Code | Issue | Location |
|------|-------|----------|
| **C10** | **Pooled modal dialogs recycle with live foreign delegate subscriptions** — PushModalWidget hands out pool-recycled instances; `UMOConfirmationBase::NativeDestruct` clears only button bindings, never OnConfirmed/OnCancelled/OnTextConfirmed (header even documents they "persist"); consumers AddDynamic per-open with no remove (`MOSaveSlotListPanel.cpp:284, 312`, `MOSystemMenuUIController.cpp:819-820`). Combined with H42 (Esc closes with no cancel event, leaving PendingActionSlotName armed), the verified chain ends with **a confirm on one panel's dialog deleting another panel's pending save slot** — and duplicate same-consumer bindings double-fire. Fix at the layer that owns it: clear result delegates in NativeOnDeactivated after broadcasting a definitive result; never at call sites (pooling is invisible to them). | `MOGameUIManagerSubsystem.cpp:361`, `MOConfirmationBase.cpp:34-47` |
| H42 | **Base preview handler swallows Tab/Escape ancestor-first with a bare DeactivateWidget** — found independently by both UI clusters, four victim classes (all bubble-phase handlers are unreachable dead code): (a) **main menu: Escape/Tab on the title screen deactivates the whole menu → soft-lock** (pawn-less level gets FInputModeGameOnly + hidden cursor; ShowMainMenu only sets visibility on the stack-released widget — hand-verified `MOMainMenuWidget.h:118` overrides only bubble phase); (b) confirmations close silently with no OnCancelled result (PendingActionSlotName left armed — feeds C10's wrong-save-delete chain); (c) in-game menu: Escape closes the whole menu instead of the open save/load/options panel (`NativeOnHandleBackAction` dead — bIsBackHandler=false); (d) Tab while typing in the rename dialog tunnels past the text box and destroys the input. Fix at the base: route preview through a virtual close-request hook subclasses override to emit results/panel-first behavior; exempt Tab when focus is in an editable text widget. | `MOActivatableWidget.cpp:45-65`, `MOConfirmationBase.cpp:55-61`, `MOMainMenuWidget.cpp:164-179`, `MOInGameMenu.cpp:157-168`, `MOModalWidget.cpp:16, 45-59` |
| H43 | `GetActiveMenuCount` counts Game-mode progress overlays as menus (excludes only the stub class), so closing a context menu mid-harvest skips the FInputModeGameOnly restore and parks all-user focus on a passive overlay for the whole timed action — degraded mouse-look until the stub reactivates. Classify "open menu" by GetDesiredInputConfig mode (Menu vs Game), not by stub-class exclusion (Principle 6). | `MOPrimaryGameLayout.cpp:238-263`, `MOGameUIManagerSubsystem.cpp:470-552` |
| H44 | Outside-click close branch deactivates synchronously inside Slate's OnFocusChanging broadcast, so the remaining widget's reclaimed focus is clobbered when the in-flight Mouse focus change finalizes — the exact mechanism the opt-out branch 20 lines up documents and defers around. Defer the survivor's focus reclaim to next tick, mirroring the opt-out branch. | `MOGameUIManagerSubsystem.cpp:121-122` vs `:97-119` |
| H45 | Context menus never claim keyboard focus at construct, so the advertised Escape/Tab close (header claims it; Principle 3) only works after the user clicks the menu — and is left to N spawn sites, of which only the ghost menu remembers (`MOBuildingUIController.cpp:350`). With viewport focus + GameAndUI, Escape can open the in-game menu on top of a fresh context menu. Claim focus in `UMOContextMenuBase::NativeConstruct`, delete the per-site calls (Principle 2). | `MOContextMenuBase.cpp:67-93` |
| M22 | Grouped: `RemoveWidgetFromLayer` success predicate vacuous (`!IsInViewport()` is true for every stack widget — reports success without removing; both C++ callers already ban it); `PushWidgetToLayerInstance` "already activated" early-return unreachable for stack widgets → double-trigger pushes a duplicate pooled instance; **modal background is a fully dead system** (HitTestInvisible, invisible brush, OnBackgroundClicked never broadcast) whose header documents the opposite while six controllers still orchestrate Show/Hide; `NotifyPlayerRemoved` reads `*FoundLayout` after `PlayerLayouts.Remove` (verified UAF read, `MOGameUIManagerSubsystem.cpp:213-225`); global focus hook not filtered by world/Slate-user (cross-instance menu closes in PIE multi-client); ignore-move/look push unguarded against the double-activation its sibling guards + fallback pop can land on a PC that never got the push; modal-background/reticle cleanup lacks the owning-player fallback declared CRITICAL 120 lines earlier; confirmations are not bIsModal so gameplay toggle keys stay live under "Delete save permanently?"; `RequestClose` is a fail-deadly no-op when unbound (death recap can't be dismissed by key); two divergent Game-mode input configs (stub: capture+lock; progress overlays: NoCapture+DoNotLock — OS cursor can escape mid-action); `UMOModalWidget` stomps the desired-focus-target its base just set. | `MOPrimaryGameLayout.cpp:124, 188`, `MOModalBackground.cpp:12-30`, `MOGameUIManagerSubsystem.cpp:69-77, 213-225`, `MOActivatableWidget.cpp:232-237, 424-437`, `MOConfirmationBase.cpp:10-15`, `MOModalWidget.cpp:27-31, 71`, `MOGameplayInputStub.cpp:146-151` vs `MOProgressWidgetBase.cpp:20-25` |
| L14 | Grouped: header claims Escape uses CommonUI back-action chain (code handles it in preview; project opts out of the chain); `bWasCursorVisible` captured, never read; `[Activatable-DIAG]`/`[OutsideClick]` Warning-level spam on every UI interaction + MOUI_LOG always pays Printf; `MOConfirmationBase` duplicates the base GetDesiredInputConfig verbatim; ConfigureWithText's clamp claim false before construction; deferred focus lambda calls `FSlateApplication::Get()` without IsInitialized guard. | `MOActivatableWidget.h:70-72`, `MOContextMenuBase.cpp:78`, `MOTextInputDialog.cpp:27-35`, `MOGameUIManagerSubsystem.cpp:108-116` |

#### World/spawn/terrain cluster (C11–C12, H46–H52, M23, L15) — hand-verified June 11

| Code | Issue | Location |
|------|-------|----------|
| **C11** | **Container mutated during range-for** — `UpdatePersistence` range-iterates SpawnedEntities by reference; quest-timeout path calls `ConvertToCorpse → DespawnEntity → SpawnedEntities.RemoveAt(i)` and returns into the live iterator (chain verified `:948→:967→:556→:490`). Fires the ranged-for check in Dev the first time an interacted survivor's quest expires; UB in Shipping. Fix: collect pawns-to-corpse during the loop, process after. | `MOSpawnManagerSubsystem.cpp:948, 967, 490` |
| **C12** | **Deposited build materials are not in the save payload** — `BuildSaveData` is literally `OutData = Progress;` while the `DepositedMaterials` ledger is a separate transient UPROPERTY (verified `h:448`); materials already removed from inventories exist only in transient state. Deposit 8/10 logs → save → load: logs gone, not refunded, construction can't start. Fix: add the map to FMOBuildProgress / the persisted building record. | `MOBuildProgressComponent.cpp:450-453`, `.h:448` |
| H46 | Freeze/wake asymmetric — freeze disables actor tick, CMC tick, and anim policy, but the production wake path is `Brain->RestartLogic()` **only** (verified `:1458`); the full-reverse `WakeSpawnedPawn` is reachable only from the debug ForceWakeAll. Every woken mob is a statue with a running brain (MoveTo never integrates velocity, no gravity). Fix: route the wake check through WakeSpawnedPawn; extend DumpFreezeState's anomaly predicate to both directions. | `MOSpawnManagerSubsystem.cpp:1458` vs `:1239-1258` |
| H47 | Wake check infers "frozen" from `Brain->IsRunning()` — conflates "stopped because frozen" with "stopped because dead," so walking up to harvest your kill restarts the BT on a ragdolled corpse (movement MOVE_None, collision off). Record `bFrozenByManager` on the spawn record and gate on it (Principle 6). | `MOSpawnManagerSubsystem.cpp:1449-1458` |
| H48 | Unconditional voxel-runtime lockout — GameMode pre-flight destroys the auto-created runtime on **every** BeginPlay, but only the two pending-flag paths recreate it: direct PIE into the gameplay level, legacy seed-0 saves, and the documented bAutoInitializeVoxelWithSeed=false option all land in a **void world** (and two log messages claim otherwise). Scope the lockout to the branches that recreate, or add an else that recreates with defaults. | `MOGameMode.cpp:108-130` vs `:139, :176, :217-242` |
| H49 | Completed buildings lose solid collision after save/load — load path is BeginPlay (applies IgnoreOnlyPawn for under-construction) → ApplySaveData whose Complete branch calls only `SetCompletedVisual()`; the BlockAll promotion lives only in the live OnConstructionCompleted event. Every loaded wall/door ignores pawns until rebuilt. Fix: shared `EnterCompletedState()` used by live completion and load. | `MOBuildableActor.cpp:495-540` vs `:424-433` |
| H50 | CompleteHarvest computes the depletion NodeKey **after** RemoveInstance invalidated the index — for destroy-actions the key reads whatever instance now occupies the slot, charging depletion to a neighboring node (or skipping it). The header's promised stored-transform re-validation ("if instance moves, harvest fails") was never implemented (Principle 3). Fix: key from the BeginHarvest-captured InstanceTransform; re-validate before removal. | `MOHarvestSubsystem.cpp:633-664`, `.h:39-41` |
| H51 | New-game name handshake broken — `SpawnInitialPawn` rolls a fresh name from its own duplicated 20/15-name pool instead of consuming `PendingSurvivorName` (written by the new-game panel, which derived the save slot from it). Slot "Alex_Smith-01" contains "Olivia Davis," and the stale pending name is later consumed by the first random wandering survivor. Fix: consume PendingSurvivorName in SpawnInitialPawn; delete the duplicate pool (Principle 2). | `MOGameMode.cpp:805-829` vs `MONewGamePanel.cpp:197-221` |
| H52 | Load-time landing/reground probes pick the **topmost** surface at X/Y — a save made inside a dug tunnel or cave re-snaps the pawn to the hill surface above it every 250ms (caves/mining are a project pillar). The `FMath::Max(..., 20000.0f)` floor also makes the carefully documented ±search-distance config dead. Fix: choose the blocking hit nearest SavedAnchorZ within the configured window. | `MOGameMode.cpp:625-648, 374-409` |
| M23 | Grouped: GameMode subscribes one-shot handlers to the explicitly multi-cycle OnVoxelReady and never unsubscribes (second polling cycle spawns a second initial pawn; "AddUObject is safe" comment is false); seed application ships whole-object-table TObjectIterator scans + runtime SetParameter on shared graph assets (dirties packages in PIE); terrain-mod burst sweep walks every ISM in the process per frame during bursts + one-at-a-time RemoveInstance; zone merge keeps old center+max radius (merged zones under-cover → grass respawns at worked-strip edges); dead legacy gather machinery contradicts header's tick flow + death-cancel empties the deposit ledger the comment claims another pipeline refunds; always-on harvest-debug file logger (bEnabled=true default, Printf paid before the check) on per-frame paths in all builds; HarvestHISMInstance/HarvestISMInstance ~110-line copy-paste twins already drifting; depletion config hardcoded in C++ with unlimited-by-default for unconfigured items (Principle 7); per-tick TActorIterator + per-tick Log lines in the new edge-snap path (H6-family in new code); Complete*/timer expiry not enforced at the authoritative layer — extends H23 to CompleteTerraform's documented "skip the timer" path. | `MOGameMode.cpp:277-332, 1615-1684`, `MOTerrainModificationSubsystem.cpp:84-107, 350-358, 439-453`, `MOBuildProgressComponent.cpp:484-650, 730-736`, `MOHarvestDebugSubsystem.h:54-61`, `MOPCGInteractionSubsystem.cpp:163-386`, `MOResourceDepletionSubsystem.cpp:19-34, 121-130`, `MOBuildingComponent.cpp:914+`, `MOTerraformingComponent.cpp:728-750` |
| L15 | Grouped: spawn-manager tick early-out skips wake/cleanup/persistence when no settings actor (force-spawned pawns frozen forever); "per-tick check" comment vs 5s cadence + wake latency silently coupled to SpawnCheckInterval; mid-file #includes; function-local static dedup sets accumulate cross-world; wall-clock `FDateTime::Now()` (local!) drives cooldowns/persistence/respawns (DST jump expires them); MakeNodeKey's class-name "disambiguator" is always the same HISM class; TerrainMod ApplySaveDataAuthority enforces nothing + dead local; GameMode duplicates the readiness subsystem's delay constant in a log; Build.cs private-reflection PreBuildStep; wake force-resets anim policy to engine default instead of restoring the cached pre-freeze value. | `MOSpawnManagerSubsystem.cpp:159-166, 835, 873-946, 1256-1292`, `MOResourceDepletionSubsystem.cpp:148-195, 174-178`, `MOTerrainModificationSubsystem.cpp:223-229`, `MOFramework.Build.cs:36-52` |

*Cross-cluster convergence: the world cluster independently re-found H37 (depletion not persisted) and extended H23 (Complete* trust) — both already tracked above. The CommonUI cluster independently re-found H26 (overlay cursor stomp) with the added detail that the deactivation path correctly checks the registry while activation doesn't, and that the GameOverlay stub then defers its cursor reset because `GetActiveMenuCount() > 0` (H43's conflation) — the two bugs interlock.*

#### Menus/panels cluster (H53–H55, M24, L16) — hand-verified June 11

*This cluster also confirmed two good things on master: the May-June input-restore work is genuinely fixed (no rogue SetInputMode calls; context-menu input restore centralized in ApplyContextMenuInputState/RestoreContextMenuInputState), and the no-pause policy is enforced at its single chokepoint. The pre-existing "Centralized Input Mode Management" plan (worktree-era) is obsolete — its target bugs no longer exist on master.*

| Code | Issue | Location |
|------|-------|----------|
| H53 | **Cancelling a build after the ghost menu was ever closed/reopened destroys all deposited materials** — refund bookkeeping (`DepositedMaterialsForRefund`) lives in the transient UI widget and `InitializeForGhost` empties it on every open (hand-verified `:86`); the menu auto-closes on mouse-leave (0.3s grace), so the common sequence deposit → mouse drifts → reopen → cancel refunds zero and destroys the building. The UI should display refund state, not own it — move the ledger to `UMOBuildProgressComponent` (which already tracks deposits) and have cancel query it. Pairs with C12 (the same ledger also isn't saved). | `MOGhostContextMenu.cpp:86, 222-258, 729-762` |
| H54 | **Key rebinding mutates the shared InputMappingContext asset** — `ApplyBinding` does `Context->UnmapKey/MapKey` directly on the project asset (hand-verified `:131-144`); the process-static defaults cache only captures true defaults if read before the first mutation, and `ApplyAllBindingsFromSettings` runs at PlayerController startup without priming it — so "Reset to defaults" restores the player's custom key, and the in-memory asset stays mutated across PIE sessions (saveable into the project). Also: dead `NewMapping` local; context re-added at hardcoded priority 0. Fix: keep the asset immutable (UEnhancedInputUserSettings or per-player applied contexts), or snapshot all defaults before any apply; preserve original priority. | `MOKeyBindingManager.cpp:10, 131-144` |
| H55 | **UI debug file logger always-on in every build** — `bEnabled = true` default with no shipping guard; every Log() takes a critical section + UTF-8 convert + synchronous Write+Flush on the game thread; hooks global Slate OnFocusChanging; MOUI_DUMP_STATE writes a multi-line all-layers dump per menu open; the MOUI_LOG macro pays FString::Printf before the enable check; a new timestamped file per session with no cleanup. Same family: `[*-DIAG]` Warning-level logs on every activation/deactivation/outside-click. Fix: default off (console-command opt-in), compile out in Shipping, move the enable check before formatting, demote DIAG to Verbose. | `MOUIDebugSubsystem.h:65`, `.cpp:144-155`, `MOActivatableWidget.cpp:57, 159, 268, 273` |
| M24 | Grouped: item context menu auto-closes at **half** the configured delay (NativeTick and a 0.05s looping timer both feed the same accumulator — effective 0.075s vs the 0.15s the property claims); RefreshSaveList's focus-restore fix is inert (gates on `IsActivated()` which never passes for switcher-embedded panels — the "click before Tab/Escape works" symptom it claims to fix is back); 99-slot exhaustion: "New Save" silently targets the existing `-99` slot (and the in-game host turns it into a misleading overwrite prompt) + the slot-name generator is copy-pasted in two widgets (Principle 2); new-game slot name staged by the panel is unconditionally stomped by `StartNewGame`'s `World_<timestamp>` fallback — the recognizable-save-name feature never works for the initial save (pairs with H51's name-pool half of the handshake); **"Loot All Nearby" is an instant zero-duration batch pickup** — no combined-duration timer, no interruptibility, client-side destroys (Principle 11 + M15/H21 family); in-game-menu toggle orchestration duplicated across UIManager and SystemMenu controller with diverging menu sets (neither closes skills/building first); PushWidgetToLayerInstance reuse-guard breakage independently re-found (see M22) with added hazard: the harvest-vs-carcass flows bind different handler sets to the shared progress-widget cache and only stay correct because reuse never actually happens — fixing the guard without unconditional rebinding routes a carcass cancel to the tree-harvest handler. | `MOItemContextMenu.cpp:63-89, 455-501`, `MOSaveSlotListPanel.cpp:90-105`, `MOSavePanel.cpp:80-86`, `MONewGamePanel.cpp:205-231`, `MOMainMenuPlayerController.cpp:327-337`, `MOInventoryUIController.cpp:465-532`, `MOUIManagerComponent.cpp:732-775` vs `MOSystemMenuUIController.cpp:120-171`, `MOCraftingUIController.cpp:633-636` vs `:936-937` |
| L16 | Grouped: MOSurvivorTaskMenu re-init overwrites the cached job queue before unbinding the old one (RemoveDynamic targets the wrong queue); seven close-handler comments attribute input restoration to NativeOnDeactivated, which is false for non-activatable context menus (actual path: NativeDestruct → RestoreContextMenuInputState — works today, but an edit trusting the comment strands input); stale doc claims (in-game menu "pauses game" contradicts the no-pause policy; intro "skip delay" never gates skipping); HideBuildWidget hides the modal background without the IsAnyMenuOpen gate every sibling uses + EndPlay RemoveFromParent on an activated stack widget; bStatusPanelVisible dead flag (also in M16 — convergent); RegisterCachedMenu appends a duplicate OnDeactivated lambda per reuse call (no idempotence) and three widget caches bypass the documented "SYSTEM RULE" registration; save-slot entries synchronously re-decode their PNG screenshot per entry per list rebuild (rename/delete re-pays full decode cost; "cleared when entry is reused" comment stale — entries are never reused). | `MOSurvivorTaskMenu.cpp:99-104`, `MOSystemMenuUIController.cpp:1018, 1073-1096`, `MOInGameMenu.h:39-40`, `MOIntroWidget.h:32-33`, `MOBuildingUIController.cpp:39-42, 376`, `MOUIControllerBase.h:248-275`, `MOSaveSlotEntry.cpp:117-150` |

#### Cross-cluster verdict

All 12 audit units complete. Independent convergence was high — the same roots were re-found from different directions five times (H37 depletion, H26/H43 cursor-vs-menu-count interlock, H42 preview handler, M22/M24 reuse guard, M16/L16 dead flag), which is strong evidence the sweep saturated. The dominant systemic patterns, in fix-priority order:

1. **Live-path side effects never replayed by restore/secondary paths** — collision promotion (H49), full unfreeze (H46), component restoration (H30), runtime recreation (H48), name handshake (H51): one shared "enter state X" routine per system, called by both paths, is the fix shape.
2. **State owned by the wrong layer** — refunds in a transient widget (H53), harvest clock in a widget (H23), depletion config in C++ (M23), dialog results on pooled widgets (C10): move state to the layer that owns the lifecycle.
3. **Orphaned wiring** — save APIs (H34, H37–H39), adrenaline (H15), exposure pipeline (H12), ResetAllQuests (H33): both ends built, nobody owns the coupling.
4. **Base-class defaults silently overriding subclass intent** (H42 family) and **convention-by-memory instead of convention-by-structure** (per-spawn-site focus claims H45, per-consumer dialog binding C10, RemoveAll-before-bind drift M19).

---

## UE5.7 Refactoring Priority

| # | Target | Status | Impact |
|---|--------|--------|--------|
| 1 | CommonUI Full Adoption | Stage 3A complete, 4A-7A pending | HIGH |
| 2 | EQS for AI Queries | C++ done, BP assets pending | HIGH |
| 3 | Gameplay Tags (enums → tags) | NOT STARTED | HIGH |
| 4 | Smart Objects (interaction) | NOT STARTED | HIGH |
| 5 | Data Registry | NOT STARTED | MEDIUM |
| 6 | Enhanced Input Triggers (hustle) | NOT STARTED | LOW |
| 7 | PCG Distance Culling | NOT STARTED | LOW |
| DEFERRED | StateTree, GAS | N/A | Current BT/medical fine |

---

## What's Excellent (Audit Highlights)

These areas are strong and should be preserved (re-confirmed June 11 unless noted):
- **CommonUI migration** — Follows Lyra patterns, proper layer stacks, GetDesiredInputConfig() throughout; May's M8/M9/M10/L5/L6/L7 all verified fixed
- **Perf fixes held** — All four May perf fixes (H1 AI loads, H2 combat tick, H4 inventory index, H5 blackboard throttle) adversarially re-verified intact, with the H4 index self-validating
- **Inventory/identity/persistence authority discipline** — HasAuthority gates on nearly every mutator, fail-loudly pattern, COND_OwnerOnly where right. *(June 11 correction to May's "multiplayer-ready" claim: combat, terraforming, building, and crafting-queue layers are NOT yet authority-gated — see H17-H20. The discipline exists; four newer systems didn't adopt it.)*
- **Model implementations to clone** — `MOItemComponent::GiveToInteractorInventory` (canonical authoritative pickup; H21 is about deleting its bypasses), `UMOTerraformingComponent`'s component-owned interruptible timer (the pattern harvest needs, H23), `UMOSurvivorJobQueueComponent` (guards every mutator — the pattern crafting queue needs, H20)
- **Modern UE5.7 patterns** — All TObjectPtr<>, GENERATED_BODY(), 58+ UENUMs, excellent forward declarations
- **Data-driven architecture** — DataTables + UDeveloperSettings throughout; the *content* now needs the #65 validation harness (H40/M21 are data bugs, not architecture bugs)
- **Interface decoupling** — 9+ interfaces, no circular include chains
- **Medical system architecture** — Clean cascade, best test coverage; the June sim-correctness findings (C6, H11-H16) are wiring/unit bugs inside a sound structure

---

## Recent Development History

| Date | Milestone |
|------|-----------|
| 2026-07-03 | **Co-op travel fixed (Pillar 1A first rung)** — `UMOTravelUtils::TravelToGameplayLevel` (net-mode-aware chokepoint: standalone→OpenLevel unchanged; hosting→`ServerTravel(...?listen)`) replaces raw OpenLevel in the new-game + load-save flows (host exit-to-menu deliberately stays OpenLevel = session end); `bUseSeamlessTravel` on AMOGameMode + `net.AllowPIESeamlessTravel=1` (engine refuses seamless in PIE without it and silently hard-travels, which drops the client into a reconnect that races world-gen and hangs in PendingNetDriver). mptest now: host stays `ListenServer PCs=2` post-travel and **the client follows into the generated world** (both worlds on MOPCGScattering). Remaining red assert = next chunk, precisely located: the pawn-spawn flow is single-player-shaped (one "initial pawn" on OnVoxelReady) — **no spawn path exists for a second player** (Pillar 1C join-spawn via HandleSeamlessTravelPlayer/PostLogin + per-player pawn records). |
| 2026-07-03 | **Charter Move 2: 2-client PIE harness live** — `UMOEditorTestHelper` (enumerate/target PIE worlds by net mode from python; ConfigurePIE player-count/listen-server, editor-only) + `Content/Python/test_multiplayer.py` (claude_seq) + `ue.py mptest`. **First-ever 2-player boot found 2 real MP bugs:** (1) CRASH — all 3 GameInstance console-command subsystems (Cheat/HarvestDebug/UIDebug) double-register global console names under multi-GI PIE → teardown frees aliased IConsoleObjects → EXCEPTION_ACCESS_VIOLATION in UnregisterConsoleCommands; fixed with a single-owner registration guard in all three, verified by clean rerun. (2) **CO-OP BLOCKER, evidence-backed** — pre-travel the session is `ListenServer PCs=2` (client connects fine at the menu), but StartNewGame does a plain local OpenLevel: post-travel world is `Standalone PCs=1` — the listen-server role and the connected client are DROPPED. Co-op cannot survive starting a game until the flow uses ServerTravel (charter Pillar 1A / Move 3 prerequisite). Harness keeps an expected-red assert ("host is STILL a listen server after new-game travel") as the precise gap marker. |
| 2026-07-03 | **Charter Move 1 complete** (1676e96, 35ce77c): Pillar-0 drive primitives — MO.Test.Input (IMOControllableInterface seam, #144 moot), MO.Test.ClickWidget (UMOCommonButton::SimulateClick guarded click + Slate pointer fallback; verified: scripted click switched a real menu tab), MO.AI.DumpBlackboard/SetKey (typed, readback); `ue.py auto` runs the dormant 91-test automation suite headless (~22 s, exit-coded) — first run exposed 12 stale tests (pre-campaign behavior), reconciled with per-test verdicts → **91/91 green**. Findings: player jump max height only 20 cm (BP JumpZ=200 vs C++ 700, tuning chip filed); background-throttled PIE ≈3 fps (sample transients per-frame in seq tests); several "passing" tests silently no-op under ROLE_None (hardening pass flagged). |
| 2026-07-02 | **Tools/ue.py unified CLI** (5ee3087): correlated bridge calls (begin/end markers + per-command log delta), MCP session caching + safe one-row-at-a-time DataTable authoring, `cycle` = close→build→relaunch→boot→RunAll as one command (139 s live-verified). **#155 medical chains** (d717d4b): reconciliation showed all 11 treatment-consumed items uncraftable → 12 paleolithic recipes authored via MCP, zero new items, in-game craft PASS. **Fable-5 charter** (d365e41): 9-pillar framework→game roadmap, 3 milestones, 4 gates. |
| 2026-07-01 | Fable-5 audit pass (3 agents over the fix-campaign's own churn). Fixed + committed (0b68eb0, 532ce4d): permadeath-on-load (dead pawns resurrected), in-game-load-failure permafreeze, loaded-building free-build, craft-completion dangling-ref (C6/H27 class), terraform co-op persistence skip + server validation, craft Count int-overflow dupe, destroyed-GUID suppression latch; M1 blackboard-key centralization (MOBlackboardKeys.h, 58 sites). 2 findings were in this campaign's OWN earlier fixes (#132/#158). Deferred → #162 (building state replication), #163 (MP cheat-hardening), #164 (memory-safety [M]s + save/load [M]s). |
| 2026-06-30 → 07-01 | MO.Test.* MP-authority console harness + file-I/O bridge (claude_bridge) revived & proven under UE5.8 = screenshot-free PIE test loop; fixed #159 (H21 pickup regression, runtime-verified), #158 (in-game load), #160/#161 (FindWidget + menu label swap); #132 crafting/terraform client→server transport |
| 2026-06-11 | Full re-audit (17 agents + 40 adversarial verifications, 156K LOC, 511 files, 12 units); 27 May issues re-verified (11 fixed, 7 incidentally); 9 new criticals C4–C12, 48 new highs H8–H55, ~135 M/L items; tasks #124–#143 filed |
| 2026-05-17 → 06-10 | Game clock, UDS/UDW weather bridge, exposure model, audio subsystem, HUD root + moodles, save/modal/focus overhaul, cheat + mod runtime, AI freeze pipeline, voxel docs + World Features architecture |
| 2026-05-17 | Full codebase audit (6 agents, 114K LOC, 467 files) |
| 2026-04-04 | PROJECT_STATUS + TECHNICAL_REFERENCE consolidated |
| 2026-03-31 | UI Architecture audit with 7 issues identified |
| 2026-03-28 | Master Plan consolidated, Stages 0-3A complete |
| 2026-03-18 | UE5.7 audit (16 agents), EQS + CommonUI C++ done |
| 2026-03-01 | Survivor command overhaul, wolf predators |
| 2026-02-18 | Creature AI system, character appearance |
| 2026-02-11 | UIManager split into 6 controllers |

---

## Document Map

| Document | Purpose | Status |
|----------|---------|--------|
| `PROJECT_STATUS.md` | This file — metrics, progress, issues | Active |
| `TECHNICAL_REFERENCE.md` | Architecture patterns, APIs, guidelines | Active |
| `MO57_Master_Plan.md` | Stage execution plans | Active |
| `UI_Overhaul_Architecture.md` | CommonUI migration details + 15 pitfalls | Active (reference during UI work) |
| `MobAIPlan.md` | Creature AI design | Active (mostly implemented) |
| `PCG_Integration_Plan.md` | PCG world items | Active (not started) |
| `devlog_*.md` | Historical records | Archive |
| `Archive/*` | 4 superseded docs | Archive |
