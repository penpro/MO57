# MO57 Patch Notes

This file tracks changes, bug fixes, and new features. Updated incrementally to preserve history across development sessions.

---

## [2026-02-21] Camera Shoulder Toggle, Loading Overlay & Inventory QOL

### New Features

**Camera Shoulder Toggle**
- New `IA_View` input action to toggle camera between left/right shoulder
- Smooth animated transition (0.5s default, configurable via `CameraTransitionDuration`)
- Camera Y offset now controlled via FollowCamera relative location (not CameraBoom SocketOffset)
- Ease-out curve for polished transition feel
- Supports interrupting mid-transition to reverse direction

**Widget-Based Loading Overlay**
- New `UMOLoadingOverlay` widget replaces MoviePlayer-based loading screen
- Full-screen black overlay with optional loading text
- Smooth fade-out animation when dismissed
- Loading screen persists until pawn has landed safely on terrain
- Added `bIsLoadingIntoGameplay` flag to `UMOGameSettings` for tracking gameplay transitions
- Added `DismissLoadingScreen()` method to `UMOGameInstance` for manual dismissal
- Added `ShouldShowLoadingScreen()` to skip loading screen during initial launch to intro

**Pawn Landing Detection**
- `CheckPawnLanded()` polls pawn's movement component to detect when character is grounded
- `OnPawnLandedSafely()` dismisses loading screen and clears transition flag
- Works for both new game spawns and loaded game pawn re-grounding

**Inventory Quality of Life**
- Double-click items to transfer between inventories (same as shift-click)
- Configurable double-click threshold (default 0.3s)

### Bug Fixes

- **Fixed loading screen ending too early**: Loading screen now waits for pawn to land on ground instead of dismissing when map loads
- **Fixed seeing terrain generation**: Players no longer see voxel terrain loading or pawn falling during new game start
- **Fixed seeing pawn repositioning**: Loading screen now covers the re-grounding of loaded pawns after voxel regeneration
- **Fixed loading screen during intro**: Initial game launch now stays black until intro video starts (no loading screen flash)
- **Fixed character mesh deformation around neck**: Resolved mesh distortion issue in the neck area of character models
- **Fixed inventory grid showing too many slots**: Grid now shows exact slot count from inventory component instead of padding to MinimumVisibleSlotCount

### Technical Details

**Camera Transition:**
- `ToggleCameraShoulder()` initiates transition by setting target Y and resetting alpha
- `Tick()` lerps camera position using ease-out curve: `1 - (1 - alpha)²`
- Transition state tracked via `bIsCameraTransitioning`, `CameraTargetY`, `CameraStartY`, `CameraTransitionAlpha`

**Loading Screen Flow:**
1. New Game/Load Game → set `bIsLoadingIntoGameplay = true`
2. `BeginLoadingScreen()` detects flag → shows `UMOLoadingOverlay` widget
3. Map loads → `EndLoadingScreen()` does NOT dismiss (manual mode)
4. GameMode waits for voxel → spawns pawn → starts landing check timer
5. Pawn lands → `OnPawnLandedSafely()` → `DismissLoadingScreen()` → fade out

### Blueprint Setup Required

**Loading Overlay:**
1. Create `WBP_LoadingOverlay` (parent: `UMOLoadingOverlay`)
2. Add `BackgroundImage` (Image widget, full screen black)
3. Add `LoadingText` (TextBlock, optional)
4. In `BP_MOGameInstance`, set `LoadingOverlayClass` to `WBP_LoadingOverlay`

**Camera Toggle:**
1. `IA_View` input action already created
2. Bind to desired key in `IMC_MODefault` (e.g., V key)
3. `ViewAction` property in `BP_MOPlayerController` should reference `IA_View`

---

## [2026-02-21] Weather Integration, Mid-Game Load Fix & Input Lockout

### New Features

**Weather Provider Interface Overhaul**
- Simplified `IMOWeatherProviderInterface` to use native UDS/UDW types directly
- `GetDateTime()` returns `FDateTime` directly from UDS (replaces `GetTimeOfDay()`)
- `GetCurrentWeatherPreset()` returns `UObject*` (actual UDS_Weather_Settings) instead of `FName`
- `SetDateTime(FDateTime)` replaces separate `SetTimeOfDay()`, `SetSeason()`, `SetDayOfYear()` methods
- `SetWeatherPreset(UObject*)` for direct weather preset application
- Added `BuildWeatherSaveData()` and `ApplyWeatherSaveData()` for persistence

**Weather Save/Load Support**
- `FMOWeatherSaveData` struct for persisting weather and time state
- Stores `FDateTime`, `CloudCoverage`, `FogDensity` overrides
- `WeatherPresetObject` marked Transient (runtime only, not serialized to disk)
- Integrated with `MOPersistenceSubsystem` for automatic save/load
- Pending save data system: weather state queued if provider not yet registered, applied when provider registers

**Mid-Game Save Loading**
- Voxel world now properly regenerates when loading a save with a different seed
- `InitializeVoxelWorldWithSeed()` destroys and recreates voxel runtime for mid-game loads
- New `RegroundAllPawns()` function repositions all characters to terrain after voxel regeneration
- Waits for voxel collision generation before re-grounding (3 second delay)

### Bug Fixes

- **Fixed Blueprint interface detection**: `TScriptInterface::GetInterface()` returns null for Blueprint-implemented interfaces - changed to `ImplementsInterface()` which works for both C++ and Blueprint implementations
- **Fixed weather restore timing**: Weather save data now stored as pending if provider not registered yet, applied when `RegisterWeatherProvider()` is called
- **Fixed mid-game load terrain mismatch**: Loading a save from a different world now regenerates voxel terrain with correct seed before repositioning pawns
- **Fixed gameplay input during menus**: All gameplay actions now blocked when menus are open:
  - Jump (start/end)
  - Hustle/Sprint (start/triggered/end)
  - Crouch
  - Interact
  - Primary action (press/release)
  - Secondary action (press/release)
  - Terraform (toggle/cycle)
- **Fixed split stack not working**: Split stack was immediately re-stacking items because `AddItemByGuid` auto-stacks into existing entries of the same item type. Added `AddItemByGuidNoStack()` method that forces creation of a new stack entry without auto-stacking behavior.

### Blueprint Setup (BP_WeatherBridge)

1. Create actor implementing `IMOWeatherProviderInterface`
2. In BeginPlay: Get UDS/UDW actors → store in variables → delay → register with subsystem
3. Add validity checks in interface implementations (UDWActor and UDWActor.Weather may be null during init)
4. `BuildWeatherSaveData`: Get DateTime from UDS, CloudCoverage/Fog from current state
5. `ApplyWeatherSaveData`: Set DateTime on UDS, apply cloud/fog overrides to UDW

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

## [2026-02-18] Creature AI System & Character Appearance Framework

### New Features

**Creature AI System**
- `AMOCreature` base class with full medical system inheritance (wounds, vitals, mortality)
- `AMOCreatureController` with sight/hearing perception and threat memory
- Activity states: Active, Resting, Sleeping, Fleeing, Fighting, Dead
- Day/night behavioral cycles (creatures rest at dusk, sleep at night)
- Footstep noise generation for AI hearing awareness
- Hit reaction and death animation montage support

**Behavior Tree Components**
- `BTTask_FleeFromThreat` - Flee behavior with configurable distance
- `BTTask_CreatureWander` - Random wandering with return-to-home radius
- `BTTask_CreatureAttack` - Attack behavior with damage and cooldowns
- `BTTask_CreatureRest` - Rest/sleep state management
- `BTService_CreatureActivity` - Monitors time of day for rest/sleep cycles

**Character Appearance Framework**
- `UMOAppearanceSubsystem` - Centralized appearance management
- `UMOCharacterAppearance` component for per-pawn visual customization
- `AMOCustomizableCharacter` with MetaHuman integration support
- Genesis 8 character model compatibility
- MetaHuman common assets integration

**Fall-Through Safety System**
- `CheckFallThroughSafety()` in MOCharacter detects characters falling through terrain
- Teleports character back above ground after 2 seconds of falling with no terrain below
- Prevents soft-locks from voxel terrain loading gaps

### Bug Fixes

- Fixed AI perception ConfigureSense warnings by deferring calls to next tick
- Added DataTable caching to `MOSkillDatabaseSettings` to prevent repeated load log spam
- Increased spawn height offset to 200 units for safer terrain landing

### Blueprint Setup Required

**Creature Animation Blueprint:**
1. Add states: Locomotion, Resting (IdleRest), Sleeping (IdleSleep), Death
2. Add variables: `GroundSpeed` (float), `IsDead`, `IsResting`, `IsSleeping` (bool)
3. State transitions based on activity state variables

**Creature Blackboard:**
- Add keys: `ActivityState` (enum), `IsDead`, `IsResting`, `IsSleeping`

**Creature Behavior Tree:**
- Add `BTService_CreatureActivity` to root
- Add rest/sleep branches with time-of-day decorators

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
