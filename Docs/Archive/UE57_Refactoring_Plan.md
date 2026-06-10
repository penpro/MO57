# UE5.7 Native Functionality Refactoring Plan

> **SUPERSEDED**: This document has been consolidated into `MO57_Master_Plan.md` (March 28, 2026).
> Kept for historical reference only. See Master Plan for current status.

*Created: March 18, 2026*
*Status: Superseded*

---

## Overview

This document tracks the planned migration from custom MOFramework implementations to UE5.7 native functionality. Findings are based on the comprehensive audit completed on March 18, 2026.

---

## Priority 1: CommonUI Full Adoption

**Status**: Planning Complete (March 18, 2026)
**Impact**: HIGH - Reduces ~600 LOC, automatic input handling
**Effort**: 4 weeks (phased)
**Current Compliance**: ~85% (20 widgets already use CommonUI)

### Architecture Decisions

#### ADR-1: UI Manager Pattern
- **Decision**: Create `UMOGameUIManagerSubsystem` (world subsystem) + keep `UMOUIManagerComponent` as thin wrapper
- **Rationale**: Gradual migration without breaking existing Blueprint references or input handlers

#### ADR-2: Widget Layer Structure
| Layer Tag | Purpose | Z-Priority | Examples |
|-----------|---------|------------|----------|
| `MO.UI.Layer.HUD` | Always-visible HUD | 0 | Reticle, Mode Indicator |
| `MO.UI.Layer.Game` | Gameplay menus | 50 | Inventory, Crafting, Skills |
| `MO.UI.Layer.GameOverlay` | Overlays on gameplay | 100 | Station Context, Harvest Progress |
| `MO.UI.Layer.Menu` | System menus | 150 | In-Game Menu, Possession |
| `MO.UI.Layer.Modal` | Modal dialogs | 200 | Confirmation, Item Context |

#### ADR-3: Input Mode Handling
- **Decision**: Replace manual `SetInputMode()` with `GetDesiredInputConfig()` overrides per widget
- **Pattern**:
```cpp
// Gameplay menus
TOptional<FUIInputConfig> GetDesiredInputConfig() const override {
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
}
// Modal dialogs
TOptional<FUIInputConfig> GetDesiredInputConfig() const override {
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}
```

#### ADR-4: Modal Background
- **Decision**: Deprecate `UMOModalBackground`; add background to modal widget templates
- **Rationale**: Removes reference counting, simplifies layer behavior

### Migration Phases

#### Phase 1: Foundation (Week 1)
**Goals**: Create subsystem infrastructure without breaking existing functionality

**Files Created**:
| File | Description |
|------|-------------|
| `MOGameUIManagerSubsystem.h/cpp` | World subsystem managing UI policy |
| `MOPrimaryGameLayout.h/cpp` | Primary layout with layer stacks |
| `MOUIPolicy.h/cpp` | UI policy defining layout class |

**Acceptance Criteria**:
- [ ] Subsystem initializes on world creation
- [ ] `WBP_PrimaryGameLayout` visible in PIE
- [ ] Existing UI still works via old code path

#### Phase 2: Game Layer Migration (Week 2)
**Goals**: Migrate switchable menus to layer push/pop

**Widgets Migrated**:
- `UMOUnifiedInventoryMenu` -> `MO.UI.Layer.Game`
- `UMOCraftingMenu` -> `MO.UI.Layer.Game`
- `UMOSkillsPanel` -> `MO.UI.Layer.Game`
- `UMOBuildingMenu` -> `MO.UI.Layer.Game`

**Code Pattern Change**:
```cpp
// Before:
ShowModalBackground();
ApplyInputModeForMenuOpen(Widget);
Widget->AddToViewport(InventoryMenuZOrder);

// After:
UMOGameUIManagerSubsystem::Get(World)->PushWidgetToLayerForPlayer(
    PC, FGameplayTag::RequestGameplayTag("MO.UI.Layer.Game"), MenuClass);
```

**Acceptance Criteria**:
- [ ] All 4 menus work via layer system
- [ ] Escape closes topmost menu
- [ ] Input mode transitions automatic

#### Phase 3: Modal and Overlay Migration (Week 3)
**Goals**: Migrate context menus, dialogs, progress widgets

**Widgets to Modal Layer**: ItemContextMenu, ConfirmationDialog, GroundContextMenu, SurvivorContextMenu
**Widgets to GameOverlay**: HarvestProgress, InspectionProgress, StationContext, GhostContext

**Cleanup**:
- [ ] Delete `ShowModalBackground()`, `HideModalBackground()`
- [ ] Add background visuals to modal widget templates

#### Phase 4: Menu/HUD + Cleanup (Week 4)
**Goals**: Complete migration, remove deprecated code

**Widgets to Menu Layer**: InGameMenu, PossessionMenu
**Widgets to HUD Layer**: Reticle, ModeIndicator, ToolHint, FPSCounter

**Major Cleanup**:
- [ ] Remove ~1000 lines from `MOUIManagerComponent`
- [ ] Delete `ApplyInputModeForMenuOpen/Closed()`
- [ ] Delete Z-order constants
- [ ] Delete input mode properties

### Files Changed Summary
| File | Action | LOC Change |
|------|--------|------------|
| `MOGameUIManagerSubsystem.h/cpp` | Create | +300 |
| `MOPrimaryGameLayout.h/cpp` | Create | +150 |
| `MOUIPolicy.h/cpp` | Create | +80 |
| `MOUIManagerComponent.h/cpp` | Major cleanup | -1000 |
| `MOModalBackground.h/cpp` | Delete | -150 |
| `MOUIControllerBase.cpp` | Remove methods | -50 |
| All controllers | Refactor to push/pop | -120 |
| All widgets | Add `GetDesiredInputConfig()` | +190 |
| **Net Result** | | **~-600** |

### Open Questions
1. **CommonGame Plugin**: Included in UE5.7 or pull from Lyra?
2. **Split-Screen**: Need per-player UI layouts?
3. **Quest HUD**: HUD layer or separate always-visible layer?

### Performance Wins Expected
- Eliminate 200ms focus hint polling timer
- `IsAnyMenuOpen()` O(1) via layer stack instead of O(13) booleans
- Widget pooling for context menus (6+ CreateWidget calls eliminated)
- Batch delegate bindings (4 ops/open → 1 ops/lifetime)

---

## Priority 2: EQS for AI Queries

**Status**: Not Started
**Impact**: HIGH - O(n) to O(log n) performance improvement
**Effort**: 1-2 weeks

### Current State
- `TActorIterator` loops scanning all actors
- Manual distance checks and filtering
- Duplicate query code across BT tasks

### Target State
- EQS queries for resource finding
- `BTTask_RunEQSQuery` in behavior trees
- Reusable query assets

### EQS Queries to Create
- [ ] `EQS_FindNearestGatherable` - Find HISM resources by tag
- [ ] `EQS_FindForageItem` - Find ground pickup items
- [ ] `EQS_FindFleeLocation` - Find safe location away from threat
- [ ] `EQS_FindWanderPoint` - Find wander destination

### Files Affected
- `BTTask_SurvivorGather.cpp`
- `BTTask_SurvivorForage.cpp`
- `BTTask_FleeFromThreat.cpp`
- `BTTask_CreatureWander.cpp`
- `MOSurvivorController.cpp`

---

## Priority 3: Gameplay Tags Migration

**Status**: Not Started
**Impact**: HIGH - Extensibility for modding
**Effort**: 1-2 weeks

### Enums to Convert
| Enum | Target Tag Hierarchy |
|------|---------------------|
| `EMOConditionType` | `MO.Condition.*` |
| `EMOEquipmentSlot` | `MO.Equipment.Slot.*` |
| `EMOObjectiveType` | `MO.Quest.Objective.*` |
| `EMOToolType` | `MO.Tool.*` |
| `EMOItemType` | `MO.Item.Type.*` |

### Files Affected
- `MOAnatomyComponent.h/.cpp`
- `MOMentalStateComponent.h/.cpp`
- `MOEquipmentComponent.h/.cpp`
- `MOQuestTypes.h`
- `MOItemDefinitionRow.h`

### Migration Steps
- [ ] Define tag hierarchy in `DefaultGameplayTags.ini`
- [ ] Create `FGameplayTagContainer` replacements
- [ ] Update DataTable schemas
- [ ] Migrate condition checks to tag queries
- [ ] Update Blueprint references

---

## Priority 4: Smart Objects

**Status**: Not Started
**Impact**: HIGH - Modern UE interaction pattern
**Effort**: 2-3 weeks

### Current State
- `UMOInteractableComponent` custom interaction
- `UMOHISMInteractableComponent` for ISM interaction
- Manual interaction validation

### Target State
- `USmartObjectComponent` on interactable actors
- Smart Object Definition assets per interaction type
- `USmartObjectSubsystem` for queries

### Files Affected
- `MOInteractableComponent.h/.cpp`
- `MOInteractorComponent.h/.cpp`
- `MOInteractionSubsystem.h/.cpp`
- `MOHISMInteractableComponent.h/.cpp`

---

## Priority 5: Data Registry

**Status**: Not Started
**Impact**: MEDIUM - Async loading, caching
**Effort**: 1-2 weeks

### DataTables to Migrate
- [ ] `DT_ItemDefinitions` → Data Registry
- [ ] `DT_RecipeDefinitions` → Data Registry
- [ ] `DT_QuestDefinitions` → Data Registry

### Files Affected
- `MOItemDatabaseSettings.h/.cpp`
- `MORecipeDatabaseSettings.h/.cpp`
- `MOQuestSubsystem.h/.cpp`

---

## Priority 6: Enhanced Input Triggers

**Status**: Not Started
**Impact**: LOW - Quick win
**Effort**: 1 day

### Current State
- Manual tap/hold timing in `HandleHustleStart/Triggered/End`
- `HustlePressTime` tracking variable
- `bHustleHoldTriggered` state flag

### Target State
- `UInputTriggerTap` for jog toggle
- `UInputTriggerHold` for sprint
- Remove manual timing code

### Files Affected
- `MOPlayerController.cpp` (lines 683-718)
- Input Action assets

---

## Priority 7: PCG Distance Culling

**Status**: Not Started
**Impact**: MEDIUM - Remove custom node
**Effort**: 2-3 days

### Current State
- `MOPCGDistanceCullingSettings` custom PCG node
- Manual point splitting by distance

### Target State
- PCG native `FPCGDistanceSettings`
- PCG built-in distance attributes

### Files Affected
- `MOPCGDistanceCullingSettings.h/.cpp`
- PCG graph assets

---

## Deferred: StateTree Migration

**Status**: Deferred
**Impact**: HIGH - Cleaner AI architecture
**Effort**: 3-4 weeks
**Reason**: Current BT implementation is clean and functional

### When to Revisit
- When adding complex new AI behaviors
- When debugging becomes difficult
- When designer iteration speed is bottleneck

---

## Deferred: GAS for Combat

**Status**: Deferred
**Impact**: HIGH - Standard combat pattern
**Effort**: 4-6 weeks
**Reason**: Current system well-integrated with medical simulation

### When to Revisit
- When adding multiplayer prediction requirements
- When modding combat abilities becomes priority
- When current system becomes unmaintainable

---

## Systems Confirmed as Correctly Custom

These should NOT be refactored:

| System | Reason |
|--------|--------|
| `UMOIdentityComponent` | Native `ActorGuid` only works in dev builds |
| `UMOPersistenceSubsystem` | Custom GUID tracking required |
| Medical components | GAS overkill for physiological simulation |
| `MOBuildableActor` | No native alternative for weighted build system |
| `FFastArraySerializer` usage | Already correct pattern |
| `TSoftObjectPtr` usage | Already correct pattern |

---

## Testing Checklist

After each priority completion:

- [ ] All menu flows work (open/close/toggle)
- [ ] Input modes transition correctly
- [ ] Gamepad navigation works
- [ ] Mouse/keyboard navigation works
- [ ] No duplicate delegate bindings
- [ ] Save/load still functions
- [ ] Multiplayer replication works
- [ ] Performance profiling shows improvement
- [ ] No regressions in existing functionality

---

## Notes

- Always create feature branch before major refactoring
- Compile and test after each file change
- Update CLAUDE.md with new patterns
- Document any deviations from plan
