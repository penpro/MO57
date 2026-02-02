# MOFramework

A comprehensive Unreal Engine 5.7 plugin providing modular gameplay systems for survival, inventory management, crafting, and UI. Built with replication support and designed for extensibility.

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Systems Overview](#systems-overview)
  - [Inventory System](#inventory-system)
  - [Survival Stats System](#survival-stats-system)
  - [UI Manager](#ui-manager)
  - [Item Database](#item-database)
  - [Persistence System](#persistence-system)
  - [Interaction System](#interaction-system)
  - [Skills & Knowledge System](#skills--knowledge-system)
  - [Building System](#building-system)
  - [Crafting System](#crafting-system)
- [Architecture & Delegate Flows](#architecture--delegate-flows)
- [Widget Setup Guide](#widget-setup-guide)
- [Survival Game Design Considerations](#survival-game-design-considerations)
- [Best Practices](#best-practices)
- [UE Documentation Links](#ue-documentation-links)

---

## Features

- **Inventory System** - Slot-based inventory with drag-drop, stacking, and world item dropping
- **Survival Stats** - Health, stamina, hunger, thirst, energy, temperature with nutrition tracking
- **UI Framework** - Inventory menus, context menus, player status HUD, modal dialogs
- **Item Database** - DataTable-driven item definitions with nutrition, inspection, and crafting data
- **Persistence** - Save/load system with GUID-based identity tracking
- **Interaction** - Line-trace based interaction with interactable actors
- **Skills & Knowledge** - XP-based skill progression and item inspection/knowledge unlocks
- **Replication** - Full multiplayer support with FastArray serialization

---

## Requirements

- Unreal Engine 5.7+
- CommonUI Plugin (enabled by default in UE5)
- Enhanced Input Plugin

---

## Installation

1. Copy the `MOFramework` folder to your project's `Plugins/` directory
2. Regenerate project files
3. Enable the plugin in Edit → Plugins → MOFramework
4. Add module dependencies to your game module's `Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "MOFramework" });
```

---

## Quick Start

### 1. Add Components to Your Character

```cpp
// In your character header
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UMOInventoryComponent> InventoryComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UMOSurvivalStatsComponent> SurvivalStatsComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UMOInteractorComponent> InteractorComponent;
```

### 2. Add UI Manager to Player Controller

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UMOUIManagerComponent> UIManagerComponent;
```

### 3. Create Widget Blueprints

Create the following Widget Blueprints based on MOFramework classes:
- `WBP_InventoryMenu` (parent: `UMOInventoryMenu`)
- `WBP_InventorySlot` (parent: `UMOInventorySlot`)
- `WBP_InventoryGrid` (parent: `UMOInventoryGrid`)
- `WBP_ItemInfoPanel` (parent: `UMOItemInfoPanel`)
- `WBP_ItemContextMenu` (parent: `UMOItemContextMenu`)
- `WBP_PlayerStatus` (parent: `UMOPlayerStatusWidget`)

### 4. Configure UI Manager

In your Player Controller Blueprint, set the widget classes on the UIManagerComponent:
- `InventoryMenuClass` → `WBP_InventoryMenu`
- `ItemContextMenuClass` → `WBP_ItemContextMenu`
- `PlayerStatusWidgetClass` → `WBP_PlayerStatus`

### 5. Setup Item DataTable

1. Create a DataTable with row type `FMOItemDefinitionRow`
2. Configure items with properties, nutrition data, icons
3. Set the DataTable in Project Settings → MO Item Database

---

## Systems Overview

### Inventory System

The inventory system uses a slot-based approach with GUID-tracked items.

**Key Classes:**
- `UMOInventoryComponent` - Actor component managing inventory entries and slots
- `UMOInventorySlot` - Widget representing a single inventory slot
- `UMOInventoryGrid` - Widget containing a grid of inventory slots
- `UMOInventoryMenu` - Full inventory UI with grid and info panel

**Features:**
- Drag-drop between slots
- Right-click context menu
- Drop items to world
- Stack splitting (planned)
- Replication via FastArraySerializer

**Usage:**
```cpp
// Add item
InventoryComponent->AddItemByGuid(NewGuid, FName("apple01"), 5);

// Remove item
InventoryComponent->RemoveItemByGuid(ItemGuid, 1);

// Drop to world
InventoryComponent->DropItemByGuid(ItemGuid, Location, Rotation);
```

### Survival Stats System

Tracks player vitals with automatic decay/regeneration.

**Key Classes:**
- `UMOSurvivalStatsComponent` - Manages all survival statistics
- `FMOSurvivalStat` - Individual stat with current/max, regen/decay rates
- `FMONutritionStatus` - Tracks accumulated nutrition levels

**Stats Tracked:**
- Health, Stamina (regenerate)
- Hunger, Thirst (decay over time)
- Energy/Fatigue
- Temperature
- Vitamins (A, B, C, D)
- Minerals (Iron, Calcium, Potassium, Sodium)

**Usage:**
```cpp
// Consume item (applies nutrition, removes from inventory)
SurvivalStats->ConsumeItem(InventoryComponent, ItemGuid);

// Modify stat directly
SurvivalStats->ModifyStat(FName("Health"), -10.0f);

// Check stat status
if (SurvivalStats->IsStatCritical(FName("Hunger")))
{
    // Show warning
}
```

**Delegates:**
- `OnStatChanged` - Fired when any stat value changes
- `OnStatDepleted` - Fired when a stat reaches zero
- `OnStatCritical` - Fired when a stat enters critical threshold
- `OnNutritionApplied` - Fired when food is consumed

### UI Manager

Centralized UI management with modal support.

**Key Classes:**
- `UMOUIManagerComponent` - Manages all UI widgets
- `UMOModalBackground` - Click-outside-to-close support
- `UMOItemContextMenu` - Right-click item actions
- `UMOPlayerStatusWidget` - Survival stats HUD

**Features:**
- Toggle menus with input actions
- Tab/Escape to close menus
- Click outside to close
- Modal background for focus management
- Automatic input mode switching

**Usage:**
```cpp
// Toggle inventory
UIManager->ToggleInventoryMenu();

// Toggle player status HUD
UIManager->TogglePlayerStatus();

// Show context menu
UIManager->ShowItemContextMenu(InventoryComp, ItemGuid, SlotIndex, ScreenPos);
```

### Item Database

DataTable-driven item definitions.

**Key Classes:**
- `UMOItemDatabaseSettings` - Project settings for item DataTable
- `FMOItemDefinitionRow` - DataTable row structure
- `FMOItemNutrition` - Nutrition data for consumables
- `FMOItemInspection` - Knowledge/skill data for inspection

**Item Properties:**
- Core: ID, type, rarity, display name, description
- Stacking: max stack size
- Economy: weight, base value
- Flags: consumable, equippable, quest item, can drop, can trade
- UI: icons (small/large), tint color
- World: mesh, material, physics settings
- Nutrition: calories, water, protein, carbs, fat, vitamins, minerals
- Inspection: skill XP grants, knowledge unlocks

### Persistence System

Save/load with GUID identity preservation.

**Key Classes:**
- `UMOPersistenceSubsystem` - GameInstance subsystem for save/load
- `UMOworldSaveGame` - SaveGame class with world state
- `UMOIdentityRegistrySubsystem` - GUID ↔ Actor mapping

**Features:**
- Multiple save slots
- Actor GUID tracking across save/load
- Inventory state preservation
- World item spawning/despawning

### Interaction System

Line-trace based interaction.

**Key Classes:**
- `UMOInteractorComponent` - Performs interaction traces
- `UMOInteractableComponent` - Marks actors as interactable
- `UMOInteractionSubsystem` - World subsystem managing interactions
- `IMOInteractableInterface` - Interface for custom interaction logic

### Skills & Knowledge System

XP-based progression with item inspection.

**Key Classes:**
- `UMOSkillsComponent` - Tracks skill levels and XP
- `UMOKnowledgeComponent` - Tracks learned knowledge
- `FMOSkillDefinitionRow` - DataTable row for skill definitions
- `FMORecipeDefinitionRow` - DataTable row for crafting recipes

### Building System

Place-and-construct building system with ghost preview and timed construction.

**Key Classes:**
- `UMOBuildingComponent` - On PlayerController, manages placement mode
- `AMOBuildableActor` - Base class for all placeable buildings
- `UMOBuildProgressComponent` - Tracks construction progress
- `UMOBuildingMenu` - Building recipe selection UI
- `UMOBuildWidget` - Ghost interaction/construction start UI

**Build States:**
1. **Ghost** - Placed but construction not started
2. **Constructing** - Active timed construction in progress
3. **Paused** - Construction paused (manual or missing materials)
4. **Complete** - Fully built and functional

**Placement Flow:**
```
Player presses B → Building Menu opens
Select building → Enter placement mode
Ghost follows camera trace → Click to place
Right-click ghost → Build Widget opens
Configure sources + Start → Timed construction
Construction completes → Building functional
```

**Material Sources:**
- Player inventory
- Nearby containers
- Surrounding area (world items)

**Weighted Build Parts:**
Each build part has quantity and weight. Time is distributed proportionally:
```
Part: Stone x20, Weight=1 → 20 weight units
Part: Dig x1, Weight=5 → 5 weight units
Total: 25 units over 60s = 2.4s per unit
Stone phase: 48s, Dig phase: 12s
```

### Crafting System

Station-based crafting with queued production.

**Key Classes:**
- `UMOCraftingSubsystem` - World subsystem for craft execution
- `UMOCraftingQueueComponent` - Manages queued crafts on pawns
- `UMORecipeDiscoveryComponent` - Tracks discovered recipes
- `FMORecipeDefinitionRow` - DataTable row for recipe definitions
- `UMOCraftingMenu` - Crafting UI with recipe list and queue

**Recipe Features:**
- Station requirements (Campfire, Forge, etc.)
- Skill level requirements
- Tool requirements (with durability consumption)
- Knowledge prerequisites
- Discovery system (hidden until learned)
- XP rewards on craft completion

**Crafting Flow:**
```
Open Crafting Menu → Filter by station/category
Select recipe → View requirements in detail panel
Click Craft → Recipe enqueued
Queue processes → Materials consumed over time
Craft completes → Output added to inventory + XP granted
```

---

## Architecture & Delegate Flows

### Component Ownership Hierarchy

```
AMOPlayerController
├── UMOUIManagerComponent      - All UI widgets
├── UMOPossessionComponent     - Pawn possession
├── UMONotificationComponent   - Notifications
└── UMOBuildingComponent       - Building placement

APawn (Character)
├── UMOInventoryComponent      - Inventory system
├── UMOSkillsComponent         - Skill XP/levels
├── UMOKnowledgeComponent      - Learned knowledge
├── UMOCraftingQueueComponent  - Active crafts
├── UMORecipeDiscoveryComponent- Discovered recipes
├── UMOVitalsComponent         - Health/stamina
├── UMOMetabolismComponent     - Hunger/thirst
├── UMOMentalStateComponent    - Mental state
├── UMOAnatomyComponent        - Body parts/injuries
└── UMOInteractorComponent     - Interaction traces

AMOBuildableActor
├── UMOIdentityComponent       - GUID tracking
├── UMOInteractableComponent   - Enables interaction
└── UMOBuildProgressComponent  - Construction progress
```

### UI Delegate Flow Pattern

All menus follow this consistent pattern:

```
[User Action] → [Widget] → OnRequestClose → [UIManager Handler] → Close*()

Example:
User presses Escape
  → UMOInventoryMenu::NativeOnKeyDown()
  → OnRequestClose.Broadcast()
  → UMOUIManagerComponent::HandleInventoryMenuRequestClose()
  → CloseInventoryMenu()
  → RemoveFromParent(), restore input mode
```

### Input Flow

```
Enhanced Input Action Triggered
  → AMOPlayerController::Handle*()
  → If UI action: UIManager->Toggle*Menu()
  → If pawn action: Pawn->IMOControllableInterface::Receive*Input()
  → If building action: BuildingComponent->*()
```

### Building System Delegate Flow

```
UMOBuildingComponent:
  OnPlacementModeEntered(FName RecipeId)
    → Listeners: UI mode indicators
  OnPlacementModeExited(bool bPlaced)
    → Listeners: UI mode indicators, input context
  OnGhostPlaced(AMOBuildableActor* Ghost)
    → Listeners: Persistence system

UMOBuildProgressComponent:
  OnConstructionStarted()
    → Listeners: Audio, visual effects
  OnConstructionProgress(float Progress)
    → Listeners: UI progress bars
  OnPartCompleted(int32 Index, FMOBuildPart Part)
    → Listeners: Audio (placement sounds)
  OnConstructionCompleted()
    → Listeners: AMOBuildableActor::OnConstructionCompleted
  OnMaterialNeeded(FName ItemId, int32 Qty)
    → Listeners: UI notification
```

### Inventory Delegate Flow

```
UMOInventoryComponent:
  OnInventoryChanged()
    → Listeners: UMOInventoryMenu, UMOCraftingMenu
  OnSlotsChanged()
    → Listeners: UMOInventoryGrid (rebuilds slots)
  OnItemAdded(FGuid, FName, int32)
    → Listeners: Notification system
  OnItemRemoved(FGuid, FName, int32)
    → Listeners: UI refresh
```

---

## Widget Setup Guide

### Inventory Slot (`WBP_InventorySlot`)

Required widgets (mark "Is Variable"):
- `SlotButton` (Button) - Main clickable area
- `ItemIconImage` (Image) - Item icon display
- `QuantityText` (TextBlock) - Stack quantity
- `QuantityBox` (Widget) - Container for quantity (hidden when qty ≤ 1)

Optional:
- `SlotBorder` (Border) - Visual feedback for drag/hover
- `DebugItemIdText` (TextBlock) - Debug display

### Item Info Panel (`WBP_ItemInfoPanel`)

All optional (mark "Is Variable"):
- `InfoGrid` (Panel) - Container for all detail widgets
- `PlaceholderText` (TextBlock) - "Click an item for details"
- `ItemNameText`, `ItemTypeText`, `RarityText`
- `DescriptionText`, `ShortDescriptionText`
- `QuantityText`, `MaxStackText`, `WeightText`, `ValueText`
- `FlagsText`, `TagsText`, `PropertiesText`
- `ItemIconImage` (Image)

### Context Menu (`WBP_ItemContextMenu`)

Required (mark "Is Variable"):
- `ButtonContainer` (Panel) - Contains all buttons, used for mouse detection
- `UseButton`, `Drop1Button`, `DropAllButton` (UMOCommonButton)
- `InspectButton`, `SplitStackButton`, `CraftButton` (UMOCommonButton)

### Player Status (`WBP_PlayerStatus`)

All optional (mark "Is Variable"):
- `HealthBar`, `StaminaBar`, `HungerBar`, `ThirstBar`, `EnergyBar` (ProgressBar)
- `HealthText`, `StaminaText`, `HungerText`, `ThirstText`, `EnergyText` (TextBlock)

---

## Best Practices

### Component Setup
- Add components in C++ constructor for reliable initialization
- Use `VisibleAnywhere` + `BlueprintReadOnly` for component UPROPERTY
- Initialize component references in BeginPlay, not constructor

### Replication
- Use `DOREPLIFETIME_CONDITION` with `COND_OwnerOnly` for player-specific data
- Leverage FastArraySerializer for efficient array replication
- Authority checks before modifying replicated state

### UI Development
- Use CommonUI for cross-platform input handling
- Implement `NativeOnKeyDown` for keyboard shortcuts
- Use `SetIsFocusable(true)` for widgets that need input
- Call `SetKeyboardFocus()` in NativeConstruct for modal widgets

### Data-Driven Design
- Use DataTables for item/skill/recipe definitions
- Leverage `TSoftObjectPtr` for asset references (lazy loading)
- Use `meta=(GetOptions="...")` for dropdown support in editor

### Memory Management
- Use `TWeakObjectPtr` for widget references
- Clean up delegates in NativeDestruct
- Remove widgets from parent in EndPlay

---

## UE Documentation Links

### Core Systems
- [Gameplay Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine)
- [Actor Components](https://dev.epicgames.com/documentation/en-us/unreal-engine/components-in-unreal-engine)
- [GameInstance Subsystems](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine)

### UI/UMG
- [UMG UI Designer](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine)
- [CommonUI Plugin](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine)
- [Slate Architecture](https://dev.epicgames.com/documentation/en-us/unreal-engine/slate-architecture)
- [Widget Binding](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-widget-type-reference-for-unreal-engine)

### Input
- [Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine)
- [Input Mapping Context](https://dev.epicgames.com/documentation/en-us/unreal-engine/input-mapping-context-in-unreal-engine)

### Networking
- [Network Replication](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-and-multiplayer-in-unreal-engine)
- [Property Replication](https://dev.epicgames.com/documentation/en-us/unreal-engine/replicated-properties-in-unreal-engine)
- [FastArraySerializer](https://dev.epicgames.com/documentation/en-us/unreal-engine/replicated-subobjects-in-unreal-engine)

### Data Assets
- [DataTables](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-driven-gameplay-elements-in-unreal-engine)
- [Developer Settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/DeveloperSettings/UDeveloperSettings)
- [Soft Object References](https://dev.epicgames.com/documentation/en-us/unreal-engine/asynchronous-asset-loading-in-unreal-engine)

### Save System
- [SaveGame](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine)
- [Serialization](https://dev.epicgames.com/documentation/en-us/unreal-engine/serialization-in-unreal-engine)

### Interaction & Collision
- [Line Traces](https://dev.epicgames.com/documentation/en-us/unreal-engine/traces-with-raycasts-in-unreal-engine)
- [Collision Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-in-unreal-engine)

---

## Survival Game Design Considerations

Based on research of popular survival games (Valheim, Rust, Subnautica, The Forest), here are key design pitfalls to avoid and best practices to follow.

### Common Pitfalls to Avoid

#### 1. Grinding Over Skill
**Problem:** Games that require extensive grinding before players can compete or have fun. New players spending 20 minutes chopping trees just to get started creates frustration.

**Solution:**
- Allow skilled players to contribute quickly
- Provide meaningful early-game tools
- Balance time investment with skill rewards

#### 2. Lack of Unifying Goals
**Problem:** Players asking "what do I do in this game?" with no clear direction beyond basic survival.

**Solution:**
- Implement visible landmarks and objectives (like Valheim's bosses)
- Add narrative purpose (like The Forest's story to find your son)
- Create shared goals in multiplayer that unite players

#### 3. Poor RNG/Reward Systems
**Problem:** Dungeons with "15 enemies and 0 loot" - inconsistent reward loops that feel unfair.

**Solution:**
- Guarantee minimum rewards for effort invested
- Use weighted RNG that improves with bad luck streaks
- Make resource locations somewhat predictable

#### 4. Unintuitive UI/UX
**Problem:** Players needing YouTube tutorials just to understand basic mechanics.

**Solution:**
- Clear, concise labeling in inventory/crafting
- Visual feedback when interacting with items
- Contextual tooltips and tutorials
- Consistent iconography across item types

#### 5. World State During Menus
**Problem:** Getting killed while managing inventory because game doesn't pause.

**Solution:**
- Consider pausing or slowing world in single-player
- Provide quick-access toolbar for emergencies
- Clear visual indicator of vulnerability while in menus

### Best Practices Observed

#### From Subnautica
- Colorful, distinctive environments for navigation
- Linear story gated by crafting progression
- Clear crafting blueprint system

#### From The Forest
- Immersive inventory design (blanket with items)
- Strong AI threats that encourage cooperation
- Narrative purpose beyond pure survival

#### From Valheim
- Boss progression provides clear goals
- Biome-based content gating
- Meaningful building that serves gameplay purposes

### UI/UX Recommendations

1. **Inventory Design**
   - Avoid over-categorization
   - Provide filtering and auto-sort
   - Include quick-use functions for common items
   - Show clear stack counts and weights

2. **Crafting UI**
   - Single window for complex crafting
   - Clear iconography for all resources
   - Show have/need counts for materials
   - Queue system for multiple crafts

3. **Accessibility**
   - Color-blind friendly options
   - Scalable UI elements
   - Customizable key bindings
   - Clear audio cues for important events

### References

- [Current Issues With Survival Games - Game Developer](https://www.gamedeveloper.com/design/current-issues-with-survival-games)
- [Survival Game Design Principles - Game Design Skills](https://gamedesignskills.com/game-design/survival/)
- [Game UI/UX Design - Medium](https://medium.com/@brdelfino.work/ux-and-ui-in-game-design-exploring-hud-inventory-and-menus-5d8c189deb65)
- [Inventory UX Best Practices - The Wingless](https://thewingless.com/index.php/2021/07/26/10-simple-ways-you-can-improve-your-videogame-inventory-screen-game-ui-ux-design-course/)

---

## License

This plugin is part of the MO57 project. See project root for license details.

---

## Contributing

1. Follow Unreal Engine coding standards
2. Use the `MO` prefix for all classes
3. Document public APIs with UFUNCTION/UPROPERTY metadata
4. Test multiplayer scenarios before committing replication changes
