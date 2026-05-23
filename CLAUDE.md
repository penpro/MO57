# MOFramework Project Memory

## STOP - Check Before These Operations

| Operation | Check This Section First |
|-----------|-------------------------|
| **Modify any CSV file** | "UE DataTable CSV Manipulation" - USE THE UTILITY |
| **Add columns to DataTable** | Use `add-column` command, not direct CSV edit |
| **Compile C++** | Prompt user to close UE Editor first |
| **Git commit** | Only when user explicitly asks |
| **"Fix this bug"** | Read "Engineering Principles" below FIRST |
| **"Add X to systems A, B, C"** | Read "Engineering Principles" — centralize, don't duplicate |

**CSV files = NEVER edit directly. Always use `Tools/ue_csv_utils.py`**

---

## Engineering Principles (MANDATORY — read every session)

**This project is targeting production-ready software. The user has explicitly
called out a pattern of patching symptoms instead of systems and will reject
work that does it. These rules override convenience and override speed.**

### 1. Trace bugs to the layer that creates the bad state, not where it surfaces

When a bug is reported, the first impulse is to fix the visible failure.
Resist it. Walk the call stack backward to the layer that produces the
incorrect state, and fix the bug there.

**Concrete example from this codebase:** A "Inventory full!" notification was
firing falsely. The visible failure was the `CanAddItem` check, but the
actual bug was that `FMOCraftResult` stored depletion failures and inventory
failures in the same `FailedItems` map. The UI couldn't distinguish them, so
it always showed "Inventory Full!" — even when the problem was a depleted
node. Patching `CanAddItem` would have masked the symptom and left the
classification bug to cause future false positives elsewhere.

**Rule:** Before editing, ask "where does the wrong value first appear?"
If you're editing further downstream than that, you're patching a symptom.

### 2. If N systems need the same behavior, build one abstraction — not N copies

When asked to add a behavior to multiple systems (e.g. "moving interrupts
building AND inspection AND gathering"), the wrong move is N parallel edits.
The right move is one shared mechanism (interface, delegate, base class) that
each system plugs into.

**Concrete example from this codebase:** Movement interruption is implemented
as `IMOMovementInterruptibleInterface` + a registration list on `AMOCharacter`.
A new interruptible action (lockpicking, surgery, fishing) plugs in by
implementing the interface and calling `RegisterMovementInterruptible` — no
changes to `AMOCharacter`, no coupling between systems.

**Rule:** If your patch touches N existing files to add behavior B, you've
probably built B in the wrong place. Build it once, register N times.

### 3. A comment claiming behavior doesn't make it true — verify

Documentation drifts. Aspirational comments survive. "Uses click-outside" in
`MOContextMenuBase` was wrong for 2+ years — the click-outside handler was
never implemented. Anyone who trusted the comment had a broken UX.

**Rule:** When a comment describes load-bearing behavior, confirm the code
matches. If it doesn't, fix the code OR fix the comment — never just the
caller that depends on the lie.

### 4. Read the whole flow before changing one node

A single function in a chain rarely has enough context to fix correctly.
Read up to the caller, down to what's called, and across to similar
functions in the same file. The CanAddItem bug had THREE related functions
(`CanAddItem(FGuid)`, `CanAddItemByDefinitionId`, `AddItemByGuid`) plus the
caller path — reading only one was why the first three patches missed.

**Rule:** Before editing a function, read its caller, its callee(s), and
its siblings.

### 5. When the cause is unclear, add diagnostic logging — don't guess and patch

If you've made two patches without certainty, stop. The next patch will
probably also miss. Instrument the suspect layer with enough logging to
prove what's happening, build, have the user reproduce, then read the data.
This is faster than three more guess-patches.

**Rule:** Two failed patches in a row = stop patching, start instrumenting.

### 6. Failure modes must be distinguishable at the data layer

If a struct lumps "kind A failure" and "kind B failure" into the same field,
every consumer downstream has to guess what to do — and most will guess
wrong. Distinguish failures where they're produced. The UI's job is to
display, not to disambiguate.

**Rule:** When a function can fail for multiple reasons, the return value
must carry enough information to tell them apart. Don't make callers guess.

### 7. Defaults are policy

Default values in headers propagate to every subclass that doesn't override
them. `bCloseOnMouseLeave = false` in the base meant every menu was broken
until someone explicitly opted in. The base's default should be the behavior
most subclasses want, with explicit overrides for exceptions.

**Rule:** When you set a default, ask "will most code want this?" If not,
change the default — don't leave it to every subclass to remember to opt in.

### 8. "It compiles" is not "it works"

Build success tells you the types match. It tells you nothing about whether
the logic is correct. Always provide a concrete reproduction step the user
can run, and verify behavior — not just compilation.

**Rule:** Don't claim a fix is done because the build succeeded. Describe
exactly how to test the behavior, and what the expected vs. failure output
looks like.

### 9. Production-ready means extension without modification

If adding a new resource type means editing 5 files, the abstraction is
wrong. If adding a new interruptible action means editing `AMOCharacter`,
the abstraction is wrong. New features should plug in, not require surgery.

**Rule:** When you finish a feature, ask: "to add the next instance of this
pattern, how many existing files would I touch?" If the answer is more than 1
(the new code itself), refactor first.

### 10. Diagnose before designing the fix

Don't propose a solution until you can explain the bug in one sentence
that names the responsible layer. "The inventory check is wrong" is a
symptom. "Depletion failures share a map with inventory failures, so the UI
can't tell them apart" is a diagnosis. Only the diagnosis tells you where
the fix belongs.

**Rule:** If you can't write a one-sentence diagnosis naming the
responsible layer, you don't understand the bug yet.

---

## Design Policies (load-bearing — never violate without policy doc update)

| Policy | Document | Enforcement |
|--------|----------|-------------|
| **No pause, ever** (real-time always; single-player and co-op alike) | `Docs/PAUSE_POLICY.md` | `AMOPlayerController::SetPause` refuses + logs warning |

If you find yourself wanting to add `SetGamePaused`, `SetPause(true)`, a "pause game" feature, or anything equivalent: don't. Read the policy doc first. The enforcement is intentional.

---

## Project Vision

**MO57** is an ultra-realistic procedural open-world survival game with a fully destructible/mutable voxel terrain. Think Minecraft's freedom meets hardcore realism - no fantasy creatures, grounded physics, detailed medical/survival simulation.

### Core Pillars
1. **Realism First** - All systems rooted in real-world mechanics (medical, crafting, physics)
2. **Emergent Civilization** - Solo primitive survival → multi-pawn settlements → castle cities
3. **Total World Mutability** - Dig, mine, build, terraform via Voxel Plugin Pro 2.0
4. **Modding Foundation** - Full C++ mod support; base game is a realistic framework others can reskin/extend

### Multiplayer
- Steam-based co-op (Satisfactory-style): play solo or invite friends to help
- Not MMO - small group collaboration on shared worlds

### Pawn System
- **Possession**: Player can possess any pawn they control; idle pawns run on AI
- **Assignments**: Assign pawns to jobs (gather wood, teach, craft) and bind to house + workplace
- **Relationships**: Pawns have family, loyalty, morale; villages can ally or wage war
- **AI Autonomy**: Full survival instincts (eat, sleep, flee) with streamlined routines for jobs
- **Permadeath**: Pawn death is permanent; if last pawn dies, respawn ~5 miles away as new pawn, old pawn's gear remains at death location
- **Population Cap**: Soft cap via resource/survival difficulty, not arbitrary limits

### Skills & Progression
- **Extensive Skill Trees**: Primitive crafting (knapping, pitch-making) through medieval engineering and beyond
- **Learning Methods**:
  - Direct action (slow)
  - Being taught by skilled pawn (2x speed)
  - Schools maintain entire skill tree (prevents decay)
- **Skill Decay**: Unused skills degrade over time unless maintained via schooling
- **Tech Accessibility**: No hard locks; player has "genetic memory" (lore: galactic seeding program) allowing attempts at any tech, but practical prerequisites make skipping difficult (can't smelt without foundry, can't build foundry without tools/materials)

### World Generation
- Voxel Plugin Pro 2.0 for destructible/buildable terrain
- Finite large flat world with world border (engine supports earth-sized spheres for future)
- Procedurally generated biomes, resources, points of interest
- Chunked loading for performance

### Lore (Revealed Gradually)
- Players are colonists with encoded genetic memory, sent to seed new planets
- Knowledge unlocks feel like "remembering" rather than inventing
- Sci-fi origins revealed in late-game/DLC content

### Development Phases
1. **MVP - Solo Survival Loop**: Single pawn, primitive survival, core medical/crafting systems
2. **Pawn Discovery**: Find survivors after exploring ~100mi², simple automated tasks initially
3. **Full Pawn AI**: Autonomous survival behavior, job systems, relationships
4. **Civilization Building**: Housing, workplaces, teaching, population growth
5. **Multiplayer Polish**: Steam integration, world sharing
6. **DLC Pipeline**: Medieval → Industrial → Modern → Sci-fi planetary expansion

### Modding Philosophy
- Full C++ mod support (like Java Minecraft modding)
- Realistic foundation that modders can extend with any theme (fantasy, sci-fi, historical)
- Clean separation of engine/framework code from content

---

## Development Environment
- IDE: Rider for C++
- Engine: Unreal Engine 5.7
- Engine Install Path: `D:\UnrealEngine\UE_5.7`
- Engine User Data: `C:\Users\penum\AppData\Local\UnrealEngine\5.7`
- Build Tool Logs: `C:\Users\penum\AppData\Local\UnrealBuildTool`

## Workflow Rules
- **Before compiling**: Prompt user to close Unreal Editor (Live Coding blocks CLI builds)
- **After every successful compile**: Run `git add -A && git commit -m "checkpoint" && git push` to enable rollback if needed
- User will confirm compile success before git operations

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

## UE5.7 Native Refactoring Roadmap

*Full audit completed March 18, 2026 - see `Docs/MO57_Master_Plan.md` for consolidated planning*

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

---

## Implementation Notes

### Subsystem Architecture

| Subsystem | Type | Responsibility |
|-----------|------|----------------|
| `UMOPersistenceSubsystem` | GameInstance | Save/load, pawn records, destroyed GUID tracking |
| `UMOIdentityRegistrySubsystem` | World | GUID-to-Actor mapping, identity lifecycle events |
| `UMOInteractionSubsystem` | World | Interaction system coordination |
| `UMOCraftingSubsystem` | World | Recipe validation, crafting operations |
| `UMOPossessionSubsystem` | World | Pawn possession management |
| `UMOMedicalSubsystem` | GameInstance | DataTable lookups for medical definitions |
| `UMOColonyManagerSubsystem` | World | Colony alerts, character enumeration, task assignment |
| `UMOGameUIManagerSubsystem` | World | CommonUI layer management, widget stacks |

### Colony Management System (Planned)

*Full design in `Docs/MO57_Colony_Management_Design.docx`, implementation in `Docs/MO57_Master_Plan.md`*

**Core Components**:
| Component | Purpose |
|-----------|---------|
| `UMOColonyManagerSubsystem` | Alert queue, character enumeration, task delegation |
| `UMOPersonalityComponent` | Character personality traits (Conscientiousness, Sociability, Stability) |
| `UMOCharacterHistoryComponent` | Event log, relationship tracking, mood/activity summaries |
| `UMORecruitmentComponent` | Tracks recruitment state (existing - determines colony membership) |
| `UMOSurvivorJobQueueComponent` | Job queue management (existing - task assignment API) |

**Colony UI Widgets**:
| Widget | Layer | Purpose |
|--------|-------|---------|
| `UMOColonyBarWidget` | HUD | Persistent character strip with portraits |
| `UMOColonyOverviewWidget` | Menu | Full colony management screen (replaces/extends possession menu) |
| `UMOCharacterCardWidget` | - | Character detail view (mood, history, relationships, skills) |
| `UMOColonyPortrait` | - | Reusable portrait with mood expression, activity, alert state |
| `UMOTaskAssignmentWidget` | - | Task picker and job stack management |

**Alert Tiers**:
| Tier | Name | Examples | Display |
|------|------|----------|---------|
| 1 | Critical | Health <15%, combat while away | Pulsing red border, sound, cannot dismiss |
| 2 | Urgent | Health <40%, idle >30min | Orange dot on portrait |
| 3 | Notable | Task complete, skill gained | Colony log only |
| 4 | Log | Routine activities | Character history only |

**Personality System (UMOPersonalityComponent)**:
- **Conscientiousness**: Diligent (methodical, slower, higher quality) vs Adaptable (quick, shortcuts, variable)
- **Sociability**: Social (performs better near others) vs Reserved (performs better alone)
- **Stability**: Stable (consistent mood) vs Volatile (strong mood swings, high ceiling/floor)

**Key Design Principles**:
1. Colony overview is a "window into community" not just a "management interface"
2. Characters should feel like people with opinions, not interchangeable labor
3. Trust is earned through demonstrated reliability
4. Alert tiering prevents notification fatigue while surfacing critical issues
5. Task assignment works without possession via `UMOSurvivorJobQueueComponent`

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

### Medical System Cascade

```
Wounds (bleed) → Vitals (blood volume) → Mental (consciousness)
                      ↓
              Heart/Lung damage → SpO2/BP → Death timers
                      ↓
Metabolism (glucose) → Vitals (blood glucose) → Mental (confusion)
                      ↓
Dehydration → Vitals (+HR, -BP, +Temp) → Performance penalties
```

### UI Widget Patterns

**BindWidget Meta** - Blueprint/C++ widget binding:
```cpp
// Required binding (compile error if missing in Blueprint)
UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
TObjectPtr<UScrollBox> ContentScrollBox;

// Optional binding (null-safe, no error if missing)
UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
TObjectPtr<UMOCommonButton> OptionalButton;
```

**Common UI Button Binding**:
```cpp
// In NativeConstruct()
if (MyButton)
{
    MyButton->OnClicked().RemoveAll(this);  // Prevent duplicate bindings
    MyButton->OnClicked().AddUObject(this, &UMyWidget::HandleButtonClicked);
}
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

### UE 5.7 Best Practices Observed

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

**CRITICAL - Persistence↔Inventory Circular Dependency:**
- `MOInventoryComponent.DropItemByGuid()` calls `MOPersistenceSubsystem.IsGuidDestroyed()`
- Creates runtime mutual dependency during drop operations
- **Mitigation**: Consider `IMOPersistenceProvider` interface to abstract

**HIGH - Possession System Component Requirements:**
- `MOPossessionSubsystem` requires `UMOIdentityComponent` + `UMOInventoryComponent`
- Cannot possess pawns lacking these components
- **Mitigation**: Make component requirements optional/configurable

**RESOLVED - UIManager Orchestration Bottleneck:**
- Previously: `MOUIManagerComponent` was ~4000 lines handling all UI
- **Fixed**: Split into 6 specialized controllers (Character, Building, Crafting, System, Inventory + Base)
- UIManager now acts as thin orchestrator delegating to controllers
- See `Docs/MO57_Master_Plan.md` for remaining migration to CommonUI layer stack

**MEDIUM - Monolithic Module Structure:**
- All 60+ classes in single `MOFramework` module
- Cannot use only specific systems
- **Future**: Consider splitting into Core, Interaction, Inventory, Medical, UI submodules

**Portability Score: 6.5/10** - Good fundamentals, needs abstraction layer work

### Pending Implementation (Blueprint Setup Required)

**Creature Animation Blueprint Setup:**
The C++ infrastructure for creature activity states is complete, but Blueprint setup is pending:

1. **Deer ABP State Machine** - Add states for: Locomotion, Resting (IdleRest anim), Sleeping (IdleSleep anim), Death
2. **ABP Variables** - Add: `GroundSpeed` (float), `IsDead` (bool), `IsResting` (bool), `IsSleeping` (bool)
3. **Blackboard Keys** - Add to creature blackboard: `ActivityState` (enum), `IsDead`, `IsResting`, `IsSleeping`
4. **Animation Montages** - Create montages for HitReaction and Death, assign to BP_Deer's `HitReactionMontage` and `DeathMontage` properties
5. **Behavior Tree** - Add `BTService_CreatureActivity` to BT_Prey root, add rest/sleep branches with decorators

**State Machine Transitions:**
- Locomotion → Resting: `IsResting == true AND GroundSpeed < 10`
- Locomotion → Sleeping: `IsSleeping == true AND GroundSpeed < 10`
- Resting/Sleeping → Locomotion: states become false OR `GroundSpeed > 10`
- Any → Death: `IsDead == true`

**New Game Panel Blueprint Setup:**
The C++ widget `UMONewGamePanel` is complete but needs a Blueprint widget:

1. **Create `WBP_NewGamePanel`** - Parent: `UMONewGamePanel`
   - Add `SeedInputBox` (EditableTextBox) - for entering seed text/number
   - Add `RandomSeedButton` (UMOCommonButton) - labeled "Random"
   - Add `StartGameButton` (UMOCommonButton) - labeled "Start Game"
   - Add `BackButton` (UMOCommonButton, optional) - labeled "Back"
   - Add `SeedPreviewText` (TextBlock, optional) - shows computed seed integer

2. **Update `WBP_MainMenu`** FocusWindowSwitcher:
   - Index 0: Empty placeholder
   - Index 1: WBP_NewGamePanel (NEW)
   - Index 2: WBP_LoadPanel (shifted)
   - Index 3: WBP_OptionsPanel (shifted)

3. **Voxel Graph Seed Integration:**
   - Voxel graphs can call `UMOGameSettings::GetWorldSeed()` (Blueprint Pure) to get the player-selected seed
   - Use this seed in noise/random nodes for consistent world generation
   - The global `FMath::RandInit()` is also set with this seed at game start

---

**UE5.7 Refactoring - Blueprint Setup Required:**

**CommonUI Layer System (Phase 1):**
The C++ infrastructure for the UI layer system is complete, but Blueprint setup is pending:

1. **Create `WBP_PrimaryGameLayout`** - Parent: `UMOPrimaryGameLayout`
   - Add `HUDLayer` (UCommonActivatableWidgetStack) - Z-Order 0
   - Add `GameLayer` (UCommonActivatableWidgetStack) - Z-Order 50
   - Add `GameOverlayLayer` (UCommonActivatableWidgetStack) - Z-Order 100
   - Add `MenuLayer` (UCommonActivatableWidgetStack) - Z-Order 150
   - Add `ModalLayer` (UCommonActivatableWidgetStack) - Z-Order 200

2. **Configure Subsystem:**
   - Set `PrimaryLayoutClass` on `UMOGameUIManagerSubsystem` to `WBP_PrimaryGameLayout`
   - Can be done via Project Settings or Blueprint

3. **Hook into PlayerController:**
   - Call `UMOGameUIManagerSubsystem::Get()->NotifyPlayerAdded(this)` when player joins
   - This creates the layout widget for the player

**EQS Resource Queries (Phase 2):**
The C++ EQS components are complete, but Blueprint EQS query assets need creation:

1. **Create `EQ_FindHarvestableItems`** - Environment Query asset
   - Generator: `HarvestableItems` (custom generator)
   - SearchRadius: 5000 (50 meters)
   - Tests: Distance (prefer closer), PathExists (filter unreachable)
   - Usage: Survivor AI ground resource finding (stone, fiber, sticks)

2. **Create `EQ_FindHarvestTargets`** - Environment Query asset
   - Generator: `HarvestTargets` (custom generator)
   - RequiredTag: "GivesStick" (for wood gathering)
   - Tests: Distance, PathExists
   - Usage: Survivor AI tree harvesting

3. **Create `EQ_FindEscapeRoute`** - Environment Query asset
   - Generator: Points around querier (donut/circle)
   - Context: Threat (provides threat actor from blackboard)
   - Tests: EscapeRoute (custom test), PathExists
   - Usage: Prey creature IsCornered() check

4. **Update Behavior Trees:**
   - Replace `FindNearestGatherResource()` calls with EQS RunQuery
   - Replace IsCornered() placeholder with EQS query + threshold check

**Note:** The underlying ForagingSubsystem still uses O(n) iteration. Future optimization will add spatial indexing.

**Colony Management System (Stage 1-2 of Master Plan):**

New files to create for colony system foundation:

1. **`MOColonyTypes.h`** - Shared enums and structs
   ```cpp
   UENUM(BlueprintType)
   enum class EAlertState : uint8 { None, Notable, Urgent, Critical };

   UENUM(BlueprintType)
   enum class EPersonalityAxis : uint8 { Conscientiousness, Sociability, Stability };

   USTRUCT(BlueprintType)
   struct FMOCharacterRelationship { /*...*/ };

   USTRUCT(BlueprintType)
   struct FMOCharacterHistoryEntry { /*...*/ };
   ```

2. **`UMOPersonalityComponent`** - Character personality traits
   - Three personality dimensions with float values (-1.0 to 1.0)
   - Affects task performance and mood responses
   - Persists via save data

3. **`UMOCharacterHistoryComponent`** - Character history and relationships
   - Event log (capped at 50 entries)
   - Relationship tracking with other characters
   - `GetMoodSummary()`, `GetActivitySummary()` for UI

4. **`UMOColonyManagerSubsystem`** - Colony-level coordination
   - Alert queue with 4 tiers
   - `GetAllColonyCharacters()` - enumerate recruited pawns
   - `AssignTask()` - delegate to `UMOSurvivorJobQueueComponent`

5. **Generic Widget Base Classes** (see Stage 1 of Master Plan)
   - `UMOScrollListBase`, `UMOListEntryBase`, `UMODetailPanelBase`
   - `UMOProgressWidgetBase`, `UMOConfirmationBase`, `UMOColonyPortrait`

## Planned Plugins
- **Ultra Dynamic Sky** - Dynamic sky/atmosphere system
- **Ultra Dynamic Weather** - Weather effects and systems
- **Oceanology** - Ocean/water simulation
- **Voxel Plugin Pro 2.0** - Voxel terrain/world generation

---

## UE DataTable CSV Manipulation

**ALWAYS use `Tools/ue_csv_utils.py` when modifying CSV files!**

See `Tools/UE_CSV_FORMAT.md` for full documentation.

### IMPORTANT: Schema Changes

**When modifying DataTable row structs (e.g., `FMOItemDefinitionRow`, `FMORecipeDefinitionRow`):**

**ADDING new fields to struct:**
```bash
# 1. Add columns to database with defaults
python Tools/ue_csv_utils.py add-column Tools/recipes.db recipes bIsBuilding False

# 2. Export back to CSV
python Tools/ue_csv_utils.py export Tools/recipes.db Plugins/MOFramework/Content/Data/Recipes.csv recipes

# 3. Reimport in UE
```

**REMOVING or RENAMING fields:**
```bash
# 1. Use import-safe to preserve manual data (Icon, UI, WorldVisual)
python Tools/ue_csv_utils.py import-safe Items.csv Tools/items.db items

# 2. Export with new schema
python Tools/ue_csv_utils.py export Tools/items.db Items.csv items
```

**Check for drift:**
```bash
python Tools/ue_csv_utils.py check <csv> <db> [table]
```

**Protected fields (preserved automatically by import-safe):**
- `UI` - IconSmall, IconLarge, Tint
- `WorldVisual` - StaticMesh, MaterialOverride, WorldActorClass
- `Icon` - Recipe icons

**Row struct files to watch:**
- `MOItemDefinitionRow.h` → Items.csv → Tools/items.db
- `MORecipeDefinitionRow.h` → Recipes.csv → Tools/recipes.db
- `MOSkillDefinitionRow.h` → Skills.csv

### Quick Reference

```bash
# Import CSV to SQLite database
python Tools/ue_csv_utils.py import Plugins/MOFramework/Content/Data/Items.csv items.db

# Query items
python Tools/ue_csv_utils.py query items.db "SELECT ItemId, DisplayName FROM items WHERE Rarity='Rare'"

# Update items
python Tools/ue_csv_utils.py update items.db "UPDATE items SET MaxStackSize=50 WHERE ItemType='Resource'"

# Export back to CSV
python Tools/ue_csv_utils.py export items.db Plugins/MOFramework/Content/Data/Items.csv
```

### Critical CSV Rules
1. **Encoding**: Usually `utf-8-sig`, always detect first
2. **Field Size**: Set `csv.field_size_limit(sys.maxsize)`
3. **Quoting**: UE uses `QUOTE_ALL` - every field is quoted
4. **Quote Escaping**: Quotes inside quoted fields are doubled (`""`)
   - Write single quotes in Python → CSV writer doubles them → UE reads as single
   - NEVER manually double quotes or you get `""""` (broken)
5. **Row Name Column**: UE exports the first column (row name / `---`) WITHOUT quotes, but our utility quotes all fields. This can cause crashes when UE reimports.
   - **Workaround**: After modifying data via the utility, make edits in DataTable editor and re-export from UE
   - Complex struct arrays (like `TArray<FMOToolRequirement>`) should be set manually in the DataTable editor, not via CSV

### Struct Array Format (TArray<FStructType>)
- Empty array: `""` (empty string) or `()`
- Single item: `((Field1=Value1,Field2=Value2))`
- Multiple items: `((Field1=A,Field2=B),(Field1=C,Field2=D))`
- All struct fields should be included with full precision floats (e.g., `1.000000`)
- Example working format for `TArray<FMOToolRequirement>`:
  ```
  ((ToolType="Axe",MinQuality=1.000000,DurabilityConsumed=1,bIsRequired=True,MissingToolTimeMultiplier=1.000000,MissingToolQualityMultiplier=1.000000))
  ```

### Inspection Field Format (Current)
```
(Grants=((Id="SkillName",bIsKnowledge=False,XPAmount=5.0,MaxLevel=3),(Id="KnowledgeName",bIsKnowledge=True,XPAmount=100.0,MaxLevel=3)))
```

---

## CLI Commands

### Build Commands (PowerShell)
```powershell
# Build Editor (Development)
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57Editor Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

# Build Game (Development)
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57 Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

# View build logs
powershell.exe -Command "Get-Content 'C:\Users\penum\AppData\Local\UnrealBuildTool\Log.txt' -Tail 50"
```

### File Operations (PowerShell)
```powershell
# List directory contents
powershell.exe -Command "Get-ChildItem 'D:\ueprojects\mo57' -Directory"

# Find files recursively
powershell.exe -Command "Get-ChildItem 'D:\ueprojects\mo57' -Recurse -Filter '*.cpp'"
```

### Git Operations
```bash
git status
git add -A && git commit -m "message" && git push
git log --oneline -10
```

---

## Similar Games Research & Design Lessons

### SCUM (Medical/Metabolism Reference)
**What works well:**
- Granular nutrition (vitamins, minerals, macros) adds depth without overwhelming
- Body composition affects gameplay (fat = cold resistance, muscle = strength)
- Real-time metabolism with time scale multiplier keeps it manageable
- BCU implant provides UI justification for detailed stats

**Community feedback to consider:**
- Many find vitamin/mineral tracking tedious → Consider "good enough" thresholds vs micromanagement
- Bathroom mechanics divisive → Keep optional/toggleable
- Medical system praised but complex → Tiered UI: simple overview vs detailed mode

### Project Zomboid (Pawn Management)
**What works well:**
- Moodles (mood indicators) provide quick status at a glance
- Trait system gives each character personality
- Skill progression through use feels natural
- NPCs in multiplayer add social dynamics

**Community feedback:**
- NPC AI needs better pathfinding and combat → Invest in behavior trees
- Multiplayer desync issues → Authoritative server model (already using)
- Players want more NPC interaction options → Job assignments, relationship building

### RimWorld (Colony/Job System)
**What works well:**
- Work priorities (1-4 scale) simple yet powerful
- Mood system with cascading effects creates emergent stories
- Schedule system (work/sleep/recreation blocks)
- Social relationships affect mood and productivity

**Community feedback:**
- Micro-management can become tedious at scale → Automation/standing orders
- Players love emergent stories from personality clashes
- Medical operations with skill requirements feel meaningful

### Kenshi (Multi-Pawn Adventure)
**What works well:**
- Seamless switching between pawns
- Squads with autonomous behavior
- Each character has individual skills/stats
- Base building + exploration loop

**Community feedback:**
- AI pathing issues in complex terrain → Navigation mesh quality important
- Players want more control over idle behavior
- Equipment management for many pawns gets tedious → Templates/loadouts

### DayZ/Tarkov (Medical Realism)
**What works well:**
- Body part damage zones feel impactful
- Medical items require knowledge to use effectively
- Bleeding/fractures create tension
- Status effects (tremors, limping) provide feedback

**Community feedback:**
- Too punishing without teammates → Solo should be viable
- Inventory tetris divisive → Consider slot-based (already implemented)
- Real-time healing works in survival context

### Design Principles Derived

1. **Tiered Complexity**
   - Simple overview for quick checks
   - Detailed view for interested players
   - Don't force micromanagement

2. **Visual Feedback Over Numbers**
   - Use moodles/icons for quick status
   - Color coding for severity
   - Reserve detailed numbers for inspection

3. **Graceful Degradation**
   - Injuries impair, don't immediately kill
   - Time to react and treat
   - Death should feel preventable in hindsight

4. **Automation at Scale**
   - Single pawn: manual control fine
   - Many pawns: need priorities/schedules/jobs
   - Standing orders for common tasks

5. **Emergent Narrative**
   - Character traits create stories
   - Relationships matter
   - Memorable moments from systems interacting

---

## UE Python Widget Blueprint Automation

**Documentation:** See `Content/Python/README_WIDGET_AUTOMATION.md` for full details.

### Quick Reference

| Operation | Works? | Function |
|-----------|--------|----------|
| Find widget by name | ✅ | `unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, Name)` |
| Add new widget | ✅ | `unreal.EditorUtilityLibrary.add_source_widget(wbp, class, name, parent)` |
| Set IsVariable flag | ❌ | **NOT EXPOSED** - `b_is_variable` not accessible via Python |
| Access widget tree | ❌ | **NOT EXPOSED** - `widget_tree()` returns None |
| Rename widget | ❌ | **NO API** - must delete and recreate |

### Critical Limitation: IsVariable Flag

The "Is Variable" checkbox (required for `BindWidget` meta) is stored in WidgetTree metadata and **cannot be set via Python API**.

**Workarounds:**
1. Manual fix: Right-click widget in hierarchy → "Set as Variable"
2. Use `add_source_widget()` when creating new widgets (may auto-mark as variable - needs testing)
3. Expose a custom C++ editor utility to set `bIsVariable`

### Available Scripts

```bash
# Inspect widget blueprints and check IsVariable status
py "D:/UEProjects/MO57/Content/Python/inspect_widget_blueprints.py"

# Explore available API methods
py "D:/UEProjects/MO57/Content/Python/explore_widget_tree.py"

# Add missing widgets to blueprints
py "D:/UEProjects/MO57/Content/Python/setup_widget_bindings.py"
```

### When BindWidget Fails

If Blueprint compilation fails with "required widget binding not found":
1. Run `inspect_widget_blueprints.py` to check if widget exists and IsVariable status
2. If widget exists but `[NOT VAR]`: Manual fix required (right-click → Set as Variable)
3. If widget missing: Run `setup_widget_bindings.py` or add manually
