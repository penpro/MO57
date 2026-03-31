# MO57 Master Development Plan

*Consolidated: March 28, 2026*
*Supersedes: UE57_Refactoring_Plan.md, consolidation-session-checklist.md, UIManagerSplit_TestingNotes.md*

---

## Overview

This document consolidates all active development planning for MO57. It covers two parallel workstreams:

1. **UI Refactor Track** - Rebuilding the widget library on CommonUI with modular base classes and Python-automated WBP generation
2. **Colony Feature Track** - Implementing colony overview, character cards, persistent colony bar, task assignment without possession, alert system, and possession transition

Both tracks share a foundation phase (Stages 0-2) that must complete first.

---

## Quick Reference

### Session Rules

Paste at the top of every Claude Code session:

```
SESSION RULES
- Read CLAUDE.md before writing any code
- Build after every file change - fix errors before the next file
- Never remove or modify existing public API without checking callsites first
- Follow MOFramework patterns - match naming, include structure, UCLASS specifiers
- All new UI classes: Abstract, Blueprintable, inherit from UCommonActivatableWidget or UUserWidget
- New delegates: use types from MOUIDelegates.h - do not invent new delegate signatures
- Do not delete any file - mark deprecated, verify no callsites, then flag for deletion
- End every session with a build - report final status before stopping
- Update the relevant doc (this file or CLAUDE.md) at session end
```

### Branch Strategy

| Branch | Stages |
|--------|--------|
| `feature/foundation` | 0, 1, 2 (merge to main first) |
| `feature/ui-refactor` | 3A, 4A, 5A, 6A, 7A |
| `feature/colony-management` | 3B, 4B, 5B, 6B, 7B |
| `feature/integration` | 8, 9, 10 (merge both feature branches first) |

### Asset Paths

| Type | Location |
|------|----------|
| Generic widgets | `Plugins/MOFramework/Content/UI/CommonUI/` |
| Colony widgets | `Plugins/MOFramework/Content/UI/Colony/` |
| Existing WBPs | `Plugins/MOFramework/Content/UI/` (various subdirs) |

---

## Stage Map

| Stage | Phase | Name | Track | Status |
|-------|-------|------|-------|--------|
| 0 | Foundation | CLAUDE.md + Python Plugin Setup | Both | **COMPLETE** |
| 1 | Foundation | Generic C++ Widget Base Classes | Both | **COMPLETE** |
| 2 | Foundation | Python WBP Batch Generator | UI | **COMPLETE** |
| 3A | UI Refactor | CommonUI Layer Stack (Blueprint) | UI | **COMPLETE** |
| 3B | Colony | Character Data + History Log | Colony | Not Started |
| 4A | UI Refactor | Scroll List + Detail Panel Migration | UI | Not Started |
| 4B | Colony | Colony Bar (Persistent HUD) | Colony | Not Started |
| 5A | UI Refactor | Context Menu + Progress Widget Migration | UI | Not Started |
| 5B | Colony | Colony Overview Screen | Colony | Not Started |
| 6A | UI Refactor | Inventory + Crafting UI Migration | UI | Not Started |
| 6B | Colony | Task Assignment Without Possession | Colony | Not Started |
| 7A | UI Refactor | System Menus + Delegate Cleanup | UI | Not Started |
| 7B | Colony | Alert Tier System | Colony | Not Started |
| 8 | Integration | Possession Transition Experience | Both | Not Started |
| 9 | Integration | Relationship Simulation | Colony | Not Started |
| 10 | Integration | Full Integration Test | Both | Not Started |

---

## Phase 0-2: Foundation

### Stage 0: CLAUDE.md + Python Plugin Setup

**Goal**: Update CLAUDE.md, enable Python scripting plugin, verify build.

**Tasks**:
1. Update CLAUDE.md with colony management system architecture
2. Add `PythonScriptPlugin` and `EditorScriptingUtilities` to MO57.uproject
3. Build MO57Editor and verify clean compile

**Definition of Done**:
- [ ] CLAUDE.md updated with colony system docs
- [ ] Python plugin enabled in .uproject
- [ ] MO57Editor builds clean

---

### Stage 1: Generic C++ Widget Base Classes

**Goal**: Create 7 generic C++ base widget classes that eliminate duplication.

**Context Files to Read**:
- MOUIControllerBase.h, MOCommonButton.h, MOContextMenuBase.h, MOUIDelegates.h
- MORecipeListWidget.h, MOBuildingRecipeListWidget.h
- MOCraftingQueueWidget.h, MOBuildingQueueWidget.h
- MORecipeDetailPanel.h, MOBuildingDetailPanel.h
- MOHarvestProgressWidget.h, MOInspectionProgressWidget.h

**Classes to Create**:

| Class | Purpose | Replaces |
|-------|---------|----------|
| `UMOScrollListBase` | Generic scrollable list with TSubclassOf<UMOListEntryBase> | MORecipeListWidget, MOBuildingRecipeListWidget, queue widgets |
| `UMOListEntryBase` | Base entry widget with OnSelected/OnDeselected | MORecipeEntryWidget, MOBuildingEntryWidget, MOSkillEntryWidget |
| `UMODetailPanelBase` | Generic detail panel (title, desc, icon, action) | MORecipeDetailPanel, MOBuildingDetailPanel |
| `UMOProgressWidgetBase` | Progress bar with label, cancel, time remaining | MOHarvestProgressWidget, MOInspectionProgressWidget |
| `UMOConfirmationBase` | Configurable confirmation dialog | MOConfirmationDialog |
| `UMOColonyPortrait` | Character portrait for colony UI | NEW |
| `UMOPersonalityComponent` | Character personality traits | NEW |

**Also Create** `MOColonyTypes.h`:
```cpp
// EAlertState - created early to avoid circular dependency
UENUM(BlueprintType)
enum class EAlertState : uint8
{
    None,
    Notable,   // Tier 3 - colony log only
    Urgent,    // Tier 2 - orange dot
    Critical   // Tier 1 - pulsing red
};

// EPersonalityTrait - Big 3 personality dimensions
UENUM(BlueprintType)
enum class EPersonalityAxis : uint8
{
    Conscientiousness,  // Diligent <-> Adaptable
    Sociability,        // Social <-> Reserved
    Stability           // Stable <-> Volatile
};
```

**Definition of Done**:
- [ ] All 7 new .h/.cpp pairs compiling
- [ ] MOColonyTypes.h created with EAlertState, EPersonalityAxis
- [ ] UMOContextMenuBase verified covers all subclasses
- [ ] MO57Editor builds clean with no new warnings

---

### Stage 2: Python WBP Batch Generator

**Goal**: Write Python editor scripts for batch WBP creation.

**Scripts to Create**:

1. `Content/Python/create_generic_widgets.py`:
   - Accepts manifest of {cpp_class, output_path, layer_tag}
   - Creates Widget Blueprints with C++ parent
   - Skips existing assets

2. `Content/Python/create_primary_layout.py`:
   - Creates WBP_MOPrimaryGameLayout
   - Configures 5 layer stacks

**Manifest for Generic WBPs**:
```python
GENERIC_WIDGETS = [
    {"cpp_class": "UMOScrollListBase", "output": "/MOFramework/UI/CommonUI/WBP_MOScrollList"},
    {"cpp_class": "UMOListEntryBase", "output": "/MOFramework/UI/CommonUI/WBP_MOListEntry"},
    {"cpp_class": "UMODetailPanelBase", "output": "/MOFramework/UI/CommonUI/WBP_MODetailPanel"},
    {"cpp_class": "UMOProgressWidgetBase", "output": "/MOFramework/UI/CommonUI/WBP_MOProgressWidget"},
    {"cpp_class": "UMOConfirmationBase", "output": "/MOFramework/UI/CommonUI/WBP_MOConfirmation"},
    {"cpp_class": "UMOColonyPortrait", "output": "/MOFramework/UI/Colony/WBP_MOColonyPortrait"},
]
```

**Definition of Done**:
- [ ] Both scripts written and syntax-checked
- [ ] README comments explain how to run from editor console
- [ ] Scripts do NOT run automatically - user runs manually

---

## Phase 3-7: Parallel Tracks

### UI Refactor Track (3A, 4A, 5A, 6A, 7A)

See detailed prompts in original execution plan. Key goals:
- Stage 3A: Complete CommonUI Blueprint setup (WBP_MOPrimaryGameLayout, layer stacks)
- Stage 4A: Migrate scroll lists and detail panels to generic bases
- Stage 5A: Migrate context menus and progress widgets
- Stage 6A: Migrate inventory/crafting to layer push/pop
- Stage 7A: System menus + cleanup deprecated UIManager methods

### Colony Feature Track (3B, 4B, 5B, 6B, 7B)

See detailed prompts in original execution plan. Key goals:
- Stage 3B: Create FMOCharacterRelationship, FMOCharacterHistoryEntry, UMOCharacterHistoryComponent, UMOColonyManagerSubsystem
- Stage 4B: Create UMOColonyBarWidget (persistent HUD)
- Stage 5B: Create UMOColonyOverviewWidget, UMOCharacterCardWidget (replaces/extends possession menu)
- Stage 6B: Task assignment without possession (highest risk)
- Stage 7B: 4-tier alert system

---

## Phase 8-10: Integration

### Stage 8: Possession Transition Experience

Create `UMOPossessionTransitionWidget`:
- Fade in 0.3s on possession start (shows departing character)
- Fade out 0.3s on possession complete (shows arriving character + mood)
- "While you were away" summary on repossession
- Skippable with any key

### Stage 9: Relationship Simulation

Implement basic relationship mechanics:
- `UpdateRelationshipFromProximity()` - proximity builds relationships
- `AddRelationshipEvent()` - notable events affect relationships
- Surface in character card
- Relationship notes in task assignment

### Stage 10: Full Integration Test

Test script covering all systems together. Update CLAUDE.md with final architecture.

---

## Existing System APIs (Reference)

### Job Assignment API

`UMOSurvivorJobQueueComponent` already provides:
```cpp
FGuid EnqueueJob(EMOSurvivorJobType JobType, int32 RepeatCount = 1);
FGuid EnqueueJobAtLocation(EMOSurvivorJobType JobType, FVector Location, int32 RepeatCount = 1);
FGuid EnqueueJobWithTarget(EMOSurvivorJobType JobType, AActor* Target, int32 RepeatCount = 1);
bool CancelJob(const FGuid& JobId);
void CancelAllJobs();
FMOSurvivorJobEntry GetCurrentJob() const;
TArray<FMOSurvivorJobEntry> GetAllJobs() const;
```

### Colony Membership

`UMORecruitmentComponent` tracks recruitment state:
- `EMORecruitmentState::Recruited` = colony member
- `IsPossessable()` returns true for recruited pawns

### Standard UI Delegates (MOUIDelegates.h)

- `FMOUIRequestClose` - Menu close requests
- `FMOUICraftRequest` - Craft requests (RecipeId, Quantity)
- `FMOUIRecipeSelected` - Recipe selection
- `FMOUIProgressUpdate` - Progress updates
- `FMOUIVisibilityChanged` - Visibility changes

---

## UI Layer Architecture

| Layer Tag | Z-Order | Purpose | Examples |
|-----------|---------|---------|----------|
| `MO.UI.Layer.HUD` | 0 | Always-visible HUD | Reticle, Colony Bar, Mode Indicator |
| `MO.UI.Layer.Game` | 50 | Switchable gameplay menus | Inventory, Crafting, Skills, Building |
| `MO.UI.Layer.GameOverlay` | 100 | Overlays on gameplay | Station Context, Harvest Progress |
| `MO.UI.Layer.Menu` | 150 | System menus | In-Game Menu, Colony Overview |
| `MO.UI.Layer.Modal` | 200 | Modal dialogs | Confirmation, Item Context |

---

## Design Principles (From Colony Design Doc)

### Character Depth
- Characters have 3 personality dimensions (Conscientiousness, Sociability, Stability)
- Characters form relationships (Mentor/Student, Friends, Rivals, Romantic, Enemies)
- Character history is tracked and surfaced
- Trust is earned through demonstrated capability

### Alert Tiers
| Tier | Urgency | Examples | Display |
|------|---------|----------|---------|
| 1 | Critical | Health <15%, combat while away | Pulsing red, sound, cannot dismiss |
| 2 | Urgent | Health <40%, idle >30min, task failed | Persistent orange dot |
| 3 | Notable | Task complete, skill gained, relationship change | Colony log |
| 4 | Log | Routine activities | History only |

### Colony Overview Design
- Left panel: Character roster with portraits
- Center: Selected character card (mood, history, relationships, skills)
- Right panel: Task assignment interface
- Top bar: Colony health summary
- Replaces/extends possession menu (P key)

---

## Files Superseded by This Document

| Old File | Status |
|----------|--------|
| `UE57_Refactoring_Plan.md` | Superseded - priorities incorporated here |
| `consolidation-session-checklist.md` | Superseded - remaining items in Stage 7A |
| `UIManagerSplit_TestingNotes.md` | Superseded - testing checklist in this doc |
| `UI_Class_Structure_Report.md` | Reference only - class inventory |

## Files Still Active

| File | Purpose |
|------|---------|
| `MO57_Colony_Management_Design.docx` | Vision/philosophy document |
| `MO57_ClaudeCode_Plan.docx` | Detailed stage prompts |
| `PCG_Integration_Plan.md` | Separate feature track |
| `MobAIPlan.md` | Separate feature track |
| `devlog_*.md` | Historical logs (keep separate) |

---

## Progress Log

### March 28, 2026
- Consolidated planning documents
- Clarified Stage 1 dependency (EAlertState in MOColonyTypes.h)
- Confirmed job assignment API exists (UMOSurvivorJobQueueComponent)
- Confirmed colony membership tracked via UMORecruitmentComponent
- Decision: UMOPersonalityComponent for traits (Option A - decoupled)
- Decision: Colony overview replaces/extends possession menu on P key
- Decision: Placeholder portraits for now (art pass later)

**Stage 0 COMPLETE:**
- CLAUDE.md updated with colony management architecture
- Added `PythonScriptPlugin` and `EditorScriptingUtilities` to MO57.uproject
- Created `Content/Python/` directory with `init_unreal.py`
- MO57Editor builds clean (35.36s)

**Stage 1 COMPLETE:**
- Created `MOColonyTypes.h` with EAlertState, EPersonalityAxis, FMOPersonalityTraits, ERelationshipType, FMOCharacterRelationship, EHistoryEntryType, FMOCharacterHistoryEntry, EColonyActivityState
- Created `UMOPersonalityComponent` - 3-axis personality traits component
- Created `UMOListEntryBase` - Generic list entry widget base
- Created `UMOScrollListBase` - Generic scrollable list widget base
- Created `UMODetailPanelBase` - Generic detail panel widget base
- Created `UMOProgressWidgetBase` - Generic progress widget with timer support
- Created `UMOConfirmationBase` - Generic confirmation dialog base
- Created `UMOColonyPortrait` - Colony character portrait with alert pulsing
- MO57Editor builds clean (54.06s)

**Stage 2 COMPLETE:**
- Created `Content/Python/create_generic_widgets.py` - Batch creates 6 generic WBPs from manifest
- Created `Content/Python/create_primary_layout.py` - Creates WBP_MOPrimaryGameLayout with setup instructions
- Scripts include README comments for running from editor console
- Scripts skip existing assets to prevent overwrites

---

## UI Migration Plan (Stages 4A-7A Detailed)

### Migration Strategy

**Approach**: Refactor existing classes to inherit from new generic bases while preserving all functionality. Test after each phase.

**Order**: Start with simplest (progress widgets), then entries, then lists, then layer hookup.

### Phase 4A-1: Progress Widgets (Low Risk)

Both already inherit from `UCommonActivatableWidget`. Change to `UMOProgressWidgetBase`.

| Widget | Change | Risk |
|--------|--------|------|
| MOHarvestProgressWidget | Parent → UMOProgressWidgetBase | Low |
| MOInspectionProgressWidget | Parent → UMOProgressWidgetBase | Low |

**Preserve**: Wall-clock timing, component weak refs, specific delegates

### Phase 4A-2: List Entry Widgets (Medium Risk)

Change parent from `UUserWidget` to `UMOListEntryBase`.

| Widget | Change | Risk |
|--------|--------|------|
| MORecipeEntryWidget | Parent → UMOListEntryBase | Medium |
| MOBuildingEntryWidget | Parent → UMOListEntryBase | Medium |
| MOSkillEntryWidget | Parent → UMOListEntryBase | Medium |
| MOCraftingQueueEntryWidget | Check if exists, migrate | Medium |

**Preserve**: Color configurations, cached entry data, dual delegates

### Phase 4A-3: Scroll List Widgets (Medium Risk)

Change parent from `UUserWidget` to `UMOScrollListBase`.

| Widget | Change | Risk |
|--------|--------|------|
| MORecipeListWidget | Parent → UMOScrollListBase | Medium |
| MOBuildingRecipeListWidget | Parent → UMOScrollListBase | Medium |
| MOCraftingQueueWidget | Parent → UMOScrollListBase | Medium |

**Preserve**: Filtering, selection, entry creation patterns

### Phase 5A: Reconcile Detail Panel Bases

`MORecipeDetailPanelBase` already exists and works. Options:
1. Keep it as-is (it's domain-specific and works)
2. Make it inherit from `MODetailPanelBase` for consistency

**Recommendation**: Keep MORecipeDetailPanelBase, mark MODetailPanelBase for colony-specific use.

### Phase 6A: CommonUI Layer Hookup (High Impact)

Update UI controllers to use layer push/pop instead of AddToViewport:

| Controller | Widgets to Update | Target Layer |
|------------|-------------------|--------------|
| MOInventoryUIController | InventoryMenu, UnifiedInventoryMenu | Game |
| MOCraftingUIController | CraftingMenu | Game |
| MOBuildingUIController | BuildingMenu | Game |
| MOCharacterUIController | SkillsPanel, StatusPanel | Game |
| MOSystemMenuUIController | InGameMenu | Menu |
| All Controllers | Context menus | Modal |
| All Controllers | Progress widgets | GameOverlay |

### Phase 7A: Cleanup

- Remove duplicate delegate patterns (keep standard, remove legacy)
- Delete deprecated MOModalBackground if layer system handles it
- Update CLAUDE.md with final patterns

---

**Stage 3A COMPLETE:**
- Created `UMOUISettings` - Developer Settings for UI configuration (Project Settings -> Plugins -> MO UI Settings)
- Updated `UMOGameUIManagerSubsystem` to load PrimaryLayoutClass from settings
- Added `NotifyPlayerAdded()` hook in `AMOPlayerController::BeginPlay()`
- WBP_MOPrimaryGameLayout created and configured in Project Settings
- **Python Widget Automation Discovery:**
  - `EditorUtilityLibrary.add_source_widget(wbp, widget_class, name, parent)` adds widgets to WBP hierarchy
  - `EditorUtilityLibrary.find_source_widget_by_name(wbp, name)` finds existing widgets
  - TextBlock.set_text(), ProgressBar.set_percent() work for property configuration
  - MOCommonButton can be added via `unreal.load_class(None, "/Script/MOFramework.MOCommonButton")`
  - Widget sizes/anchors need manual Designer work
- Created Python scripts:
  - `Content/Python/setup_widget_bindings.py` - Adds widget hierarchies to WBPs
  - `Content/Python/configure_all_widgets.py` - Sets default text/properties on widgets
  - `Content/Python/fix_missing_buttons.py`, `try_widget_tree.py`, `try_editor_utility.py` - Exploration scripts
- All 6 generic WBPs configured and validated:
  - WBP_MOConfirmation, WBP_MOProgressWidget, WBP_MODetailPanel
  - WBP_MOListEntry, WBP_MOScrollList, WBP_MOColonyPortrait
