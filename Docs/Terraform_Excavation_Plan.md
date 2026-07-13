# Terraform Unit 3 — Designation-Based Pawn Excavation

**Status:** design / awaiting fork sign-off (July 13 2026). Units 1 (volume→duration
`14e0d432`) + 2 (incremental partial-on-interrupt `bd893aac`) landed. This is the
"account for the dug-up soil" half of Wes's terraform directive.

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

| Stage | What | Gate | Fork-dep |
|-------|------|------|----------|
| **1. Earth primitive + conservation core** | `TerraformAtLocationEx` returning moved-volume; `ItemsPerCubicMeter` knob; add `Dirt01` to DT_Items | Headless/PIE: dig V at A + raise V at B → ledger balances, Dirt count matches | none |
| **2. Designation subsystem + dev verbs** | `UMODesignationSubsystem` (IMOSaveDomain) owning `FMODigZone`/`FMODumpZone` (center+extent+kind+remaining-volume+default dump target); `MO.Terraform.Designate*`/`Designations` verbs | Designate zones → save/reload → persist | geometry, scoping |
| **3. ExcavateAndHaul pawn job** | job type + entry fields (dig anchor, dump discriminator+GUID/loc, Dirt id+count) + `EnqueueExcavateJob` + `CanExecuteSimply` + `Start/UpdateExcavateJobExecution` (band 30+, mirror RefuelStation 20-23); **batch sculpt ops** to avoid per-op world sweep | Enqueue on a survivor → digs (terrain drops, Dirt produced) → hauls → deposits (fill-zone raise OR container); conservation holds | — |
| **4. Colony dispatch pass** | `RunExcavationPass` in upkeep tick: idle villagers auto-assigned, one-worker-per-zone, `ShelteringVillagers`-excluded | Designate dig+dump, recruit villagers, run upkeep → auto-excavation, no manual assign | scoping |
| **5. Flatten decomposition** | flatten designation → paired dig-high/fill-low work under conservation | Designate flatten over uneven ground → levels toward target | geometry |
| **6. UI: designate tool + dump popup** | `DesignateZoneAction` input + `UMODumpDestinationContextMenu` (clone `UMOKeepOnHarvestContextMenu`) + nearby-container enumeration + `FMODumpDestination{Kind,GUID}` routing | Computer-PIE / widget smoke + dev-verb backend | geometry |
| **7. (later) Cart actor** | minimal mobile container (`AActor` + `UMOInventoryComponent` + `IMOInventoryHolderInterface`) for "fill nearby inventory" | overlap-enumerated + deposit | — |

Stages **1–5 are backend** (headless/PIE gate-able autonomously); **6 is UI**; **7 optional**.

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
