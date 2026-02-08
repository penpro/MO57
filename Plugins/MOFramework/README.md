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
  - [Terraforming System](#terraforming-system)
  - [PCG Integration](#pcg-integration)
  - [Water System](#water-system)
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
- `UMOBuildProgressComponent` - Tracks construction progress with material deposits
- `UMOBuildingMenu` - Building recipe selection UI
- `UMOGhostContextMenu` - Simple context menu for ghost interaction (deposit materials, start build, cancel)

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
Interact with ghost → Ghost Context Menu opens
Select material sources (checkboxes) → Click "Add" to deposit
Materials deposited progressively → "Add" becomes "Build"
Click "Build" → Timed construction begins
Construction completes → Building functional
```

**Ghost Context Menu Features:**
- Checkboxes for material sources (inventory, containers, surrounding area)
- Material list showing "Material Name X/Y" format (deposited/required)
- "Add" button gathers materials from selected sources
- "Build" button starts construction (available when all materials deposited)
- "Cancel" button destroys ghost and drops deposited materials
- 10% build time penalty for interrupting construction

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

### Terraforming System

Runtime terrain sculpting integrated with save/load using the Voxel plugin.

**Key Classes:**
- `UMOTerraformingComponent` - On PlayerController, manages terraform tool state
- `MOPersistenceSubsystem` - Captures/restores Voxel sculpt data per actor

**Features:**
- Toggle terraforming mode (T key)
- Cycle between Dig/Raise/Flatten/Smooth tools (R key)
- Primary action applies terrain modification
- Sculpt data persisted per VoxelHeightActor with GUID tracking

**Voxel Integration Pattern:**
```cpp
// Capture sculpt data synchronously (required for Voxel plugin async API)
FVoxelHeightSculptSave SculptSave;
Voxel::ExecuteSynchronously([&]
{
    return UVoxelHeightSculptBlueprintLibrary::K2_GetSave(SculptSave, HeightActor, true);
});
```

**Known Limitation:** The Voxel plugin's `GetSave()` async API causes assertion failures on unsculpted actors. Use `K2_GetSave` with `Voxel::ExecuteSynchronously` instead.

### PCG Integration

Procedural Content Generation for ground spawns with custom MOFramework nodes.

**Key Classes:**
- `UMOPCGItemSpawnerSettings` - Custom PCG node for item-driven spawning
- `FMOPCGItemSpawnerElement` - PCG element executing the spawner logic
- `UMOPCGInteractionSubsystem` - Manages PCG-spawned item interactions
- `UMOHISMInteractableComponent` - Enables interaction with HISM instances

**MO Item Spawner Node:**
A custom PCG node that assigns item metadata to points:
- Takes weighted item entries referencing datatable rows
- Reads mesh from item's `WorldVisual.StaticMesh`
- Outputs points with `MOItemId`, `MOQuantityMin`, `MOQuantityMax`, `StaticMesh` attributes
- Feeds into Static Mesh Spawner for HISM creation

**PCG Workflow:**
```
Surface Sampler → MO Item Spawner → Static Mesh Spawner → HISM
                       ↓
              Item DataTable lookup
              (mesh, quantity, ID)
```

**HISM Interaction:**
Players can interact with PCG-spawned items (rocks, sticks) via hit detection on HISM instances. The interaction subsystem tracks which HISM instance index was hit and provides item metadata.

### Water System

Gerstner wave-based water simulation with ocean and lake actors, plus buoyancy physics.

**Key Classes:**
- `AMOWaterActorBase` - Base class with Gerstner wave math, procedural mesh, surface queries
- `AMOInfiniteOceanActor` - Infinite ocean at Z=0, follows camera, large waves
- `AMOLakeActor` - Bounded water body, optional voxel detection, elliptical shapes
- `UMOBuoyancyComponent` - Applies buoyancy forces, supports multi-point for boats
- `UMOWaterMaterialGenerator` - Editor utility to generate water materials

**Gerstner Waves:**
Realistic wave displacement with horizontal and vertical motion:
```cpp
// Per-wave displacement
Displacement.XY = Direction * Steepness * Amplitude * cos(Phase);
Displacement.Z  = Amplitude * sin(Phase);

// Phase calculation
Phase = dot(Direction, Position.XY) * Frequency + Time * Speed + PhaseOffset;
```

**Wave Configuration:**
```cpp
FMOGerstnerWave Wave;
Wave.Direction = FVector2D(1.0f, 0.3f);  // Wave travel direction
Wave.Amplitude = 50.0f;                   // Height from rest to peak (cm)
Wave.Wavelength = 400.0f;                 // Distance between peaks (cm)
Wave.Steepness = 0.5f;                    // Sharpness (0-1)
Wave.Speed = 1.0f;                        // Animation speed multiplier
Wave.PhaseOffset = 0.0f;                  // Phase offset for variety
```

**Ocean Actor Setup:**
```cpp
// Infinite ocean at sea level
AMOInfiniteOceanActor* Ocean = World->SpawnActor<AMOInfiniteOceanActor>();
Ocean->OceanLevel = 0.0f;           // Z height
Ocean->OceanExtent = 50000.0f;      // Visible range (500m)
Ocean->bFollowCamera = true;         // Mesh follows player
Ocean->SnapGridSize = 1000.0f;       // Update frequency
```

**Lake Actor Setup:**
```cpp
// Bounded lake
AMOLakeActor* Lake = World->SpawnActor<AMOLakeActor>();
Lake->LakeSizeX = 2000.0f;          // Width
Lake->LakeSizeY = 3000.0f;          // Length
Lake->bEllipticalShape = true;       // Oval shape
Lake->EdgeFalloffDistance = 200.0f;  // Wave dampening near shore

// Auto-detect bounds from voxel terrain
Lake->bAutoDetectFromVoxel = true;
Lake->DetectBoundsFromVoxel();
```

**Surface Queries (for gameplay):**
```cpp
// Get water height at any location
float Height = WaterActor->GetWaterHeightAtLocation(WorldPos);

// Check if underwater
bool bUnderwater = WaterActor->IsUnderwater(WorldPos);

// Get full surface info
FMOWaterSurfaceInfo Info = WaterActor->GetWaterSurfaceInfo(WorldPos);
// Info.SurfaceHeight, Info.SurfaceNormal, Info.bIsInWaterBounds
```

**Buoyancy Component:**
```cpp
// Add to any physics-enabled actor
UMOBuoyancyComponent* Buoyancy = Actor->CreateDefaultSubobject<UMOBuoyancyComponent>();
Buoyancy->BuoyancyForce = 2000.0f;   // Upward force when submerged
Buoyancy->WaterLinearDrag = 3.0f;     // Movement resistance
Buoyancy->WaterAngularDrag = 1.0f;    // Rotation resistance

// Multi-point for boats
Buoyancy->bUseMultiplePoints = true;
Buoyancy->BuoyancyPoints.Add(FMOBuoyancyPoint{FVector(100, -50, 0), 1.0f, 1.0f});
Buoyancy->BuoyancyPoints.Add(FMOBuoyancyPoint{FVector(100, 50, 0), 1.0f, 1.0f});
// ... etc for all pontoon positions
```

**Material Generation (Editor Console):**
```
MO.GenerateWaterMaterial   // Creates /MOFramework/Materials/M_Water
MO.GenerateOceanMaterial   // Creates /MOFramework/Materials/M_Ocean
```

---

#### Future: UE5 Built-in Water Integration

**Recommendation:** Use UE5's built-in Water plugin for rendering, with custom voxel detection for lake boundaries.

**UE5 Water Plugin Key Classes:**
- `AWaterBodyOcean` - Infinite ocean (no spline needed)
- `AWaterBodyLake` - Spline-defined lake boundaries
- `AWaterBodyRiver` - Spline-based river
- `UWaterBodyComponent` - Core water functionality
- `UWaterSplineComponent` - Defines water body shape (can be modified at runtime)

**Programmatic Spline Control:**
```cpp
// Get the water spline
AWaterBodyLake* Lake = ...;
UWaterSplineComponent* Spline = Lake->GetWaterSpline();

// Clear and rebuild spline from detected voxel boundaries
Spline->ClearSplinePoints();
for (const FVector& Point : DetectedBoundaryPoints)
{
    Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World);
}
Spline->UpdateSpline();
Lake->OnWaterBodyChanged(); // Rebuild mesh
```

**Voxel Detection → Spline Generation Pattern:**
1. Raycast down from grid to find terrain
2. Flood-fill from lake center to find water-filled area
3. Edge-detect to find boundary points
4. Convert boundary to ordered spline points
5. Update `UWaterSplineComponent`

**Key UE5 Water Interfaces:**
- `IWaterBrushActorInterface` - For actors that modify water
- `AWaterBodyExclusionVolume` - Cuts holes in water
- `AWaterBodyIsland` - Creates islands within water

**Oceanology Plugin Reference (D:\UnrealEngine\UE_5.5):**
- Uses Material Parameter Collection for wave sync
- Gerstner formula: `k = (PI * 4.0) / WaveLength` (not 2*PI)
- Direction as angle (0-1 range): `sin(Dir * 2*PI), cos(Dir * 2*PI)`
- Quad-tree mesh with LOD for performance
- Single Layer Water shading model for efficient rendering

**Key Oceanology Files (for reference):**
```
Source/Oceanology_Plugin/Private/Components/Wave/OceanologyGerstnerWaveSolverComponent.cpp
Source/Oceanology_Plugin/Public/Structs/OceanologyWaves.h
Source/Oceanology_Plugin/Public/Components/QuadTree/OceanologyWaterMeshComponent.h
```

**Gerstner Wave Math (Oceanology pattern):**
```cpp
double DivideWave(double WaveLength) { return (PI * 4.0) / WaveLength; }
double MultiplyWave(double Steepness) { return (PI * 2.0) * Steepness; }

FVector DirectionWave(double Direction) {
    double Angle = Direction * (PI * 2.0);
    return FVector(FMath::Sin(Angle), FMath::Cos(Angle), 0);
}

FVector GerstnerWave(FVector Position, double Direction, double Speed,
                     double WaveLength, double Amplitude, double Steepness, double NumWaves)
{
    double k = DivideWave(WaveLength);  // Wave number
    double Q = MultiplyWave(Steepness) / (NumWaves * (PI * 2.0) * k * Amplitude);
    FVector Dir = DirectionWave(Direction);

    double Phase = FVector::DotProduct(k * Dir, Position) + GameTimeInSeconds * k * Speed;

    FVector Result;
    Result.X = Q * Amplitude * Dir.X * FMath::Cos(Phase);
    Result.Y = Q * Amplitude * Dir.Y * FMath::Cos(Phase);
    Result.Z = Amplitude * FMath::Sin(Phase);
    return Result;
}
```

---

### Medical System

Comprehensive medical simulation with anatomy, vitals, metabolism, and mental state tracking.

**Key Classes:**
- `UMOVitalsComponent` - Vital signs, exertion, stamina, activity tracking
- `UMOMetabolismComponent` - Nutrition, digestion, calorie expenditure, body composition
- `UMOAnatomyComponent` - Body parts, wounds, conditions, healing
- `UMOMentalStateComponent` - Consciousness, shock, stress, visual effects
- `UMOMedicalSubsystem` - DataTable lookups for body parts, wounds, treatments

**Components on Pawn:**
```
APawn (Character)
├── UMOVitalsComponent      - Blood, heart rate, BP, SpO2, temperature, glucose
├── UMOMetabolismComponent  - BMR, TDEE, digestion, glycogen, body fat
├── UMOAnatomyComponent     - 55+ body parts, wounds, conditions
└── UMOMentalStateComponent - Consciousness level, shock, visual impairment
```

**Activity Level System:**

Activities drive calorie burn, stamina drain, and fatigue accumulation:

| Activity | Calorie Multiplier | Stamina Drain | Use Case |
|----------|-------------------|---------------|----------|
| Resting | 0.9x BMR | Recovery | Sleep |
| Idle | 1.0x BMR | Recovery | Standing |
| Walking | 3.0x BMR | None | Normal movement |
| Jogging | 7.5x BMR | 2/sec | Running |
| Sprinting | 14x BMR | 10/sec | Sprint |
| LightWork | 2.5x BMR | 0.5/sec | Weaving, carving |
| MediumWork | 5x BMR | 1.5/sec | Hammering, cooking |
| HeavyWork | 9x BMR | 4/sec | Mining, forging |
| Combat | 12x BMR | 8/sec | Fighting |

**Activity Integration:**
```cpp
// In your Character movement code:
void AMyCharacter::UpdateMovementActivity()
{
    if (VitalsComponent)
    {
        float Speed = GetVelocity().Size();
        if (bIsSprinting && Speed > SprintThreshold)
            VitalsComponent->SetActivityLevel(EMOActivityLevel::Sprinting);
        else if (Speed > JogThreshold)
            VitalsComponent->SetActivityLevel(EMOActivityLevel::Jogging);
        else if (Speed > WalkThreshold)
            VitalsComponent->SetActivityLevel(EMOActivityLevel::Walking);
        else
            VitalsComponent->SetActivityLevel(EMOActivityLevel::Idle);
    }
}
```

**Stamina System:**
- Stamina drains during intense activities (sprinting, combat, heavy work)
- Recovers during rest/walking (faster with better cardiovascular fitness)
- Depleted stamina forces activity downgrade (sprint → jog → walk)
- Max stamina scales with cardiovascular fitness (70-130 range)

**Delegates:**
```
UMOVitalsComponent:
  OnActivityChanged(OldActivity, NewActivity)
    → Listeners: UI activity indicator, animation system
  OnStaminaChanged(OldValue, NewValue)
    → Listeners: Stamina bar UI
  OnStaminaDepleted()
    → Listeners: Movement system (force downgrade)
  OnVitalSignChanged(Name, OldValue, NewValue)
    → Listeners: Health UI, warning systems
```

**Body Part Hierarchy:**
- 55+ distinct body parts including fingers/toes
- Vital organs (brain, heart, lungs) = instant/rapid death if destroyed
- Limbs can be wounded, destroyed, or amputated
- Wound types: laceration, puncture, blunt, burns, fracture, dislocation

**Medical Conditions:**
- Infection (local → sepsis progression)
- Blood loss stages (Class 1-3)
- Shock (hypovolemic/traumatic)
- Temperature disorders (hypothermia/hyperthermia)
- Nutrient deficiencies (scurvy, anemia, etc.)

### Combat-Medical Integration

The framework provides bridge types for integrating a combat system with the medical system.

**Key Classes:**
- `FMOCombatHitInfo` - Describes a combat hit (body part, damage, category)
- `FMOCombatStaminaCosts` - Configurable stamina costs for combat actions
- `UMOCombatMedicalHelpers` - Blueprint function library for integration

**Damage Categories → Wound Types:**

| Category | Wound Type | Characteristics |
|----------|-----------|-----------------|
| Slash | Laceration | Heavy bleeding |
| Pierce | Puncture | High infection risk |
| Blunt | Blunt | Internal damage, fractures |
| Fire | BurnSecond | Pain, scarring |
| Cold | Frostbite | Tissue damage |
| Crush | Blunt + Fracture | Severe trauma |

**Body Part Damage Multipliers:**

| Part | Multiplier | Notes |
|------|-----------|-------|
| Brain | 5.0x | Instant death |
| Heart | 4.0x | Instant death |
| Lungs | 2.5x | ~3 min death timer |
| Head | 2.0x | Concussion risk |
| Spine | 1.8-2.5x | Paralysis risk |
| Torso | 1.2x | Organ damage |
| Limbs | 0.6-1.0x | Disabling |
| Extremities | 0.4x | Low damage |

**Combat Integration Example:**
```cpp
// In your combat system when a hit lands
void AMyWeapon::OnHitConfirmed(AActor* Target, FHitResult& Hit)
{
    // Get medical components from target
    UMOAnatomyComponent* AnatomyComp = Target->FindComponentByClass<UMOAnatomyComponent>();
    UMOVitalsComponent* VitalsComp = Target->FindComponentByClass<UMOVitalsComponent>();
    UMOMentalStateComponent* MentalComp = Target->FindComponentByClass<UMOMentalStateComponent>();

    // Build hit info
    FMOCombatHitInfo HitInfo;
    HitInfo.TargetBodyPart = UMOCombatMedicalHelpers::BoneNameToBodyPart(Hit.BoneName);
    HitInfo.BaseDamage = WeaponDamage;
    HitInfo.DamageCategory = EMODamageCategory::Slash;  // Sword
    HitInfo.ArmorPenetration = 0.8f;  // 80% pen
    HitInfo.bCausesHeavyBleeding = bIsCriticalHit;

    // Apply damage through medical system
    UMOCombatMedicalHelpers::ApplyCombatDamage(HitInfo, AnatomyComp, VitalsComp, MentalComp);
}

// Stamina check before attack
bool AMyCharacter::CanAttack() const
{
    return UMOCombatMedicalHelpers::CanPerformCombatAction(VitalsComp, CombatCosts.LightAttack);
}
```

### Adrenaline System

The adrenaline system models the fight-or-flight response, providing gameplay balance for permadeath survival:

**Key Classes:**
- `UMOAdrenalineComponent` - Component managing adrenaline state
- `FMOAdrenalineState` - Current adrenaline level, phase, effects
- `FMOAdrenalineConfig` - Configurable thresholds and rates
- `FMOThreatInfo` - Threat assessment data

**Adrenaline Phases:**
```
Baseline → Spiking → Sustained → Crashing → Baseline
    ↑                              │
    └──────── (combat) ────────────┘
```

**Effects During Active Adrenaline:**

| Effect | At Max Adrenaline | Purpose |
|--------|------------------|---------|
| Pain Masking | 80% | Ignore wound pain during combat |
| Bleed Reduction | 50% | Vasoconstriction slows blood loss |
| Accuracy Penalty | 40% | Shaky hands from adrenaline |
| Tunnel Vision | 60% | Reduced peripheral awareness |
| Heart Rate | 2x | Physiological response |
| Stamina Drain | 0.8x | Adrenaline push-through |

**Crash Phase Effects:**
- Stamina drain: 2x normal
- Masked pain returns (shock/disorientation)
- Bleed rate normalizes (wounds "open up")

**Skill-Based Response:**

Combat skill determines threat threshold and adrenaline dampening:

| Skill Level | Threat Threshold | Max Adrenaline | Accuracy Penalty Reduction |
|-------------|-----------------|----------------|---------------------------|
| Novice (0) | 0% | 100 | 0% |
| Trained (25) | 20% | 90 | 15% |
| Experienced (50) | 50% | 75 | 35% |
| Veteran (75) | 75% | 60 | 55% |
| Master (100) | 90% | 40 | 75% |

**Integration Example:**
```cpp
// In your combat system when entering combat
void ACombatManager::OnCombatStarted(AActor* Enemy)
{
    if (UMOAdrenalineComponent* AdrenalineComp = Player->FindComponentByClass<UMOAdrenalineComponent>())
    {
        FMOThreatInfo ThreatInfo;
        ThreatInfo.ThreatActor = Enemy;
        ThreatInfo.ThreatPower = GetEnemyCombatPower(Enemy);
        ThreatInfo.Distance = FVector::Dist(Player->GetActorLocation(), Enemy->GetActorLocation());
        ThreatInfo.bIsAttacking = true;

        AdrenalineComp->EnterCombat(ThreatInfo);
    }
}

// Apply accuracy penalty to weapon
float AMyWeapon::GetEffectiveAccuracy() const
{
    float BaseAccuracy = WeaponAccuracy;

    if (UMOAdrenalineComponent* AdrenalineComp = GetOwner()->FindComponentByClass<UMOAdrenalineComponent>())
    {
        BaseAccuracy = AdrenalineComp->CalculateEffectiveAccuracy(BaseAccuracy);
    }

    return BaseAccuracy;
}
```

### Skeleton Mapping System

Maps skeletal mesh bone names to body parts for localized damage.

**Key Classes:**
- `UMOSkeletonMapping` - Static helper for bone lookups
- `FMOSkeletonMappingConfig` - Complete skeleton mapping configuration
- `FMOBoneMapping` - Single bone-to-body-part mapping

**Supported Skeletons:**
- UE5 Mannequin (89 bones)
- MetaHuman (342 bones)
- Custom skeletons via fuzzy matching

**Usage:**
```cpp
// In your damage system
void OnHitDetected(const FHitResult& Hit)
{
    EMOBodyPartType BodyPart = UMOSkeletonMapping::BoneToBodyPart(Hit.BoneName);
    EMOBodyRegion Region = UMOSkeletonMapping::BoneToBodyRegion(Hit.BoneName);

    // Apply damage to specific body part
    AnatomyComp->InflictDamage(BodyPart, Damage, WoundType);
}
```

**Physics Asset Setup:**

For per-bone hit detection, configure your Physics Asset:

1. Open Physics Asset Editor (double-click Physics Asset)
2. Create collision bodies for each damage zone:
   - Head: Sphere on `head` bone
   - Torso: Capsule on `spine_03` to `spine_05`
   - Arms: Capsules on `upperarm_*`, `lowerarm_*`
   - Legs: Capsules on `thigh_*`, `calf_*`
3. Enable "Simulation Generates Hit Events" on each body
4. Set collision responses for `Projectile` and `Weapon` channels

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
├── UMOAdrenalineComponent     - Combat stress response
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

## Future Systems Roadmap

Based on analysis of implemented systems versus typical survival game requirements (comparing to Valheim, Rust, Subnautica, The Forest, Green Hell), this section identifies missing systems and recommended additions.

### Currently Implemented (✓)

| System | Status | Key Components |
|--------|--------|----------------|
| Identity & Persistence | ✓ Complete | `UMOIdentityComponent`, `UMOPersistenceSubsystem` |
| Inventory | ✓ Complete | `UMOInventoryComponent`, slots, stacking, drag-drop |
| Crafting | ✓ Complete | `UMOCraftingSubsystem`, queues, stations, recipes |
| Building | ✓ Complete | `UMOBuildingComponent`, placement, timed construction |
| Medical - Vitals | ✓ Complete | Blood, heart rate, BP, SpO2, temperature, glucose |
| Medical - Metabolism | ✓ Complete | Calories, macros, vitamins, digestion |
| Medical - Anatomy | ✓ Complete | 55+ body parts, wounds, conditions |
| Medical - Mental | ✓ Complete | Consciousness, shock, stress |
| Activity Levels | ✓ Complete | Walk/jog/sprint/work/combat integration |
| Combat-Medical Bridge | ✓ Complete | Damage→wound conversion helpers |
| Survival Stats | ✓ Complete | Health, stamina, hunger, thirst, energy |
| Skills & XP | ✓ Complete | `UMOSkillsComponent`, leveling, XP |
| Knowledge | ✓ Complete | `UMOKnowledgeComponent`, discoveries |
| Interaction | ✓ Complete | Line-trace, interactables |
| Possession | ✓ Complete | Multi-character control |
| UI Framework | ✓ Complete | CommonUI widgets, modals |
| Terraforming | ✓ Complete | Voxel sculpting with persistence |
| PCG Integration | ✓ Complete | Custom nodes, HISM interaction |

### Missing Systems (Priority Order)

#### Priority 1: Combat System
**Gap:** Only bridge layer exists (`MOCombatMedicalTypes`), no actual combat mechanics.

**Recommended Components:**
```
UMOWeaponComponent       - Weapon stats, attack execution
UMOCombatComponent       - Hit detection, damage calculation
FMOWeaponDefinitionRow   - DataTable for weapon definitions
AMOProjectile            - Ranged projectile actor
```

**Features Needed:**
- Melee attack states (light/heavy/combo)
- Block/parry/dodge mechanics
- Ranged weapon support (bow, thrown)
- Hit detection (collision + line trace)
- Stamina integration (already designed)

---

#### Priority 2: Equipment/Armor System
**Gap:** No equipment slots, armor, or wearable clothing.

**Recommended Components:**
```
UMOEquipmentComponent    - Equipment slot management
FMOEquipmentSlot         - Individual slot data
FMOEquipmentDefinitionRow- DataTable for equipment
```

**Equipment Slots:**
- Head, Face, Neck
- Chest, Back, Waist
- Shoulders, Arms, Hands
- Legs, Feet
- Accessories (rings, amulets)

**Features Needed:**
- Armor value reducing damage
- Temperature/insulation bonuses
- Durability degradation
- Set bonuses (optional)
- Visual attachment (skeletal mesh sockets)

---

#### Priority 3: Status Effects/Buffs System
**Gap:** No generic buff/debuff system (some conditions exist in medical).

**Recommended Components:**
```
UMOStatusEffectComponent - Active effects on pawn
FMOStatusEffect          - Effect definition (duration, stacking, ticks)
UMOStatusEffectSubsystem - Effect application/removal
```

**Common Effects:**
- Poisoned, Bleeding, Burning, Freezing (some exist in medical)
- Well Fed, Rested, Inspired (buff states)
- Encumbered, Exhausted, Dehydrated
- Food buffs (stamina regen, damage boost)
- Environmental (wet, cold, hot)

**Integration:**
- Metabolism provides nutrition-based buffs
- Medical provides injury-based debuffs
- Combat applies damage-over-time effects
- Environment applies temperature effects

---

#### Priority 4: Weather/Environment System
**Gap:** No weather, temperature zones, or environmental hazards.

**Recommended Components:**
```
UMOWeatherSubsystem      - World weather state
UMOEnvironmentZone       - Area-based temperature/hazards
AMOWeatherController     - Weather transitions, effects
```

**Weather Types:**
- Clear, Cloudy, Rain, Storm, Snow, Fog
- Wind speed/direction
- Temperature variation by time of day

**Environmental Effects:**
- Rain extinguishes campfires
- Cold zones require warm clothing
- Heat zones cause dehydration
- Wet debuff from rain/swimming

---

#### Priority 5: Sleep/Rest System
**Gap:** Fatigue exists in vitals but no dedicated sleep mechanics.

**Recommended Components:**
```
AMOBed                   - Bed/bedroll buildable actor
UMOSleepSubsystem        - Time skip, rest bonuses
```

**Features:**
- Time-of-day requirement (night sleep)
- Fatigue recovery during sleep
- "Rested" buff after sleeping
- Optional: time skip to morning
- Dreams/nightmares (mental state integration)

---

#### Priority 6: Resource Nodes/Gathering
**Gap:** Interaction exists but no dedicated harvestable resources.

**Recommended Components:**
```
AMOResourceNode          - Trees, rocks, ore deposits
UMOHarvestableComponent  - Harvest progress, tool requirements
FMOResourceDefinitionRow - Resource spawn data
```

**Features:**
- Tool-gated harvesting (axe for trees, pickaxe for ore)
- Harvest progress (multiple hits)
- Resource depletion and respawn
- Yield based on skill level
- Different gather actions (chop, mine, dig, pick)

---

#### Priority 7: Creature AI System
**Gap:** Basic `MOAIController` exists but no behavior framework.

**Recommended Components:**
```
UMOCreatureComponent     - Creature stats, behavior config
UMOThreatComponent       - Aggro/threat tracking
AMOCreatureCharacter     - Base creature pawn
Behavior Trees           - Per-creature behavior
```

**Behavior Types:**
- Passive (deer, rabbits) - flee when threatened
- Neutral (boars) - attack when provoked
- Aggressive (wolves) - hunt players
- Territorial (bears) - guard areas
- Pack (wolves) - coordinate attacks

---

#### Priority 8: Map/Exploration System
**Gap:** No map, waypoints, or fog of war.

**Recommended Components:**
```
UMOMapComponent          - Discovered areas, markers
UMOMinimapWidget         - HUD minimap
UMOWorldMapWidget        - Full-screen map
```

**Features:**
- Fog of war revealing as explored
- Player-placed markers/waypoints
- Points of interest auto-discovery
- Compass direction indicator

---

### Lower Priority Systems

| System | Priority | Notes |
|--------|----------|-------|
| Quest/Objectives | Low | Many survival games work without formal quests |
| Audio Manager | Medium | UE built-in sound cues may suffice initially |
| World Events | Low | Boss encounters, waves - can add post-core |
| Localization | Medium | Important for release, not for development |
| Tutorial System | Medium | Can use notification system initially |
| Photo Mode | Low | Nice-to-have feature |

---

### Research Notes: Weather Integration (UDW Bridge)

**Status:** Interface created, BP implementation pending

**Files Created:**
- `MOWeatherTypes.h` - Structs for weather/time data
- `MOWeatherProviderInterface.h` - Interface for weather providers
- `MOWeatherIntegrationSubsystem.h/.cpp` - World subsystem with delegates
- `MOWeatherBlueprintLibrary.h/.cpp` - Helper functions for BP

**BP Implementation Steps:**
1. Create Actor BP `BP_UDWWeatherProvider` implementing `MOWeatherProviderInterface`
2. Add variables: `UDS Reference` (Ultra_Dynamic_Sky), `UDW Reference` (Ultra_Dynamic_Weather)
3. On BeginPlay: Get actors by class, then call `Register Weather Provider` on subsystem
4. Implement interface functions using helper nodes:
   - `Make Time Of Day (from UDS)` - Pass TimeValue, Season, bIsDaytime
   - `Make Weather State (from UDW)` - Pass all Get* results
   - `Make Weather Exposure (from UDW)` - Pass exposure test results

**Subsystem provides:**
- `GetTemperatureAtLocation()`, `GetFeelsLikeTemperature()`
- `GetColdStress()`, `GetHeatStress()` (0-1 for medical integration)
- Delegates: `OnRainChanged`, `OnSnowChanged`, `OnTemperatureThresholdCrossed`, `OnDayNightChanged`

---

### Research Notes: Procedural Water Generation

**Status:** Research complete, awaiting implementation

This section documents findings for procedural river/lake generation using the Voxel Plugin.

#### Available Voxel Plugin Nodes

| Node | Purpose | Usage |
|------|---------|-------|
| `GetGradient2D` | Calculate terrain slope direction | Identify downhill flow direction |
| `GetGradient3D` | Full 3D gradient vector | More accurate for complex terrain |
| `NormalToSlope` | Convert normal to slope angle | Threshold steep vs flat terrain |
| `Voxel Spline System` | Built-in spline support | Designed for rivers/roads |

#### Recommended Algorithm: Gradient-Based Flow Detection

**Step 1: Find Valleys**
- Sample terrain height at grid points
- Calculate gradient (slope direction) at each point using `GetGradient2D`
- Valleys are where gradient vectors converge (flow accumulates)

**Step 2: Flow Accumulation**
```
For each point P:
  1. Get gradient direction (downhill)
  2. Trace downhill path until hitting:
     - A local minimum (potential lake)
     - Map edge
     - Existing water body
  3. Increment "flow count" for each cell along path
  4. High flow count = river candidate
```

**Step 3: River Spline Generation**
- Connect high-flow points into continuous paths
- Use Voxel Plugin spline system for mesh generation
- Width varies with flow accumulation value

**Step 4: Lake Detection**
- Local minima = potential lakes/ponds
- Fill algorithm from minimum point up to spillover height
- Lake size determined by basin volume

#### Performance Considerations

| Approach | Speed | Accuracy |
|----------|-------|----------|
| Full simulation (every cell) | Slow | Best |
| Sampled + interpolation | Medium | Good |
| Pre-computed at load | Fast | Fixed |

**Recommendation:** Pre-compute major rivers at world generation time, use gradient sampling for small streams/details.

#### Voxel Plugin Integration Points

- **Voxel Height Actor:** Primary terrain source
- **Sculpt data:** Respect player modifications when calculating flow
- **Spline meshes:** Use for river geometry
- **Material blending:** Blend water material at river edges

#### Reference Games

- **Dwarf Fortress:** Full flow simulation, computationally expensive
- **Cities: Skylines:** Simplified heightmap-based water placement
- **Valheim:** Pre-placed water at fixed elevation

---

### Integration Dependencies

```
Combat System
├── Requires: Medical System (damage→wounds) ✓
├── Requires: Activity Levels (stamina costs) ✓
├── Requires: Equipment System (weapon stats)
└── Requires: Skills System (combat skills) ✓

Equipment System
├── Requires: Inventory System (equip from inventory) ✓
├── Requires: Persistence (save equipped items) ✓
└── Integrates: Medical System (armor reduces damage)

Weather System
├── Integrates: Medical System (temperature effects) ✓
├── Integrates: Building System (shelter detection)
├── Integrates: Status Effects (wet, cold buffs)
└── Integrates: Crafting (fire extinguishing)

Resource Nodes
├── Requires: Interaction System ✓
├── Requires: Inventory System (yield items) ✓
├── Requires: Skills System (gathering skills) ✓
├── Integrates: Activity Levels (gathering = work) ✓
└── Integrates: Tools (equipment system)
```

### Recommended Implementation Order

1. **Equipment System** - Enables armor, tools, weapons as equippable items
2. **Combat System** - Core gameplay loop, uses medical + equipment
3. **Status Effects** - Consolidates buffs/debuffs across systems
4. **Resource Nodes** - Dedicated gathering gameplay
5. **Weather System** - Environmental challenge layer
6. **Sleep System** - Complete the survival loop
7. **Creature AI** - Populate the world with threats
8. **Map System** - Navigation and exploration tracking

---

## UI Architecture & Patterns

This section documents the UI architecture, reusable patterns, and best practices gathered from codebase analysis and industry research.

### Current Architecture Overview

The MOFramework UI follows a **Manager-Widget-Delegate** pattern:

```
AMOPlayerController
└── UMOUIManagerComponent (centralized orchestration)
    ├── Creates/Destroys widgets
    ├── Manages input modes (Game ↔ UI)
    ├── Handles delegate callbacks from widgets
    └── Coordinates cross-widget communication

Widget Lifecycle:
  UIManager::Show*() → CreateWidget → AddToViewport → SetKeyboardFocus
  Widget::OnRequestClose → UIManager::Handle* → Close*() → RemoveFromParent
```

### Widget Categories (40+ Classes)

| Category | Widgets | Z-Order |
|----------|---------|---------|
| HUD/Status | PlayerStatus, Reticle, ToolHint, ModeIndicator | 0-50 |
| Panels | InventoryMenu, CraftingMenu, SkillsPanel, StatusPanel | 100 |
| Menus | BuildingMenu, PossessionMenu, LoadPanel, SavePanel | 100-150 |
| Context | ItemContextMenu, GhostContextMenu, StationContextMenu | 200 |
| Modal | ConfirmDialog, ItemInspector, ModalBackground | 250 |

### Reusable Widget Components

#### MOInventorySlot
Self-contained slot widget used across multiple menus:
- Inventory grid (16x slot)
- Crafting material display
- Container inventory
- Equipment slots

**Features:** Drag-drop, hover effects, quantity display, context menu trigger

#### MOStatusField
Generic labeled value display with progress bar:
- Survival stats (health, hunger, thirst)
- Skill levels and XP
- Crafting progress

**Features:** Label, value text, optional progress bar, threshold coloring

#### MORecipeEntryWidget
Recipe display for crafting/building menus:
- Shows icon, name, materials
- "Can Craft" indicator
- Click to select

### Communication Patterns

#### Widget → Manager (Recommended)
```cpp
// In Widget header
UPROPERTY(BlueprintAssignable)
FMOInventoryMenuRequestCloseSignature OnRequestClose;

// In Widget implementation
OnRequestClose.Broadcast();

// In UIManager (bound in NativeConstruct or after creation)
Widget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleInventoryMenuRequestClose);
```

#### Manager → Widget (Direct calls)
```cpp
// UIManager directly calls widget methods
InventoryMenu->RefreshInventoryDisplay();
CraftingMenu->SetActiveStation(StationActor);
```

#### Cross-Widget (Via Manager)
```cpp
// ItemContextMenu broadcasts action
OnDropItem.Broadcast(ItemGuid);

// UIManager handles and coordinates
void HandleContextMenuDropItem(FGuid ItemGuid)
{
    CloseItemContextMenu();
    InventoryComponent->DropItemByGuid(ItemGuid);
    InventoryMenu->RefreshInventoryDisplay();
}
```

### Performance Patterns

#### Widget Pooling with FUserWidgetPool
For frequently created/destroyed widgets (inventory slots, recipe entries):

```cpp
// In header
UPROPERTY()
FUserWidgetPool SlotWidgetPool;

// In implementation
void UMOInventoryGrid::RebuildSlots(int32 SlotCount)
{
    // Release all existing slots back to pool
    SlotWidgetPool.ReleaseAll();

    // Get or create slots from pool
    for (int32 i = 0; i < SlotCount; ++i)
    {
        UMOInventorySlot* Slot = SlotWidgetPool.GetOrCreateInstance<UMOInventorySlot>(
            SlotWidgetClass,
            this
        );
        Slot->SetSlotIndex(i);
        SlotsContainer->AddChild(Slot);
    }
}
```

**Benefits:** Eliminates GC churn, faster rebuilds, reduced memory fragmentation

#### Invalidation Boxes
For widgets with static content surrounded by dynamic content:

```
UInvalidationBox
├── Static header/footer (rarely invalidates)
└── Dynamic content area (frequently invalidates)
```

**Usage:** Wrap static portions in `SInvalidationBox` to prevent full widget rebuild on every tick.

#### Lazy Loading
Defer expensive widget creation until needed:

```cpp
void UMOUIManagerComponent::ShowSkillsPanel()
{
    if (!SkillsPanel)
    {
        // Create on first open, reuse thereafter
        SkillsPanel = CreateWidget<UMOSkillsPanel>(GetOwningPlayerController(), SkillsPanelClass);
    }
    SkillsPanel->AddToViewport(100);
}
```

### MVVM/ViewModel Pattern (Future Enhancement)

For data-heavy displays (health bars, status panels), consider UE5's ViewModel system:

```cpp
// ViewModel - pure data container
UCLASS()
class UMOVitalsViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, FieldNotify)
    float Health;

    UPROPERTY(BlueprintReadOnly, FieldNotify)
    float MaxHealth;

    UPROPERTY(BlueprintReadOnly, FieldNotify)
    float HealthPercent;
};

// In Widget Blueprint: bind HealthBar.Percent to HealthPercent
// ViewModel updates automatically propagate to UI
```

**Benefits:**
- Decouples UI from game logic
- Testable view models
- Automatic binding updates
- Cleaner widget code

### CommonUI Integration

MOFramework uses CommonUI for input-aware widgets:

#### Activatable Widget Stack
```cpp
// Widgets derive from UCommonActivatableWidget
class UMOInventoryMenu : public UCommonActivatableWidget

// Override activation hooks
virtual void NativeOnActivated() override;
virtual void NativeOnDeactivated() override;
virtual UWidget* NativeGetDesiredFocusTarget() const override;
```

#### Input Routing
CommonUI handles input routing automatically:
- Focused widget receives input first
- Back action (Escape/B button) automatically handled
- Focus chains for gamepad navigation

#### When to Use CommonUI vs Raw UMG

| Use Case | Recommendation |
|----------|----------------|
| Main menus (inventory, crafting) | CommonUI - needs input management |
| HUD elements (health bar) | Raw UMG - always visible, no input |
| Context menus | CommonUI - modal focus |
| Tooltips | Raw UMG - passive display |
| Dialog confirmations | CommonUI - modal with focus |

### UI Best Practices from Industry Research

#### From Lyra Sample Project
- Use CommonUI button styles for consistency
- Implement `GetDesiredFocusTarget()` for gamepad support
- Handle back navigation with `UCommonActivatableWidget::SetBindVisibilities()`

#### From Dead Cells / Hades (Action Games)
- Minimize menu time during gameplay
- Quick-use hotbar for common items
- Visual feedback on every interaction (flash, scale, sound)

#### From Valheim / The Forest (Survival Games)
- Single-window crafting with categories
- Clear "have X / need Y" display
- Queue system for multiple crafts
- Auto-sort and filtering for inventory

### Improvement Opportunities

Based on audit findings:

1. **Implement Widget Pooling** for MOInventorySlot, MORecipeEntryWidget
   - Estimated performance gain: 30-50% on menu open/close

2. **Add ViewModel Layer** for StatusPanel components
   - Decouples VitalsComponent from UI
   - Enables testing without gameplay

3. **Consolidate Context Menu Actions** into action handler class
   - Current: 160-line if/else chain in UIManager
   - Target: Strategy pattern with action classes

4. **Add Invalidation Boxes** to StatusPanel
   - Header/tab bar is static
   - Only body content changes

5. **Implement Widget Animations**
   - Fade in/out for menus
   - Slide for context menus
   - Scale pulse on item pickup

---

## Technical Debt & Known Issues

This section documents known technical debt identified through code audits for future improvement.

### Recently Completed

#### ✅ MOMedicalTypes.h Split (Commit: 95a7e56)
**Status:** COMPLETE - Split 1,088-line monolith into 6 focused headers

| New File | Lines | Contents |
|----------|-------|----------|
| `MOActivityTypes.h` | 193 | Activity levels, stamina, exertion |
| `MOBodyPartTypes.h` | 208 | Body parts, wound/condition enums |
| `MOWoundTypes.h` | 193 | Wound/condition FastArray structs |
| `MOVitalsTypes.h` | 130 | Vital signs (heart rate, BP, etc.) |
| `MOMetabolismTypes.h` | 291 | Body composition, nutrients, digestion |
| `MOMentalTypes.h` | 64 | Mental state, consciousness |

`MOMedicalTypes.h` is now a 24-line umbrella include for backwards compatibility.

#### ✅ Component Caching (Commit: 6f7733f)
**Status:** COMPLETE - Eliminated repeated FindComponentByClass calls

Added pawn component caching to `MOUIManagerComponent`:
- `CachePawnComponents(APawn*)` called on possession
- `ClearCachedPawnComponents()` called on unpossess
- 9 components cached: Inventory, Skills, Knowledge, CraftingQueue, RecipeDiscovery, Vitals, Metabolism, MentalState, SurvivalStats
- Replaced 11 FindComponentByClass calls with cached accessors

### Deferred (Low Priority)

#### MOUIManagerComponent - God Class (3,541 lines)
**Status:** Deferred - Current state is acceptable

The UI manager handles 15+ responsibilities but is well-organized with section comments.
Each menu follows a consistent Open/Close/Toggle/Handle* pattern.

**If splitting becomes needed:**
1. `UMOItemActionHandler` - Extract context menu action strategy
2. `UMOInputModeManager` - Input mode switching
3. Per-menu sub-components

**Note:** `HandleContextMenuAction()` (160 lines) is tightly coupled to other UIManager methods, making clean extraction complex.

### Large File Summary (>800 lines)

| File | Lines | Status |
|------|-------|--------|
| `MOUIManagerComponent.cpp` | 3,541 | Deferred - well-organized |
| `MOPersistenceSubsystem.cpp` | 2,043 | Complex orchestration |
| `MOInventoryComponent.cpp` | 1,474 | Multi-concern |
| `MODataImportCommandlet.cpp` | 1,088 | Monolithic import |
| `MOStatusPanel.cpp` | 1,049 | Multi-component UI |
| `MOAnatomyComponent.cpp` | 1,043 | Wound system |
| `MOCharacter.cpp` | 931 | Component factory |
| `MOMetabolismComponent.cpp` | 913 | Metabolism calculations |
| `MOUnifiedInventoryMenu.cpp` | 885 | Complex UI |

### High Priority

#### 1. Remaining FindComponentByClass Calls
**~30 remaining instances in non-UI code**

Affected files (not yet optimized):
- `MOPersistenceSubsystem.cpp` (15 instances - save/load, acceptable)
- `MOCraftingQueueComponent.cpp` (2 instances)
- Various other files (one-time lookups)

#### 2. Missing Interface Abstractions
Several systems would benefit from formal UInterface definitions:

| Interface | Classes That Would Implement | Benefit |
|-----------|------------------------------|---------|
| `IInventoryHolder` | Character, Container, Station | Consistent inventory access |
| `IBuildable` | BuildableActor, future structures | Extensible building system |
| `ISaveable` | All persistent actors/components | Centralized persistence |
| `IMaterialSource` | Inventory, Container, WorldItem | Flexible material gathering |

#### 3. Incomplete TODO Items
Active TODO comments indicate unfinished features:
- `MOAnatomyComponent.cpp:781` - Add setter in VitalsComponent for condition effects
- `MOStatusPanel.cpp:190` - Expose threshold setters on UMOStatusField
- `MOStatusPanel.cpp:439` - Add visual selected state to buttons
- `MOStatusPanel.cpp:971` - Show input dialog for value changes
- `MOStatusPanel.cpp:980` - Show name change dialog
- `MOUnifiedInventoryMenu.cpp:835` - Visual indication of selected tab (highlight/underline)
- `MOVitalsComponent.cpp:683` - Integrate lung damage with anatomy component

### Medium Priority

#### 4. Inconsistent Delegate Patterns
- Mixed use of `DYNAMIC_MULTICAST_DELEGATE` and non-dynamic delegates in same components
- Example: `MOInteractableComponent.h` uses both types

**Recommendation:** Standardize to `DYNAMIC_MULTICAST_DELEGATE` for Blueprint accessibility.

#### 5. Missing `const` Qualifiers
Several query methods lack const:
- `MOInteractionSubsystem.h` methods that don't modify state

#### 6. UPROPERTY Category Inconsistency
Category naming varies between files:
- Some use `"MO|System|Section"` (good)
- Some use inconsistent nesting levels

**Recommendation:** Standardize to max 3 levels: `"MO|<System>|<Subsection>"`

### Low Priority

#### 7. Delegate Naming Inconsistency
- Most follow `FMO<Component><Event>Signature` (good)
- Some lack component name prefix
- Event tense varies: `Changed` vs `Change`

#### 8. Documentation Gaps
- Many delegates lack detailed documentation on when they fire
- Some public methods missing param documentation

---

## Interface Opportunities (Future Work)

Based on code analysis, these interfaces would improve modularity and reduce coupling:

### ISaveable Interface (Critical Priority)
```cpp
UINTERFACE(MinimalAPI)
class UMOSaveableInterface : public UInterface { GENERATED_BODY() };

class IMOSaveableInterface
{
    GENERATED_BODY()
public:
    virtual void BuildSaveData(FMOGenericSaveData& OutData) = 0;
    virtual bool ApplySaveData(const FMOGenericSaveData& InData) = 0;
    virtual FGuid GetPersistentId() const = 0;
};
```

**Would implement:** Character, BuildableActor, WorldItem, InventoryComponent, VitalsComponent, MetabolismComponent, CraftingQueueComponent

### IInventoryHolder Interface
```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class UMOInventoryHolderInterface : public UInterface { GENERATED_BODY() };

class IMOInventoryHolderInterface
{
    GENERATED_BODY()
public:
    virtual UMOInventoryComponent* GetInventoryComponent() const = 0;
    virtual bool HasItem(FName ItemId, int32 Quantity = 1) const = 0;
};
```

**Would implement:** Character, ContainerActor, CraftingStationActor

### IMaterialSource Interface
```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class UMOMaterialSourceInterface : public UInterface { GENERATED_BODY() };

class IMOMaterialSourceInterface
{
    GENERATED_BODY()
public:
    virtual bool CanProvideMaterial(FName MaterialId, int32 Quantity) const = 0;
    virtual bool GatherMaterial(FName MaterialId, int32 Quantity) = 0;
    virtual TArray<FName> GetAvailableMaterials() const = 0;
};
```

**Would implement:** InventoryComponent, ContainerActor, WorldItem

---

## Code Health Metrics

**Last Audit:** 2026-02-07

| Metric | Value | Status |
|--------|-------|--------|
| Public Headers | 102 files | Good |
| Dynamic Delegates | 140+ | Good - event-driven |
| TODO Comments | 7 | Improved |
| FindComponentByClass calls | 40+ | Needs refactoring |
| TObjectPtr usage | 60+ | Good - modern UE5 |
| TSoftObjectPtr usage | 70+ | Excellent |
| Custom PCG Nodes | 1 | MO Item Spawner |

**Overall Health Score:** 7.8/10

---

## Project Configuration Reference

This section documents all config file settings required for MOFramework. Copy these when porting to a new project or UE version.

### DefaultGame.ini

```ini
; =============================================================================
; MOFramework Database Settings
; Set these paths to your project's DataTables
; =============================================================================

[/Script/MOFramework.MOItemDatabaseSettings]
ItemDefinitionsDataTable=/Game/Data/DT_Items.DT_Items

[/Script/MOFramework.MORecipeDatabaseSettings]
RecipeDefinitionsDataTable=/Game/Data/DT_Recipes.DT_Recipes

[/Script/MOFramework.MOSkillDatabaseSettings]
SkillDefinitionsDataTable=/Game/Data/DT_Skills.DT_Skills

[/Script/MOFramework.MOMedicalDatabaseSettings]
MedicalTreatmentsTable=/Game/Data/DT_MedicalTreatment.DT_MedicalTreatment
BodyPartDefinitionsTable=/Game/Data/DT_BodyParts.DT_BodyParts
WoundTypeDefinitionsTable=/Game/Data/DT_Wounds.DT_Wounds
ConditionDefinitionsTable=/Game/Data/DT_Conditions.DT_Conditions

[/Script/MOFramework.MOPersistenceSettings]
DefaultPersistedPawnClass=/MOFramework/Characters/BP_MOCharacter.BP_MOCharacter_C

; =============================================================================
; Packaging Settings (CRITICAL for cooked builds)
; These ensure soft-referenced assets are included in packaged games
; =============================================================================

[/Script/UnrealEd.ProjectPackagingSettings]
; Add directories containing DataTables and assets referenced by them
+DirectoriesToAlwaysCook=(Path="/Game/Data")
+DirectoriesToAlwaysCook=(Path="/MOFramework")
; Add additional directories as needed for your project:
; +DirectoriesToAlwaysCook=(Path="/Game/Icons")
; +DirectoriesToAlwaysCook=(Path="/Game/Art/Items")
```

### DefaultEngine.ini

```ini
; =============================================================================
; Game Mode Settings
; =============================================================================

[/Script/EngineSettings.GameMapsSettings]
GlobalDefaultGameMode=/MOFramework/BP_MOGameMode.BP_MOGameMode_C

; =============================================================================
; Custom Collision Channel for Interaction System
; =============================================================================

[/Script/Engine.CollisionProfile]
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="Interactable")
```

### DefaultInput.ini

```ini
; =============================================================================
; Enhanced Input Settings (required for MOFramework input handling)
; =============================================================================

[/Script/Engine.InputSettings]
DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
```

### Required Plugin Dependencies

The MOFramework.uplugin requires these plugins:

```json
"Plugins": [
  { "Name": "EnhancedInput", "Enabled": true },
  { "Name": "CommonUI", "Enabled": true },
  { "Name": "Niagara", "Enabled": true },
  { "Name": "PCG", "Enabled": true }
]
```

Optional (if using Voxel terrain):
```json
{ "Name": "Voxel", "Enabled": true }
```

### Packaging Checklist

When packaging for distribution:

1. **Verify DirectoriesToAlwaysCook** includes:
   - `/Game/Data` (DataTables)
   - `/MOFramework` (plugin content)
   - Any folders containing icons, meshes, or other assets referenced by DataTable rows

2. **Check output log during cook** for:
   - `[MOFramework] Preloaded Item Database: ...`
   - `[MOFramework] Preloaded X item definitions, Y soft-referenced assets`
   - Any warnings about missing assets

3. **Test packaged build** for:
   - Item icons displaying correctly
   - Picking up items works
   - Crafting menu shows recipes
   - Save/load functions

### Common Packaging Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Default icons in packaged build | Icons not cooked | Add icon folder to DirectoriesToAlwaysCook |
| Can't pick up items | DataTable not loaded | Check DefaultGame.ini paths, add /Game/Data to cook |
| Inspect does nothing | Item definitions missing | Verify DT_Items is in cooked directory |
| Recipe list empty | Recipe DataTable missing | Add recipe table path to cook directories |

---

## License

This plugin is part of the MO57 project. See project root for license details.

---

## Contributing

1. Follow Unreal Engine coding standards
2. Use the `MO` prefix for all classes
3. Document public APIs with UFUNCTION/UPROPERTY metadata
4. Test multiplayer scenarios before committing replication changes
