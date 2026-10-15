# Terraform Unit 3 — Designation-Based Pawn Excavation

**Status (refreshed 2026-10-09 against current master):** forks LOCKED (sphere zones, settlement-scoped dispatch, 80 items/m3, solo player digs manually).
**Stages 1, 1b, 2, 3 are LANDED and in the code** (`TerraformAtLocationEx`, `Dirt01` + earth materials, `UMODesignationSubsystem` + `MODesignationTypes.h`,
`ExcavateAndHaul` job in `MOSurvivorController` / `MOSurvivorJobQueueComponent`); **Stage 4 LANDED Oct 14 2026** (`UMOColonyManagerSubsystem::RunExcavationPass` + pure `DecideExcavationWork`; headless `MOFramework.Colony.Excavation.*`, live gate `Content/Python/test_excavation_dispatch.py` 10/10); **Stage 5 LANDED Oct 14 2026** (live bite points + flatten; `Content/Python/test_excavation_flatten.py` 6/6); **Stages 6-7 are not started**.

**Oct 14 2026 -- earth accounting fixed at the primitive.** The Voxel plugin's height sculpt moves `Radius/10 x Strength` cm at the centre (smooth falloff to the rim), but `ComputeMovedVolumeCubicMeters` booked `pi r^2 x Strength x 1 m`: an excavation bite (120 cm, 0.15) was booked at 0.68 m3 and minted 54 Dirt while the ground lost 0.046 m3; the player's default dig (250 cm, 0.1) was charged 16 h for 0.28 m3 (2.3 h at the anchor). The conversion now integrates the plugin's own rule (`MOVoxel::HeightSculptMovedVolumeM3`); `test_excavation_primitive.py` measures the real ground drop (2.39 cm vs 2.5 predicted). Durations for height Dig/Raise are now ~7x shorter for the player and ~15x for a pawn bite -- still Wes's anchor (5 min per 0.01 m3), now charged for earth that really moved.
Units 1 (volume->duration `14e0d432`) + 2 (incremental partial-on-interrupt `bd893aac`) landed earlier. See "Remaining work" at the bottom.

## Goal (Wes, binding)

Player **designates** separate **dig** / **dump-fill** / **flatten** areas (multiple
allowed). Idle **villagers automate** the work: dig a bounded volume from a dig zone,
haul it, and deposit it either at a designated fill zone (raising terrain there) OR into
a nearby container (cart/sack/chest) as a carryable **Dirt** resource. **Flatten** =
the same loop with dig = the high spots and fill = the low spots, under **conservation
of earth**. A **"where do you want to dump the contents?"** popup lists: already-designated
sites / **"fill inventory"** / **"fill nearby inventory"** (nearby containers).

This is the RimWorld terraforming loop and the CLAUDE.md "delegate tedium to AI pawns"
killer feature: the player designates, the simulation executes without them, at realistic
(volume-based) time cost — which also collapses terraform RPC frequency.

## Verdict from the integration map (6-agent codebase sweep)

**Nothing to reuse wholesale, but everything to compose.** There is **no** existing
"player designates a work AREA that pawns act on": building placement is point/single-actor
and isn't even pawn-worked (no Build job); **no farm/crop/plot system exists** (the
"waters crops" idea is a commented-out stub); no stockpile/zone/selection-volume tooling.
`FMOTerrainModifiedZone` is an *after-the-fact* record of worked ground (and its sweep
deletes grass) — **do not overload it.** So designation is **built new, by composing two
proven patterns**:

1. **Storage/persistence** → clone the *shape* of `UMOTerrainModificationSubsystem` (world
   subsystem owning a flat list of spatial records + a `FIntPoint` spatial grid + `IMOSaveDomain`
   save/load) into a new **`UMODesignationSubsystem`**.
2. **Dispatch** → clone `RunQuotaPass`/`RunHearthPass` (colony upkeep tick finds idle
   `AMOSurvivorController` villagers and enqueues jobs on their `UMOSurvivorJobQueueComponent`
   via a pure-static decision fn) into a new **`RunExcavationPass`**.

Everything else (job queue, replication, GUID-rehydrated save/load, authority guards, XP,
inventory transfer, context-menu popup) is an existing seam we plug into.

## The earth-moving + conservation model

- Every `AMOCharacter` — player AND AI — already owns a `UMOTerraformingComponent` (default
  subobject, `MOCharacter.cpp:105`). `TerraformAtLocation(WorldLocation, Mode)` is a
  **location-based, authority-gated** apply that any pawn can call — the AI dig/raise primitive.
- **Conservation is arithmetic on an estimate, not a measured quantity.** One op moves
  `π·r_m² · depth` m³, where `depth = RaiseLowerStrength · TerraformDepthPerStrengthMeters`
  (`GetActionDepthMeters`). The Voxel plugin returns no actual m³. So to move spoil `V` down
  at A and up at B: compute `ops = V / per-op-volume`, Dig A `ops` times, Raise B `ops`
  times — the ledger balances **by construction**. `ComputeTerraformDurationSeconds`
  (static, headless-testable) sizes the realistic duration = `V · TerraformSecondsPerCubicMeter`.
- **New primitive (recommended): `TerraformAtLocationEx(Location, Mode, Radius, Strength)`
  returning moved-volume m³.** The current Dig/Raise read the *shared* `Config.Radius/Strength`
  singleton — mutating Config per-call stomps the pawn's brush settings and isn't re-entrancy
  safe. A parameterized overload plumbs explicit values into `MOVoxel::HeightSculpt` and
  returns the estimated volume for the ledger.
- **Spoil = a real `Dirt01` item** (Material). Inventory is slot-based (no weight cap), so
  carry limit = slots × `MaxStackSize`. Volume→item via an `ItemsPerCubicMeter` config knob
  (grounded default ~80/m³ ≈ one shovelful per item; tunable). Raise/fill **consumes** Dirt
  1:1 with the raised volume → conservation enforced at the item layer, and it's what makes
  "fill inventory / fill nearby container" real options.
- **Flatten = paired Dig-high + Fill-low ops, NOT the Flatten sculpt mode** (whose moved
  volume is sign-varying and unusable as a spoil source).

## Staged plan (each stage = one gated, committable unit)

| Stage | What | Gate | Status |
|-------|------|------|----------|
| **1. Earth primitive + conservation core** | `TerraformAtLocationEx` returning moved-volume; `ExcavationItemsPerCubicMeter` knob + `SpoilItemsForVolume` | `test_excavation_primitive.py` | ✅ DONE `0583d1ed` (7/7) |
| **1b. Earth materials** | added `Dirt01` + `Topsoil01`/`Gravel01`/`Mud01`/`Silt01` (Material, stack 20, droppable) to `DT_Items` | readback-verified | ✅ DONE (working-tree `DT_Items.uasset`, uncommitted w/ codex's M21 — coop CL-0020) |
| **2. Designation subsystem** | `UMODesignationSubsystem` (IMOSaveDomain) owning `FMODesignationZone` (sphere: center+radius+kind+remaining-volume+default dump target); CRUD + persistence | `test_designation_persist.py` | ✅ DONE `ebac9e92` (7/7) |
| **3. ExcavateAndHaul pawn job** | job type + entry fields (dig-zone GUID, dump discriminator+GUID/loc, Dirt id) + `EnqueueExcavateJob` + `CanExecuteSimply` + `Start/UpdateExcavateJobExecution` (band 30-33, mirror RefuelStation 20-23) | `test_excavation_job.py` — survivor digs/hauls/fills, conservation holds | ✅ DONE `90a177a9` (5/5) |
| **4. Colony dispatch pass** | `RunExcavationPass` in upkeep tick: idle villagers auto-assigned to dig zones w/ a dump target, one-worker-per-zone, `ShelteringVillagers`-excluded | Designate dig+dump, recruit villagers, run upkeep → auto-excavation, no manual enqueue | ⏳ NEXT (unblocked) |
| **5. Flatten decomposition** | flatten designation → paired dig-high/fill-low work under conservation | Designate flatten over uneven ground → levels toward target | pending |
| **6. UI: designate tool + dump popup** | `DesignateZoneAction` input + `UMODumpDestinationContextMenu` (clone `UMOKeepOnHarvestContextMenu`) + nearby-container enumeration + `FMODumpDestination{Kind,GUID}` routing; also the `MO.Terraform.Designate*` dev verbs (deferred here) | Computer-PIE / widget smoke + dev-verb backend | pending |
| **7. (later) Cart actor** | minimal mobile container (`AActor` + `UMOInventoryComponent` + `IMOInventoryHolderInterface`) for "fill nearby inventory" | overlap-enumerated + deposit | optional |

Stages **1–5 are backend** (headless/PIE gate-able autonomously); **6 is UI**; **7 optional**.
Stage 3 is the next unblocked backend stage once `Dirt01` (1b) exists — the job references
the spoil item by `UMOTerraformingComponent::ExcavationSpoilItemId` (default `"Dirt01"`).

## Key integration anchors (from the map)

- **Job model to clone:** `RefuelStation` machine — `MOSurvivorController.cpp:1324-1492`
  (Start/Update), move-leg helper `:1272-1322`, `CanExecuteSimply :568-584`, band dispatch
  `:720-730`, `CompleteSimpleJob :818-858` (extend its reset block for excavate scratch).
  Enum `MOSurvivorJobTypes.h:87`; entry fields `:160-183`; `EnqueueRefuelJob`
  `MOSurvivorJobQueueComponent.cpp:161-200`. **GUID-mirror every actor ref** (H39,
  `ResolveJobActorRefs :285-308`) — a dump *zone* as a plain `FVector` sidesteps this.
- **Dispatch to clone:** `RunHearthPass MOColonyManagerSubsystem.cpp:1269`, `RunQuotaPass
  :282` + pure-static `DecideQuotaWork :252`, upkeep `RunUpkeepTick :695` (insert pass ~`:732`),
  roster `GetColonyRoster :102`. Idle test = AI controller + not `ShelteringVillagers` +
  `GetCurrentJob().IsValid()==false`. **Claimed-set to enforce one worker per zone.**
- **Apply:** `TerraformAtLocation MOTerraformingComponent.cpp:220-291` (authority-gated `:225`);
  `HeightSculpt MOVoxelAlias.cpp:65`; volume math `ComputeTerraformDurationSeconds :669` /
  `GetActionDepthMeters :679`; knobs `.h:346,351`. **Perf: every apply fires
  `RegisterWorkedGround→SweepModifiedZones` (full-world ISM sweep) + ~1s burst — batch.**
- **Inventory/containers:** gate deposits with `CanAddItemByDefinitionId
  MOInventoryComponent.cpp:652` FIRST (AddItemByGuid silently overflows on full);
  nearby-container enum = clone `FindMaterialSources MOBuildProgressComponent.cpp:365-419`
  (SphereOverlap + filter `IMOInventoryHolderInterface` + room). Add `Dirt01` to
  `DT_Items` (UTF-16 — use the datatable-json tooling, never PowerShell).
- **Designation storage to clone:** `UMOTerrainModificationSubsystem.h:65-342` (records +
  grid + `IMOSaveDomain`). New save-domain name (e.g. "Designations"); **separate store**,
  do NOT overload `FMOTerrainModifiedZone`.
- **Dump popup to clone:** `UMOKeepOnHarvestContextMenu` (runtime-built list) →
  `UMODumpDestinationContextMenu`; lifecycle in a controller mirroring
  `MOCraftingUIController.cpp:452-525`. **Selection payload = struct `{Kind, GUID}`**, not
  a bare FName (heterogeneous list). Note: click-outside-close IS implemented now (stale
  CLAUDE.md warning refuted); set `bCloseOnClickOutside=false` during world-click designation.

## Gotchas to honor (from the map)

- **Authority-only** everything (dispatch, sculpt, inventory) — runs server/host side.
- **One-worker-per-zone** via a Claimed set built from in-flight jobs each pass, or every
  tick piles another villager on the same zone.
- **`ShelteringVillagers` exclusion** — cold pawns are owned by the shelter pass; a stay-order
  suspends job processing (an in-flight dig aborts, not pauses).
- **120s wedge watchdog** (`:1373`) assumes a ~35s cycle; a big multi-trip dig legitimately
  exceeds it — size to worst-case or reset per trip.
- **Batch sculpt ops** — a hauling loop of many small `TerraformAtLocation` calls triggers many
  full-world foliage sweeps (real perf hazard).
- **Height-sculpt is open-pit only** (can't cut overhangs/tunnels). Fine for pits/pads/flatten;
  tunnels are a separate volume-mode concern (out of scope for unit 3).

## Design decisions (LOCKED — Wes, July 13 2026)

1. **Zone geometry = SPHERE (center + radius).** Reuses the terraform brush and the terrain-mod
   spatial grid verbatim — least new code. Dig/dump/flatten zones are all `center + radius`.
2. **Scoping = SETTLEMENT-SCOPED.** The colony upkeep pass (`RunExcavationPass`) dispatches idle
   recruited villagers to zones inside the settlement, reusing the hearth/quota dispatch loop.
   Requires a founded settlement + recruited villagers; pre-settlement the solo player digs
   manually with the existing terraform tool (designation is a pawn-delegation feature).
3. **Volume→item constant** — default `ExcavationItemsPerCubicMeter = 80` (~1 shovelful/item),
   config-tunable.
4. **Solo player** keeps digging manually via the existing terraform tool; designation drives pawns.


## Remaining work (written 2026-10-09; nothing below is built)

Order is the table's: 4 -> 5 -> 6 (-> 7). Each is a gated, committable unit; 4 and 5 are backend and can be proven with a headless/seq gate or a `ue.py nettest` run; 6 has UX surface and needs Wes's checkpoint before it is built.

**Stage 4 - `RunExcavationPass` (colony dispatch). DONE (Oct 14 2026):** one worker per Dig zone with earth left (claimed set = every queued/running `ExcavateAndHaul` job), sheltering villagers excluded, settlement-scoped, last pass of the upkeep tick (food, warmth and fires first), nearest idle villager to the dig. Spoil goes to the zone's `DefaultDumpTargetGuid` fill zone if it has room, else the nearest fill zone with room, else a container (the default target when it is a container GUID, else the nearest settlement container), else the zone WAITS (no heap on the ground). Exhausted zones: `ConsumeZoneVolume` already drops a zone whose budget reaches 0 (stage 2 behaviour), so the open policy question is moot unless Wes wants exhausted zones kept. The watchdog risk below was already handled (`ExcavateWatchdogSeconds` scales with the bite). Original card: Tasks: (a) pure static `DecideExcavationWork(idle roster, zones, claimed set)` returning (pawn, zone, dump target) pairs, unit-testable like `DecideQuotaWork`; (b) the pass in `RunUpkeepTick` next to the hearth pass; (c) claimed set rebuilt from in-flight `ExcavateAndHaul` jobs every pass (one worker per zone); exclude `ShelteringVillagers`; (d) budget exhaustion: a zone with `RemainingVolumeM3 <= 0` is skipped and (policy to confirm) removed. Gate: designate dig + fill, recruit two villagers, run upkeep: both work different zones, nobody doubles up, conservation holds (`test_excavation_job.py` pattern + the claimed-set control). Risk: the 120 s wedge watchdog (`MOSurvivorController.cpp` ~1373) assumes a ~35 s cycle; a long dig needs a per-trip reset.

**Stage 5 - flatten decomposition. DONE (Oct 14 2026):** every bite now comes from the LIVE ground (`UMODesignationSubsystem::FindBitePoint`: voxel-only traces on a grid over the zone's disc): a dig zone digs its highest ground (until then every bite went to the centre: a pit), a fill zone fills its lowest ground below its target (was: a mound at the centre that ignored the target) and counts as full when nothing is below target - 15 cm. A flatten zone is its own source and sink (`FindFlattenWork`): while some ground is more than `LevelToleranceCm` (15 cm) off target and there is ground on both sides, dig the highest point above target and fill the lowest below it (earth inside the tolerance band still finishes the other side). Off target on one side only = it needs outside earth: it waits. Gate: mound + pit (equal earth), target = the disc's mean height: 8 trips, level within 15 cm, no spoil left. **For Wes (stage 6 UI):** the default flatten target should be the mean ground height of the zone -- the only target the zone can reach with its own earth; any other needs a dig or fill zone beside it. Original card: Tasks: (a) flatten zone = target height + sphere; sample ground on a small grid; (b) split into dig-high / fill-low work items with equal volume (conservation by construction, as the primitive's estimate); (c) re-sample after each pair and stop within tolerance. Gate: uneven ground -> within X cm of target, dug volume == filled volume +- one bite. Open question for Wes: what is the target height when the player does not say (mean of the sphere's ground, or the height at its centre)?

**Stage 6 - UI (needs Wes's checkpoint first).** Tasks: designate tool input action (C++ in `AMOPlayerController::SetupInputComponent`, per the project rule), a zone preview in the world, `UMODumpDestinationContextMenu` cloned from `UMOKeepOnHarvestContextMenu` (payload `{Kind, GUID}`), nearby-container enumeration cloned from `FindMaterialSources`, and the `MO.Terraform.Designate*` dev verbs. **Co-op:** a client must create/edit zones through a server RPC that applies the SAME trust rules the placement fix added (reach from the requesting pawn + a zone-count cap + settlement membership); add a `nettest actions` case with a far-away designation as the refusal control. UI is built with the spec toolset (`python Tools/ue.py ui ...`, remember the SizeBox fix in `Docs/UI_TOOLING.md`) and checked in a packaged window with real clicks (Docs/AUTONOMOUS_TOOLING.md).

**Stage 7 (optional) - cart actor.** Only if "fill nearby inventory" needs something to fill besides existing containers.

**Decisions only Wes can make** (none is blocking Stage 4): flatten target height (above); whether an exhausted zone deletes itself; the per-zone `RemainingVolumeM3` default when the player draws a zone; whether zones are visible to other players in co-op (recommend yes -- but `UMODesignationSubsystem` has no replication at all today (checked: no replicated state, no RPCs), so a client would see nothing until Stage 6 adds it); the dump popup's default choice.
