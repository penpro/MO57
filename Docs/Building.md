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
Deposited build materials are part of the save payload. Persistence patterns: `Docs/Persistence.md`.
