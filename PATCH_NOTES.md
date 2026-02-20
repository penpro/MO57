# MO57 Patch Notes

This file tracks changes, bug fixes, and new features. Updated incrementally to preserve history across development sessions.

---

## [2026-02-20] Quest Framework

### New Features

**Quest System Core**
- Added `UMOQuestSubsystem` - GameInstance subsystem for quest management
- Data-driven quest definitions via `DT_Quests` DataTable
- Event-based objective completion system
- Support for sequential and parallel objectives
- Auto-start quests when prerequisites are met
- Quest state persistence (save/load support)

**Quest Types**
- `FMOQuestDefinitionRow` - DataTable row for quest definitions
- `FMOQuestObjective` - Single objective with type, target, and count
- `FMOQuestState` - Runtime quest progress tracking
- `EMOObjectiveType` - Event, ItemCraft, ItemPickup, ItemDrop, SkillLevelUp, LocationReach, Custom

**Quest UI Widgets**
- `UMOQuestHUDWidget` - HUD objective tracker showing tracked quests
- `UMOQuestTrackerEntry` - Single quest entry on HUD
- `UMOQuestLogPanel` - Full quest log (Common UI panel)
- `UMOQuestLogEntry` - Quest list entry with selection

**Quest Delegates**
- `OnQuestStarted`, `OnQuestCompleted`, `OnQuestAbandoned`
- `OnObjectiveUpdated`, `OnObjectiveCompleted`
- `FireGameEvent()` for custom event triggers

### Integration

- Quest data automatically saved/loaded with world saves
- Hooks into crafting system (`OnCraftCompleted`)
- Skill level up detection (`OnSkillLevelUp`)
- Configurable via Project Settings > Game > Quest System

### Blueprint Setup Required

1. **Create `DT_Quests` DataTable** using `FMOQuestDefinitionRow` struct
2. **Configure Quest Settings** in Project Settings > Game > Quest System
   - Set `QuestDefinitionTable` path
   - Set `MaxTrackedQuestsOnHUD` (default 3)
   - Enable `bAutoStartTutorials` for tutorial quests
3. **Create Blueprint Widgets**:
   - `WBP_QuestTrackerEntry` (parent: `UMOQuestTrackerEntry`)
   - `WBP_QuestHUDWidget` (parent: `UMOQuestHUDWidget`)
   - `WBP_QuestLogEntry` (parent: `UMOQuestLogEntry`)
   - `WBP_QuestLogPanel` (parent: `UMOQuestLogPanel`)
4. **Add HUD Widget** to your HUD Blueprint

### Example Tutorial Quest

```
Row Name: Tutorial_FirstCraft
QuestId: Tutorial_FirstCraft
DisplayName: "First Steps"
Description: "Learn the basics of crafting."
bIsTutorial: true
bAutoStart: true
Prerequisites: []
Objectives:
  - ObjectiveId: "OpenCraftMenu"
    Description: "Open the crafting menu"
    Type: Event
    TargetEventOrId: "CraftingMenuOpened"
    RequiredCount: 1
  - ObjectiveId: "CraftStick"
    Description: "Craft a stick"
    Type: ItemCraft
    TargetEventOrId: "Stick01"
    RequiredCount: 1
SortOrder: 1
```

### Technical Notes

- Quest subsystem is GameInstance-scoped (persists across level loads)
- UI widgets are abstract and require Blueprint children with bound widgets
- Non-blocking design - players can ignore tutorials
- FireGameEvent() enables custom objective triggers from Blueprint

---

## [2026-02-19] Terrain-Aware Spawning, Voxel Seed System & PCG Optimization Tools

### New Features

**Beach Spawn System**
- Characters now spawn on beaches instead of mountain peaks
- Configurable height range filters spawn locations to terrain between 100-3000cm above water level
- Algorithm searches expanding rings to find the lowest valid terrain
- Fallback system prefers lower elevations when no ideal beach is found

**Voxel Height Graph Seed Parameter Support**
- Added `ApplySeedToHeightGraphParameter()` - Sets seed parameter on Voxel height graphs before terrain generation
- New `VoxelSeedParameterName` property (default "Seed") - Configurable parameter name to match your Voxel graph
- Iterates loaded `UVoxelHeightGraph` assets and `UVoxelHeightLayer` layers to set seed values
- Works with Voxel Plugin Pro 2.0's `FVoxelExposedSeed` type

**PCG HISM Tools** *(Editor/PIE only)*
- Added `UMOHISMCullingSubsystem` - World subsystem that periodically refreshes tagged PCG actors
- Added `MO Force HISM Tree Build` PCG node - Forces HISM tree rebuilding after mesh spawning
- Tag-based filtering allows selective refresh of specific PCG volumes (e.g., "FarTreesPCG")

### Improvements

**MOGameMode Configuration**
- New `MaxSpawnHeightAboveWater` property (default 3000cm) - defines beach ceiling
- New `MinSpawnHeightAboveWater` property (default 100cm) - defines beach floor
- Improved spawn search logging for debugging terrain detection
- Voxel seed integration documented for procedural world generation

### Bug Fixes

- Fixed spawn algorithm preferring highest terrain instead of lowest
- Fixed search loop exiting on first land hit instead of continuing to find beaches
- Fixed fallback spawn using mountain peaks instead of lowest available terrain
- Fixed crafting menu retaining station name and fuel display after closing (now properly resets to "Hand Crafting")
- Fixed voxel terrain not regenerating when loading a saved game (world seed now persisted and applied on load)
- Added `GetWorldSeedAsVoxelString()` Blueprint function for Voxel graphs (returns 8-char A-Z format)

### Technical Notes

- PCG HISM refresh features use editor-only APIs and will not function in packaged builds
- For runtime distance culling, use Voxel Plugin's scatter system with RenderDistance nodes or UE's built-in shadow distance settings
- `InstanceMinDrawDistance` on HISM components does not work as documented - this is a known UE limitation

**Voxel Graph Seed Setup:**
To use dynamic world seeds with Voxel terrain:
1. Open your Voxel heightmap graph in the editor
2. Delete any hardcoded "Get Seed From Game Settings" nodes
3. Create a new **Parameter** (right-click > Add Parameter):
   - Name: `Seed` (or match `VoxelSeedParameterName` in game mode)
   - Type: `Seed` (FVoxelExposedSeed)
4. Connect this parameter output to your noise/generation nodes
5. Ensure `VoxelWorld->bCreateRuntimeOnBeginPlay = false` in your level
6. Enable `bAutoInitializeVoxelWithSeed = true` on your game mode
7. The seed from New Game dialog will now affect terrain generation

---

## [2026-02-18] Critical Audit Fixes

### Bug Fixes

- Fixed logging and debug code issues identified in code audit
- Removed marketplace assets and build artifacts from git tracking
- Enabled Git LFS for large binary files
- Added large asset directories to .gitignore

---

## Template for Future Entries

```
## [YYYY-MM-DD] Title

### New Features
- Feature description

### Improvements
- Improvement description

### Bug Fixes
- Bug fix description

### Technical Notes
- Any important technical context
```
