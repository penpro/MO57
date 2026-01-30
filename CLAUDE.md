# MOFramework Project Memory

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

### Component Architecture

**Player Controller Components (AMOPlayerController):**
- `UMOUIManagerComponent` - All UI management (menus, dialogs, notifications)
- `UMOPossessionComponent` - Pawn possession state

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

**HIGH - UIManager Orchestration Bottleneck:**
- `MOUIManagerComponent` knows about Persistence, Possession, Inventory subsystems
- Single point of failure for all UI operations
- **Mitigation**: Break into specialized UI controllers per system

**MEDIUM - Monolithic Module Structure:**
- All 60+ classes in single `MOFramework` module
- Cannot use only specific systems
- **Future**: Consider splitting into Core, Interaction, Inventory, Medical, UI submodules

**Portability Score: 6.5/10** - Good fundamentals, needs abstraction layer work

## Planned Plugins
- **Ultra Dynamic Sky** - Dynamic sky/atmosphere system
- **Ultra Dynamic Weather** - Weather effects and systems
- **Oceanology** - Ocean/water simulation
- **Voxel Plugin Pro 2.0** - Voxel terrain/world generation

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
