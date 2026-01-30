# MO57

**Ultra-realistic procedural open-world survival game** with fully destructible voxel terrain. Minecraft's freedom meets hardcore realism - no fantasy creatures, grounded physics, and detailed medical/survival simulation.

*Developed with Unreal Engine 5.7*

---

## Table of Contents

1. [Vision & Core Pillars](#vision--core-pillars)
2. [Gameplay Systems](#gameplay-systems)
3. [Architecture Overview](#architecture-overview)
4. [MOFramework Plugin](#moframework-plugin)
5. [UI Implementation Guide](#ui-implementation-guide)
6. [Technical Debt & Known Issues](#technical-debt--known-issues)
7. [Development Setup](#development-setup)
8. [Roadmap](#roadmap)

---

## Vision & Core Pillars

### Core Pillars

| Pillar | Description |
|--------|-------------|
| **Realism First** | All systems rooted in real-world mechanics (medical, crafting, physics) |
| **Emergent Civilization** | Solo primitive survival → multi-pawn settlements → castle cities |
| **Total World Mutability** | Dig, mine, build, terraform via Voxel Plugin Pro 2.0 |
| **Modding Foundation** | Full C++ mod support; base game is a realistic framework others can reskin/extend |

### Multiplayer

- **Steam-based co-op** (Satisfactory-style): Play solo or invite friends to help
- Not MMO - small group collaboration on shared worlds

### World Generation

- Voxel Plugin Pro 2.0 for destructible/buildable terrain
- Finite large flat world with world border
- Procedurally generated biomes, resources, points of interest
- Chunked loading for performance

### Lore

Players are colonists with encoded genetic memory, sent to seed new planets. Knowledge unlocks feel like "remembering" rather than inventing. Sci-fi origins revealed in late-game/DLC content.

---

## Gameplay Systems

### Pawn System

| Feature | Description |
|---------|-------------|
| **Possession** | Player can possess any pawn they control; idle pawns run on AI |
| **Assignments** | Assign pawns to jobs (gather wood, teach, craft) and bind to house + workplace |
| **Relationships** | Pawns have family, loyalty, morale; villages can ally or wage war |
| **AI Autonomy** | Full survival instincts (eat, sleep, flee) with streamlined routines for jobs |
| **Permadeath** | Pawn death is permanent; if last pawn dies, respawn ~5 miles away as new pawn |
| **Population Cap** | Soft cap via resource/survival difficulty, not arbitrary limits |

### Skills & Progression

- **Extensive Skill Trees**: Primitive crafting (knapping, pitch-making) through medieval engineering
- **Learning Methods**:
  - Direct action (slow)
  - Being taught by skilled pawn (2x speed)
  - Schools maintain entire skill tree (prevents decay)
- **Skill Decay**: Unused skills degrade over time unless maintained via schooling
- **Tech Accessibility**: No hard locks; "genetic memory" allows attempts at any tech, but practical prerequisites make skipping difficult

### Medical System

Detailed anatomical simulation with cascading effects:

```
Wounds (bleed) → Vitals (blood volume) → Mental (consciousness)
                      ↓
              Heart/Lung damage → SpO2/BP → Death timers
                      ↓
Metabolism (glucose) → Vitals (blood glucose) → Mental (confusion)
                      ↓
Dehydration → Vitals (+HR, -BP, +Temp) → Performance penalties
```

**Body Parts**: ~55 anatomically correct body parts with individual damage tracking
**Treatments**: Wound care, splinting, surgery with skill requirements
**Status Effects**: Pain, shock, tremors, limping provide feedback

### Crafting System

- **Queue-based crafting** with time-based progression
- **Tool requirements** with durability and quality modifiers
- **Recipe discovery** through experimentation, schematics, or skill unlocks
- **Crafting stations** (forge, workbench, campfire) with fuel requirements

### Inventory System

- **Slot-based inventory** with drag-drop support
- **Item durability** tracking for tools
- **World dropping** - items can be dropped into the world
- **GUID-based persistence** for stable cross-session references

---

## Architecture Overview

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
| `UMOSkillsComponent` | Skill levels and XP | N/A |
| `UMOKnowledgeComponent` | Known recipes/techniques | N/A |
| `UMOCraftingQueueComponent` | Per-pawn crafting queue | Tick |
| `UMORecipeDiscoveryComponent` | Discovered recipes tracking | N/A |

### Interface-Based Decoupling

**IMOControllableInterface** - Pawn control delegation
- Used by: `AMOPlayerController` to send input to any pawn type
- Methods: `RequestMove`, `RequestLook`, `RequestJumpStart/End`, `RequestInteract`

**IMOInteractionInterface** - Interaction system
- Used by: `UMOInteractorComponent` to interact with world objects
- Implementors: Items, doors, containers, NPCs

### DataTable-Driven Design

| Row Type | DataTable | Purpose |
|----------|-----------|---------|
| `FMOItemDefinitionRow` | DT_ItemDefinitions | Items, nutrition, equipment |
| `FMOSkillDefinitionRow` | DT_SkillDefinitions | Skills, XP curves |
| `FMORecipeDefinitionRow` | DT_RecipeDefinitions | Crafting recipes |
| `FMOBodyPartDefinitionRow` | DT_BodyPartDefinitions | ~55 body parts |
| `FMOMedicalTreatmentRow` | DT_MedicalTreatments | Wound treatments |

### Replication Patterns

- **FastArraySerializer** for efficient array replication (inventory, wounds, conditions)
- **GUID-Based Identity** for stable cross-session references
- **Authority-Only State Modification** pattern for all components

---

## MOFramework Plugin

The MOFramework plugin (`Plugins/MOFramework/`) provides all core gameplay systems.

### Module Structure

```
Plugins/MOFramework/
├── Source/MOFramework/
│   ├── Public/           # Headers (60+ classes)
│   │   ├── MO*Component.h
│   │   ├── MO*Subsystem.h
│   │   ├── MO*Widget.h
│   │   └── MO*Interface.h
│   └── Private/          # Implementations
└── Content/
    └── UI/               # Widget Blueprints
```

### Key Design Patterns

**UDeveloperSettings** - Project Settings integration:
```cpp
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Item Database"))
class UMOItemDatabaseSettings : public UDeveloperSettings
{
    UPROPERTY(Config, EditAnywhere)
    FSoftObjectPath ItemDefinitionTable;
};
```

**Save/Load Pattern**:
```cpp
// All components follow this pattern
void BuildSaveData(FMOSaveData& OutSaveData) const;  // Any caller
bool ApplySaveDataAuthority(const FMOSaveData& InSaveData);  // Server only
```

---

## UI Implementation Guide

### Recommended Content Folder Structure

```
Content/
└── UI/
    ├── Common/
    │   ├── WBP_MOCommonButton.uasset
    │   └── WBP_DragVisual.uasset
    ├── HUD/
    │   ├── WBP_PlayerStatus.uasset
    │   ├── WBP_Reticle.uasset
    │   └── WBP_Notification.uasset
    ├── Inventory/
    │   ├── WBP_InventoryMenu.uasset
    │   ├── WBP_InventoryGrid.uasset
    │   ├── WBP_InventorySlot.uasset
    │   ├── WBP_ItemInfoPanel.uasset
    │   └── WBP_ItemContextMenu.uasset
    ├── Crafting/
    │   ├── WBP_CraftingMenu.uasset
    │   ├── WBP_RecipeList.uasset
    │   ├── WBP_RecipeEntry.uasset
    │   ├── WBP_RecipeDetailPanel.uasset
    │   ├── WBP_CraftingQueue.uasset
    │   └── WBP_CraftingQueueEntry.uasset
    ├── Status/
    │   ├── WBP_StatusPanel.uasset
    │   ├── WBP_StatusField.uasset
    │   └── WBP_CharacterInfoEntry.uasset
    ├── Possession/
    │   ├── WBP_PossessionMenu.uasset
    │   └── WBP_PawnEntry.uasset
    └── Menu/
        ├── WBP_InGameMenu.uasset
        ├── WBP_OptionsPanel.uasset
        ├── WBP_SavePanel.uasset
        ├── WBP_LoadPanel.uasset
        └── WBP_SaveSlotEntry.uasset
```

### Widget Reference

#### Legend
- **Required** (`BindWidget`): Widget MUST exist with exact name
- **Optional** (`BindWidgetOptional`): Widget is optional but uses exact name if present

---

### Common Widgets

#### WBP_MOCommonButton
**Parent Class:** `UMOCommonButton` (extends `UCommonButtonBase`)

Base button for all MOFramework UI. Override `UpdateButtonText` event to set your text widget.

**Properties:** `ButtonLabel` (FText)

---

### Inventory Widgets

#### WBP_InventoryMenu
**Parent Class:** `UMOInventoryMenu`

| Widget | Type | Required |
|--------|------|----------|
| `InventoryGrid` | UMOInventoryGrid | **Required** |
| `ItemInfoPanel` | UMOItemInfoPanel | **Required** |

#### WBP_InventoryGrid
**Parent Class:** `UMOInventoryGrid`

| Widget | Type | Required |
|--------|------|----------|
| `SlotsUniformGrid` | UUniformGridPanel | **Required** |

**Config:** `SlotWidgetClass`, `Columns`, `MinimumVisibleSlotCount`

#### WBP_InventorySlot
**Parent Class:** `UMOInventorySlot`

| Widget | Type | Required |
|--------|------|----------|
| `SlotButton` | UButton | **Required** |
| `ItemIconImage` | UImage | Optional |
| `QuantityText` | UTextBlock | Optional |
| `QuantityBox` | UWidget | Optional |
| `SlotBorder` | UBorder | Optional |

#### WBP_ItemContextMenu
**Parent Class:** `UMOItemContextMenu`

| Widget | Type | Required |
|--------|------|----------|
| `ButtonContainer` | UPanelWidget | **Required** |
| `UseButton` | UMOCommonButton | **Required** |
| `Drop1Button` | UMOCommonButton | **Required** |
| `DropAllButton` | UMOCommonButton | **Required** |
| `InspectButton` | UMOCommonButton | **Required** |
| `SplitStackButton` | UMOCommonButton | **Required** |
| `CraftButton` | UMOCommonButton | **Required** |

---

### Crafting Widgets

#### WBP_CraftingMenu
**Parent Class:** `UMOCraftingMenu`

| Widget | Type | Required |
|--------|------|----------|
| `RecipeList` | UMORecipeListWidget | **Required** |
| `DetailPanel` | UMORecipeDetailPanel | **Required** |
| `QueueWidget` | UMOCraftingQueueWidget | Optional |
| `CloseButton` | UMOCommonButton | Optional |

#### WBP_RecipeList
**Parent Class:** `UMORecipeListWidget`

| Widget | Type | Required |
|--------|------|----------|
| `RecipeScrollBox` | UScrollBox | Optional |
| `RecipeContainer` | UVerticalBox | Optional |

**Config:** `RecipeEntryWidgetClass`

#### WBP_RecipeEntry
**Parent Class:** `UMORecipeEntryWidget`

| Widget | Type | Required |
|--------|------|----------|
| `EntryButton` | UButton | Optional |
| `RecipeNameText` | UTextBlock | Optional |
| `RecipeIcon` | UImage | Optional |
| `BackgroundBorder` | UBorder | Optional |

#### WBP_RecipeDetailPanel
**Parent Class:** `UMORecipeDetailPanel`

| Widget | Type | Required |
|--------|------|----------|
| `RecipeNameText` | UTextBlock | Optional |
| `RecipeDescriptionText` | UTextBlock | Optional |
| `RecipeIcon` | UImage | Optional |
| `IngredientsContainer` | UVerticalBox | Optional |
| `OutputsContainer` | UVerticalBox | Optional |
| `SkillRequirementText` | UTextBlock | Optional |
| `CraftTimeText` | UTextBlock | Optional |
| `CraftButton` | UMOCommonButton | Optional |
| `CraftAmountSpinBox` | USpinBox | Optional |

#### WBP_CraftingQueue
**Parent Class:** `UMOCraftingQueueWidget`

| Widget | Type | Required |
|--------|------|----------|
| `QueueScrollBox` | UScrollBox | Optional |
| `CurrentCraftNameText` | UTextBlock | Optional |
| `CurrentProgressBar` | UProgressBar | Optional |
| `TimeRemainingText` | UTextBlock | Optional |
| `CancelAllButton` | UMOCommonButton | Optional |

**Config:** `QueueEntryWidgetClass`, `ProgressUpdateInterval`

#### WBP_CraftingQueueEntry
**Parent Class:** `UMOCraftingQueueEntryWidget`

| Widget | Type | Required |
|--------|------|----------|
| `RecipeNameText` | UTextBlock | Optional |
| `RecipeIcon` | UImage | Optional |
| `CountText` | UTextBlock | Optional |
| `ProgressBar` | UProgressBar | Optional |
| `TimeRemainingText` | UTextBlock | Optional |
| `CancelButton` | UMOCommonButton | Optional |

---

### Status Widgets

#### WBP_StatusPanel
**Parent Class:** `UMOStatusPanel`

| Widget | Type | Required |
|--------|------|----------|
| `CategorySwitcher` | UWidgetSwitcher | **Required** |
| `InfoScrollBox` | UScrollBox | **Required** |
| `VitalsScrollBox` | UScrollBox | **Required** |
| `NutritionScrollBox` | UScrollBox | **Required** |
| `NutrientsScrollBox` | UScrollBox | **Required** |
| `FitnessScrollBox` | UScrollBox | **Required** |
| `MentalScrollBox` | UScrollBox | **Required** |
| `WoundsScrollBox` | UScrollBox | **Required** |
| `ConditionsScrollBox` | UScrollBox | **Required** |
| Tab buttons (8x) | UMOCommonButton | Optional |

**Config:** `StatusFieldClass`, `CharacterInfoEntryClass`, `FieldConfigs`

#### WBP_StatusField
**Parent Class:** `UMOStatusField`

| Widget | Type | Required |
|--------|------|----------|
| `TitleText` | UTextBlock | Optional |
| `ValueText` | UTextBlock | Optional |
| `ValueBar` | UProgressBar | Optional |
| `IconImage` | UImage | Optional |

---

### Possession Widgets

#### WBP_PossessionMenu
**Parent Class:** `UMOPossessionMenu`

| Widget | Type | Required |
|--------|------|----------|
| `PawnListScrollBox` | UScrollBox | **Required** |
| `CreateCharacterButton` | UMOCommonButton | Optional |
| `CloseButton` | UMOCommonButton | Optional |

**Config:** `PawnEntryWidgetClass`

#### WBP_PawnEntry
**Parent Class:** `UMOPawnEntryWidget`

| Widget | Type | Required |
|--------|------|----------|
| `NameText` | UTextBlock | Optional |
| `AgeText` | UTextBlock | Optional |
| `HealthBar` | UProgressBar | Optional |
| `PossessButton` | UMOCommonButton | Optional |

---

### Menu Widgets

#### WBP_InGameMenu
**Parent Class:** `UMOInGameMenu`

| Widget | Type | Required |
|--------|------|----------|
| `ButtonsBox` | UPanelWidget | **Required** |
| `OptionsButton` | UMOCommonButton | **Required** |
| `SaveButton` | UMOCommonButton | **Required** |
| `LoadButton` | UMOCommonButton | **Required** |
| `ExitToMainMenuButton` | UMOCommonButton | **Required** |
| `ExitGameButton` | UMOCommonButton | **Required** |
| `FocusWindowSwitcher` | UWidgetSwitcher | **Required** |

**Switcher Indices:** 0=None, 1=Options, 2=Save, 3=Load

#### WBP_SavePanel / WBP_LoadPanel
**Parent Class:** `UMOSavePanel` / `UMOLoadPanel`

| Widget | Type | Required |
|--------|------|----------|
| `SaveSlotsScrollBox` | UScrollBox | **Required** |
| `NewSaveButton` | UMOCommonButton | **Required** (Save only) |
| `BackButton` | UMOCommonButton | **Required** |

**Config:** `SaveSlotEntryClass`

---

### Creating Widget Blueprints

1. **Create the Blueprint**: Right-click > User Interface > Widget Blueprint, select parent class
2. **Add Required Widgets**: Use exact names from tables above, mark as "Is Variable"
3. **Set Class References**: In Class Defaults, set `*WidgetClass` properties
4. **Configure Properties**: Set colors, thresholds in Class Defaults

---

## Technical Debt & Known Issues

### Code Audit Summary

The MOFramework has solid core systems but has accumulated technical debt in several areas:

### Critical Issues

| Issue | Location | Impact |
|-------|----------|--------|
| Debug logging in production | `MOInventoryComponent.cpp:868-909` | Performance, log spam |
| Subsystem direct coupling | `MOInteractorComponent.cpp:179` | Architecture violation |

### High Priority

| Issue | Location | Impact |
|-------|----------|--------|
| MOUIManagerComponent god object | `MOUIManagerComponent.cpp` | 7+ UI responsibilities |
| Duplicate viewpoint resolution | 3 files | Code duplication |
| Poor save/load error handling | `MOPersistenceSubsystem.cpp` | Silent failures |
| Missing server validation | `MOCraftingQueueComponent.cpp` | Security |

### Known Coupling Issues

**CRITICAL - Persistence↔Inventory Circular Dependency:**
- `MOInventoryComponent.DropItemByGuid()` calls `MOPersistenceSubsystem.IsGuidDestroyed()`
- **Mitigation**: Consider `IMOPersistenceProvider` interface

**HIGH - UIManager Orchestration Bottleneck:**
- `MOUIManagerComponent` knows about Persistence, Possession, Inventory subsystems
- **Mitigation**: Break into specialized UI controllers per system

**MEDIUM - Monolithic Module Structure:**
- All 60+ classes in single `MOFramework` module
- **Future**: Split into Core, Interaction, Inventory, Medical, UI submodules

### Tracked TODOs

*Last updated: 2026-01-30*

#### Medical System
- [ ] `MOAnatomyComponent.cpp:257` - Get treatment definition from DataTable and apply effects
- [ ] `MOAnatomyComponent.cpp:737` - Add setter in VitalsComponent for external access
- [ ] `MOAnatomyComponent.cpp:823` - Get condition definition from DataTable
- [ ] `MOAnatomyComponent.cpp:968` - Implement DataTable lookup via MOMedicalSubsystem
- [ ] `MOAnatomyComponent.cpp:975` - Implement DataTable lookup via MOMedicalSubsystem
- [ ] `MOVitalsComponent.cpp:557` - Integrate lung damage from anatomy component

#### Crafting System
- [ ] `MOCraftingQueueComponent.cpp:356` - Store active station GUID when AMOCraftingStationActor is implemented (Phase 3)
- [ ] `MOCraftingQueueComponent.cpp:655` - Apply tool quality modifiers when tool system is integrated

#### UI System
- [ ] `MOUIManagerComponent.cpp:1114` - Implement inspection (show detailed item info, grant knowledge XP)
- [ ] `MOUIManagerComponent.cpp:1119` - Implement stack splitting UI
- [ ] `MOUIManagerComponent.cpp:1124` - Implement crafting UI filtered to this item
- [ ] `MOStatusPanel.cpp:190` - Expose threshold setters on UMOStatusField
- [ ] `MOStatusPanel.cpp:439` - Add visual selected state to buttons
- [ ] `MOStatusPanel.cpp:971` - Show an input dialog to change the value
- [ ] `MOStatusPanel.cpp:980` - Show name change dialog

#### Save/Load System
- [ ] `MOSaveSlotEntry.cpp:72` - Load screenshot thumbnail from ScreenshotPath if available
- [ ] `MOSavePanel.cpp:104` - Load actual metadata from save file

### Portability Score: 6.5/10

Good fundamentals with interface-based decoupling, but needs abstraction layer work before scaling to complex multiplayer scenarios.

---

## Development Setup

### Environment

- **IDE**: Rider for C++
- **Engine**: Unreal Engine 5.7
- **Engine Path**: `D:\UnrealEngine\UE_5.7`

### Build Commands (PowerShell)

```powershell
# Build Editor (Development)
& 'D:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57Editor Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'

# Build Game (Development)
& 'D:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57 Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'
```

### Workflow Rules

1. **Before compiling**: Close Unreal Editor (Live Coding blocks CLI builds)
2. **After successful compile**: `git add -A && git commit -m "checkpoint" && git push`

### Code Conventions

- Always call `RemoveAll(this)` before binding delegates in `NativeConstruct()`
- UI widgets use CommonUI (`UCommonActivatableWidget`, `UCommonButtonBase`)
- Use Warning log level for important flow events, Log for routine
- **Input action handling always in C++** - All handlers go in `AMOPlayerController::SetupInputComponent()`

### Planned Plugins

- **Ultra Dynamic Sky** - Dynamic sky/atmosphere
- **Ultra Dynamic Weather** - Weather effects
- **Oceanology** - Ocean/water simulation
- **Voxel Plugin Pro 2.0** - Voxel terrain/world generation

---

## Data Import & Modding

### CSV-Based Content Definition

Items and recipes are defined in CSV files for easy bulk editing and modding support.

**File Locations:**
- `Content/Data/Items.csv` - Item definitions
- `Content/Data/Recipes.csv` - Recipe definitions

### CSV Formats

#### Items.csv
```csv
RowName,DisplayName,ItemType,Rarity,MaxStackSize,Weight,bConsumable,bIsTool,ToolType,ToolQuality,MaxDurability,Calories,Water,Protein,Carbs,Fat,Fiber,Tags,Description
stone,Stone,Material,Common,50,1.0,false,false,,0,0,0,0,0,0,0,0,Material|Primitive|Stone,A common stone.
flint_knife,Flint Knife,Tool,Common,1,0.3,false,true,Knife,0.6,40,0,0,0,0,0,0,Tool|Primitive|Knife,Sharp flint blade.
```

| Field | Values |
|-------|--------|
| ItemType | None, Consumable, Material, Tool, Weapon, Ammo, Armor, KeyItem, Quest, Currency, Misc |
| Rarity | Common, Uncommon, Rare, Epic, Legendary |
| Tags | Pipe-separated: `Food\|Raw\|Meat` |

#### Recipes.csv
```csv
RowName,DisplayName,CraftTime,Station,SkillId,SkillLevel,SkillXP,Category,bRequiresDiscovery,Ingredients,Outputs,Tools,Description
cook_meat,Cook Meat,60.0,Campfire,Cooking,0,20,Food,false,raw_meat:1,cooked_meat:1,,Roast meat over fire.
craft_stone_axe,Stone Axe,25.0,None,Woodworking,2,35,Tools,false,hand_axe:1|stick:2|cordage:2,stone_axe:1,,Haft a stone head.
```

| Field | Format |
|-------|--------|
| Station | None, Campfire, Workbench, Forge, Alchemy, Kitchen, Loom |
| Ingredients | `itemId:quantity\|itemId:quantity` |
| Outputs | `itemId:quantity\|itemId:quantity:chance` (chance optional, default 1.0) |
| Tools | `toolType:minQuality:durability` (quality/durability optional) |

### Import Commands

**From Editor (Blueprint):**
```cpp
UMODataImportCommandlet::ImportItemsFromCSV("Data/Items.csv", false);
UMODataImportCommandlet::ImportRecipesFromCSV("Data/Recipes.csv", false);
UMODataImportCommandlet::ImportAllFromDirectory("Data/", false);
```

**From Command Line:**
```powershell
UE5Editor.exe MO57 -run=MODataImport -items=Content/Data/Items.csv -recipes=Content/Data/Recipes.csv
UE5Editor.exe MO57 -run=MODataImport -dir=Content/Data
```

**Export (for templates):**
```cpp
UMODataImportCommandlet::ExportItemsToCSV("Content/Data/Items_Export.csv");
UMODataImportCommandlet::ExportRecipesToCSV("Content/Data/Recipes_Export.csv");
```

### Modding Workflow

1. **Create mod folder**: `Content/Mods/MyMod/Data/`
2. **Copy template CSVs** or export existing data
3. **Edit CSVs** in spreadsheet software (Excel, Google Sheets, LibreOffice)
4. **Import** using commandlet or Editor Utility Blueprint
5. **Save DataTables** in Editor

### Tips for Modders

- Row names must be unique across all imports
- Use descriptive prefixes for mod content: `mymod_iron_sword`
- Comments start with `#` or `//`
- Empty rows are skipped
- Fields with quotes can contain commas: `"A, B, C"`

---

## Roadmap

### Development Phases

| Phase | Focus | Status |
|-------|-------|--------|
| **MVP** | Solo survival loop - single pawn, primitive survival, core medical/crafting | In Progress |
| **Pawn Discovery** | Find survivors after ~100mi² exploration, simple automated tasks | Planned |
| **Full Pawn AI** | Autonomous survival behavior, job systems, relationships | Planned |
| **Civilization Building** | Housing, workplaces, teaching, population growth | Planned |
| **Multiplayer Polish** | Steam integration, world sharing | Planned |
| **DLC Pipeline** | Medieval → Industrial → Modern → Sci-fi expansion | Future |

### Design Principles

1. **Tiered Complexity** - Simple overview for quick checks, detailed view for interested players
2. **Visual Feedback Over Numbers** - Use moodles/icons for quick status, reserve numbers for inspection
3. **Graceful Degradation** - Injuries impair, don't immediately kill; time to react and treat
4. **Automation at Scale** - Single pawn: manual control; many pawns: priorities/schedules/jobs
5. **Emergent Narrative** - Character traits create stories, relationships matter

---

## License

[License information here]

## Contributing

[Contributing guidelines here]
