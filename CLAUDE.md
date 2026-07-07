# MOFramework Project Memory

## Graphify knowledge graph

This project has a graphify knowledge graph at `graphify-out/` (11k nodes,
741 code files; AST-extracted, fully local). Rules:
- For architecture / "what connects to what" questions, try the graph before
  grepping: `python -m graphify query "..."`, `explain "Symbol"`,
  `path "A" "B"`, `affected "X"` (run from repo root).
- After a session that modifies code, run `python -m graphify update .` to
  keep it current (AST-only, no LLM, ~1 min).
- `graphify-out/` is gitignored — regenerable, never commit it.
- The CLI has no PATH shim on this machine: use `python -m graphify`.

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

### 11. The simulation IS the design — every system reflects real-world phenomena

MO57's first design pillar is realism. Every system in the game — timers,
movement, combat, biology, crafting, healing, travel, weather, resource
gathering, AI behavior, social interaction — models the real-world
phenomenon it represents. Convenience shortcuts that bypass the
simulation are a design failure, not a polish step. They make a
different game.

This rule sits above the others because it determines what "correct"
means. The other principles ask "is this code right?". This one asks
"is this game right?" — and the answer drives what's allowed to ship.

**Concrete examples already in this codebase:**
- **Terraforming** runs a 5-second progress timer with
  interrupt-on-movement. Better tools shorten it; the floor is
  non-zero. A swing of the pickaxe is a timed action, not an instant
  click.
- **Wounds** bleed, scab, and scar over real game time via the medical
  cascade. No HP regen — healing is the actual recovery arc with risks
  (infection) and dependencies (clean water, suturing skill).
- **Vitals** (HR, BP, SpO2, glucose, blood volume, temperature) tick
  on real intervals. Cold doesn't just "do damage" — it shifts the
  thermal-comfort moodle, which affects metabolism, which affects
  glucose, which affects mental state.
- **Crafting** consumes materials AND time AND skill. There is no
  "click and it's done."
- **Movement-interruptible actions** register via
  `IMOMovementInterruptibleInterface`. Moving cancels lockpicking,
  surgery, terraforming, fishing — because in reality you can't do
  those things while walking.
- **AI sleep cycles** are real-clock — frozen mobs stay asleep until
  the recheck timer fires, and players can't run up on them in the
  meantime because the recheck distance is meters of travel away.

**Examples to reject, no matter how convenient they sound:**
- "Instant build / instant dig / instant craft" modes for shipped
  builds (cheat console commands for debug are a separate question)
- "HP regen over time" — use the actual healing simulation
- "Fast travel" that teleports between distant points — build roads,
  ride horses, take the journey
- Passive auto-pickup (items walk into inventory as you near them
  without input) — but see the QoL section below for the version
  that's fine
- "Skill use is free" — every skill use costs time, energy, material,
  or all three
- Hidden internal timers the player can't see while waiting — if
  something takes 30 seconds, the player must see it taking 30 seconds
- Stat decay / regen rates that read as game-y rather than biological
  ("hunger decreases 1/sec" is wrong; "metabolism burns X kcal/min
  based on activity level + body composition" is right)

### Tedium is not simulation — eliminate the first, preserve the second

Critical distinction:

- **Simulation friction** is the in-world thing taking real time.
  Cooking a stew takes 20 minutes because that's how long stew takes.
  Walking to the well takes 90 seconds because the well is 90 seconds
  away. This is sacred — see the reject list above.
- **UX tedium** is the player making the same gesture N times for the
  same simulated outcome. Clicking each of 80 berries individually to
  move them into a barrel — the *gesture* is tedious; the *simulation*
  is fine with 80 berries changing hands in N seconds.

Quality-of-life features that batch repeated gestures into one action,
with one combined-duration timer, **respect both principles at once**.
Reference example: Project Zomboid has timers on every individual
pickup / drop / equip — realistic, but a "deposit all matching" action
would massively improve the play experience without breaking realism.
Total time is still ~N × per-item duration; player just made one
gesture instead of 80.

**Good QoL patterns that don't break realism:**
- **"Deposit all matching"** / **"Take all"** — one combined-duration
  timer covering all transferred items. Total time stays realistic;
  player makes one gesture. Slight efficiency bonus is acceptable
  (real handfuls are faster than one-at-a-time).
- **Recipe queuing** — enqueue 5 meals; each cooks in sequence at its
  real duration. Player doesn't click "start next" 5 times.
- **Auto-sort inventory by category** — instant; mental organization
  is fast.
- **Press-button-to-sweep pickup** — timed action: press F, character
  spends 4 seconds picking up everything in 3m. Visible, interruptible,
  totally fine. This is the *good* version of "auto-pickup."
- **Stack / unstack** items — instant; it's a UI shuffle, not a
  world event.
- **Workshop crafting batch** — set a station to "craft 20 arrows";
  the station works for 20 × craft-duration with the player free to
  do other things, just like real fletching.

**The killer feature: delegate tedium to AI pawns.** Tedium that can't
be batched (because each instance has its own simulation cost — chop
12 trees in 12 different places) is delegate-able to recruited
survivors via `UMOSurvivorJobQueueComponent`. The player doesn't skip
the simulation; **the simulation happens without the player.** Hauling,
watering crops, restocking firewood, churning butter, processing hides,
basic crafting, tending livestock — all real-time tasks the pawn AI
does on its own clock while the player is elsewhere. This is the loop
that takes MO57 from "Project Zomboid" to "RimWorld." Every system that
the player can do should be plumbed to be pawn-assignable.

### When a system feels too slow, the right moves are (in order):
1. **Tedium check first** — is the player making repeated gestures
   with no per-gesture choice? Batch them or offer as a pawn job.
2. Better feedback so the wait feels productive — progress bar,
   incremental yield, visible environmental change, audio cues.
3. Tool / skill progression that meaningfully reduces but doesn't
   eliminate the duration.
4. Re-check whether the simulation duration was correct — tune down
   toward the realistic floor if the original value was excessive, but
   never below the floor.
5. Add parallel work the player can do while waiting (drink, eat,
   sharpen a tool, plan the next move).

Never collapse a single simulated action's duration to zero, add a
"creative mode" toggle to shipped builds, or hide the realistic timer
behind an animation that "skips" the wait.

**Rule:** Before adding any "instant" / "auto" / "fast-X" mode, ask
three questions in order:
1. *Does the simulation need tuning?* → Tune it.
2. *Is the player skipping the simulation?* → Refuse.
3. *Is this batching repeated gestures with no per-gesture choice?* →
   Allow it, with a combined-duration timer covering the batched
   work — or expose it as an assignable pawn job. Or both.

---

## Consolidated Documentation

| Document | Purpose | Read When |
|----------|---------|-----------|
| `Docs/PROJECT_STATUS.md` | Metrics, progress, **audit issue tracker** (C/H/M/L codes; June 11 2026 re-audit) | Starting any feature, checking progress, or reviewing known issues |
| `Docs/TECHNICAL_REFERENCE.md` | Architecture patterns, APIs, performance/networking guidelines | Implementing any system — UI, AI, medical, crafting, networking |
| `Docs/MO57_Master_Plan.md` | Detailed stage execution plans | Working on UI refactor or colony management |
| `Docs/UI_Overhaul_Architecture.md` | CommonUI migration details + 15 pitfalls | Any UI widget work |
| `Docs/MobAIPlan.md` | Creature AI behavior tree design | Adding new creature types |
| `Docs/PCG_Integration_Plan.md` | PCG world items architecture | Working on PCG/resource spawning |
| `Docs/Voxel_Plugin_Reference.md` | Voxel Plugin 2.0 reference + MO57 voxel integration + caves/mining playbook | Any voxel-graph, terraforming, or PCG-voxel-sampling work |
| `Docs/Terrain_Foundation_Plan.md` | VHG_Realistic recipe (multi-octave + ridged mountains + beach flattening) | Authoring or tuning base terrain generation |
| `Docs/World_Features_Architecture.md` | Unifying abstraction for caves / rivers / POIs / landmarks / ore / player builds | Before designing any new "kind of thing" that lives in the world |
| `Docs/Archive/` | Superseded docs (historical reference only) | Never — check git history if needed |

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
3. **Total World Mutability** - Dig, mine, build, terraform via Voxel Plugin (open-source dev-phy build)
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
- Voxel Plugin (open-source dev-phy build) for destructible/buildable terrain
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
- Engine: Unreal Engine 5.8 (source build)
- Engine Install Path: `D:\UnrealEngine\UE_5.8`
- Engine User Data: `C:\Users\penum\AppData\Local\UnrealEngine\5.8`
- Build Tool Logs: `C:\Users\penum\AppData\Local\UnrealBuildTool`

### UE 5.8 Migration Notes (upgraded 2026-06-30 from 5.7)
- **Voxel Plugin = open-source dev-phy build** (not Pro 2.0), vendored into `Plugins/Voxel`. The sculpt API is wrapped behind **`MOVoxel`** (`MOVoxelAlias.h/.cpp`) — call through the facade; don't include Voxel headers directly elsewhere.
- **`ACharacter::SetBase` changed signature** in 5.8: `UPrimitiveComponent*` → `FMovementBaseInterfaceData*`. Override the **new** signature (engine-version gated) or the engine silently never calls your override. See `MOCharacter.h`.
- **Subsystem `Initialize()` must not `GetSubsystem()` siblings that don't exist in a cook world** (e.g. UI subsystems). The cook commandlet promotes the resulting ensure to **fatal** and packaging dies — defer cross-subsystem binds to `OnWorldBeginPlay`. See `MOWeatherIntegrationSubsystem`.
- **Build settings: `DefaultBuildSettings = V7`** (both targets).
- Next deprecation to clear before any 5.9 move: `FCoreDelegates::OnPostEngineInit` → `GetOnPostEngineInit()`.

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

## UE5.8 Native Refactoring Roadmap

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

See `Docs/PROJECT_STATUS.md` for full status of all pending work.

**Key items needing Blueprint setup (C++ complete):**
- Creature ABP state machines (Deer locomotion/rest/sleep/death states)
- EQS query assets (EQ_FindHarvestableItems, EQ_FindHarvestTargets, EQ_FindEscapeRoute)
- NewGamePanel WBP (seed input, random button, start button)
- CommonUI layer stacks already configured via WBP_PrimaryGameLayout (Stage 3A COMPLETE)
- Colony management components (MOColonyTypes.h, UMOPersonalityComponent already created in Stage 1)

## Planned Plugins
- **Ultra Dynamic Sky** - Dynamic sky/atmosphere system
- **Ultra Dynamic Weather** - Weather effects and systems
- **Oceanology** - Ocean/water simulation
- **Voxel Plugin (open-source dev-phy build)** - Voxel terrain/world generation

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
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57Editor Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

# Build Game (Development)
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57 Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

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

## Design Principles (Derived from SCUM, Zomboid, RimWorld, Kenshi, DayZ/Tarkov research)

1. **Tiered Complexity** - Simple overview for quick checks, detailed view for interested players
2. **Visual Feedback Over Numbers** - Moodles/icons for status, color coding for severity
3. **Graceful Degradation** - Injuries impair, don't immediately kill; death preventable in hindsight
4. **Automation at Scale** - Manual control for 1 pawn, priorities/schedules/jobs for many
5. **Emergent Narrative** - Character traits, relationships, memorable moments from systems interacting

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
