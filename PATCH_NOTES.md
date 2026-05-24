# MO57 Patch Notes

This file tracks changes, bug fixes, and new features. Updated incrementally to preserve history across development sessions.

---

## [2026-05-24] Weather/Shelter Model + UDS Bridge Read-Side

Weather is now actually wired up. The MO clock drives UDS time-of-day, UDS/UDW report current weather state back through `BP_WeatherBridge`, and a new multi-axis shelter model gives survival systems the granular environmental data they need.

### Multi-Axis Exposure Shelter Model

Survival exposure isn't a single "are you sheltered?" bool. A bus-stop overhang blocks rain but does nothing for wind; a doorway recess blocks wind but leaves you in the rain; a closet does both. The new `FMOExposureShelter` struct tracks three independent axes:

- **OverheadCover** (0–1) — fraction of upper hemisphere blocked. Drives rain/snow protection and sky radiative heat loss. 9 long rays at 50m, 35° hemisphere sampling.
- **WindShelter** (0–1) — distance-weighted blocking from the upwind direction. A wall 1m upwind scores higher than a wall 14m upwind. 5-ray cone in the upwind direction. Drives wind/dust/wind-chill.
- **Enclosure** (0–1) — fraction of close (<3m) geometry on all sides. Drives "indoors" detection for thermal/insulation systems. 4 horizontal cardinal rays + recount of close overhead hits.

Plus convenience bools (`bIsCovered`, `bIsWindSheltered`, `bIsIndoors`) and raw hit counts for callers wanting custom thresholds.

### Two BP-Callable Test Functions

- **`Test Exposure Shelter (Full)`** — 18 traces, fills all three axes. Takes a wind direction so wind shelter is direction-aware. Use for characters and AI survivors. ~1-2Hz tick rate per actor.
- **`Test Overhead Cover (Cheap)`** — 9 traces, overhead only. Use for fires, dropped items, crops — anything that only cares about rain/snow protection. ~half the cost.

Both in `UMOWeatherBlueprintLibrary` (`MO|Weather|Shelter` category in BP picker).

### Continuous Modulation Helper

**`Make Weather Exposure (from UDW + Shelter)`** — combines raw UDW values with a shelter result into `FMOWeatherExposure`. Continuous, not binary: partial canopy produces partial wetness.

- Rain = raw × (1 - OverheadCover) — overhead is primary
- Snow = raw × (1 - OverheadCover) — same as rain
- Wind = raw × (1 - WindShelter) — upwind-aware
- Dust = raw × (1 - WindShelter) — dust is wind-borne

Composite `bIsSheltered` = `bIsIndoors OR (bIsCovered AND bIsWindSheltered)` for simple consumers.

### UDS Time-of-Day Sync (from MO Clock)

- **Hourly delegate sync** — `UMOWeatherIntegrationSubsystem` listens to `MOGameClockSubsystem::OnHourChanged` and pushes time-of-day to UDS at every game-hour rollover.
- **Throttled poll fallback** — between hour ticks, a 0.25s real-time poll triggers a sync if more than ~1 game-minute has elapsed since the last successful sync. Keeps motion smooth at high TimeScale without per-tick overhead.
- **Menu-open mask** — sync pauses while menus are open so the player doesn't see sun jumps mid-conversation.
- **Helper: `Date Time → UDS Time Of Day`** — converts an FDateTime to UDS's 0-2400 format (hour×100 + minute×100/60).
- **Helper: `Date Time → Day Of Year`** — converts to UDS's 1-366 day-of-year for sun-path calculation.

**IMPORTANT:** UDS actor must have `bAnimateTimeOfDay = false` for manual control to work. Otherwise UDS overwrites our value every tick.

### BP_WeatherBridge Read-Side Wired

`BP_WeatherBridge` implements `IMOWeatherProviderInterface` by translating to UDS/UDW native API calls. No state stored in the bridge — every query reads fresh from UDS/UDW. Wired so far:

- `GetGlobalTemperature` — UDW `Get Current Temperature` (Celsius hardcoded; MO is Celsius-only internally despite UDW defaulting to Fahrenheit)
- `GetTemperatureAtLocation` — UDW `Get Current Temperature` with custom sample location
- `GetCurrentWeatherState` — UDW `Get UDS Values Controlled by UDW` + `Get Normalized Wind Direction` → `MakeRotFromX`
- `GetWeatherDisplayName` — UDW `Get Display Name for Current Weather` → string-to-text
- `GetCurrentWeatherPreset` — UDW `Weather` variable
- `GetWindVelocityAtLocation` — wind direction × intensity × tunable `MaxWindSpeedCmPerSec` (default 2000 cm/s ≈ 72 km/h)
- `GetWeatherExposureAtLocation` — multi-axis shelter test + UDS values → modulated exposure
- `SetDateTime` — wired to UDS native "Set Date and Time"

Remaining: `IsLocationSheltered`, `IsDaytime`, `GetDateTime`, `BuildWeatherSaveData`, `ApplyWeatherSaveData`.

### Engineering Notes

- Shelter traces use `ECC_Visibility`. Foliage/canopy meshes that should count as cover must be configured to block visibility traces. If a tree shows zero overhead cover when you're standing under it, that's a collision-channel issue, not a math issue.
- All weather/shelter math is in C++ for testability and performance — BP only orchestrates the calls.
- The shelter model deliberately doesn't apply `Enclosure` to the four exposure channels — that's for the thermal/insulation system to consume separately. Keeps responsibility clean.

---

## [2026-05-21] Building System Overhaul + Critical Save/Load Fix

This is a big update — the building system is now fully featured, the packaged-build save/load bug that broke terrain regeneration is gone, and the editor-version-only 2-second hitching is gone. Highest-impact patch since the survivor system landed.

### New Features

**Full Building System**
- **Floor → Wall → Half-Wall → Roof → Roof Peak** snap-together construction with intelligent edge/face detection
- **Q/E rotation** cycles through the four cardinal snap edges of any target piece
- **Mouse wheel in-place flip** — 90° per click for floors (4 orientations), 180° per click for walls/roofs (2 orientations). Rotates around the cube center so position stays put while orientation mirrors. Use for triangular walls, asymmetric pieces, or just to pick which side faces inward.
- **Per-piece smart snap modes**:
  - *Floors*: edge-to-edge, end-to-end, overhang on wall edges
  - *Walls*: stack above, stack below, side-to-side, end-to-end extension along the long axis
  - *Half-walls*: vertical stacking via cube-edge math, so two half-walls = one full wall height with no gap, AND can mix-and-match with full walls
  - *Roofs*: rest on top of floor/wall with eave-on-wall alignment; tile end-to-end up the slope (rise = run for 45° pieces); tile side-to-side coplanar
  - *Roof Peaks*: same snap rules as roof tiles (symmetric)
- **Cursor-driven direction selection** for asymmetric pieces — point at the side of a wall where you want the roof to extend, hover the floor edge you want a wall to sit on, etc. Less fiddly than fixed snap-point cycling.

**New Recipes Added** (all 1-stick recipes for testing):
- `BuildStickFloor`, `BuildStickWall`, `BuildStickRoof`, `BuildStickRoofPeak45`, `BuildStickWallHalf`
- Survivor Campfire (placed near recruited survivors)

**Voxel DataTable JSON Utility** (`Tools/ue_json_utils.py`)
- Auto-detects encoding (UTF-8 vs UTF-16-LE with BOM) for compatibility with UE's exporter
- Subcommands: `list`, `get`, `has`, `add`, `add-file`, `delete`, `update`
- Documented as a Claude Code skill at `.claude/skills/ue-datatable-json/SKILL.md`
- Used for adding building recipes without round-tripping through the DataTable editor

### Critical Bug Fixes

**Voxel Terrain Seed Now Persists Across Packaged-Build Saves**
The most painful bug we've had: in packaged builds, save+load would regenerate the terrain with a different seed even though the seed was being captured. Player would respawn standing on flattened terrain at sea level with the rest of the world towering ~100m above.

Root cause was discovered through layered diagnostics: our code was setting the `Seed` parameter on the shared `UVoxelHeightGraph` *asset*, but the level uses a `FVoxelHeightGraphStamp` (inside a `UVoxelStampComponent`) that has its *own* `FVoxelParameterOverrides` map which takes precedence. The stamp's override was untouched, so the runtime read the cooked-default seed.

Fixes applied:
- `ApplySeedToHeightGraphParameter` now force-loads every cooked `UVoxelHeightGraph` via Asset Registry before iterating, fixing the original "0/0 graphs updated" issue in shipping builds where assets load lazily
- `ApplySeedToVoxelStamps` now casts `FVoxelStampRef` to `FVoxelHeightGraphStamp` and calls `SetParameter("Seed")` on the stamp's own `IVoxelParameterOverridesOwner` interface — *this* is what the user's screenshot pointed at and what the runtime actually reads
- `AVoxelWorld::bCreateRuntimeOnBeginPlay` is disabled at game-mode start, and any pre-existing runtime is destroyed, so the runtime can't auto-create with a stale seed before our save-loaded seed is applied
- `StampRef.Update()` called after parameter change so the runtime re-evaluates the stamp

**Auto-Possess Saved Pawn on Load**
After load, the spectator camera was being placed 50m above the saved pawn's location, leaving the player floating as a sky-cam. Now the game finds the pawn matching the saved `LastPossessedPawnGuid`, possesses it on the player controller after voxel terrain is ready, and the player resumes IN their character at the saved location.

**Mobs and Survivors No Longer Spawn in Trees**
`MOSpawnManagerSubsystem`'s ground traces used `LineTraceSingleByChannel` which would stop at the first hit — often a PCG-spawned tree/rock ISM, leaving creatures perched in canopies. Replaced with `LineTraceMultiByChannel` that walks the hit list and lands on the first `AVoxelWorld` hit. Applies to both mob/survivor spawn search and survivor-beacon ground-snap.

**Building Snap Bugs Fixed**
- Half-wall stacking — `ZDelta = SnapStackZOffset × 2` formula assumed the offset equals half-height (true for full walls, false for half-walls), producing a 50cm gap. Replaced with cube-edge-to-cube-edge math (ghost cube bottom = target cube top). Works for any wall height and any cube-pivot offset; mixed half-on-full and full-on-half stacking is automatic.
- `bAbove` partition for stack-above vs stack-below was comparing against the actor pivot, which is at the cube TOP for half-walls — so cursor in the lower half of the cube was always being treated as below-cube. Changed to compare against the cube center instead.
- BELOW vertical stack was a fallback-only path; cursor aimed at a wall's lower portion couldn't reliably trigger it because nearby floor-edge HorizStack would always win first. Now BELOW fires as a *primary* snap when the cursor is below the cube bottom (plus a 25cm tolerance into the cube), and stays as a fallback for the ambiguous mid-cube case.
- Wall-to-wall end-to-end snap previously didn't exist (every wall-on-wall went through vertical stack). Added Coplanar extension: cursor past a wall's long-axis end → new wall lines up end-to-end on the same Z plane.
- Roof-on-roof end-to-end tiling now uses `2 × eave-axis extent` for tile pitch instead of bounding-box extent (which is inflated by the 45° tilt), so the user-intuitive "100cm" tile size matches actual placement. Z rise = horizontal run for 45° slopes.
- Roof Z direction was inverted (clicking peak side spawned roof below instead of above); flipped the sign.
- Side-to-side roof snap from below (looking up at a roof's underside) now works via cube-relative cursor normalization.

### Performance

**Terrain Sweep 2-Second Hitching — Eliminated**
Diagnostics on the user's reproduction case showed sustained 3fps (6 frames in 2 seconds) during the periodic terrain modification sweep. Two passes of optimization:
- Replaced `TActorIterator<AActor>` (walked 5,400 actors per sweep to find ~30 ISMs) with `TObjectIterator<UInstancedStaticMeshComponent>` (iterates ISMs directly). Cuts iteration cost by ~6×.
- Replaced per-instance transform-fetch + `IsLocationModified` loop (10,000+ calls per sweep) with `ISM->GetInstancesOverlappingSphere()` per zone, which uses the ISM's internal spatial index. Calls drop from thousands to dozens.
- Added `SweepPlayerRange = 2500cm` config — zones beyond 25m from any local player are filtered out before the sweep starts. Foliage that far away isn't visible (grass fade distance is much shorter), so respawned instances there don't matter until the player approaches.
- Added `TRACE_CPUPROFILER_EVENT_SCOPE(MOTerrainSweep)` so the sweep cost shows up in Unreal Insights.

Result: sweep no longer dominates frame time, no visible hitching during normal play.

**Snap-Eval Log Spam Demoted**
The per-target "Snap eval" log line was firing for every buildable in the world every tick while in placement mode (~2,000 lines/sec with ~30 buildables at 60fps). Demoted to `Verbose` so it no longer drives log file flushes / Output Log UI repaints. Re-enable for snap debugging via `Log LogMOFramework Verbose` in console. The actual snap-fire logs (VertStack, RoofSnap, WallEndToEnd, etc.) stay at `Log` level since they only fire when something matches.

### Engineering Principles Codified

Updated `CLAUDE.md` with 10 explicit engineering principles enforced for all code changes:
1. Trace bugs to the layer that creates the bad state, not where it surfaces
2. If N systems need the same behavior, build one abstraction — not N copies
3. A comment claiming behavior doesn't make it true — verify
4. Read the whole flow before changing one node
5. When the cause is unclear, add diagnostic logging — don't guess and patch
6. Failure modes must be distinguishable at the data layer
7. Defaults are policy
8. "It compiles" is not "it works"
9. Production-ready means extension without modification
10. Diagnose before designing the fix

These guide every subsequent change.

### Technical Notes

**New / Heavily-Modified Files:**
- `MOBuildingComponent.cpp/h` — major rewrite of snap math, in-place flip, per-piece-type snap modes
- `MOTerrainModificationSubsystem.cpp/h` — sweep optimization + player-range filter + Insights tracing
- `MOGameMode.cpp` — voxel seed pipeline (stamp + graph + asset-registry preload + AVoxelWorld auto-create lockout), auto-possess on load, voxel-only safe spawn trace
- `MOPersistenceSubsystem.cpp/h` — `LastPossessedPawnGuid` surfaced on `FMOLoadResult`, diagnostic logging at every seed-flow checkpoint
- `MOSpawnManagerSubsystem.cpp` — multi-trace + voxel filter for mob/survivor/beacon ground snap
- `MOBuildableActor.h` — `bSnapAsWall`, `bSnapAsRoof`, `SnapStackZOffset` UPROPERTYs for BP-author intent
- `MOPlayerController.h/cpp` — `FlipPlacementYawAction` input action + handler

**New Blueprint Assets:**
- `BP_BuildableRoofBase` (45° roof tile)
- `BP_BuildableRoofPeak45` (peaked roof apex)
- `BP_BuildableWallHalf` (half-height wall)
- `BP_SurvivorCampfire`
- `IA_FlipPlacementYaw` (input action — mouse wheel up/down)
- `BP_GameplayInputStub`

**New Recipes:** `BuildStickFloor`, `BuildStickWall`, `BuildStickRoof`, `BuildStickRoofPeak45`, `BuildStickWallHalf`

**Blueprint Setup (if any pieces don't behave correctly):**
- On each wall BP: `bSnapAsWall = true`, `SnapStackZOffset = 50.0` (or 25.0 for narrower stacking distance)
- On each roof BP: `bSnapAsRoof = true` AND `bSnapAsWall = true` (so it qualifies as "tall" for snap detection)
- `IA_FlipPlacementYaw` mapped to `Mouse Wheel Axis` (or both Up + Down) in `IMC_Building`
- Player Controller's `FlipPlacementYawAction` slot points at `IA_FlipPlacementYaw`

---

## [2026-03-01] Survivor Command Overhaul & Wolf Predators

### New Features

**Enhanced Survivor Context Menu**
- **Set Home Button**: Right-click survivor → "Set Home" to mark current location as their home
- **Inventory Button**: Right-click survivor → "Inventory" to open item exchange between player and survivor
- **Dead Survivor Handling**: All command buttons disabled when survivor is dead, except Inventory (loot the body)
- Shows "(Dead)" suffix on survivor name when deceased

**Automatic Return Home After Jobs**
- Survivors now automatically return to their assigned home location after completing assigned jobs
- New `BTTask_SurvivorGoHome` behavior tree task with:
  - Stuck detection (considers arrived if stuck near home for 3+ seconds)
  - Graceful failure if no home assigned
  - Configurable acceptance radius
- BT_Survivor updated with auto-return-home as final step after job completion

**Improved Foraging AI**
- Survivors assigned to forage now actually navigate to ground items and pick them up
- Replaced random walk (Brownian motion) with proper item-seeking behavior:
  - Queries ForagingSubsystem for nearby HISM ground items
  - Navigates to nearest item within search radius
  - Picks up item, adds to inventory, removes HISM instance
  - Repeats until target count reached or no items found
- Stuck detection prevents infinite loops on unreachable items

**Wolf Predator**
- Added wolf creature with full animation set:
  - Locomotion: Walk, Run, Turn animations with root motion variants
  - Combat: Bite, Jump Bite, Run Bite attacks
  - Idle: Breathe, Look Around, Aggressive stance
  - Rest: Go To Rest, Rest, Sleep, Wake Up
  - Reactions: Hit reactions (Front, Left, Right), Death, Howl
- Three material variants: Standard, Arctic (white), Dark
- Fur material variants for enhanced visual quality
- Full skeletal mesh with physics asset

**Carcass System**
- Added DT_Carcasses DataTable for creature carcass definitions
- Carcass spawning on creature death

### Bug Fixes

**Ragdoll Physics Fixed**
- Characters now properly ragdoll on death instead of freezing in falling pose
- Fixed by using `SetAllBodiesSimulatePhysics(true)` instead of `SetSimulatePhysics(true)`
- Added `SetAllBodiesPhysicsBlendWeight(1.0f)` for full physics blend
- Collision profile properly set to "Ragdoll" on death
- All animation montages stopped before enabling ragdoll

**Survivor Controller Spawning**
- Context menu now spawns SurvivorController on-demand for recruited survivors
- Fixes issue where recruited survivors had no AI controller after being unpossessed
- Logs controller type for debugging

### Technical Notes

**New Files:**
- `BTTask_SurvivorGoHome.h/cpp` - Return home behavior tree task
- `BTTask_ChaseTarget.h/cpp` - Chase target behavior tree task
- `MOGatheringSettingsActor.h/cpp` - Gathering configuration actor
- `MOSpawnSettingsActor.h/cpp` - Spawn configuration actor
- Wolf assets: SK_Wolf mesh, ABP_Wolf animation blueprint, BS_Wolf blend space
- Wolf animations: 25+ animation assets with root motion variants
- Wolf materials: 6 material variants (body + fur × 3 color schemes)

**Modified Files:**
- `MOSurvivorContextMenu.h/cpp` - Added Set Home, Inventory buttons and dead state handling
- `MOSystemMenuUIController.h/cpp` - Added inventory delegate handling
- `BTTask_SurvivorForage.h/cpp` - Complete rewrite for actual item navigation
- `MOCharacter.cpp` - Fixed ragdoll physics on death
- `MOCreature.cpp` - Creature death and carcass improvements
- `MOCreatureController.cpp` - Predator AI improvements

**Blueprint Setup Required:**
1. Update `WBP_SurvivorContextMenu` to add:
   - `SetHomeButton` (UMOCommonButton)
   - `InventoryButton` (UMOCommonButton)

2. `BT_Survivor` already updated with auto-return-home behavior

---

## [2026-02-24] DataTable-Driven Resource Node System

### Overview

Complete refactoring of the PCG resource spawning system to use DataTables as the single source of truth for resource definitions. All tags, meshes, and yields are now automatically generated from DataTable entries, eliminating manual tag configuration in PCG nodes.

### New Files

**MOResourceNodeDefinitionRow.h**
- `EMOResourceType` enum: Generic, Tree, Bush, Rock, Ore, Plant
- `FMOResourceMeshVariation` - Mesh variation with weight and optional material override
- `FMOResourceNodeDefinitionRow` - DataTable row for harvestable resources
  - DisplayName, Description, ResourceType
  - YieldsItems array for `Gives_{ItemId}` tag generation
  - ResourceTags for job system matching (Wood, Stone, Fiber, etc.)
  - MeshVariations array with weighted random selection
  - MinScale/MaxScale for instance variation
  - `GetAllTags()` method for automatic tag generation

**MOTagSchema.h**
- Centralized tag schema constants and utilities
- Tag prefixes: `Name `, `MOResource_`, `Gives_`
- Tag generation: `MakeDisplayNameTag()`, `MakeYieldTag()`, `MakeResourceTypeTag()`
- Tag parsing: `ParseDisplayNameTag()`, `ParseYieldTag()`, `ParseResourceTypeTag()`
- Tag queries: `HasResourceType()`, `HasKeepOnHarvest()`, `ExtractYieldItems()`

### Modified Files

**MOPCGResourceSpawnerSettings.h/cpp**
- `FMOPCGResourceEntry` now uses `FDataTableRowHandle` for resource selection
- Resources selected via dropdown from DT_ResourceNodes DataTable
- Scale overrides available per-entry (uses DataTable value if not set)
- Removed `ItemDataTable` property (meshes come from ResourceNode definition)
- Removed `TagPrefix` property (tags are auto-generated)
- Simplified specialized spawners (Tree, Bush, Rock) - they now just set defaults
- Tag generation reads all data from DataTable definition

**MOSurvivorJobTypes.h**
- Added `RequiredResourceTags` to `FMOSurvivorJobDefinitionRow`
- Added `DoesMatchResourceTags()` method for tag-based resource filtering
- Job definitions now specify which resource tags they accept

**BTTask_SurvivorGather.cpp**
- Updated `GetItemTagsForJobType()` to use DataTable's `RequiredResourceTags`
- Fallback to hardcoded tags if DataTable not configured
- Resource matching uses job definition tags instead of hardcoded values

### Auto-Generated Tags

From `FMOResourceNodeDefinitionRow::GetAllTags()`:
- `"Name {DisplayName}"` - For harvest context menu display
- `"MOResource_{Type}"` - Type classification (Tree, Rock, etc.)
- `"Gives_{ItemId}"` - For each entry in YieldsItems array
- `"KeepOnHarvest"` - If bKeepOnHarvest is true
- Direct ResourceTags - For job system matching

### Usage

**Defining Resources (DT_ResourceNodes):**
```
RowName: BlackAlder
DisplayName: "Black Alder"
ResourceType: Tree
YieldsItems: [Bark, Stick, Acorn]
ResourceTags: [Wood, Harvestable]
MeshVariations: [{Mesh: SM_BlackAlder, Weight: 1.0}]
bKeepOnHarvest: true
```

**PCG Spawner Node:**
1. Add "MO Resource Spawner" node
2. In ResourcesToSpawn, click + to add entry
3. Select resource from dropdown (e.g., "BlackAlder")
4. Optionally override scale range
5. All tags automatically applied from DataTable

**Job Definition (DT_SurvivorJobs):**
```
RowName: GatherWood
JobType: GatherWood
RequiredResourceTags: [Wood, Stick, Branch]
```

### Benefits

1. **Single Source of Truth**: All resource data in DataTables, not scattered across PCG nodes
2. **Dropdown Selection**: No manual ItemId entry, select from existing definitions
3. **Automatic Tags**: No manual tag configuration, everything generated from definitions
4. **Job System Integration**: Resources matched by DataTable tags, not hardcoded lists
5. **Easier Maintenance**: Change resource yields/tags in one place, affects all spawners
6. **Designer Friendly**: Non-programmers can add/modify resources via DataTable editor

### Migration

Existing PCG nodes using the old `FMOPCGResourceEntry` format need updating:
1. Create entries in DT_ResourceNodes for each resource type
2. Update PCG node ResourcesToSpawn entries to use new dropdown selection
3. Remove manual tag configuration (now auto-generated)

---

## [2026-02-24] Creature Recruitment Fix

**The Great Deer Unionization Prevention Act of 2026**

After discovering that local wildlife had formed a labor union and were demanding paid time off, dental coverage, and the right to unionize against predators, we've implemented emergency legislation:

- Added `bIsRecruitable` boolean to `UMORecruitmentComponent`
- `AMOCreature::BeginPlay()` now auto-sets `bIsRecruitable = false`
- Deer can no longer be recruited to gather sticks (they were frankly terrible at it anyway)
- Wolves have been denied collective bargaining rights
- Bears remain unemployable due to ongoing "mauling the interviewer" incidents

*This patch brought to you by: The Committee Against Employing Animals That Will Definitely Eat Your Inventory*

---

## [2026-02-24] Survivor AI & Task Assignment System

### New Features

**Survivor AI Controller**
- `AMOSurvivorController` - Specialized AI controller for recruited survivors
- **Follow Command**: Survivors follow player/target with configurable follow distance (200 default, 400 start threshold)
- **Stay Command**: Survivors hold position at specified location
- **Go Home Command**: Survivors return to assigned home location
- Commands are immediate actions that override job processing
- Smooth tick-based follow behavior with movement debugging

**Job Queue System**
- `UMOSurvivorJobQueueComponent` - Per-pawn replicated job queue (like crafting queue)
- Jobs can be queued, reordered, cancelled, and tracked
- Support for repeat counts (do job N times)
- FastArraySerializer replication for efficient network sync
- Home location storage with building GUID reference
- Save/load support via `BuildSaveData()` / `ApplySaveDataAuthority()`

**Job Types**
- `EMOSurvivorJobType` enum: GatherWood, GatherStone, GatherFiber, ForageNearby, DigForSupplies
- `FMOSurvivorJobEntry` - Individual job with ID, type, state, progress, repeat count, target location/actor
- `FMOSurvivorJobDefinitionRow` - DataTable row for job configuration (display name, icon, skill XP, duration)
- Job states: Queued, Active, MovingToTarget, Performing, Returning, Completed, Failed, Cancelled

**Centralized Job Database**
- `UMOSurvivorJobDatabaseSettings` - UDeveloperSettings for job definitions
- Configure via Project Settings > Plugins > MO Survivor Job Database
- Fallback path support for packaged builds
- Static caching with O(1) lookup by job type
- Separate retrieval for work tasks vs commands

**Survivor Context Menu**
- Right-click recruited survivors to open context menu
- Quick access to Follow Me, Stay Here, Go Home commands
- "Assign Tasks..." button opens full task menu
- Shows survivor name and current job status
- Auto-closes when clicking outside or pressing Escape

**Survivor Task Menu**
- Full task assignment panel (similar to crafting menu layout)
- Left panel: Available tasks organized by category
- Right panel: Current job queue with progress
- Header: Survivor name and active command
- Click task to enqueue, click job to cancel
- Tab/Escape to close

**Task Entry & Job Entry Widgets**
- `UMOSurvivorTaskEntryWidget` - Displays available task with icon and name
- `UMOSurvivorJobEntryWidget` - Displays queued job with progress and cancel button
- Both support dynamic delegate binding for click/cancel events

**Job Execution (Simple Jobs)**
- Jobs execute without behavior tree for supported types
- GatherWood: Finds trees with `GivesStick` tag, executes harvest recipe via `MOHarvestSubsystem`
- ForageNearby: Picks up ground spawns via `MOPCGInteractionSubsystem`
- DigForSupplies: Uses `MOForagingSubsystem::DigForSuppliesToInventory()`
- Move-to-target → Perform action → Award XP → Next iteration or complete

**XP Integration**
- All completed jobs award skill XP based on job definition
- XP amount and skill ID configured per job type in DataTable
- Uses existing `UMOSkillsComponent::AddExperience()` system

**Recruitment Component**
- `UMORecruitmentComponent` - Marks pawns as recruitable/recruited
- Possession eligibility checking
- Foundation for future recruitment mechanics

**Spawn Management System**
- `UMOSpawnManagerSubsystem` - World subsystem for spawn point management
- `AMOSpawnPoint` - Actor-based spawn point with visual debugging
- `UMOSpawnSettings` - UDeveloperSettings for spawn configuration
- `FMOSpawnPointData` - Spawn point data structure
- PCG spawn point integration via `UMOPCGSpawnPointSettings`

**Behavior Tree Support**
- `UBTService_SurvivorJobProcessor` - Service that monitors job queue and updates blackboard
- `UBTTask_SurvivorGather` - BT task for resource gathering
- `UBTTask_SurvivorForage` - BT task for ground foraging
- Blackboard keys: IsFollowing, ShouldStay, IsGoingHome, IsProcessingJob, CurrentJobType, etc.

### Technical Notes

**Survivor AI Architecture:**
```
Player RMB on Survivor
    ↓
MOPlayerController::HandleSecondaryAction()
    ↓ (hit survivor, not self)
MOSystemMenuUIController::ShowSurvivorContextMenu()
    ↓
UMOSurvivorContextMenu displayed
    ├─ Follow Me → AMOSurvivorController::SetFollowTarget()
    ├─ Stay Here → AMOSurvivorController::SetStayAtLocation()
    ├─ Go Home → AMOSurvivorController::GoToHome()
    └─ Open Tasks → MOSystemMenuUIController::ShowSurvivorTaskMenu()
                        ↓
                    UMOSurvivorTaskMenu displayed
                        ↓ (player selects task)
                    UMOSurvivorJobQueueComponent::EnqueueJob()
                        ↓
                    AMOSurvivorController executes job
                        ↓
                    UMOSkillsComponent::AddExperience()
```

**New Files:**
- `MOSurvivorController.h/cpp` - AI controller with follow/stay/home commands
- `MOSurvivorJobQueueComponent.h/cpp` - Replicated job queue
- `MOSurvivorJobTypes.h/cpp` - Job enums, structs, FastArraySerializer
- `MOSurvivorJobDatabaseSettings.h/cpp` - Centralized job definitions
- `MOSurvivorContextMenu.h/cpp` - Right-click context menu
- `MOSurvivorTaskMenu.h/cpp` - Full task assignment panel
- `MOSurvivorTaskEntryWidget.h/cpp` - Task list entry widget
- `MOSurvivorJobEntryWidget.h/cpp` - Job queue entry widget
- `MORecruitmentComponent.h/cpp` - Recruitment state tracking
- `MOSpawnManagerSubsystem.h/cpp` - Spawn point management
- `MOSpawnPoint.h/cpp` - Spawn point actor
- `MOSpawnSettings.h/cpp` - Spawn configuration
- `MOSpawnTypes.h` - Spawn data structures
- `BTService_SurvivorJobProcessor.h/cpp` - BT job queue service
- `BTTask_SurvivorGather.h/cpp` - BT gather task
- `BTTask_SurvivorForage.h/cpp` - BT forage task

### Blueprint Setup Required

1. **Create `DT_SurvivorJobs` DataTable** (FMOSurvivorJobDefinitionRow)
   - Configure job types with display names, icons, skill rewards

2. **Configure Job Database** in Project Settings > Plugins > MO Survivor Job Database
   - Set `JobDefinitionsDataTable` to your DT_SurvivorJobs asset

3. **Create `WBP_SurvivorContextMenu`** (parent: `UMOSurvivorContextMenu`)
   - Add: `SurvivorNameText`, `CurrentJobText` (TextBlock)
   - Add: `FollowMeButton`, `StayHereButton`, `GoHomeButton`, `OpenTasksButton` (UMOCommonButton)

4. **Create `WBP_SurvivorTaskMenu`** (parent: `UMOSurvivorTaskMenu`)
   - Add: `SurvivorNameText`, `CurrentJobText` (TextBlock)
   - Add: `TaskListScrollBox`, `JobQueueScrollBox` (ScrollBox)
   - Add: `CloseButton` (UMOCommonButton)
   - Set `TaskEntryWidgetClass` and `JobEntryWidgetClass` properties

5. **Create `WBP_SurvivorTaskEntry`** (parent: `UMOSurvivorTaskEntryWidget`)
   - Add: `TaskNameText` (TextBlock), `TaskIcon` (Image), `TaskButton` (UMOCommonButton)

6. **Create `WBP_SurvivorJobEntry`** (parent: `UMOSurvivorJobEntryWidget`)
   - Add: `JobNameText`, `ProgressText` (TextBlock), `CancelButton` (UMOCommonButton)

7. **Set widget classes** on `MOSystemMenuUIController` component:
   - `SurvivorContextMenuClass` → WBP_SurvivorContextMenu
   - `SurvivorTaskMenuClass` → WBP_SurvivorTaskMenu

---

## [2026-02-24] Save/Load System Fixes

### Bug Fixes

**Critical: Voxel World Seed on Load**
- Fixed `bAutoInitializeVoxelWithSeed` defaulting to `false`, causing voxel terrain to generate with wrong seed on load
- Pawns would spawn at saved positions but terrain was different, causing them to fall through the world
- Now defaults to `true` - terrain regenerates with correct saved seed

**Unrecruited Survivors Appearing Possessable**
- Fixed `bIsPlayerControllable` only checking creature status, not recruitment state
- Now checks both `!IsA<AMOCreature>()` AND `RecruitmentComponent->IsPossessable()`
- Unrecruited survivors no longer appear in possession menu

**Character Names Not Preserved**
- Fixed `CharacterName` not being applied to `IdentityComponent.DisplayName` on load
- Fixed save not capturing live `DisplayName` from `IdentityComponent` (was only reusing old record)
- Names now properly round-trip through save/load

**Recruited Pawns Missing AI Controller**
- Added `EnsureSurvivorAIController()` called when Recruited state is loaded
- Previously, `ApplySaveDataAuthority` set state but didn't spawn AI controller like `ForceRecruit()` does
- Loaded recruited pawns now have autonomous behavior

**Creatures Incorrectly Recruitable**
- Added `bIsRecruitable` property to `UMORecruitmentComponent` (defaults to `true`)
- Creatures (deer, wolves) inherit from `AMOCharacter` which has recruitment component
- `AMOCreature::BeginPlay()` now sets `bIsRecruitable = false` to prevent recruitment
- `BeginInteraction()` and `ForceRecruit()` check this flag and abort if not recruitable
- Future use: Can set `bIsRecruitable = true` on specific animals for taming mechanics
- Deer can no longer be recruited to gather wood. Their union rep was very persuasive. They will, however, still accept treats and aggressive head pats (taming system coming soon)

### Technical Changes

**MOGameMode.h/cpp:**
- `bAutoInitializeVoxelWithSeed` default changed from `false` to `true`
- Added warning logs when voxel initialization is skipped or seed is zero

**MOPersistenceSubsystem.cpp:**
- `bIsPlayerControllable` now checks recruitment state via `IsPossessable()`
- `RespawnPersistedPawns` applies `CharacterName` to `IdentityComponent`
- Save captures `DisplayName` from `IdentityComponent` instead of only using existing record
- Added detailed recruitment state logging on save and load

**MORecruitmentComponent.h/cpp:**
- Added `EnsureSurvivorAIController()` private method
- `ApplySaveDataAuthority()` calls `EnsureSurvivorAIController()` when loading Recruited state
- Added `bIsRecruitable` property with `IsRecruitable()` query function
- `BeginInteraction()` returns false if not recruitable
- `ForceRecruit()` returns early with warning if not recruitable

**MOCreature.cpp:**
- Added include for `MORecruitmentComponent.h`
- `BeginPlay()` sets `bIsRecruitable = false` on inherited recruitment component

---

## [2026-02-22] Ground Foraging System, Character Customization & Physics

### New Features

**Ground Foraging System**
- **Right-click on ground** to open foraging context menu (when no interactable target)
- **Search Nearby**: Reveals PCG-spawned HISM items within skill-scaled radius, converts to pickable world items
- **Dig for Supplies**: Chance-based spawning of roots, stones, flint based on Foraging skill level
- Foraging radius scales with skill: Base 300 + (5 * SkillLevel), max 800 units
- XP awarded per item found (2 XP per revealed item, 5 XP per dug item)
- Menu auto-closes when mouse leaves with 0.15s grace period
- Cursor warps to menu center to prevent immediate auto-close

**MO PCG Mesh Spawner Node (All-in-One)**
- New `MO Mesh Spawner` PCG node combines item selection, mesh spawning, and tagging
- Replaces separate MO Item Spawner + Static Mesh Spawner + MO HISM Tagger workflow
- Automatically tags HISM components with `MOItem_<ItemId>` for foraging system discovery
- Registers tag mappings with `MOPCGInteractionSubsystem` at runtime
- Configurable collision profile, shadow casting, and instances per cluster

**Character Customization Stream**
- Started animation-based character customization system
- Morph target slider support for character appearance

**Jiggle Physics**
- Added jiggle physics to character models for realistic movement

### Technical Notes

**Foraging Architecture:**
```
Player RMB on ground → MOPlayerController::HandleSecondaryAction()
    ↓ (no interactable, ground hit detected)
MOInventoryUIController::ShowGroundContextMenu()
    ↓
UMOGroundContextMenu displayed at screen center
    ↓ (player clicks action)
UMOForagingSubsystem::RevealHISMInstancesInRadius() or DigForSupplies()
    ↓ (spawns AMOWorldItem actors)
Nearby panel auto-refreshes with found items
```

**New Files:**
- `MOGroundContextMenu.h/cpp` - Context menu widget
- `MOForagingSubsystem.h/cpp` - World subsystem for HISM query and dig mechanics
- `MOPCGMeshSpawnerSettings.h/cpp` - All-in-one PCG spawner with tagging
- `MOPCGHISMTaggerSettings.h/cpp` - Standalone HISM tagger (for existing PCG setups)

**Blueprint Setup Required:**
1. Create `WBP_GroundContextMenu` (parent: `UMOGroundContextMenu`)
   - Add `ButtonContainer` (VerticalBox or PanelWidget)
   - Add `SearchNearbyButton` (UMOCommonButton)
   - Add `DigForSuppliesButton` (UMOCommonButton)
   - Optional: `RadiusText`, `SkillLevelText` (TextBlock)
2. Set `GroundContextMenuClass` on `MOInventoryUIController` component

---

## [2026-02-22] World Item Context Menu & Loading Screen UX

### New Features

**World Item Context Menu Actions**
- **Split Stack for world items**: Right-click a stack in nearby panel → Split takes half into inventory, leaves rest in world
- **Details for world items**: Right-click → Details now shows item info panel for world items
- Added `UMOItemInfoPanel::SetItemByDefinitionId()` for displaying item info without inventory lookup
- Added `UMOUnifiedInventoryMenu::SetItemByDefinitionId()` wrapper
- Nearby panel refreshes after split to show updated quantity

**Loading Screen Before Level Load**
- Loading overlay now appears BEFORE level load begins (no more frozen menu)
- 2 second delay lets player read loading screen before transition
- Uses timer-based delayed `OpenLevel()` to ensure UI renders first
- Applied to both New Game and Load Game flows

**PCG Flower Patch Clustering**
- Flower patches now spawn as natural-looking clusters instead of single scattered points
- Uses Copy Points node: small grid pattern (Source) stamped at each patch center (Target)
- Enable "Copy Each Source on Every Target" for proper cluster behavior

**Ground Spawns**
- Added flint nodules to ground spawn list (primitive tool crafting material)
- Added branches to ground spawn list (fuel, primitive crafting)

### Bug Fixes

- **Fixed context menu actions failing for world items**: Inspect, SplitStack, Details, and Pickup now correctly cache world item reference before closing context menu
- **Fixed redundant ContextMenuWorldItem.Reset() calls**: Consolidated cleanup into `CloseItemContextMenu()`
- **Fixed PCG distance culling not working**: Root cause was missing PCG Invoker component on player character - distance calculations were using PCG component origin instead of player position

### Technical Notes

**Loading Screen Flow:**
1. User clicks Start Game / Load Game
2. Settings configured, `bIsLoadingIntoGameplay = true`
3. `ShowLoadingOverlay()` called immediately
4. 2 second timer starts
5. Timer fires → `OpenLevel()` called
6. Player sees loading screen instead of frozen menu

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
- Uses Unreal's native `NativeOnMouseButtonDoubleClick` for reliable detection
- Works on both inventory slots and nearby items panel

### Bug Fixes

- **Fixed loading screen ending too early**: Loading screen now waits for pawn to land on ground instead of dismissing when map loads
- **Fixed seeing terrain generation**: Players no longer see voxel terrain loading or pawn falling during new game start
- **Fixed seeing pawn repositioning**: Loading screen now covers the re-grounding of loaded pawns after voxel regeneration
- **Fixed loading screen during intro**: Initial game launch now stays black until intro video starts (no loading screen flash)
- **Fixed character mesh deformation around neck**: Resolved mesh distortion issue in the neck area of character models
- **Fixed inventory grid showing too many slots**: Grid now shows exact slot count from inventory component instead of padding to MinimumVisibleSlotCount
- **Fixed double-click requiring 3 clicks**: UButton was consuming mouse events internally before slot widget could detect them. Fixed by setting SlotButton to `HitTestInvisible` and handling all input at the slot widget level. Now uses Unreal's native `NativeOnMouseButtonDoubleClick` event for reliable 2-click detection

**Nearby Items Context Menu**
- Right-click items in the nearby items panel to show context menu
- Available actions: Pickup, Inspect, Split Stack (if stackable), Craft, Details
- Context menu reuses existing `UMOItemContextMenu` with world item mode
- Added `PickupButton` to context menu widget (optional BindWidget)
- **Split Stack for world items**: Takes half the quantity into inventory, leaves rest in world
- **Details for world items**: Shows item info panel using `SetItemByDefinitionId()` without requiring inventory lookup
- Added `UMOItemInfoPanel::SetItemByDefinitionId()` for displaying item info directly from definition ID
- Added `UMOUnifiedInventoryMenu::SetItemByDefinitionId()` wrapper for details panel

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
