# MO57 Technical Reference

> **Current state (2026-10-17):** cross-cutting architecture, code conventions and patterns. Rebuilt during the knowledge
> consolidation: the "Development Environment", "Code Conventions", "Standard Utility Classes", "Common UI Standards",
> "Architecture", "Implementation Notes", native-refactor roadmap and plugin sections moved here verbatim from `CLAUDE.md`;
> the networking, performance-guideline and subsystem sections are this file's own (May 17 2026, Oct 2026 additions).
> Domain sections moved OUT to their system docs: CommonUI architecture + UI controllers + UI delegates + widget patterns ->
> `Docs/UI.md`; creature AI, job/colony APIs and colony design -> `Docs/AI_And_Colony.md`; medical cascade ->
> `Docs/Medical_And_Survival.md`; crafting validation -> `Docs/Crafting_And_Progression.md`; footsteps -> `Docs/Audio.md`;
> graphics quality -> `Docs/Performance.md`; data pipeline -> `Docs/TOOLING.md`.
> Statements found stale on 2026-10-17 are marked **[SUPERSEDED 2026-10-17: ...]** in place, not deleted.
> **Read when:** writing any new C++ system, touching module boundaries, replication, save/load, or performance-sensitive code.
> **Related:** `Docs/ENGINEERING_PRINCIPLES.md`, `Docs/TRAPS.md`, `Docs/INDEX.md` (system docs).

## Module structure and the load-bearing patterns

The plugin `Plugins/MOFramework/` is **three runtime modules + an editor module** (it was monolithic through July 6 2026):
**MOFrameworkCore** (types, contracts, clock, registries, the world generator `FMOWorldGen`) <- **MOFrameworkMedical**
(anatomy, vitals, metabolism, mental state, adrenaline, respiration, medical subsystem) <- **MOFramework** (everything else);
**MOFrameworkEditor** (editor utilities such as `MOWidgetEditorUtils`, `MOEditorContentUtils`). Upward includes are forbidden.

**Core-contract-and-registration** (the load-bearing pattern since the C1/C2 carve, July 2026): when an upper-layer system must
feed a lower-layer one without the lower layer knowing it exists, put a plain-C++ (or U-) interface + a tiny registry in
MOFrameworkCore; the producer registers from above, the consumer asks the registry. Live instances: `IMOSaveDomain` (+ priority
registry on persistence, `MOSaveDomainInterface.h`); `IMOAmbientEnvironmentProvider` + `IMOLocalHeatSource` on
`UMOAmbientEnvironmentRegistry` (weather / fires register, vitals consume feels-like + local heat); `IMOLocalLightSource` (flames
for eye adaptation); `IMOSkillLevelSource` (skills -> adrenaline); `IMOInsulationSource` (equipment "Warmth" -> vitals).
**How to apply:** never add an upward include to satisfy a cross-layer need -- add a Core contract + registry. A Core service that
itself needs saving gets an adapter in the upper module (pattern: `UMOClockSaveDomainAdapter`), because Core is policy-free plumbing.

**Registries instead of per-widget code** (Oct 2026): item actions (`FMOItemActions`), fire actions (`FMOFireActions`), world
actions (`IMOWorldActionProvider` + `UMOWorldActionMenu`), bug-report contributors (`UMOBugReportSubsystem::RegisterContributor`),
HUD elements (`UMOUIManagerComponent::RegisterHUDElement`), movement-interruptible actions (`IMOMovementInterruptibleInterface`),
the one removal path (`FMOHISMHarvestHelper::RemoveInstance`), the one geology->item map (`FMOGeologyYield`). A new behaviour
registers; it does not edit the consumer.

## Development Environment
- IDE: Rider for C++
- Engine: Unreal Engine **5.8.3** (CL 58210709; Wes confirmed 2026-10-17) — the **Epic Games Launcher installed build** (`Engine/Build/InstalledBuild.txt`), NOT a source build: engine
  modules are precompiled and cannot be rebuilt, so engine fixes are staged in `Docs/Engine_Patches.md`, not applied (verified Oct 16 2026)
- Engine Install Path: `D:\UnrealEngine\UE_5.8`
- Engine User Data: `C:\Users\penum\AppData\Local\UnrealEngine\5.8`
- Build Tool Logs: `C:\Users\penum\AppData\Local\UnrealBuildTool`

### UE 5.8 Migration Notes (upgraded 2026-06-30 from 5.7)
- **Voxel Plugin = open-source dev-phy build** (not Pro 2.0), vendored into `Plugins/Voxel`. The sculpt API is wrapped behind **`MOVoxel`** (`MOVoxelAlias.h/.cpp`) — call through the facade; don't include Voxel headers directly elsewhere.
- **`ACharacter::SetBase` changed signature** in 5.8: `UPrimitiveComponent*` → `FMovementBaseInterfaceData*`. Override the **new** signature (engine-version gated) or the engine silently never calls your override. See `MOCharacter.h`.
- **Subsystem `Initialize()` must not `GetSubsystem()` siblings that don't exist in a cook world** (e.g. UI subsystems). The cook commandlet promotes the resulting ensure to **fatal** and packaging dies — defer cross-subsystem binds to `OnWorldBeginPlay`. See `MOWeatherIntegrationSubsystem`.
- **Build settings: `DefaultBuildSettings = V7`** (both targets).
- Next deprecation to clear before any 5.9 move: `FCoreDelegates::OnPostEngineInit` → `GetOnPostEngineInit()`.

## Research Guidelines
- Check Unreal Engine best practices and official documentation for all new code
- Skip web research if scaffolding is already set up and we're making small changes to existing patterns

## Code Conventions
- Always call `RemoveAll(this)` or `RemoveDynamic` before binding delegates in `NativeConstruct()` to prevent duplicate bindings
- UI widgets use CommonUI (`UCommonActivatableWidget`, `UCommonButtonBase`)
- Use Warning log level for important flow events, Log for routine events
- **Input action handling always in C++** - All input action handlers go in `AMOPlayerController::SetupInputComponent()`, never in Blueprint. This keeps input logic centralized and debuggable.
- **UHT Delegate Files**: Any header declaring `DECLARE_DYNAMIC_MULTICAST_DELEGATE` at file scope MUST have at least one `USTRUCT`/`UCLASS`/`UENUM` to force UHT processing. Without this, delegates won't be found by other headers.
- **Template Methods Need Full Includes**: If a header uses a type in a template method, include the full header, not just a forward declaration. Templates instantiate at compile time and need complete type information.

## Standard Utility Classes
- **MOUIDelegates.h** - Standard UI delegate library. Prefer `FMOUIRequestClose`, `FMOUICraftRequest`, `FMOUIRecipeSelected` over per-widget delegate declarations.
- **MOViewpointUtils** - Use for viewpoint resolution and line-of-sight checks. Handles player/AI controller differences consistently.
- **MOUIUtils** - Use for formatting (`FormatQuantityDisplay`, `FormatDurationAsText`, etc.) and widget creation. Don't duplicate formatting patterns.
- *(clarified 2026-10-17, no policy change)* "Input action handling always in C++" means in `AMOPlayerController`: Enhanced
  Input actions in `SetupInputComponent()`, raw keys that Enhanced Input cannot bind (Tilde console popup, F1 HUD mode) in
  `AMOPlayerController::InputKey` (`UEnhancedInputComponent` removed `BindKey`).

## Common UI Standards
- **Always use Common UI features** for UI implementation
- **UMOCommonButton** is the standard button class for all UI (not UButton)
  - Inherits from `UCommonButtonBase`
  - Use `OnClicked().AddUObject()` for click bindings (not `OnClicked.AddDynamic`)
  - Blueprint: Create `WBP_MOCommonButton` as the reusable button widget
- Menus inherit from `UCommonActivatableWidget` for proper focus/input handling
- Use `SetIsEnabled()` for enabling/disabling buttons

## Architecture
- Plugin location: `Plugins/MOFramework/`
- Delegate chain for menus: Panel -> InGameMenu -> UIManager -> Subsystem
- Target names are case-sensitive: `MO57Editor`, `MO57` (not `mo57`)

- *(clarified 2026-10-17, no policy change)* In a Widget Blueprint the button NODE is always the Widget Blueprint
  `MOButton` / `WBP_MOCommonButton`, never the native `UMOCommonButton` class: a native UserWidget has no widget tree, so real
  clicks pass through it (see `Docs/UI.md`, `Docs/TRAPS.md`).

## Implementation Notes

### Subsystem Architecture

| Subsystem | Type | Responsibility |
|-----------|------|----------------|
| `UMOPersistenceSubsystem` | GameInstance | Save/load slots, pawn records, destroyed GUID tracking. Save DOMAINS live on their owning subsystems via `IMOSaveDomain` (MOSaveDomainInterface.h) — register, don't edit persistence |
| `UMOIdentityRegistrySubsystem` | World | GUID-to-Actor mapping, identity lifecycle events |
| `UMOInteractionSubsystem` | World | Interaction system coordination |
| `UMOCraftingSubsystem` | World | Recipe validation, crafting operations |
| `UMOPossessionSubsystem` | World | Pawn possession management |
| `UMOMedicalSubsystem` | GameInstance | DataTable lookups for medical definitions |
| `UMOColonyManagerSubsystem` | World | Colony alerts, character enumeration, task assignment |
| `UMOGameUIManagerSubsystem` | World | CommonUI layer management, widget stacks |

Colony management (planned + partly built), medical cascade and UI widget patterns: see `Docs/AI_And_Colony.md`,
`Docs/Medical_And_Survival.md`, `Docs/UI.md`.

### Component Architecture

**Player Controller Components (AMOPlayerController):**
- `UMOUIManagerComponent` - UI orchestrator, delegates to specialized controllers
- `UMOPossessionComponent` - Pawn possession state

**UI Controller Components (Sibling components on AMOPlayerController):**
| Controller | Responsibility |
|------------|----------------|
| `UMOUIControllerBase` | Base class: input mode, modal background, pawn caching |
| `UMOCharacterUIController` | Skills panel, Status panel, Item inspection |
| `UMOBuildingUIController` | Building menu, Ghost context menu, Build widget |
| `UMOCraftingUIController` | Crafting menu, Station context, Harvest operations |
| `UMOSystemMenuUIController` | In-game menu, Possession menu, Confirmations |
| `UMOInventoryUIController` | Inventory menus, Item context, Nearby items, Drop |

Controllers find siblings via `GetOwner()->FindComponentByClass<T>()` with weak pointer caching.
UIManager maintains backward-compatible public API via delegation wrappers.

**Pawn Components (AMOCharacter):**
| Component | Responsibility | Tick Rate |
|-----------|---------------|-----------|
| `UMOIdentityComponent` | GUID-based persistence identity | N/A |
| `UMOInventoryComponent` | Item storage with slot system | N/A |
| `UMOAnatomyComponent` | Body parts, wounds, conditions | 1.0s |
| `UMOVitalsComponent` | HR, BP, SpO2, temp, glucose, blood | 0.5s |
| `UMOMetabolismComponent` | Nutrition, digestion, body composition | 1.0s |
| `UMOMentalStateComponent` | Consciousness, shock, effects | 0.5s |
| `UMOSkillsComponent` | Skill levels and XP |
| `UMOKnowledgeComponent` | Known recipes/techniques |

### Interface-Based Decoupling

**IMOControllableInterface** - Pawn control delegation
- Used by: `AMOPlayerController` to send input to any pawn type
- Methods: `RequestMove`, `RequestLook`, `RequestJumpStart/End`, `RequestInteract`, etc.
- Pawns implement this to receive controller input

**IMOInteractionInterface** - Interaction system
- Used by: `UMOInteractorComponent` to interact with world objects
- Implementors: Items, doors, containers, NPCs

### Replication Patterns

**FastArraySerializer** - Efficient array replication:
```cpp
// Pattern for replicated collections (inventory, wounds, conditions)
USTRUCT()
struct FMOWoundList : public FFastArraySerializer
{
    UPROPERTY()
    TArray<FMOWound> Wounds;

    // Required callbacks
    void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
    void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
    void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams) { /*...*/ }
};

// Register type traits
template<>
struct TStructOpsTypeTraits<FMOWoundList> : public TStructOpsTypeTraitsBase2<FMOWoundList>
{
    enum { WithNetDeltaSerializer = true };
};
```

**GUID-Based Identity** - Stable cross-session references:
- `UMOIdentityComponent` generates/stores FGuid per actor
- `UMOIdentityRegistrySubsystem` maintains GUID→Actor map
- Persistence uses GUIDs, not actor pointers

### DataTable-Driven Design

**Definition Rows** (all inherit from `FTableRowBase`):
| Row Type | DataTable | Purpose |
|----------|-----------|---------|
| `FMOItemDefinitionRow` | DT_ItemDefinitions | Items, nutrition, equipment |
| `FMOSkillDefinitionRow` | DT_SkillDefinitions | Skills, XP curves |
| `FMORecipeDefinitionRow` | DT_RecipeDefinitions | Crafting recipes |
| `FMOBodyPartDefinitionRow` | DT_BodyPartDefinitions | ~55 body parts |
| `FMOMedicalTreatmentRow` | DT_MedicalTreatments | Wound treatments |

**UDeveloperSettings Pattern** - Project Settings integration:
```cpp
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Item Database"))
class UMOItemDatabaseSettings : public UDeveloperSettings
{
    UPROPERTY(Config, EditAnywhere, meta=(AllowedClasses="/Script/Engine.DataTable"))
    FSoftObjectPath ItemDefinitionTable;
};
```

### Async Loading with TSoftObjectPtr

```cpp
// In header - stores path, not loaded asset
UPROPERTY(EditAnywhere)
TSoftObjectPtr<UInputMappingContext> PawnControlContext;

// In code - load when needed
if (UInputMappingContext* Context = PawnControlContext.LoadSynchronous())
{
    // Use context
}
```

### Save/Load Pattern

**Authority-Only State Modification**:
```cpp
// All components follow this pattern
UFUNCTION(BlueprintCallable)
void BuildSaveData(FMOVitalsSaveData& OutSaveData) const;  // Any caller

UFUNCTION(BlueprintCallable)
bool ApplySaveDataAuthority(const FMOVitalsSaveData& InSaveData);  // Server only
```

### UE 5.8 Best Practices Observed

1. **TObjectPtr** - Smart pointers for UPROPERTY object references
2. **Enhanced Input System** - Input Actions + Mapping Contexts
3. **Common UI** - UCommonActivatableWidget, UCommonButtonBase
4. **World/GameInstance Subsystems** - Over singletons
5. **Soft References** - TSoftObjectPtr/TSoftClassPath for async loading
6. **Interface Decoupling** - UInterface for cross-class communication
7. **DataTables** - Over hardcoded definitions
8. **FastArraySerializer** - For replicated arrays

### Decoupling Strengths

- **Controller↔Pawn**: IMOControllableInterface allows any pawn type
- **UI↔Logic**: UIManagerComponent delegates all logic to subsystems
- **Data↔Code**: DataTables for all definitions
- **Persistence↔Components**: GUID-based, components don't know about save system
- **Medical Components**: Each component broadcasts changes, others subscribe

### Known Coupling Issues (Technical Debt)

**RESOLVED (Oct 15 2026) - Persistence↔Inventory Circular Dependency:**
- `MOInventoryComponent` used to call `MOPersistenceSubsystem.IsGuidDestroyed/ClearDestroyedGuid` before spawning a dropped item
- **Fixed**: it announces `UMOIdentityRegistrySubsystem::AnnounceReintroduction(Guid)`; persistence listens on `OnIdentityReintroduced`
  and forgets the destroyed GUID. The inventory knows nothing about persistence (the registry is the seam both already used)

**HIGH - Possession System Component Requirements:**
- `MOPossessionSubsystem` requires `UMOIdentityComponent` + `UMOInventoryComponent`
- Cannot possess pawns lacking these components
- **Mitigation**: Make component requirements optional/configurable

**RESOLVED - UIManager Orchestration Bottleneck:**
- Previously: `MOUIManagerComponent` was ~4000 lines handling all UI
- **Fixed**: Split into 6 specialized controllers (Character, Building, Crafting, System, Inventory + Base)
- UIManager now acts as thin orchestrator delegating to controllers
- See `Docs/Archive/MO57_Master_Plan.md` for remaining migration to CommonUI layer stack

**MEDIUM - Monolithic Module Structure:** **[SUPERSEDED 2026-10-17: split into MOFrameworkCore / MOFrameworkMedical / MOFramework (+ Editor) in July 2026, C1 phases 1-3; the UI module carve (phase 4) is still open, see BACKLOG C1]**
- All 60+ classes in single `MOFramework` module
- Cannot use only specific systems
- **Future**: Consider splitting into Core, Interaction, Inventory, Medical, UI submodules

**Portability Score: 6.5/10** - Good fundamentals, needs abstraction layer work

### Pending Implementation (Blueprint Setup Required)

See `Docs/PROJECT_STATUS.md` for full status of all pending work.

**Key items needing Blueprint setup (C++ complete):**
- Creature ABP state machines (Deer locomotion/rest/sleep/death states)
- EQS query assets (EQ_FindHarvestableItems, EQ_FindHarvestTargets, EQ_FindEscapeRoute)
- NewGamePanel WBP (seed input, random button, start button)
- CommonUI layer stacks already configured via WBP_PrimaryGameLayout (Stage 3A COMPLETE)
- Colony management components (MOColonyTypes.h, UMOPersonalityComponent already created in Stage 1)

## Subsystem inventory (May 2026 sizes)


| Subsystem | Type | LOC | Responsibility |
|-----------|------|-----|----------------|
| MOPersistenceSubsystem | GameInstance | 2415 | Save/load everything (C2 — too broad) **[SUPERSEDED 2026-10-17: C2 fixed July 6 2026, save domains register via `IMOSaveDomain`; see `Docs/Persistence.md`]** |
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
MOInventoryComponent → MOPersistenceSubsystem (GI) [circular dep — M11]   [SUPERSEDED 2026-10-17: fixed Oct 15 2026, the inventory announces via UMOIdentityRegistrySubsystem::AnnounceReintroduction]
```

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

### Periodic work: never walk the world (found Oct 2026)

- **`TObjectIterator<T>` visits EVERY UObject** (GUObjectArray: hundreds of thousands in a world with PCG / foliage instances), whatever `T` is. A once-a-second ticker doing it (the removed audio mix guard) cost ~30 ms: a
  1 Hz hitch in the packaged tour that is invisible to `GGameThreadTime` (core tickers run outside it), to the render thread and to the GPU, and independent of where the camera looks. `TActorIterator` /
  `GetAllActorsOfClass` scale with the actor count the same way. Periodic code keeps its own registry (register / unregister, like `UMOIdentityRegistrySubsystem`) instead of searching.
- **The actual cause (Oct 2026): `UMOUplandLakeSubsystem` ran `FMOWorldGen::GetUplandLakesNear` (the lake grid of an 8 km square, ~30 ms) on the game thread once a second.** The audio guard's `TObjectIterator` (removed) was a
  decoy that cost a build cycle: the bisection of GPU features (Lumen, VSM, TSR, sky, clouds, fog, hair) changed nothing because the stall was on the game thread, and the counters I logged LAG the frame-time measurement
  by one frame (the hitch frame's own `GGameThreadTime` shows up in the NEXT frame's log: log a ring of frames, not one). The CSV profiler (`MO.Perf.Csv 1` in the tour -> `Saved/Profiling/CSV`, rank columns by how much they
  rise on hitch frames) pointed at `Exclusive/GameThread/Tickables`, and `MO_TICK_WATCH(TEXT("Name"))` (MOTickWatchdog.h, in every tickable subsystem's Tick) named the subsystem. Fix pattern: cache the answer and re-run it only after the
  viewers moved far enough, on a worker thread when it is a pure function of immutable data (`FMOWorldGenParamsRef` is thread-safe).
- **How it was found**: `MO.Perf.Tour ... fixed_yaw` (is the hitch view dependent? no) + the `[MOPERF] HITCH` lines (frame ms vs game / render / gpu ms: all four numbers must add up; when the frame is 30 ms and every
  thread is under 14, the time is spent somewhere none of them counts) + the hitch spacing (a constant ~9 degrees of tour yaw = once a second).

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


Measurement methods, hitch hunts and benchmark results: `Docs/Performance.md`.

## Inventory and equipment ownership

- `UMOInventoryComponent::TryAddItem` is the shared capacity/stack/state boundary. It returns requested/accepted quantities and a typed
  failure; partial placement requires explicit opt-in. Bool compatibility APIs require complete acceptance. Entries reserve slots even
  while automatic slot assignment is disabled; state-incompatible items do not merge, and individually worn tools/vessels remain distinct.
- `TryTransferItem` and `TransferIngredientsTo` plan both sides before mutation. The ingredient service jointly reserves physical and
  liquid requirements, preserving GUIDs, wear and liquid provenance; hauling can move intact source vessels and explicit borrowed cargo.
  Pure snapshot copies clear Unreal FastArray metadata, so installation restores surviving live identities/keys before dirtying changes.
- Changed surviving FastArray entries need `MarkItemDirty`; `MarkArrayDirty` alone only covers collection membership changes.
  `FMOInventoryMutationScope` coalesces callbacks around an already validated compound change; it is not rollback.
- Equipment validates every slot byte before indexing. A pending swap is its item's sole durable owner; its old slot is cleared before
  returning the previous item. Conflicting operations and reentrant callbacks refuse. Save/replication includes both hand timers and
  item state. `ReduceEquippedDurability` is authority-only: ordinary wear emits `OnEquipmentDurabilityChanged`; depletion clears ownership
  and emits the equipment change. Wear-only replication does not rebuild the held mesh. Infinite durability/pending swaps refuse wear.
- Shared consumers use these contracts for pickups, harvest output capacity, split/fill/consolidate, equipment returns and queued materials.
  Do not add a caller-specific remove-then-add transfer or infer an inventory failure from a depletion result.

## Character movement

### Crouch
`NavAgentProps.bCanCrouch` must be set (it was not: `Crouch()` was a silent no-op). Capsule 60 cm, 200 cm/s; the pose is a looping dynamic montage on the locomotion ABP's `DefaultSlot` (clips: `Plugins/MOFramework/Content/Characters/Crouch`,
retargeted Manny set, see the memory note on the retarget pipeline). The swim override still replaces the ABP; crouch and swim never overlap.

Swimming, wading and breath: `Docs/Water.md`. Footsteps: `Docs/Audio.md`.

## UE5.8 Native Refactoring Roadmap

*Full audit completed March 18, 2026 - see `Docs/Archive/MO57_Master_Plan.md` (archived plan). Statuses below were not re-checked after March; open items are tracked in `Docs/BACKLOG.md`.*

### Priority Refactoring Targets

| Priority | System | Native Alternative | Status |
|----------|--------|-------------------|--------|
| 1 | UI Controllers | CommonUI `UGameUIManagerSubsystem`, widget stacks | Planning |
| 2 | AI Queries | Environment Query System (EQS) | Not Started |
| 3 | Enums → Tags | Gameplay Tags | Not Started |
| 4 | Interaction | Smart Objects | Not Started |
| 5 | Data Loading | Data Registry | Not Started |
| 6 | Hustle Input | Enhanced Input Triggers | Not Started |
| 7 | PCG Culling | Native PCG distance filtering | Not Started |

### Correctly Custom (DO NOT REFACTOR)
- **Persistence/Identity** - Native `ActorGuid` only works in dev builds
- **Medical simulation** - GAS overkill for physiological simulation
- **Building system** - No native alternative for weighted build parts
- **FastArraySerializer usage** - Already correct pattern
- **TSoftObjectPtr usage** - Already correct pattern

### When Adding New Features
Before implementing custom solutions, check:
1. **CommonUI** - For any UI widget or input handling
2. **EQS** - For any spatial queries or target finding
3. **Gameplay Tags** - For any enum-like categorization
4. **Smart Objects** - For any interaction points
5. **Data Registry** - For any DataTable caching
6. **GAS** - Only if need prediction/replication of abilities

## Systems Confirmed Custom (DO NOT REFACTOR)

| System | Reason |
|--------|--------|
| Persistence/Identity (GUID) | Native ActorGuid only works in dev builds |
| Medical simulation | GAS overkill for physiological simulation |
| Building system | No native alternative for weighted build parts |
| FastArraySerializer usage | Already correct UE5.7 pattern |
| TSoftObjectPtr usage | Already correct UE5.7 pattern |

## Plugins (installed)
- **Voxel Plugin (open-source dev-phy build)** - vendored in `Plugins/Voxel`; sculpt API wrapped behind the `MOVoxel` facade (never include Voxel headers directly). Reference: `Docs/Voxel_Plugin_Reference.md`
- **Ultra Dynamic Sky** - lives in `Content/UltraDynamicSky`; registers the weather provider (`IMOWeatherProviderInterface`) that drives ambient temperature. NOTE: feels-like temperature bases on GLOBAL temp (UDS location queries blend canopy toward interior warmth — see MOWeatherIntegrationSubsystem)
- **Ultra Dynamic Weather** - ships inside `Content/UltraDynamicSky` (UDW actor, MetaSounds, Change Weather). Was listed as "planned" in CLAUDE.md; installed and in use (Wes, 2026-10-17). See `Docs/Weather_And_Time.md`
- **Oceanology** ("Oceanology Legacy", `Plugins/Marketplace/Oceanology_Plugin`) - ocean, upland lakes, underwater look; replaced Epic Water on 2026-10-11. Was listed as "planned" in CLAUDE.md; installed (Wes, 2026-10-17). See `Docs/Water.md`
- **OnlineSubsystemSteam + SteamSockets** - co-op sessions. See `Docs/Coop_And_Networking.md`
- **ModelContextProtocol / EditorToolset / MCPClientToolset** (Epic, experimental) - the in-editor MCP at `http://127.0.0.1:8000/mcp`. See `Docs/AUTONOMOUS_TOOLING.md`
