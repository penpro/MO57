# MO57 Technical Reference

*Last updated: May 17, 2026. Architecture patterns, guidelines, and known constraints.*

---

## CommonUI Architecture

### Layer Stack

| Layer Tag | Z-Order | Purpose | Example Widgets |
|-----------|---------|---------|-----------------|
| `MO.UI.Layer.HUD` | 0 | Always-visible | Reticle, Colony Bar, Mode Indicator |
| `MO.UI.Layer.Game` | 50 | One-at-a-time gameplay menus | Inventory, Crafting, Building, Skills |
| `MO.UI.Layer.GameOverlay` | 100 | Overlays on gameplay | Context menus (non-stack), Progress bars |
| `MO.UI.Layer.Menu` | 150 | System menus | In-Game Menu, Possession Menu |
| `MO.UI.Layer.Modal` | 200 | Modal dialogs | Confirmation dialogs |

**CRITICAL**: Colony bar MUST be HUD layer. Game layer only shows one widget at a time.

### Widget Hierarchy

```
UCommonActivatableWidget
    └── UMOActivatableWidget (base: bAutoRestoreFocus, GetDesiredInputConfig)
            ├── UMOMenuWidget (ECommonInputMode::All, bIsBackHandler)
            │       └── UMOMenuWidgetBase → all gameplay menus
            ├── UMOModalWidget (ECommonInputMode::Menu, bIsModal)
            │       └── UMOInGameMenu
            └── UMOConfirmationBase, UMOProgressWidgetBase, etc.

UCommonUserWidget
    ├── UMOPrimaryGameLayout (layer stack container)
    ├── UMOContextMenuBase (ephemeral popups, NativeOnKeyDown for Escape)
    │       └── Item, Ground, Ghost, Station, Survivor, KeepOnHarvest
    └── UMOStatusField

UUserWidget — 32 non-interactive content panels (entry widgets, grids, info panels)
```

### Input Flow

```
Key press → CommonUI ActionRouter → bIsBackHandler → NativeOnHandleBackAction()
  → RequestClose() → OnRequestClose delegate → UIController closes menu
  → Widget popped from stack → CommonUI restores input from next widget
  → Stack empty → WBP_GameplayStackStub provides gameplay input config
```

### UI Pitfalls (Quick Reference)

See `UI_Overhaul_Architecture.md` for full details and code examples.

1. `ECommonInputMode::All` leaks WASD — need IMC_MenuToggle context swap (M8)
2. `FUIInputConfig` members protected in UE5.7 — use constructors
3. `PushWidget` auto-activates in UE5.5+ — never call `ActivateWidget()` after push
4. Missing `RootContentWidgetClass` — causes input death after all menus close
5. `IsAnyMenuOpen()` must check ALL layers — Game + Menu + Modal (M9)
6. Widget re-creation GC pressure — use reset-state pattern for frequent menus (L5)
7. No dedicated server guards — check `IsRunningDedicatedServer()` (M10)

### Debug Commands

```
CommonUI.DumpActivatableTree    ; Dump widget activation tree
CommonUI.DebugInputRouter 1     ; Show input routing decisions
Slate.ShowFocusedWidget 1       ; Show focused widget
```

---

## UI Controller Architecture

Sibling components on AMOPlayerController. Find each other via `GetOwner()->FindComponentByClass<T>()` with weak pointer caching.

| Controller | Key Methods |
|------------|-------------|
| `UMOUIControllerBase` | Input modes, modal background, pawn caching |
| `UMOCharacterUIController` | ToggleSkillsPanel, TogglePlayerStatus, StartItemInspection |
| `UMOBuildingUIController` | ToggleBuildingMenu, ShowGhostContextMenu, ShowBuildWidget |
| `UMOCraftingUIController` | ToggleCraftingMenu, ShowStationContextMenu, StartHarvestOperation |
| `UMOSystemMenuUIController` | ToggleInGameMenu, TogglePossessionMenu, ShowConfirmationDialog |
| `UMOInventoryUIController` | ToggleInventoryMenu, OpenInventoryWithContainer, ShowItemContextMenu |
| `UMOQuestUIController` | ToggleQuestLog, UpdateQuestHUD |

---

## Networking & Replication

### Pattern Summary

- **7 Server RPCs** (all Reliable) — interaction, possession, spawning
- **50+ authority checks** — consistent throughout
- **COND_OwnerOnly** for private data (inventory, vitals, anatomy, skills, metabolism)
- **COND_None** for public state (adrenaline, recruitment, carcass progress)
- **FastArraySerializer** for replicated arrays (inventory, wounds, jobs, nutrients)

### Authority Check Convention

Standardize on `HasAuthority()` (preferred) over `GetOwnerRole() != ROLE_Authority` (M5).

```cpp
// Preferred pattern for state mutations:
if (!GetOwner()->HasAuthority()) return;
```

### Timer Safety

All gameplay timers must check authority before `SetTimer()`:
```cpp
if (GetOwnerRole() == ROLE_Authority)
{
    GetWorld()->GetTimerManager().SetTimer(TickTimerHandle, this, &ThisClass::TickFn, Rate, true);
}
```

All existing timers follow this pattern correctly. New timers must too.

### Multiplayer Caution Points

- 3x `GetFirstPlayerController()` calls (L2) — will use wrong player in co-op
- PCG distance culling uses first player only — verify per-player culling
- Spawn manager notifications may target wrong player

---

## Performance Guidelines

### Tick Rates

| Component | Rate | Notes |
|-----------|------|-------|
| VitalsComponent | 0.5s | Timer-based, authority-only |
| AnatomyComponent | 1.0s | Timer-based, authority-only |
| MetabolismComponent | 1.0s | Timer-based, authority-only |
| MentalStateComponent | 0.5s | Timer-based, authority-only |
| AdrenalineComponent | 0.1s | Timer-based, authority-only |
| CombatComponent | **Every frame** | **H2 — should throttle to 0.1-0.2s** |
| CreatureController | **Every frame** | **H5 — blackboard updates should throttle** |
| HISMCullingSubsystem | 10.0s | Periodic refresh |

### Asset Loading Rules

- **BeginPlay**: `LoadSynchronous()` is acceptable
- **Tick/Update paths**: NEVER use `LoadSynchronous()` — preload or async (H1, H3)
- **UI widget refresh**: Pre-cache icons or use `RequestAsyncLoad()` (H3)
- **AI activation**: Preload behavior trees in `OnPossess()`, not in tick (H1)

### Container Performance

- Inventory GUID lookup is O(n) linear scan (H4) — add `TMap<FGuid, int32>` index
- `GetAllActorsOfClass` in creature pack detection is O(n) (H6) — use spatial registry
- Widget pooling already implemented for inventory grid (good pattern — extend to other lists)

### Things Already Done Well

- Component lookups cached in BeginPlay (not re-queried per frame)
- No FString concatenation in hot paths
- No TMap with FString keys
- DataTable lookups in initialization, not tick
- Widget creation deferred (not per-frame)

---

## Creature AI Architecture

### Class Hierarchy

```
AMOCharacter → AMOCreature → AMOPreyCreature (deer: flee-first)
                           → AMOPredatorCreature (wolf: hunt, flee when wounded)
```

Single controller: `AMOCreatureController` with `UAIPerceptionComponent` (sight + hearing).

### Behavior Trees

- **BT_Prey**: Death → Flee (ShouldFlee AND HasTarget) → Threatened (fight if cornered) → Idle (wander/graze)
- **BT_Predator**: Death → Flee (wounded) → Combat (chase/attack) → Hunt (lost target) → Idle/Patrol

### AI Known Issues

- Hardcoded blackboard key strings (M1) — use `FBlackboardKeySelector`
- Magic numbers for thresholds (M2) — extract to creature definition or config
- `IsCornered()` is placeholder (M3) — should use `EnvQueryTest_EscapeRoute`
- `FindPackMembers()` uses `GetAllActorsOfClass` (H6) — needs spatial registry
- Activity types not unified between player/creature (M6)

### BT Tasks (11) and Services (3)

Tasks: CreatureAttack, CreatureRest, CreatureWander, FleeFromThreat, ChaseTarget, SurvivorForage, SurvivorGather, SurvivorGoHome + 3 more
Services: CreatureActivity, UpdateCreatureState, SurvivorJobProcessor

---

## Medical Cascade

```
Wounds (bleed) → Vitals (blood volume, HR, BP) → MentalState (consciousness)
                      ↓
    Heart/Lung damage → SpO2/BP → Death timers
                      ↓
Metabolism (glucose) → Vitals (blood glucose) → MentalState (confusion)
                      ↓
Dehydration → Vitals (+HR, -BP, +Temp) → Performance penalties
```

Components tick independently: Vitals 0.5s, Anatomy 1.0s, Metabolism 1.0s, MentalState 0.5s.

---

## Subsystem Architecture

### Inventory

| Subsystem | Type | LOC | Responsibility |
|-----------|------|-----|----------------|
| MOPersistenceSubsystem | GameInstance | 2415 | Save/load everything (C2 — too broad) |
| MOMedicalSubsystem | GameInstance | 600 | DataTable lookups for medical defs |
| MOQuestSubsystem | GameInstance | 850 | Quest state, objectives, progress |
| MOAppearanceSubsystem | GameInstance | 370 | Character appearance assets |
| MOCraftingSubsystem | World | 750 | Recipe validation and execution |
| MOHarvestSubsystem | World | 800 | ISM/HISM harvesting |
| MOForagingSubsystem | World | 450 | Ground item foraging |
| MOGameUIManagerSubsystem | World | 450 | CommonUI layer management |
| MOIdentityRegistrySubsystem | World | 400 | GUID↔Actor mapping |
| MOInteractionSubsystem | World | 300 | Server-side interaction validation |
| MOPossessionSubsystem | World | 300 | Pawn possession mechanics |
| MOSpawnManagerSubsystem | World | 1180 | Dynamic entity spawning |
| MOWeatherIntegrationSubsystem | World | 700 | Weather provider queries |
| MOPCGInteractionSubsystem | World | 450 | Mesh-to-item lookup cache |
| MOHISMCullingSubsystem | World | 200 | PCG HISM distance culling |

### Dependency Map

```
MOPersistenceSubsystem (GI) → MOIdentityRegistrySubsystem (W), MOQuestSubsystem (GI), MOWeatherIntegrationSubsystem (W)
MOQuestSubsystem (GI) → MOCraftingSubsystem (W) [binds OnCraftCompleted]
MOHarvestSubsystem (W) → MOCraftingSubsystem (W), MOPCGInteractionSubsystem (W)
MOForagingSubsystem (W) → MOPCGInteractionSubsystem (W)
MOInventoryComponent → MOPersistenceSubsystem (GI) [circular dep — M11]
```

---

## Crafting Validation Pattern

```cpp
CanCraftRecipe(RecipeId, Inventory, Skills, Knowledge)  // returns bool + reasons
  → ExecuteCraft(RecipeId, Inventory)                    // only after CanCraft passes
```

Tool system uses `EMOToolType` enum. Multi-tool items supported via effectiveness ratings.

---

## Key Subsystem APIs

### Job Assignment (UMOSurvivorJobQueueComponent)

```cpp
FGuid EnqueueJob(EMOSurvivorJobType, int32 RepeatCount = 1);
FGuid EnqueueJobAtLocation(EMOSurvivorJobType, FVector, int32 RepeatCount = 1);
FGuid EnqueueJobWithTarget(EMOSurvivorJobType, AActor*, int32 RepeatCount = 1);
bool CancelJob(const FGuid& JobId);
void CancelAllJobs();
```

### Colony Membership (UMORecruitmentComponent)

`EMORecruitmentState::Recruited` = colony member. `IsPossessable()` returns true for recruited pawns.

### Standard UI Delegates (MOUIDelegates.h)

Generic: FMOUIRequestClose, FMOUIActionTriggered, FMOUIProgressUpdate, FMOUIVisibilityChanged
Crafting: FMOUICraftRequest, FMOUIRecipeSelected
Inventory: FMOUISlotClicked, FMOUIDragStarted, FMOUIDropCompleted
Building: FMOUIBuildRequest

---

## Colony Management Design (Planned)

### Personality System (UMOPersonalityComponent)

Three axes, float -1.0 to 1.0:
- **Conscientiousness**: Diligent ↔ Adaptable
- **Sociability**: Social ↔ Reserved
- **Stability**: Stable ↔ Volatile

### Alert Tiers

| Tier | Name | Examples | Display |
|------|------|----------|---------|
| 1 | Critical | Health <15%, combat while away | Pulsing red, sound, cannot dismiss |
| 2 | Urgent | Health <40%, idle >30min | Persistent orange dot |
| 3 | Notable | Task complete, skill gained | Colony log only |
| 4 | Log | Routine activities | History only |

---

## Data Pipeline

```
C++ Row Struct → DataTable (UE Editor) → CSV Export → SQLite DB → Query/Update → CSV → Reimport
```

**Tool**: `Tools/ue_csv_utils.py` — see CLAUDE.md for full command reference.

---

## Systems Confirmed Custom (DO NOT REFACTOR)

| System | Reason |
|--------|--------|
| Persistence/Identity (GUID) | Native ActorGuid only works in dev builds |
| Medical simulation | GAS overkill for physiological simulation |
| Building system | No native alternative for weighted build parts |
| FastArraySerializer usage | Already correct UE5.7 pattern |
| TSoftObjectPtr usage | Already correct UE5.7 pattern |
