# Fable 5 Autonomous Pipeline — Village + Assets + PCG, Stage by Stage

**What this is:** the executable plan for the three planning docs
(`MedievalDynasty_Village_Reference.md`, `Fable5_Village_Handoff.md`,
`Fable5_PCG_Path.md`). Each stage is a card: goal, prerequisites, the exact
autonomous recipe (files + commands), a **gate** (a command whose exit
code/output decides pass), documentation obligations, and the gap risk with
its fallback. Any session picks the next unblocked card and runs it with the
standing loop. **A stage is DONE only when its gate is green and its docs are
written.**

**Grounded 2026-07-04** against the live project: `ue.py` loop verified end-to-end
this campaign; MCP toolsets probed live (`EditorAppToolset` advertises *"asset
imaging"* + PIE session control; `AssetTools` covers *"assets in the project and
files on disk"*; StaticMesh/SkeletalMesh/Texture/Material/DataAsset/CurveTable
toolsets exist; `ProgrammaticToolset` batches calls); **GeometryScripting plugin
is NOT yet enabled** in MO57.uproject (stage A5 enables it); PCG is available via
the existing `UMOPCG*` classes.

---

## 0. The Standing Loop (what every stage runs on)

| Primitive | Command | Verified |
|---|---|---|
| Compile-verify cycle | `python Tools/ue.py cycle [--boot --seed N] [--test]` | 139 s, live |
| Headless unit tests | `ue.py auto` (91 tests ≈ 22 s, exit-coded) | live |
| Regression suite | `ue.py test` → `Saved/MOTestResults.txt` | live |
| 2-client co-op smoke | `ue.py mptest` | live |
| Drive gameplay | `ue.py run "MO.Test.Input …"` / `"MO.Test.ClickWidget …"` | live |
| Observe/tweak AI | `MO.AI.DumpBlackboard` / `MO.AI.SetKey` / `StressSpawn` | live |
| Multi-frame sequences | `ue.py seq <file.py>` (claude_seq) | live |
| Editor python (multi-line) | `ue.py py --file` | live |
| DataTable authoring | `ue.py rows set <table> --file rows.json` (per-row verify) | live |
| Raw MCP | `ue.py mcp <toolset> <tool> --args '{…}'` | live |

**Universal stage protocol:**
1. Mark the task `in_progress`. 2. Run the recipe. 3. Run the gate; iterate until
green (Law 1: verify by using). 4. Docs: commit with evidence in the message;
`PROJECT_STATUS.md` row for milestone stages (marked ◆); `AUTONOMOUS_TOOLING.md`
hit/miss row whenever a **new tool/loop capability** ships; memory pointer only
for cross-session-critical facts. 5. Mark task `completed` with a resolution note.

**Gap protocol (the meta-deliverable):** when a recipe hits a wall, classify it —
`missing-API` (loop can't reach it) / `editor-only` (needs hand work in UE) /
`account-gated` (purchase/login) / `asset-absent` (nothing to acquire) — make ONE
timeboxed workaround attempt, then **surface it**: log the miss in
`AUTONOMOUS_TOOLING.md`, file a task for the missing tool if it will recur, and
report it. Never silently hand-work around a gap twice.

---

## 1. Dependency Map

```
S0 join-spawn ──► S1 client-side MP asserts ─────────────┐
                                                          ▼
A1 ValidateArt ─► A2 asset assign ─► A3 asset shot ─► (visual-QA rung for ALL tracks)
       │                                   │
       ▼                                   ▼
A4 asset import ─────────► A5 graybox gen (GeometryScripting)
                                           │
V0 one-villager-one-real-job ◄─(graybox art)┘
   │  (co-op gate needs S0)
   ▼
V1 settlement loop + colony UI ─► V2 economy/teaching/family/seasons ─► V3 real art (◄ A-track + Fab Route B)
P1 DT_Biomes catalog ─► P2 one-biome voxel scatter ─► P3 regen-on-edit gate ─► P4 layering/density ─► P5 perf/look
   (P-track independent; P2+ uses A3 for visual QA; P5 pairs with #117)
```

Three tracks run in parallel after S0+A1: **S** (ship/co-op), **A** (asset
tooling), **V** (village), **P** (PCG). The serial spine if working alone:
`S0 → A1 → A2/A3 → V0 → P1 → P2 → V1 → A4/A5 → P3 → V2 → P4 → P5 → V3`.

---

## 2. Stage Cards

### ◆ S0 — Player-2 join-spawn (finish co-op start) — task #169
- **Goal:** a second player gets a pawn. Travel is fixed (client follows into the
  generated world, `b2c5e60`); the spawn flow is single-player-shaped (one
  "initial pawn" on OnVoxelReady).
- **Recipe:** `AMOGameMode` — handle non-host players in BOTH arrival paths
  (`HandleSeamlessTravelPlayer` for seamless travel, `PostLogin` for direct
  joins): if voxel not ready, queue the PC; on ready (reuse the existing
  OnVoxelReady subscription), run `FindSafeSpawnLocation` per queued player →
  spawn survivor pawn → create persistence pawn record (GUID identity already
  supports multiple) → possess. Name source: "Survivor N" placeholder until the
  session layer provides one.
- **Gate:** `ue.py mptest` — the red assert `client pawn possessed with
  inventory` flips green; suite fully green.
- **Gap risk:** H51 name-handshake assumed Login-path only → seamless path may
  bypass it; fix at the chokepoint, not per-path.

### S1 — Client-driven MP asserts (prove the RPC surface end-to-end)
- **Prereq:** S0.
- **Recipe:** `test_multiplayer.py` phase 5 already drafts it: grant mats to the
  client's host-side proxy pawn (authority), client-world `EnqueueCraft` →
  `ServerRequestEnqueueCraft`, assert host queue grew + replicated back. Add a
  client-world `MO.Test.DropPickup` run (its GUID assert was written for exactly
  this). Wire results into the mptest pass-count.
- **Gate:** `ue.py mptest` reports the craft-RPC + pickup asserts green.
- **Docs:** ◆ PROJECT_STATUS row — "co-op verified: client crafts/picks up on
  the host authoritatively." Unblocks honest #162/#163 verification later.

### ◆ A1 — `MO.Test.ValidateArt` (make the art gap a number) — task #171
- **Recipe (pure C++, one `cycle`):** new verb in `MOCheatSubsystem`, modeled on
  `RunDataValidation`: walk recipes (`PlacementData.PreviewMesh`,
  `BuildableActorClass`, `Icon`), items (WorldVisual mesh/actor, IconSmall/
  IconLarge), skills (Icon). Report per slot: `MISSING` (unset) or `PLACEHOLDER`
  (path contains `BasicShapes`/`EngineMeshes`/a named placeholder list). Append
  results to `WriteTestResults` and a `Data:Art` line in `MO.Test.RunAll`.
- **Gate:** `ue.py run "MO.Test.ValidateArt"` prints honest totals; run it once
  and **log the current art debt** in PROJECT_STATUS (the baseline burn-down).
- **Gap risk:** none — this is the ValidateData pattern re-aimed.

### A2 — `ue.py asset assign` (assignment is a data operation)
- **Recipe:** thin `ue.py` subcommand over the proven `rows set`: set a mesh/
  icon soft-path onto a row field, then re-run `ValidateArt` to confirm the count
  decremented. (Mostly formalization — `rows set` can already do this.)
- **Gate:** assign a known existing mesh to one building recipe → `ValidateArt`
  placeholder count drops by exactly 1; PIE-place the building and see it.

### A3 — `ue.py asset shot` (the visual-QA rung; charter Pillar-0 gap #6)
- **Recipe:** probe `EditorToolset.EditorAppToolset` with `describe_toolset` for
  its *asset imaging* / viewport-capture tools (toolset blurb confirms they
  exist; exact names verified in-stage). Wrap: `ue.py asset shot <assetPath|viewport> <out.png>`.
  The agent then Reads the PNG to QA ("does the axe read as an axe?").
- **Gate:** produce a PNG of (a) a named static mesh and (b) the PIE viewport;
  agent-review it in-session.
- **Docs:** AUTONOMOUS_TOOLING hit/miss row — this is a NEW loop capability used
  by V-track (art QA) and P-track (biome look QA).
- **Gap risk `missing-API`:** if EditorApp imaging is thumbnail-only, fallback =
  `HighResShot` via bridge + read the file from `Saved/Screenshots`.

### A4 — `ue.py asset import` (files on disk → uassets)
- **Recipe:** probe `AssetTools` tool names for import (blurb: "assets in the
  project and files on disk"); wrap `ue.py asset import <file> <destPath>` for
  FBX/glTF/PNG. Chain: import → `assign` → `shot` → agent QA. Test with a free
  local file first.
- **Gate:** an FBX and a PNG round-trip to visible, assigned, screenshotted
  assets with zero manual editor clicks.
- **Gap risk `missing-API`:** fallback = editor-python
  (`unreal.AssetImportTask` via `ue.py py --file`) — known-good UE API.

### A5 — Gray-box generation (`MO.Build.GenGrayboxMeshes`)
- **Prereq:** enable **GeometryScripting** plugin (edit `MO57.uproject` — it is
  currently absent — + `GeometryScriptingCore` in Build.cs; one `cycle`).
- **Recipe:** editor-python or C++ verb: for every building recipe missing a
  mesh (A1's list), generate a footprint-derived box+roof `UDynamicMesh` → bake
  to StaticMesh at a `/Game/Penumbra/Graybox/` path → `assign`. Tier-tinted
  materials via MaterialInstance toolset.
- **Gate:** `ValidateArt` building-mesh MISSING count → 0 (all placeholders are
  *generated* placeholders); `asset shot` contact sheet for agent review.
- **Why it matters:** unblocks V0–V2 with zero external art, forever provides
  instant stand-ins for new content.

### ◆ V0 — One villager, one REAL job (village vertical slice) — task #170
- **Prereq:** none for single-player gate; S0 for the co-op gate. Graybox (A5)
  optional — existing meshes suffice.
- **Recipe:**
  1. `UMOColonyManagerSubsystem` MVP: settlement record (name, center, roster),
     residency map (pawn GUID ⇄ house actor GUID), workshop map (building ⇄
     station + assigned job). Persist via the existing save pattern.
  2. Building roles: mark a buildable as House / Workshop / CommunalStorage
     (data hooks exist: `ProvidedStationType`, `ContainerSlotCount`).
  3. Dev verbs: `MO.Colony.Recruit <pawnSub>`, `.AssignHouse <pawn> <houseSub>`,
     `.AssignJob <pawn> <stationSub> <recipeId>`, `.Status` (roster dump,
     [MOQUERY]-tagged).
  4. Villager job AI: extend `BT_Survivor`/`UMOSurvivorJobQueueComponent` with a
     CraftAtStation job: pathfind to station → withdraw real ingredients from
     communal storage → run the REAL `EnqueueCraft` (server path) → wait craft
     time → deposit real output to storage. Blackboard keys via
     `MOBlackboardKeys.h`.
- **Gate (seq test, committed as `Content/Python/test_village_v0.py`):** boot →
  spawn/recruit a survivor → place graybox workbench+storage via dev verbs →
  stock 1× Flint01 → `AssignJob KnapFlintFlakes` → within N sim-minutes,
  `FlintFlake01` exists in communal storage and `MO.AI.DumpBlackboard` showed
  the job cycle. Exit-coded. **Co-op sub-gate (post-S0):** same via `mptest` —
  the villager is server-authoritative, client sees the result.
- **This is the fun-gate for the whole pillar. No V1 before it's green.**

### ◆ V1 — The settlement loop + colony UI
- **Recipe:** communal-storage upkeep tick (villagers eat REAL food via
  `UMOMetabolismComponent`; empty storage → hunger → mood → leaves/dies);
  housing rules (capacity, no-home mood decay) with home quality read from the
  EXISTING multi-axis shelter/exposure model; `UMOPersonalityComponent`
  (3 axes) + mood fed by the real sims (hunger/pain/cold/sleep);
  `UMOCharacterHistoryComponent` event log. Colony UI per the authored design:
  `UMOColonyBarWidget`, `UMOColonyOverviewWidget`, `UMOCharacterCardWidget`,
  `UMOColonyPortrait`, `UMOTaskAssignmentWidget` — built on the CommonUI stack;
  WBP layout is the known editor-work slice (MCP `BlueprintTools` assist where
  proven, else flag per gap protocol).
- **Gates:** (a) `test_village_v1.py` seq — 3 villagers, one accelerated day:
  food drains by real consumption, unhoused villager's mood decays, housed one's
  doesn't; (b) UI harness — open overview via UIManager, `MO.Test.ClickWidget`
  an assignment flow, assert the job landed (the V0 gate re-driven THROUGH UI);
  (c) `ue.py auto` — new headless tests for mood math + upkeep math (pure
  logic); (d) save→load round-trip of the whole settlement.

### V2 — Economy, progression, dynasty
- **Recipe (four sub-cards, each gated):** quotas/priorities (the MD abstraction
  for scale — standing orders drive V0 jobs; headless-test the quota math);
  teaching (skilled→unskilled 2×, existing skills API) + School building
  (maintains tree vs. the existing decay); relationships/family
  (`UMOCharacterHistoryComponent` graph → recruitment gating by standing,
  marriage/children as V2.5 data model first, sim later); seasons-as-gameplay
  (resource windows + winter upkeep spike on the existing clock+weather).
- **Gates:** per sub-card seq/auto tests; milestone gate = a 5-villager
  settlement runs 3 accelerated days unattended without starving/leaving,
  producing per quota (the "settlement holds" soak, exit-coded).

### V3 — Real art (the asset pipeline pays off)
- **Prereq:** A-track complete; V0–V2 fun-proven.
- **Recipe:** run `ValidateArt` → the shopping list. Route B per handoff §5C:
  claude-in-chrome Fab playbook (search → screenshot-evaluate → ranked shortlist
  with license/tri-count/voxel-compat notes) → **human purchase gate** → A4
  import → A2 assign → A3 shot QA. MetaHuman variation pass for villagers.
- **Gate:** `ValidateArt` PLACEHOLDER count for shipped-content slots → 0;
  contact-sheet review.
- **Docs:** every Fab run logged hit/miss — this is the acquisition-gap evidence
  the exercise exists to gather.

### P1 — `DT_Biomes` catalog (data-first PCG) — task #172
- **Recipe:** `FMOBiomeDefinitionRow` (BiomeId, display, terrain bands
  height/slope/moisture/temperature, species palette as
  `TArray<FMOBiomeSpeciesEntry>` {mesh soft-path, density curve params, cluster
  radius, HISM tag}, ground-cover set, edge-blend width). Author 3 starter
  biomes (TemperateForest, Meadow, RockyHighland) via `ue.py rows set`. Extend
  `ValidateData` to check biome→mesh/tag integrity.
- **Gate:** rows round-trip; validation green. Pure data+C++ — fully autonomous.

### P2 — One-biome voxel-surface scatter
- **Recipe:** `UMOPCGBiomeSpawnerSettings` (extend the existing voxel-surface
  spawner family — KEEP voxel sampling, per `Fable5_PCG_Path.md` §4): read a
  low-frequency biome mask (seeded noise over world XY) + `DT_Biomes` → emit
  per-species points with graded density → existing HISM tagging so harvest
  keeps working. Wire into the MOPCGScattering level's PCG volume (editor step —
  MCP Scene/Object tools where possible, else flag).
- **Gate:** boot seed → `ue.py py` counts HISM instances by biome tag in two
  probe regions (forest region ≫ meadow trees, etc.) → exit-coded; `asset shot`
  viewport captures for agent look-review.
- **Gap risk `editor-only`:** PCG **graph node wiring** — mitigated by design
  (thin stable graph, all variation in data); if new topology is needed, that's
  the flagged MCP-PCG gap.

### P3 — Regen-on-edit (the destructible-world superpower)
- **Recipe/Gate:** seq test — count instances in a region → `ServerApplyTerraform`
  dig via the existing verb path → assert scatter updated (counts changed,
  nothing floating: line-trace N sampled instances to surface). This gate
  *defines* MO57's PCG; if the current pipeline can't regen locally, this stage
  is where that work happens (PCG runtime generation / partial regen).

### P4 — Layering, density, transitions, variety
- **Recipe:** all biomes live; clustering/thinning curves; edge blending;
  palette breadth (Fab-harvested species via A4 import when V3-era, engine/
  existing meshes until then).
- **Gate:** statistical distribution probe per biome (P2's counter, all biomes)
  + contact-sheet look review + `ue.py auto` for the mask/band math.

### P5 — Perf + look (pairs with #117)
- **Recipe:** Nanite flags on scatter meshes (the #151/#154 pipeline), HISM/cull
  audit (partly done), World Partition/HLOD evaluation, Lumen/HWRT tuning (#117).
- **Gate:** scripted flythrough seq sampling frame-time via `ue.py py`
  (`unreal.SystemLibrary` delta-time average per region) against a budget +
  before/after shots. Best-effort honest numbers over vibes.

---

## 3. Self-Extension Rules (how the pipeline stays autonomous)

1. **Every new tool ships with its gate** — a command that proves it, wired into
   `RunAll`/`auto`/`mptest` where it fits (ValidateArt → RunAll, like ValidateData).
2. **Every gap becomes an artifact** — AUTONOMOUS_TOOLING miss-row + a task if
   recurring. The Fab purchase gate and MCP PCG-graph authoring are *expected*
   artifacts of this run; treat them as findings, not failures.
3. **Data over graphs, verbs over clicks** — prefer DataTable/data-asset-driven
   designs (MCP-authorable) and console verbs (bridge-drivable) at every fork;
   it's why V and P stay in the loop.
4. **Gray-box before beauty** — no stage blocks on art; A5 generates stand-ins on
   demand.
5. **The soak is the truth** — milestone gates (V2's 3-day settlement, P3's
   dig-regen) are *unattended* runs with exit codes, not demos.

## 4. Suggested Execution Order (solo spine)

`S0` (finish co-op start — in flight, #169) → `A1` (art debt baseline, one cycle)
→ `A3` (visual-QA rung — everything downstream wants it) → `V0` (the fun-gate)
→ `P1→P2` (world becomes places) → `S1` → `V1` → `A4/A5` → `P3` → `V2` → `P4`
→ `P5` → `V3`. Re-evaluate at every ◆ milestone against the charter's four gates
(co-op-correct / legible / fun / verified) before descending further.

---

## Codex design-review reconciliation (2026-07-07)

External review of the game's design direction, mapped against actual state.
The review's core thesis is already this project's thesis (body -> world ->
tools -> survivor -> job -> settlement, no sim-breaking), and its north-star
slice ("one villager, one real job") shipped July 4 as V0. What follows is
only the delta.

| Recommendation | Status | Action |
|---|---|---|
| One villager, one real job | **DONE** (V0, gate 5/5) | — |
| Colony through need, not UI | **DONE** (V1 shipped sim-first; UI is minimal by design) | — |
| Settlement need loop | **DONE** (V1 feeding/mood/housing + V2.1 quotas) | — |
| Seasons -> settlement planning | **PARTIAL** (winter kcal V2.4, clothing warmth 2026-07-07) | firewood demand + wet-wood drying ride the F1 fire card |
| Shelters matter before fancy buildings | **PARTIAL** (roof=insulation, wetness, shelter AI) | F1 below |
| While-you-were-away summaries | **PARTIAL** (history component logs everything) | A6 below |
| First-hour readability | **PARTIAL** (moodles, craft/harvest failure buckets) | R1 below |
| Tedium -> assignable jobs | **PARTIAL** (quotas, craft queue) | T1-T3 below |
| Blocked-task intelligence | **NEW** | B1 below — first up (pure engineering, rule-6 shaped) |
| Water progression ladder | **NEW TRACK** | W1-W4 proposed below — **design fork for Wes** |
| Creature ecology over breadth | Planned (MobAIPlan) | unchanged |

### New cards

**B1 — blocked-job reasons at the data layer.** EMOJobBlockReason
(MissingIngredient / NoPath / TooCold / TooTired / NoStation / StorageFull /
ToolMissing / ThreatNearby) on the survivor job machine; jobs report WHY they
stopped; colony upkeep raises a tier-2 alert; MO.Colony.Status prints it.
Gate: force each blocker in a seq, assert the reason surfaces.

**A6 — possession-transition summary.** On repossess, surface the pawn's
recent history: actions, consumption, health/mood delta, interruptions,
learning. Data-layer API on UMOCharacterHistoryComponent first
(BuildAwaySummary(sinceGameSeconds)), UI panel later. Gate: possess-away-
repossess seq asserts the summary contains the villager's real activity.

**F1 — fire as a local heat source. UNITS 1-4 LANDED 2026-07-07**
(e7ab946, d11a7e1, 965ab3e, f100fa7): IMOLocalHeatSource + registry
aggregation; stations radiate while lit+fueled (campfire/forge 40C,
kitchen 20C); vitals folds the delta into feels-like; wet clothing dries
~11x by the fire (works in rain); cold villagers seek the nearest lit
hearth (homeless included); colony hearth pass issues RefuelStation jobs
— firewood demand joined food demand. Gates: test_fire_heat 5/5,
test_village_hearth 3/3, test_village_firewood 3/3. REMAINING: wet wood
burns poorly (needs per-item wetness state — design first).

**T1 — transfer-all with combined timer** (take all / deposit matching —
one gesture, real total duration). **T2 — sweep pickup** (timed radius
gather, visible + interruptible). **T3 — haul/restock survivor jobs**
(source -> storage; water/food restocking as standing orders). FIRST CUT
LANDED 2026-07-07 (f100fa7): RefuelStation job = the haul-job pattern
(fetch into pack -> deliver -> station-specific handling); firewood
restocking is live. Generalize to food/water via the same states-20+
machine.

**R1 — readability audit.** Environmental "why" readout (cold because wet +
wind + no roof), action timers visible everywhere, "why failed" strings for
every failure bucket that already exists at the data layer.

### W-track proposal (NEEDS WES APPROVAL — new pillar)

W1: drink from world water (ocean unsafe -> illness risk; containers fill).
W2: freshwater spring as a discoverable world feature (World_Features arch),
limited flow. W3: rain collector building (weather -> reliability). W4:
pawn water-hauling as a T3 job. Rationale: gives exploration survival
meaning and feeds the settlement-logistics chain. Fork logged rather than
built: it is a new world-features + illness-model commitment.
