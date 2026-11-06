# Building (placement, construction, build state)

> **Covers:** placing a building (ghost), depositing materials, constructing, cancel / refund, how a building looks in each state,
> and the co-op rules. Terraforming and excavation: `Docs/Terraform_Excavation_Plan.md`. The building system is deliberately custom
> (weighted build parts; no native alternative: `Docs/TECHNICAL_REFERENCE.md` "Systems Confirmed Custom").
> **Current state (2026-10-17):** construction is server-authoritative (Oct 13); placement goes through one server path; a building
> looks pale-white "planned" until complete (Oct 16, Wes kept it); a new player can place a building (the Oct 15 aim-trace blocker
> is fixed); the first campfire completes end to end.
> **Entry points:** `UMOBuildingComponent` (`RequestBuildAction` -> Server RPC `ServerBuildRequest`, `EMOBuildRequest`, 15 m reach;
> `TryPlaceGhost` -> `ServerPlaceBuilding`), `UMOBuildProgressComponent` (authority-gated gather / start / cancel-refund; replicates
> `FMOBuildRepState`), `AMOBuildableActor` (`RefreshBuildStateVisual`, `PlannedColor` / `PlannedGlow`),
> `AMOPlayerController::ForwardUIContractRequest` -> `ClientUIContractRequest`, `UMOInteractionSubsystem::IsWithinInteractReach`.
> **Tests:** `MO.Test.Build`, `nettest actions` steps `_client_build`, `_client_building_look`. **Open items:** `Docs/BACKLOG.md` §
> Building and terraform. **Decisions:** DECISIONS 2026-10-13 (server-driven), 2026-10-16 (pale ghosts), F7 (cancel / refund semantics).

## 1. Server-driven construction (Wes, Oct 13: "co-op build for sure needs to be server/host driven"; 757e47c1)

- Players reach construction only through `UMOBuildingComponent::RequestBuildAction` (a Server RPC from a client, 15 m reach) with an
  `EMOBuildRequest`. `UMOBuildProgressComponent` mutators refuse without authority; it owns gather / start / cancel-refund and
  replicates `FMOBuildRepState` (a TMap ledger flattened to a sorted array); clients re-broadcast its delegates.
- **UI asked of a remote controller:** interaction handlers run on the server, so a UI request made on the server's copy of a joiner's
  controller opened nowhere. `AMOPlayerController::ForwardUIContractRequest` -> `ClientUIContractRequest` sends it to the owning client
  (build menu, containers, crafting, station menu, butchering). **A new `IMOUIContractInterface` request must start with
  `ForwardUIContractRequest`.**
- **Rule:** a new player action = a request through a controller-owned component RPC, with the logic in an authority-gated component;
  never in a widget. Test new co-op verbs with a `nettest actions` step that checks on the HOST and includes a "direct client call
  refused" control.

## 2. Placement and the ghost

- One path: `TryPlaceGhost -> ServerPlaceBuilding` (in place on the authority); the placement preview never replicates. Before Oct 16
  the authority PROMOTED its own preview into the building (graybox PreviewMesh + ghost MID carried over), which gave host-only green boxes.
- The aim trace from the shoulder camera must reach the ground within reach: before Oct 15 it never did within 10 m, so the ghost
  floated invalid, and its tint wrote a "Color" parameter `M_BuildPreview` does not have (invalid looked green). **The co-op build test
  passed throughout because it places through a server call, not the ghost: test the real input path.**
- **The site is cleared** (Oct 18, N-079): `ServerPlaceBuilding` registers a worked-ground zone over the new building (radius = its bounds'
  half-diagonal + 30 cm, 100-800 cm) and sweeps it, so the decorative ground cover (the AutoSweepTags scatter: grass; never harvestable
  plants) under and around it goes on every machine and stays gone in the save. A campfire no longer sits in tall grass. Measured on a host +
  joiner: a campfire placed by the joiner, radius 118 cm, 17 instances swept on the host and the same 17 on the joiner.
- Server checks (Oct 18 re-check of the July H19 note): `ServerPlaceBuilding` holds the transform to the pawn's reach
  (`MaxPlacementDistance + ServerMaxPlacementMargin`) and, since Oct 20, to the recipes the build menu lists for that pawn
  (`UMOBuildingComponent::IsBuildingRecipeOpenTo`: one that needs discovering only once the server's copy of the pawn's discoveries has it;
  the menu uses the same function). Build requests need the pawn within `MaxBuildRequestReachCm`. Not re-checked on the server: collision
  and slope (the client's ghost decides; BACKLOG N-171, low priority for friends-only co-op).
- **A building's skill is how well, never whether** (F31 A, Wes Oct 19: "building skill slows the work and wastes material, never gates"):
  `FMOBuildSkillPenalty` (MOBuildingTypes.h) is the one rule. When the work first starts (`UMOBuildProgressComponent::ApplyBuilderSkill`) the
  builder's level against the recipe's scales the build time (x2 with no skill, linearly to x1 at the level) and plans how many units the work
  spoils (a quarter of the material with no skill); the scaled time replicates (`FMOBuildRepState::TotalBuildTime`) and is saved. Spoils fall
  at even points through the work (`SpoilOneUnit`): the most-used material loses a unit, replaced at once from the builder's sources (pack,
  nearby containers, the ground) or the build pauses "add another to go on". The menu shows the time at your skill and "Smithing: 0/30
  (slower, wastes some material)"; the list no longer treats skill as a lock. Crafting still gates on skill (F18 is open). Verified: a Forge
  at Smithing 0 / 30 -- 240 s instead of 120, 9 of 36 units spoiled, the first five replaced from 5 spare stones, the sixth paused the build.

## 3. How a building looks

`AMOBuildableActor::RefreshBuildStateVisual` is the one look per build state: every mesh in the ghost MID is tinted `PlannedColor` /
`PlannedGlow` until Complete (Wes, Oct 16: pale-white ghosts stay). Unbuilt floors used to look finished, so "fell through the floor"
reports were unbuilt plans.

The ghost material is translucent and Nanite draws only opaque / masked materials: on a Nanite mesh (the campfire's stones, the stick
pieces) the engine drew its default grey material instead ("Invalid material ... used on Nanite static mesh"). Since Oct 19
`ApplyGhostMaterialToAllMeshes` forces the non-Nanite fallback on each static mesh while it wears the ghost and
`RestoreOriginalMaterials` gives Nanite back (the component's own setting is saved with its materials). Verified: placing a campfire and
a stick wall logs no Nanite warning (the build before logged one for each).

### Building mode: where the plans show (Oct 26)

Wes, Oct 26: "make a 'building' mode [next to] the terraforming and exploration mode where we can see the build ghosts better". Since BR25-4
(Oct 25: "make them not visible if you're not in build mode trying to place an item") a planned, unbuilt building's ghost is hidden in
Explore. **Building mode** (`UMOBuildingComponent::SetBuildingMode`, a local view mode: nothing replicates) shows every plan on this machine
(`IsLocalPlayerBuilding`; placing counts as building mode) and the HUD says Build. Keys (Claude's small calls, noted in DECISIONS):

- **B** outside the mode enters it and opens the build menu; B with the menu open closes it and leaves the mode (the menu's own toggle-key
  path, `TryCloseMenuForToggleKey`, does the same when it holds focus); B with the menu closed reopens it; B while placing cancels the
  placement and shows the menu again.
- **Back** (Esc / Tab) closes the menu and keeps the mode; Back with nothing open leaves the mode (before it would open the system menu).
- **T** (terraform) leaves building mode, and B leaves terraform: the two modes exclude each other.
- The plans wear the Oct 24 dimmer tint (`PlannedColor` alpha 0.3, `PlannedGlow` 0.5); whether they read well enough in the mode is on
  Wes's by-eye list.

Verified in a game (one instance, real input actions): a placed campfire plan -- Explore 0 of 30 meshes shown; B: the 10 stones shown;
B again: hidden; B, Back (mode kept), T: left; B, Back, Back: left.

### A finished building always shows (Oct 26 fix)

`AMOBuildableActor::EnterCompletedState` re-shows every part (`ApplyStagedParts` all, `ApplyGhostVisibility`). Before, only the live
construction path did (`HandleBuildStateChanged` -> `RefreshBuildStateVisual`), so a building that came straight to Complete stayed hidden
on the host where its plan had been hidden: a survivor's beacon fire (`FinishAsBuilt`, BR25-4's hidden plan) and a loaded finished building.
Found by the building-mode check (the beacon fire turned visible when the mode refreshed it); verified: the beacon fire visible in Explore, a
creative-built campfire visible, and still visible after a save and a reload.

### Creative worlds build instantly (Oct 26)

Wes: "in creative mode make it so building happens instantly, no build time etc". A building placed in a creative world is finished where it
is placed (`ServerPlaceBuilding` -> `UMOBuildProgressComponent::CompleteInstantly`, credited to the placer), and every build time in it reads
"Instant" (`FMOWorldRules::ScaleSeconds` returns 0 for building and crafting in a creative world; `Docs/World_Creation.md`). Survival worlds
are unchanged. Verified: `MO.Test.PlaceBuildingRPC` in a creative world logs "built instantly (creative world)".

### Staged parts: it goes up piece by piece (Oct 24)

Wes: "while it's building it will switch each stone to it's proper texture as the building timer fills up" (the campfire) and "they become
visible in order as the build timer fills up so it looks like you're actually building the things" (the stick buildings).

- `AMOBuildableActor::StagedBuildParts` names mesh components in build order. Part i of N is built once progress reaches (i + 1) / N
  (`CountBuiltStagedParts`, test `Building.StagedPartsBuildInOrder`) and wears its own materials from then on.
- Before construction starts every part shows the plan. During it an unbuilt part keeps the ghost (`UnbuiltStagedPartStyle` Ghost: the
  campfire's stones) or is hidden (Hidden: the stick buildings).
- It updates on `OnConstructionProgress`, which the server ticks, a client advances between updates, and every replicated update re-sends,
  so a joiner sees the same parts.
- A part turned real mid-build keeps its saved materials entry, so the next ghost pass does not take the ghost for its original.
- An empty list keeps the old look: the whole building turns real at completion.

**Who uses it:**

- **The stone-ring campfire.** `BuildCampfire` places `BP_Campfire_StoneRing`, made by `Content/Python/make_stone_ring_campfire.py`. It is
  BP_Campfire with its merged Megascans campfire mesh replaced by loose parts:
  - ten RiverCobble01 stones (the recipe deposits ten cobbles), staged round the ring, each at a random yaw, tilt (±18°), size (x0.85-1.25)
    and sink (1-4.5 cm), seeded so a rebuild matches (Wes: "add some random rotation to the stone so they don't look so uniform");
  - twenty Firewood01 fuel sticks (below), standing inside the ring;
  - BP_Campfire's fire (NFire_03), light and sounds.
  No other sticks: the first build also had three fresh sticks beside the ring and burnt pieces in the bed, which read as fuel before any
  wood went in (Wes: "fireplace sticks visible before firewood being added, some sticks are also on outside of fireplace"). The fresh and
  burnt sticks are the fuel sticks themselves, so an empty fire is an empty ring. The placement preview still shows the old merged mesh.
- **The stick buildings.** `make_staged_stick_builds.py` orders each stick Blueprint's sticks: the main ones first (a wall's uprights, a
  floor's boards, a roof's rafters) along the panel, then the crossing rails bottom up. This covers the wall, half wall, floor and roof.
  Rerun it after editing a stick Blueprint.

### Fuel sticks (Oct 24)

Wes: "when you add wood it shows various levels of "full" by making the firewood assets visible (so like barely full is one stick visible,
compeltely full is 20 sticks visible) and after you burnt it it turns to charred looking sticks".

- `AMOCraftingStationActor::FuelStickParts` shows `CountVisibleFuelSticks(CurrentFuel, MaxFuel, N)` sticks: none when empty, one for any
  fuel, all when full (test `Fire.FuelSticksShowTheFuel`). At the campfire's MaxFuel 100, one fuel item (10) is two sticks.
- While the fire burns every visible stick chars (`CharredFuelMaterial`: MI_Firewood_Charred, a near-black tint of the firewood's own MI).
  The server keeps the charred count (`CharredFuelSticks`, replicated).
- When the fire is out, burnt-away sticks leave the count and wood added on top stays fresh until lit.
- The charred count is not saved: a fire loaded lit chars again, a cold one shows its sticks fresh.
- Verified in PIE (seed 777): fuel 10 showed 2 sticks; 50 showed 10; lit, 10 charred; burnt to 23, 5 charred; put out and two firewood
  added, 9 (5 charred, 4 fresh); topped up, 19.

### Collision: nothing solid until it is finished (BR25-7, Oct 24-25)

Wes: "make sure nothing has collission until it's fully build". `AMOBuildableActor::ApplyUnbuiltCollision` puts every part of a planned or
constructing building (`GetBuildingMeshes`: the Mesh component and every static mesh on the actor, the set the visuals tint and stage) on the
unbuilt collision: query only, ignoring everything but `ECC_Visibility`, the interaction trace, so a plan can still be clicked and nothing else
bumps it. Completion (`EnterCompletedState`) makes the Mesh subtree BlockAll (the structural contract) and gives every other part the
collision its Blueprint authored (`RestoreAuthoredCollision`, from the component's archetype). The first pass walked only the Mesh subtree,
and the stick walls' 16 staged sticks hang off the root: they blocked pawns while unbuilt. PIE: a planned stick wall's 18 parts all
ignore pawns; built, all 18 block. Test `Building.EveryPartUnbuiltUntilComplete`.

A building that comes into the world finished (a survivor's beacon fire, the dev fire and colony spawns) goes through
`AMOBuildableActor::FinishAsBuilt`: the recipe, the progress record Complete (the save-restore route), the completed state. Before it the
beacon fire sat in the Ghost state with a Build menu (N-182) and, with the unbuilt collision on every part, walk-through. PIE: the beacon
is Complete, solid and lit.

## 4. Using a finished station

E on a finished station opens its station menu: the server checks the client's claimed target with
`UMOInteractionSubsystem::IsWithinInteractReach` (one reach rule for E and right-click). Before Oct 15 the server re-derived the target
with its own 12 cm sweep vs the client's 30 cm and refused. Interaction targeting in general: `Docs/Coop_And_Networking.md` §5.

## 5. First campfire, end to end (verified Oct 15)

Place -> leave -> walk up -> Add Materials -> Build -> 30 s; then light it (`Docs/Crafting_And_Progression.md` §2). The site context
menu's kit spec is `Content/Python/ui_specs/kit_build_context.py`.

## 6. Saves

Loaded buildings repopulate their recipe BuildParts (a July fix: restore once left materials vacuously satisfied = free build).
Deposited build materials are part of the save payload. Persistence patterns: `Docs/Persistence.md`. A building piece also saves its
posts' lengths and whether its door stands open (section 7).

## 7. Building pieces: wattle and daub, framing, stone, thatch, doors, shelters (Nov 2-3)

Wes, Nov 2: "take the wattle walls we have and add moss dobbing for insulation", "wooden 'framing' where some branches snap to the edges
of the wall panels so we can create a framed in look", "wooden floors that don't have anything in between them and the ground get wooden
supports like the framing above", "cobblestone foundations and walls", "thatch to the roof wattle framing", "doors", "hardwood framing and
cobble and dob", "hide lean to and tent and wattle and thatch lean to ... remember to have the assets 'pop in' as building goes on so it looks
like it's actually finishing".

### The one abstraction: a piece is data

`AMOBuildPieceActor` (MOBuildPieceActor.h) is a building made of **parts in build order**. Its Blueprint holds data only: `Parts` (a mesh,
once or instanced), `Colliders` (hidden boxes: solid once built, clickable as a plan), `SnapBox` (what snapping measures, tag
`MO_SnapBounds`), a door, supports, and the snapping / shelter flags on `AMOBuildableActor`. The actor makes the components when it is
constructed, on every machine alike, and hands the parts to the staging (section 3): part i of N appears when the work reaches (i + 1) / N,
the unbuilt ones hidden. A new piece is one entry in `Tools/building_pieces.py` and one builder in the Blender script, not new code.

| Step | Where |
|---|---|
| What each piece is (frame, cost, time, skill, snapping, colliders, porosity) | `Tools/building_pieces.py` `PIECES` |
| The parts, built procedurally in build order (Unreal's frame, world-scaled UVs) | `Tools/blender_buildings_blender.py` (`GEOMETRY`) |
| Ten tiling surfaces baked seamless (4D noise on a torus): Bark, Rod, Hewn, Plank, Moss, Daub, Thatch, Hide, Mortar, Cord | the same script, job `surfaces` -> `/Game/BlenderBuildings/Surfaces/MI_Surf_*` on `M_Surf_Base` |
| Import, the scanned stones laid in courses, `BP_Piece_<Piece>`, the recipe row | `Content/Python/import_blender_buildings.py` |
| All of it | `python Tools/blender_buildings.py [Piece ...] [--surfaces] [--rows]` |
| In a game | `python Tools/check_buildings.py [--shots]` (verbs `MO.Test.PlacePiece / PieceProgress / PieceInfo / PieceDoor / ShelterHere`) |

### Natural variation: no two sticks alike (Nov 3)

Wes, Nov 3: "give all the assets you made in blender some natural variation so they're not so uniform". One chokepoint, not per-piece
fixes: the Blender script's primitives (`tube`, `beam`, `blob`, `sheet`, `daub_slab`) each draw their own look from the piece's one seeded
source (`Vary`, seeded by the piece's name, so a rebuild is the same piece):

- **Tint.** Each primitive's light and weathering ride in the mesh's second UV channel ("Tint", plus a slow field across the whole piece).
  `/Game/BlenderBuildings/Surfaces/M_Surf_Base` (made by the import script) scales the base colour by the surface's `light` range and moves
  it toward its `weather` colour (bark silvers, moss yellows, hide darkens with smoke; `Tools/building_pieces.py` `SURFACES`). Each
  primitive also starts at its own place on the tiling texture. M_Surf_Base reads the weather as 1 - V (the FBX import flips V); it is built once and
  version-tagged; a graph change bumps `SURF_BASE_VERSION`, and the old one is deleted on disk with the editor closed (`Docs/TRAPS.md` §12).
- **Shape.** A straight-ish stick bows a little (0.7% of its length), swells and narrows along it and is cut on a slant at its ends; a
  timber bows, twists and is not quite square. A hoop, a helix of cord or a fork keeps the shape it was given.
- **Layout.** Stakes set out by eye and leaning; wattle courses packed by each rod's own thickness, every rod tapering from its own butt;
  moss pressed on as cushions of every size; thatch laid in bundles whose butts stagger and fullness varies (`Bundles`), the gauge
  uneven; hides with an animal's outline and their own sag; the tent's seams wandering (shared by the two hides they join, so the overlaps
  stay closed), each pole its own length and thickness; boards of random widths filling the span.
- **Stones.** Eighteen scans (`STONES`): the three cobbles, seven field stones of the small-stones pack and a scatter rock (`WALLING`), and
  the pebble pack's seven (`PINNING`). Each course is its own height and built of three walling stones (a few ISM parts per course).
  Short, middle and long stones are mixed. Each stone is a little shy of the course and bedded at its own height. It stands proud or
  set back, turned any face out, and now and then a pinning stone is knocked into a joint.

Frames are the stick pieces' (measured Nov 2): a wall's panel centred at X -50 (Y +-50, Z +-50) as the stick wall's, a roof's eave at X 0,
Z -50 rising to the ridge at X -100, Z +50 as the stick roof's, a floor centred with its top at Z +5. So a daub or a thatch takes its host's
transform and fits the old stick pieces and the new ones alike.

### Attaching pieces (`EMOPieceAttach`, `UMOBuildingComponent::TryAttachSnap`)

- **Face**: a moss or clay daub on a wall tagged `Wattle` (the stick walls and the wattle wall), a thatch on a `Roof`, a timber brace on any
  `Wall`. It takes the host's transform; the host's panel must be its size (a full wall's daub does not go on a half wall).
- **Edge**: a frame member centred on the host edge nearest the aim that runs its way and is its length: a post (`Vertical`) on a wall's
  side, a rail or beam (`Horizontal`) on a wall's top or bottom, or hung under a floor's edge. A post where two walls meet covers both.
- An attaching piece never stands alone (no host = invalid), is never a snap target itself (it would steal the aim from its host), and is
  one of a kind per place (a second daub on the same wall is refused).
- `PieceTags` say what a piece is for these: the stick walls `Wall, Wattle` (the half wall also `Half`), the stick roof `Roof` (its peak
  `RoofPeak`: no thatch fits it yet), floors `Floor` (`Content/Python/tag_stick_builds.py` sets the stick pieces').

### Supports under floors

A floor with `bGrowsSupports` measures, when it is placed (on the server), the drop under each support point to whatever solid is below --
the ground or a finished building (a plan does not count); 6 cm or less needs no post, more than 3 m finds none. The lengths replicate and
are saved (`FMOPersistedBuildingRecord::SupportLengthsCm`). Each becomes a post (`SupportMesh`, 100 cm tall, scaled to its length; solid
once built) that goes up **first**, and its material and time join the build (`UMOBuildProgressComponent::SetExtraWork`: one `SupportItem`
per `SupportCmPerItem` of its length, one lashing each, `SupportSecondsPerPost`). A corner a neighbour's post already carries (within 20 cm)
gets none of its own: two floors side by side share their common edge's posts. The placement preview shows the posts as it moves. The stick
floor is reparented onto `AMOBuildPieceActor` for this and keeps its own sticks and staging (pole posts of straight branches); the plank
floor's posts are hewn timber.

### Doors

A door piece is a wall two panels high (posts, lintel, sill) and a leaf (its parts and collider marked `bDoorLeaf`) hung on `DoorHinge`.
E on the finished door opens and closes it on the server (`SetDoorOpen`: replicated, saved); the leaf swings over `DoorSwingSeconds` and its
collider with it. Flip the piece while placing it to hang the door the other way.

### What a piece lets through: draughts and leaks (`IMOShelterPorosity`)

The shelter rays (`UMOWeatherBlueprintLibrary::TestExposureShelter`) trace on through a porous solid carrying what passed: each ray's cover
is 1 - the product of what everything along it lets through (`RainPass` for the overhead rays, `WindPass` for the wall and wind rays), and
the hit counts keep the rays that stop at least half. A bare wattle or stick wall lets the wind through (0.45-0.5), moss daub (0.05) or clay
daub (0) stops it; a stick roof lets the rain through (0.85) until it is thatched (0). Wind chill inside follows (the vitals read the wind
channel of the exposure). A plan or a site under way stops nothing.

### The pieces

| Piece (recipe) | Frame | Build order (parts) | Cost | Time | Lets through |
|---|---|---|---|---|---|
| Wattle Wall (`BuildWattleWall`) | wall | 5 stakes, then ~42 rods woven, in 7 courses | 5 straight branches, 24 green rods | 40 min | wind 0.45 |
| Wattle Half Wall (`BuildWattleHalfWall`, Nov 3) | half wall (the stick half wall's panel) | 5 stakes, the rods in 4 courses | 5 straight branches, 12 green rods | 20 min | wind 0.45 |
| Moss Daubing (`DaubWallMoss`) | on a Wattle wall | 8 bands of moss, bottom up | 12 moss | 25 min | wind 0.05 |
| Clay Daubing (`DaubWallClay`) | on a Wattle wall | 8 bands of daub, both faces | 10 mud, 4 clay, 4 dried grass | 45 min | wind 0 |
| Moss / Clay Daubing, half wall (`DaubHalfWallMoss`, `DaubHalfWallClay`, Nov 3) | on a Half wall (the stick or wattle one) | 4 bands | 6 moss / 5 mud, 2 clay, 2 dried grass | 12 / 23 min | wind 0.05 / 0 |
| Pole Post / Pole Rail (`BuildPolePost`, `BuildPoleRail`) | a wall's side / its top or bottom, a floor's edge | pole, lashings | 1 straight branch, 2 cord | 10 min | -- |
| Timber Post / Beam (`BuildTimberPost`, `BuildTimberBeam`) | the same | hewn oak, pegs | 1 log, 1 hardwood; an axe | 1 h | -- |
| Timber Brace (`BuildTimberBrace`) | on a wall | brace, pegs | 4 hardwood; an axe | 40 min | -- |
| Cobble Foundation (`BuildCobbleFoundation`) | floor, 40 cm deep | mortar core, rammed top, 3 courses of stones on four sides | 55 building stones, 15 mud | 4 h | -- |
| Cobble Wall (`BuildCobbleWall`) | wall, 40 cm thick | mortar core, 6 courses of stones on both faces | 55 building stones, 15 mud | 4 h | nothing |
| Cobble and Daub Infill (`BuildCobbleInfill`) | wall, 14 cm | core, 9 courses of small stones, the daub over them | 18 building stones, 8 mud, 3 clay, 3 dried grass | 2 h | wind 0 |
| Dry-Stone Wall (`BuildDryStoneWall`, Nov 3) | wall, 56 cm at the foot battered to 38 | 4 bands of hearting, each with its courses on both faces and the wall heads, then the coping on edge | 70 building stones | 4 h | wind 0.15 |
| Grass Thatch (`ThatchRoof`) | on a stick Roof | 7 courses from the eaves up, ridge roll, sways | 40 dried grass, 4 rods, 6 cord | 1 h | rain 0 |
| Grass Thatch, roof peak (`ThatchRoofPeak`, Nov 3) | on a stick Roof Peak | both slopes' courses side by side, lapped over the ridge, ridge roll, sways | 44 dried grass, 4 rods, 8 cord | 70 min | rain 0 |
| Plank Floor (`BuildPlankFloor`) | floor; timber posts below | joists, 5 boards | 5 planks, 1 log, 1 hardwood; an axe | 1 h | -- |
| Wattle Door (`BuildWattleDoor`) | wall, 2 m | posts, sill, lintel, the hurdle's stakes, its weave, rawhide hinges | 8 straight branches, 30 rods, 2 branches, 1 scraped hide, 4 cord | 80 min | -- |
| Plank Door (`BuildPlankDoor`) | wall, 2 m | posts, sill, lintel, planks, battens, pegs | 2 logs, 4 planks, 3 hardwood; an axe | 3 h | -- |
| Lean-To Shelter (`BuildLeanTo`) | shelter | uprights, ridge, rafters, lattice, 7 thatch courses | 2 branches, 6 straight branches, 10 rods, 50 dried grass, 6 cord | 90 min | rain 0 |
| Hide Lean-To (`BuildHideLeanTo`) | shelter | uprights, ridge, rafters, 3 hides, lashings | 2 branches, 6 straight branches, 3 scraped hides, 10 cord | 1 h | rain 0 |
| Hide Tent (`BuildHideTent`) | shelter, 3.2 m across | 8 poles in two lifts, the tie, 3 courses of hide cover, the door flap, pegs and stones | 16 straight branches, 8 scraped hides, 14 cord, 12 rods | 6 h | rain 0.05 |

Times are real work at a builder's pace (Principle 11); the building skill doubles them and spoils a quarter of the material with none.
Two new materials: **Moss01** (an armful, 0.5 kg: "Strip moss" on a mossy rock) and **FieldStone01** (a building stone, 12 kg: "Break off a
building stone" on a boulder with a hammerstone; the boulder stays) -- `UMOCoverForageSettings`.

### Not done yet

- A smoke hood. (Nov 3: the roof peak's thatch, the half walls and their daub, the dry-stone wall done; a hurdle fence is the wattle wall on
  open ground -- walls run on end to end where the aim passes a wall's end.)
- The clay daubs' two big faces pointed inward until Nov 3 (`daub_slab` reversed the wrong face): Unreal culled them, so clay daub never
  showed in a game (Blender draws back faces, the previews looked right). Fixed; the stick walls stand ~9 cm out from their panel's plane,
  so the clay daub now stands 6.5 cm (lumps 5-8) off it.
- Look-and-feel (the moss, the daub, the stone courses, the thatch's colour) is on Wes's by-eye list (`Docs/BACKLOG.md`).
- **Wattle and moss, second pass (Wes, Nov 5: "perfect tubes with perfectly woven curves", "the moss should be a MUCH thinner layer and the wattle should still be visible
  through it").** Stakes and rods go through `rough_stick`: node swellings, kinks, a section that isn't round, trimmed nubs where a limb came off. A course is one or two
  overlapping rods with a hard taper and its own push and rise, and the rod surface is rebaked matte with split bark. The moss has no sheet and no lumps: thin flakes and fine
  strands rooted on the wattle's own rods and stakes (`geo_moss_daub` rebuilds that wattle from the same seed), so nothing floats and most of the weave stays bare. Rendering
  the panel to look at it: `blender -b --factory-startup --python Tools/blender_buildings_blender.py -- wall_look Saved/BlenderBuildings/_look`. The door and the half wall share
  these functions.
- The pieces' assets (`/Game/BlenderBuildings`) and the changed stick Blueprints stay local until the LFS budget allows (as the Blender
  items): `python Tools/blender_buildings.py --surfaces --rows` remakes all of them on another machine.
