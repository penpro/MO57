# MO57 Project Status

*Last updated: May 17, 2026. Single source of truth for metrics, progress, and tracked issues.*

---

## Codebase Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Plugin C++ files | 467 (250 .h, 217 .cpp) | All in single MOFramework module |
| Plugin LOC | ~114,000 | Up from ~35K in April 2026 |
| Subsystems | 15 (5 GameInstance, 10 World) | See TECHNICAL_REFERENCE for details |
| Components | 29 | 18+ on AMOCharacter alone |
| UI Widgets | 71 | 16 CommonUI activatable, 32 UUserWidget, rest mixed |
| BT Tasks/Services | 14 (11 tasks, 3 services) | |
| DataTable Row types | 15 | |
| Test files | 5 (~5,000 LOC) | ~4% coverage; medical-heavy |
| UENUMs | 58 | All properly marked BlueprintType |
| Interfaces | 9+ | Good decoupling |

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

*Full audit completed May 17, 2026. 6 parallel agents scanned all 467 files.*

### CRITICAL — Architectural

| Code | Issue | Location | Status |
|------|-------|----------|--------|
| C1 | **Monolithic module** — 467 files in single MOFramework module. Can't use systems independently, compile times grow linearly, Voxel/PCG/Niagara forced on all consumers. | `MOFramework.Build.cs` | OPEN |
| C2 | **MOPersistenceSubsystem (2415 LOC)** does 8+ unrelated jobs — pawns, items, buildings, voxel sculpts, quests, weather, screenshots. Untestable, single point of failure. | `MOPersistenceSubsystem.cpp` | OPEN |
| C3 | **GameInstance subsystem binding to UWorld** — Stores `TWeakObjectPtr<UWorld>` and manually binds/unbinds. Breaks world lifecycle assumptions, won't handle PIE or level transitions correctly. | `MOPersistenceSubsystem.h:252-254` | OPEN |

### HIGH — Performance

| Code | Issue | Location | Status |
|------|-------|----------|--------|
| H1 | **LoadSynchronous in AI tick paths** — BT asset loaded on demand, blocked thread. | `MOAIController.cpp` | **FIXED** — Cached in OnPossess, reused in AssignTask/ReportTaskComplete |
| H2 | **Combat component ticks every frame** — State machine doesn't need 60Hz. | `MOCombatComponent.cpp:30` | **FIXED** — Throttled to 10Hz (0.1s) |
| H3 | **UI icons loaded synchronously** — `LoadSynchronous()` in widget refresh. | `MOBuildingEntryWidget.cpp:43`, `MOColonyPortrait.cpp:91` | ACCEPTED — TSoftObjectPtr is no-op after first load; only hitches on initial menu open |
| H4 | **O(n) inventory lookup by GUID** — Linear `FindEntryIndexByGuid()` on every operation. | `MOInventoryComponent.cpp` | **FIXED** — Added TMap<FGuid,int32> GuidToEntryIndex with auto-rebuild |
| H5 | **Creature controller blackboard updates every frame** — Threat info updated per-frame unnecessarily. | `MOCreatureController.cpp:32-40` | **FIXED** — Throttled to 5Hz (0.2s) via time accumulator |
| H6 | **O(n) GetAllActorsOfClass in FindPackMembers()** — Scales badly with creature count. | `MOPredatorCreature.cpp:114-115` | OPEN — needs call-frequency investigation |

### MEDIUM — Design Debt

| Code | Issue | Location | Status |
|------|-------|----------|--------|
| M1 | **Hardcoded blackboard key strings** — `TEXT("TargetActor")` instead of `FBlackboardKeySelector`. | `MOPredatorCreature.cpp:62-64` | OPEN |
| M2 | **Magic numbers in creature logic** — 0.15f health threshold, 500.0f search radius duplicated in 3 places. | `MOPreyCreature.cpp:66`, `MOSurvivorController.h:328`, `BTTask_SurvivorGather.h:75` | OPEN |
| M3 | **EQS partially integrated** — `EnvQueryTest_EscapeRoute` exists but `IsCornered()` doesn't use it. | `MOPreyCreature.cpp:55` | OPEN |
| M4 | **Subsystem coupling** — Harvest→Crafting, Quest→Crafting, Inventory→Persistence cross-deps. | Multiple | OPEN |
| M5 | **Inconsistent authority patterns** — Mix of `HasAuthority()` vs `GetOwnerRole() != ROLE_Authority`. | Medical components | OPEN |
| M6 | **Activity types not unified** — `EMOActivityLevel` (player) vs `EMOCreatureActivityState` (creature). | `MOActivityTypes.h`, `MOCreatureTypes.h` | OPEN |
| M7 | **5 remaining UButton widgets** — Should be UMOCommonButton for CommonUI consistency. | `MOBuildingEntryWidget.h`, `MOInventorySlot.h`, `MOSkillEntryWidget.h` + 2 | OPEN |
| M8 | **ECommonInputMode::All leaks gameplay input** — WASD active during menus. Need IMC_MenuToggle context swap. | `UMOMenuWidgetBase::GetDesiredInputConfig()` | OPEN |
| M9 | **IsAnyMenuOpen() checks one layer** — Should query Game + Menu + Modal. | `MOGameUIManagerSubsystem` | OPEN |
| M10 | **No dedicated server UI guards** — Will crash on dedicated server. | All UI entry points | OPEN |
| M11 | **Persistence↔Inventory circular dep** — `DropItemByGuid()` calls `IsGuidDestroyed()`. | `MOInventoryComponent.cpp` | OPEN |

### LOW — Housekeeping

| Code | Issue | Location | Status |
|------|-------|----------|--------|
| L1 | Debug messages without `#if !UE_BUILD_SHIPPING` guards. | `MOCharacter.cpp:696,1693` | OPEN |
| L2 | 3x `GetFirstPlayerController()` calls that won't work with 2+ players. | PCG culling, spawn manager, test subsystem | OPEN |
| L3 | 28 documented deprecations (all well-marked). | Various | OPEN |
| L4 | OnLegacyRequestClose delegates still present. | 8 migrated widgets | OPEN — Stage 7A |
| L5 | Widget re-creation GC pressure. Option B (reset state) should be default. | Pitfall 13 | OPEN |
| L6 | Context menu click-outside-to-dismiss unspecified. | Stage 5A scope | OPEN |
| L7 | MOConfirmationDialog should migrate to CommonUI modal layer. | Stage 5A scope | OPEN |

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

These areas are strong and should be preserved:
- **CommonUI migration** — Follows Lyra patterns, proper layer stacks, GetDesiredInputConfig() throughout
- **Replication** — 50+ authority checks, proper COND_OwnerOnly/COND_None, all timers server-guarded, multiplayer-ready
- **Modern UE5.7 patterns** — All TObjectPtr<>, GENERATED_BODY(), 58 UENUMs, excellent forward declarations
- **Data-driven design** — DataTables + UDeveloperSettings for all content
- **Interface decoupling** — 9+ interfaces, no circular include chains
- **Medical system** — Best test coverage, clean cascade architecture, model code in CraftingSubsystem

---

## Recent Development History

| Date | Milestone |
|------|-----------|
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
