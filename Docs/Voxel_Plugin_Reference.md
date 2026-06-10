# Voxel Plugin Pro 2.0 — Reference & Lessons

> Living document. Add to it as we test more techniques and discover
> capabilities / pitfalls. Reference both for MO57 contributors and for
> any future modders building custom terrain.

> Companion to `Docs/Terrain_Foundation_Plan.md` (the actionable recipes).

## TOC

1. [How the pieces fit together](#1-how-the-pieces-fit-together)
2. [Graph types & when to use which](#2-graph-types--when-to-use-which)
3. [Node taxonomy](#3-node-taxonomy)
4. [The Output Height node — three pins, not one](#4-the-output-height-node)
5. [Materials & auto-materials](#5-materials--auto-materials)
6. [Voxelized mesh stamps & caves](#6-voxelized-mesh-stamps--caves)
7. [Metadata layers](#7-metadata-layers)
8. [PCG integration](#8-pcg-integration)
9. [Lumen, ray tracing, Nanite](#9-lumen-ray-tracing-nanite)
10. [VoxelCharacter & gameplay queries](#10-voxelcharacter--gameplay-queries)
11. [Runtime sculpting](#11-runtime-sculpting)
12. [Seed & determinism rules](#12-seed--determinism-rules)
13. [Performance characteristics](#13-performance-characteristics)
14. [Common pitfalls](#14-common-pitfalls)
15. [Sandbox testing protocol](#15-sandbox-testing-protocol)
16. [Test progression](#16-test-progression)
17. [MO57's voxel integration — what's already wired](#17-mo57s-voxel-integration--whats-already-wired)
18. [Caves & mining playbook](#18-caves--mining-playbook)
19. [Glossary](#glossary)

---

## 1. How the pieces fit together

The biggest surprise (for anyone, including the docs as originally drafted):
**`AVoxelWorld` does not hold a reference to a voxel graph.** The flow is:

```
   AVoxelWorld actor (in level)
        │
        ▼
   LayerStack: UVoxelLayerStack  (asset reference)
        │
        ├── HeightLayers : TArray<UVoxelHeightLayer*>   ← height field "slots"
        ├── VolumeLayers : TArray<UVoxelVolumeLayer*>   ← volume "slots"
        └── MaxDistance  : float (how far up/down the height field extends)

   Then, in the level itself, one or more:

   AVoxelStampActor #1 ──► FVoxelHeightGraphStamp   { Graph = UVoxelHeightGraph,    Layer = HeightLayer }
   AVoxelStampActor #2 ──► FVoxelHeightHeightmapStamp{ Heightmap = UTexture2D, ... }
   AVoxelStampActor #3 ──► FVoxelVolumeGraphStamp   { Graph = UVoxelGraph,          Layer = VolumeLayer }
   AVoxelStampActor #4 ──► FVoxelMeshStamp           { Mesh  = UVoxelStaticMesh,    Subtractive = true   }
```

### The actual placement workflow

The Goosey getting-started video shows it like this — **the world is created
automatically when you place your first stamp**:

1. **Place → Volume / Height** menu in the editor places a stamp actor in
   the level. Editor pops up "Voxel world added to the level" and "voxel
   world has no mega material assigned" — note these, we fix the
   MegaMaterial below.
2. The new stamp shows up as `AVoxelStampActor`; a new `AVoxelWorld` is
   added at origin if one didn't exist.
3. Select the stamp. In its Details, **set the kind**:
   - `Volume → Shape → Plane / Sphere / Mesh`
   - `Volume → Mesh` (voxelized mesh stamp — see §6)
   - `Height → Heightmap` (texture-driven)
   - `Height → Spline` (spline-driven — powerful but advanced)
   - **`Height → Graph`** (our case for procedural terrain)
4. For `Height → Graph`, the Details panel offers **Create New Asset →
   Voxel Height Graph**. That makes the `UVoxelHeightGraph` asset on disk
   and assigns it to the stamp in one step.

There is **no separate "assign graph to world" step**. The graph lives on
the stamp; the stamp lives on a layer; the layer lives in the LayerStack;
the LayerStack is on the world. The world is just the runtime container.

### Roles

**`AVoxelWorld`** — the level actor that owns the terrain runtime. Says
"there is voxel terrain here" but does NOT describe what the terrain looks
like. In shipping levels (per `CLAUDE.md`) set `bCreateRuntimeOnBeginPlay
= false` so `MOGameMode` can apply the seed first. In the sandbox you
usually leave it `true`.

**`UVoxelLayerStack`** (asset) — thin container listing which Height and
Volume layers exist. Default `/Voxel/Default/DefaultStack.DefaultStack`
has one of each; you only need a custom stack if you want strict ordering
between multiple stamp layers (e.g. "base terrain → carving → metadata
mask").

**`UVoxelHeightGraph`** (asset, "VHG_*") — the graph you author in the
Voxel Graph editor. A new VHG is preseeded with a `VoxelNode_AdvancedNoise2D`
+ `Make Box 2D From Radius` → `OutputHeight`. This IS the recipe.

**`AVoxelStampActor`** (actor, in the level) — owns a
`UVoxelStampComponent` which holds the polymorphic stamp data. For a
height graph that's `FVoxelHeightGraphStamp { Graph = ..., ParameterOverrides
= ... }`. Position / scale / rotate the actor to control where the graph
applies in the world.

**Layers vs stamps, the short version:**
- *Layers* are the rails (height vs. volume; ordering). The stack defines
  them once per world.
- *Stamps* are the trains (per-instance applications of a graph /
  heightmap / mesh / spline to a layer). A world can have many stamps
  per layer.

### How the seed flows through (MO57 specific)

```
MOGameSettings::PendingWorldSeed (int32)
        │
        ▼
MOGameMode::ApplySeedToHeightGraphParameter(WorldSeed)
        │
        ├─ for each UVoxelHeightGraph in memory (force-preload first)
        │     └─ SetParameter("Seed", FVoxelExposedSeed{ SeedString })
        │
        └─ for each FVoxelHeightGraphStamp in the level
              └─ same SetParameter on the per-stamp override

  → THEN AVoxelWorld::CreateRuntime() runs, terrain generates deterministically.
```

The graph's `Seed` input pin must be named **exactly `Seed`** —
MOGameMode looks it up by name. Save / load / regenerate all rely on this.

---

## 2. Graph types & when to use which

| Need | Graph type | UCLASS | Stamp struct |
|---|---|---|---|
| Standard outdoor terrain (heightmap) | **Height Graph** | `UVoxelHeightGraph` | `FVoxelHeightGraphStamp` |
| Caves, overhangs, floating islands | **Volume Graph** | `UVoxelGraph` (the base class) | `FVoxelVolumeGraphStamp` |
| Material assignment by slope / altitude / metadata | **Material Graph** (VMG) | `UVoxelMaterialGraph` | Wrapped in `UVoxelAutoMaterial` |
| Per-point logic in PCG (filter, snap, sample metadata) | **Voxel PCG Graph** (VPCG) | `UVoxelPCGGraph` | Called from PCG via `Call Voxel Graph` node |

| Use case | Pick |
|---|---|
| Terrain elevation | Height Graph |
| Caves + overhangs added on top of terrain | Volume Graph (stamp it on top of the height stamp; layer order matters) |
| Slope-based grass/rock blending | Material Graph wrapped in Auto Material |
| Foliage scattering, mesh selection, point filtering | PCG → VPCG (via Call Voxel Graph) |

**MO57 currently uses Height Graph** (`VHG_Flat`). For paleolithic survival
this is the right call — caves are a future feature, and Volume Graph is
3–5× more expensive per chunk. The Material Graph + Auto Material pattern
(§5) is highly relevant when we move beyond DefaultMegaMaterial.

---

## 3. Node taxonomy

### 3a. Noise primitives (from `VoxelNoiseNodes.h`)
- `Perlin Noise 2D` / `Perlin Noise 3D` — classic smooth noise.
- `Simplex Noise 2D` / `Simplex Noise 3D` — like Perlin, faster + fewer
  directional artifacts. Use when you want isotropic features.
- `Value Noise 2D` / `Value Noise 3D` — older / blockier. Good for
  large-scale bias maps where you want patches rather than gradients.
- `Cellular Noise 2D` / `Cellular Noise 3D` — Worley/Voronoi distance.
  Outputs distance-to-nearest-cell-center. Good for "cracked earth" looks
  or generating region boundaries.
- `True Distance Cellular Noise 2D` / `3D` — Euclidean distance variant.
  Smoother cell edges.

### 3b. Composite noise — **the workhorses** (from `VoxelAdvancedNoiseNodes.h`)

These are the nodes you reach for 90% of the time. Voxel Plugin ships full
FBm composition as a single node — the hand-built "stack of Perlins"
pattern from other tutorials is not how you do it here.

- **`Advanced Noise 2D`** (`FVoxelNode_AdvancedNoise2D`) — multi-octave
  noise generator. One node does FBm + ridged + billowy via parameters.
  Pins:

  | Pin | Default | What it does |
  |---|---|---|
  | `Position` | — | Feed `Get Voxel Position 2D` here |
  | `Amplitude` | `10000` | Height range of the largest octave (in voxel units; 10000 = 100m at VoxelSize=100cm) |
  | `FeatureScale` | `100000` | World distance one big feature spans (100000 = 1km). Higher = bigger continents. |
  | `Lacunarity` | `2.0` | Frequency multiplier per octave |
  | `Gain` | `0.5` | Amplitude multiplier per octave (this is "persistence") |
  | `CellularJitter` | `0.9` | Only used if the octave type is Cellular |
  | `NumOctaves` | `10` | How many layers to sum. Higher = more detail at small scales |
  | `Seed` | — | Wire `FVoxelExposedSeed` here |
  | `DefaultOctaveType` | `SmoothPerlin` | The flavor — see enum below |
  | `OctaveType[i]` (variadic) | `Default` | Per-octave override of flavor |
  | `OctaveStrength[i]` (variadic) | `1.0` | Per-octave amplitude multiplier (on top of Gain) |
  | `Value` (output) | — | Sum of all octaves; signed |

  **Octave flavors** (`EVoxelAdvancedNoiseOctaveType`):
  - `SmoothPerlin` — Perlin (default; rolling hills)
  - `BillowyPerlin` — `abs(Perlin) * 2 - 1` (bulbous bumps, cloud-like)
  - `RidgedPerlin` — `(1 - abs(Perlin)) * 2 - 1` (sharp ridges — **mountains**)
  - Same triple exists for Cellular, Simplex, Value.

  **Goosey's empirical defaults** (from the getting-started video, for a
  demo-scale world): drop FeatureScale to 50000 (500m features), Amplitude
  to 3000 (30m), set the first octave to `RidgedPerlin` for sharp ridges
  poking through smooth hills. For continental-scale MO57 worlds we run
  at the opposite end (FeatureScale 300000–800000, Amplitude 6000–12000).

- **`Advanced Noise 3D`** — identical, for volume-graph applications.

### 3c. Seed plumbing
- `Make Seeds` — split one seed into N derived seeds (extendable pin set).
  Use to give each independent sub-feature its own seed.
- `Mix Seeds` — combines two seeds via MurmurHash. The "salt" you pass as
  the second arg uniquifies what the same input produces. Use between
  *independent* Advanced Noise nodes; not needed within a single one
  (it decorrelates its own octaves).

### 3d. Math (from `VoxelMathFunctionLibrary`)
- `Add`, `Subtract`, `Multiply`, `Divide`, `Power`
- `Abs`, `Negate`, `Saturate` (clamp 0–1), `Clamp(min,max)`
- `Lerp(A, B, T)` — linear interpolation, the workhorse for blending
- `Smoothstep(A, B, T)` — Hermite blend, much nicer for visual transitions
- `Min`, `Max`

### 3e. Position
- `Get Voxel Position 2D` / `Get Voxel Position 3D` — input to every noise
  node. `Advanced Noise 2D` consumes Position 2D directly. If using legacy
  `Perlin Noise 2D`, multiply Position by a small constant first for
  frequency control.

### 3f. Curves (from `VoxelCurveFunctionLibrary`)
- `Apply Voxel Curve` — runs a value through an authored `UCurveFloat` /
  `UCurveLinearColor`. The cleanest way to shape a noise output without
  building math node chains. Author curves in `Content/Voxel/Curves/`.

### 3g. Box / spatial bounds (from `VoxelBoxFunctionLibrary`)
- `Make Box 2D From Radius` — bounds for the stamp's coverage area.
  Already wired in the default VHG output. **Promote `Radius` to a
  parameter** — once promoted, the stamp actor's Details panel exposes
  it under "Default" so designers can resize the stamp's coverage without
  re-opening the graph.
- `Make Box 2D From Min Max` — same idea but with explicit corners.

### 3h. Output
- `OutputHeight` — see §4 below. Three pins, not one.

### 3i. Material nodes (from `VoxelMaterialFunctionLibrary`)
- `Blend Materials` — takes two voxel materials + an alpha, returns
  blended material. Alpha can be a noise output, a slope value, a metadata
  read, anything 0..1.
- `Get Vertex Normal` (material graphs only) — gets the surface normal at
  the sampled point. Break the pin to get the Z component (= slope cosine).

### 3j. Metadata nodes (from §7)
- `Get Voxel Metadata` (material) — sample a metadata layer in a material.
- `Query Voxel Layer` (Blueprint) — sample distance / material / metadata
  at a world position from gameplay code.

### 3k. Stamp spawning from PCG / graphs
- `Voxel Stamp Spawner` (PCG node) — spawns stamps from PCG points. Layer
  ordering matters: spawned stamps must land on a layer *later* than the
  one being sampled.
- `Voxel Height Graph Stamp` — used in the graph editor when you want a
  graph to drop a sub-stamp into the height field (rivers carving etc.).

---

## 4. The Output Height node

The default `OutputHeight` node has **three** pins, not one. Easy to miss
because most tutorials only ever wire the first:

| Pin | Type | What it does | Default if unwired |
|---|---|---|---|
| `Height` | float | Surface height at each (X, Y) | 0 |
| `Material` | voxel material | Surface material at each (X, Y) | DefaultMegaMaterial layer 0 |
| `Bounds` | Box 2D | The region where the graph evaluates. Outside the box, the stamp does nothing. | empty → graph evaluates nowhere → invisible stamp |

**Critical**: the new-graph template wires a `Make Box 2D From Radius`
into Bounds. If you delete or break that wiring, the stamp evaluates
nowhere and you'll see flat terrain (or nothing) at runtime. The "promote
Radius to parameter" trick (§3g) is the right way to expose stamp coverage
to the level designer.

`OutputVolume` (volume graphs) and the material/PCG variants have their
own pin lists; same principle though — read the node's tooltips and wire
all the relevant ones.

---

## 5. Materials & auto-materials

### 5a. Voxel materials are NOT regular UE materials

Terrain materials must be **`UVoxelMaterial`** assets that *reference* a
regular UE Material. Voxel won't accept raw `M_*.uasset` in the stamp's
Material slot.

Workflow:
1. **Make a UE material** (or use an existing one, e.g. NaniteMaterials sample).
2. **Right-click in Content Browser → Voxel → Voxel Material**. Name it
   `VMI_MyMaterial`. Set its Material slot to the UE material.
3. **In the stamp** (or the Material pin of a Height Graph): assign
   `VMI_MyMaterial`.

### 5b. The MegaMaterial popup

When you assign a new voxel material that the world hasn't seen, you get
"missing mega material entry" in the bottom-right. **Click Add**, wait
for shader compilation, and the material renders. You must do this for
every new voxel material added to the level — it's not automatic.

If you ignore the popup, the surface renders as the default checkerboard.

### 5c. Auto Materials (slope-based blending)

The cleanest way to do "grass on flat ground, rock on slopes" is:

1. **Make a Voxel Material Graph (VMG)**:
   - Right-click → Voxel → Voxel Graph → **Material Graph**.
   - Inside, the output is a Material. Use `Blend Materials` to mix two
     `UVoxelMaterial` assets by an alpha.
   - Get the alpha from a slope source: `Get Vertex Normal` → break →
     Z component → `Multiply 2 → Subtract offset` → Saturate → into alpha.
2. **Wrap in an Auto Material**:
   - Right-click → Voxel → **Voxel Auto Material**.
   - Set its Graph to the VMG from step 1.
3. **Use in your Height Graph**:
   - Wire the Auto Material asset into the `Material` pin of `OutputHeight`.

The VMG runs as "a post-process on the whole world" — it gets the position
and surface normal of every sampled voxel and returns the material to draw.
This is how you decouple terrain *shape* from terrain *appearance*.

VMGs can also read metadata (§7), so you can paint zones (snow biome,
desert biome) by writing a low-frequency noise into a metadata layer and
sampling it in the VMG.

---

## 6. Voxelized mesh stamps & caves

Any `UStaticMesh` can be voxelized and dropped into the world as a stamp:

1. Right-click a static mesh → **Create Voxel Static Mesh Asset**. Generates
   a `UVoxelStaticMesh` asset alongside it.
2. Drag the `UVoxelStaticMesh` into the viewport → auto-creates a stamp
   actor with the mesh assigned.

### Resolution limits

Mesh stamps render at the world's VoxelSize. With VoxelSize=100 (default,
1m), small meshes look blocky. With VoxelSize=50 (50cm) you get usable
detail for small props. Don't drop to 25 cm globally — chunk count
quadruples and perf tanks.

### Cave carving (additive vs. subtractive)

Mesh stamps default to **additive** (adds the mesh's volume to terrain).
For caves, flip to **subtractive**:

- In the stamp's Details: `Behavior → Subtractive`.
- Resize / rotate / overlap multiple subtractive mesh stamps to carve
  complex cave geometry.

### Smoothness

Mesh stamps and shape stamps both expose `Smoothness` (default 100). Higher
values smooth-blend the stamp into surrounding terrain — useful for making
boulders read as part of the landscape rather than as glued-on props.

---

## 7. Metadata layers

Underused but high-leverage. Metadata is "any random data you attach to
the world at a position" — separate from height, materials, and shape.
Stamps can write metadata; gameplay / PCG / materials can read it.

### Creating a metadata layer

1. Right-click in Content Browser → Voxel → **Voxel Float Metadata** (or
   Voxel Vector / Voxel Integer if you need richer values). Name it e.g.
   `VFM_BiomeMask`. **Default value is 0** for all new metadata.

### Writing metadata via a stamp

The trick is a "metadata-only" stamp — turn off shape and material, leave
just the metadata override on:

1. Place any stamp actor.
2. In its Details: **disable Shape** and **disable Material**.
3. **Metadata Overrides → Add → VFM_BiomeMask → set value (e.g. 1.0)**.
4. The stamp's bounds now define a region where metadata is overridden
   to the chosen value, without disturbing terrain or look.
5. **`Smoothness` on a metadata stamp** controls the falloff from "fully
   overridden" at the center to "default value" at the edge.

### Reading metadata in PCG (foliage masking)

In a VPCG graph:
1. Add the metadata to the Voxel Sampler's **Query Metadata** list. Without
   this, the metadata isn't fetched for the points.
2. `Get Point Attribute → VFM_BiomeMask` → use as a `Density Filter` to
   mask out points in regions where the metadata is high (or low — flip
   the logic).

Foliage clearings, no-go zones around landmarks, and biome boundaries are
all metadata-stamp work.

### Reading metadata in a Material

In a material (regular UE material, used by a voxel material that's used
by the auto-material):
1. Add `Get Voxel Metadata → VFM_BiomeMask` node.
2. Use its output as a mask between two material attributes (e.g. snow vs.
   dirt base color).
3. Apply. Voxel plugin **auto-generates a texture in the back end** to
   sample the metadata per pixel. This is dynamic — runtime stamps
   updating metadata also update the material.

### Reading metadata from Blueprint / C++

`Query Voxel Layer` nodes (multi-query variant for batches) take a world
position and return:
- Distance (volume layer; negative = inside the surface, positive = in air)
- Height (height layer)
- Material at that point
- Metadata values for any layer registered to the world

Use cases:
- Footstep sound by material at the player's feet.
- Skill / damage modifiers by biome metadata (warm zone, cold zone).
- "Can build here?" checks by metadata bitmasks.

### Per-instance custom data (PCG ↔ foliage shader)

In a VPCG graph, read metadata at a point → pipe into `Set Per-Instance
Custom Data`. In the foliage's UE material, sample `Per Instance Custom
Data[0]`. This is how you get **ground-color-aware foliage** — bushes
tinted by the underlying terrain biome.

---

## 8. PCG integration

We already use PCG for resource scattering. Voxel adds two PCG hookups:

### 8a. PCG component boilerplate

1. Place a PCG actor in the level.
2. PCG Component → **Generation Trigger = Generate at Runtime**. So PCG
   updates when terrain edits happen.
3. PCG Component → **Is Partitioned = true**. Chunked updates — only the
   chunks near a runtime edit re-generate, not the whole world.
4. PCG World Actor → **Treat Editor Viewport As Generation Source = true**.
   So previewing works in editor, not just in PIE.
5. In the PCG graph itself → **Use Hierarchical Generation = true**. Needed
   alongside Is Partitioned.

### 8b. Voxel Sampler node

The entry point to sampling voxel data from PCG. Inputs:
- **Bounding Shape** (needed for grid sizing — `Get Actor Data → Get Bounds`
  is the usual feed).
- **Grid Size / Cell Size** — controls density. Smaller cell = more points.
  Goosey uses 200–300cm for foliage.
- **Stack / Layer** — which `UVoxelLayerStack` and which layer to sample
  from. For most setups just the default stack + default height layer.
- **Query Metadata** (array) — list metadata layers you want to read at
  the sampled points. Without listing them here, downstream "Get Point
  Attribute" on metadata returns nothing.

### 8c. VPCG graph (per-point logic)

PCG has `Call Voxel Graph` which calls a **Voxel PCG Graph** (VPCG asset).
Inside a VPCG:
- `Get Position`, `Get Point Seed`
- `Apply Translation`, `Apply Scale`, `Set Point Rotation`
- `Random Float`, `Random Select`
- `Get Point Attribute` / `Set Point Attribute` — read/write per-point data
- `Ray March Layer Distance Field` — snap a point to the terrain surface
  (kills floating foliage when sampling is sparse)
- `Density Filter` — drop points by a 0..1 mask

### 8d. Static mesh selection

To spawn different meshes per point:
1. PCG `Static Mesh Spawner` node → **Selection Method = Selected By
   Attribute**.
2. **Attribute Name = `StaticMesh`** (or whatever you used in `Set Point
   Attribute`).
3. In your VPCG, `Set Point Attribute "StaticMesh"` with type **Soft
   Object Path** (it's a wildcard type by default — you must pick Soft
   Object Path explicitly).
4. Plug in either a fixed mesh, an array via `Make Array`, or a
   `Random Select` from an array parameter for randomized variety.

### 8e. Slope / material-weight based filtering

The Voxel Sampler emits a `voxal materials` per-point attribute — a map of
material → weight at that voxel. Pull weights via `Get Point Attribute
<MaterialAssetName>` → convert to float → use as a `Density Filter`.

This is the right way to do "bushes only on muddy ground, not on
gravel slopes" — read the slope-driven Auto Material's output directly
rather than re-implementing the slope math.

### 8f. PCG can also spawn voxel stamps

`Voxel Stamp Spawner` PCG node — places stamps procedurally. The stamps
must target a layer *later* in the LayerStack than the one being sampled,
or you get circular dependency (the stamp would modify what PCG is reading
from to decide where to place stamps). Read the Stack/Layer docs before
going down this path — easy to deadlock.

---

## 9. Lumen, ray tracing, Nanite

### Lumen

`AVoxelWorld → Enable Lumen` button must be ticked for the voxel surface
to bounce light correctly. Verify with **Show → Lumen → Lumen Overview**;
if voxel terrain appears in the overview, Lumen sees it.

### Hardware ray tracing — **incompatible** as of Voxel 2.0 P7

The voxel plugin **does not support hardware ray tracing** (HWRT) yet. If
your project has HWRT enabled, voxel terrain won't contribute to Lumen.
Symptom: voxel surface looks unlit / black in Lumen Overview, with yellow
"HWRT" text instead of "SWRT".

Fix:
- Project Settings → search "lumen" →
  **Use Hardware Ray Tracing When Available = false**.
- If that's grayed out, hardware ray tracing is already off project-wide —
  no change needed.

### Nanite

`AVoxelWorld → bEnableNanite = true` (default). Enables Nanite for the
voxel mesh, including tessellation. Caveats:
- `NaniteMaxTessellationLOD` (default 2): chunks above LOD 2 won't tessellate.
  Increase if you see holes in far chunks.
- `NanitePositionPrecision` (default 6): increase if you see holes between
  far LODs. Higher = more memory.
- `bCompressNaniteVertices` (default false): saves memory at slight quality cost.

---

## 10. VoxelCharacter & gameplay queries

### Use VoxelCharacter, not Character

**Critical:** if your gameplay character inherits from UE's `ACharacter`,
players teleport / jitter when standing on voxel terrain. Voxel plugin
ships `AVoxelCharacter` which fixes this.

For MO57: `AMOCharacter` must inherit from `AVoxelCharacter` (or copy the
relevant snapping code from it if our parent class is fixed). Audit
ticket: confirm `AMOCharacter`'s parent is the right one before we add
more characters; the symptom (teleport) is subtle and easy to misdiagnose.

### Editor waits for world generation by default

`AVoxelWorld::bWaitOnBeginPlay = true` (default) holds simulation until
the world is ready — characters won't fall through unloaded chunks. Keep
this on unless you know what you're doing.

### Querying voxel data from gameplay

`Query Voxel Layer` nodes (and `QueryMulti` variants for batches):
- Inputs: layer stack, layer, world position(s)
- Outputs: distance/height, material, queried metadata values

Use cases:
- Footstep sounds by surface material under the foot.
- Cold/hot biome effects driven by metadata under the player.
- "Can build here" checks (no caves under, surface flat enough, etc.).

---

## 11. Runtime sculpting

Voxel Plugin 2.0 ships two layers of runtime-sculpt API. Pick the right
one based on what you're doing.

### 11a. High level — Sculpt Actors (the path MO57 uses)

`AVoxelHeightSculptActor` and `AVoxelVolumeSculptActor` are dedicated
actors that apply `FVoxelHeightModifier` / `FVoxelVolumeModifier` operations
to the world. Each has an `ApplyModifier()` method that takes a modifier
struct describing what to do (Dig, Raise, Flatten, Smooth — brush radius,
strength, location).

Save/load is baked in — the actor persists its sculpt history (or an
`ExternalSaveAsset` ref) and replays modifiers on load. **This is the
right API for player-driven terraforming**: it's stateful, save-aware,
and matches Voxel Plugin's own undo/redo behaviour in editor.

**MO57 wiring (already built):**
- `UMOTerraformingComponent` on `AMOPlayerController` owns
  `TWeakObjectPtr` references to the level's Height and Volume sculpt
  actors (auto-find via `AutoFindSculptActors`).
- Dig / Raise / Flatten / Smooth modes funnel through `HeightDig` /
  `VolumeDig` / etc., which build the appropriate modifier struct and
  call `ApplyModifier` on the right actor.
- `bUseVolumeSculpting` toggles between Height (2D sculpt) and Volume
  (3D sculpt — what you'd use to carve cave entrances).
- Five-second timed actions with movement-interrupt integration, progress
  widget hook, and reporting to `UMOTerrainModificationSubsystem` for
  PCG-foliage cleanup.

### 11b. Low level — Instance Stamp Component (Goosey's demo)

`UVoxelInstanceStampComponent` is the raw API. You add it to an actor,
build `FVoxelStamp` structs (mesh / sphere / shape) by hand, and call
`AddStamp` to drop them into the world. Stamps are in **world space**,
so they don't move with the owning actor.

Useful for:
- One-off VFX-style stamps (explosion craters, lightning strikes) — quick
  to spawn, no save-system involvement.
- Cases where you want to drop an entire pre-authored stamp (e.g. a
  voxelized mesh) at runtime — `AVoxelVolumeSculptActor` doesn't compose
  arbitrary mesh stamps as easily.
- Programmer prototypes before deciding whether the operation deserves a
  proper SculptActor backing.

**Caveat:** the runtime InstanceStampComponent API is "currently
unfinished" per Goosey (Voxel 2.0 P7) — expect some rough edges. Prefer
the SculptActor route when the operation is gameplay-meaningful (player
mining, building demolition, anything that needs to persist).

### 11c. PCG keeps up automatically

For both paths, PCG with `Generation Trigger = Runtime` + `Is Partitioned`
re-generates affected chunks after a sculpt. The
`UMOTerrainModificationSubsystem` augments this by tracking *intentional*
sculpt zones and running a sweep to remove any PCG-respawned grass /
foliage inside those zones — so the worked ground stays worked even after
PCG re-evaluates.

---

## 12. Seed & determinism rules

**Rule 1 — the seed has to be exposed.** Add an `FVoxelExposedSeed`
parameter named **exactly `Seed`** (case-sensitive). MOGameMode looks it
up by name. Wire it into every Advanced Noise node's Seed pin.

**Rule 2 — decorrelate INDEPENDENT features with `Mix Seeds`.** If you
have a base-height node AND a mountain-mask node, they shouldn't share
the same raw seed:

```
[Seed] ──► [Mix Seeds: salt=1] ──► [Advanced Noise 2D: ContinentBase]
       └─► [Mix Seeds: salt=2] ──► [Advanced Noise 2D: MountainShape]
       └─► [Mix Seeds: salt=3] ──► [Advanced Noise 2D: MountainMask]
```

**Rule 3 — don't salt octaves of one Advanced Noise node.** The node
handles internal octave decorrelation. Per-octave salting is a legacy
pattern from "stack of Perlins" FBm.

**Project seed pipeline (MO57 specific):**
- `MOGameSettings` holds `PendingWorldSeed` (int32, set by new-game UI).
- `MOGameMode::ApplySeedToHeightGraphParameter(WorldSeed)` iterates all
  loaded `UVoxelHeightGraph` assets (force-preloaded via Asset Registry)
  AND every `FVoxelHeightGraphStamp` in the level, calling
  `SetParameter("Seed", FVoxelExposedSeed{...})` on each.
- Happens before `AVoxelWorld::CreateRuntime()`.
- Same seed → same world.

---

## 13. Performance characteristics

Order of magnitude per chunk (rough — depends on chunk size + LOD):

| Operation | Cost |
|---|---|
| Single Perlin/Simplex 2D | ~1 unit |
| Advanced Noise 2D (1 octave) | ~1 unit |
| Advanced Noise 2D (4 octaves) | ~4 units (linear) |
| Advanced Noise 2D (10 octaves, default) | ~10 units — overkill for base height |
| Cellular Noise 2D | ~3–5× Perlin |
| 3D noise (anything) | ~5–10× the 2D equivalent |
| Stamp application | constant per stamp; depends on stamp size |
| `Apply Voxel Curve` | very cheap |
| Math nodes | effectively free |
| Voxel Sampler in PCG | depends on cell size — quadratic in points/m² |
| Auto Material VMG | runs per surface voxel; budget like a regular shader |
| Metadata texture (per layer used by material) | one extra texture sample per layer |

**Optimization hints:**
- Default `NumOctaves=10` is generous. Drop to 4–6 for base height; reserve
  high octave counts for fine detail at LOD0 only.
- Compute big-scale (continent) noise once, reuse for biome mask + base
  elevation.
- Push small details to LOD0 only — large-scale features dominate the
  silhouette at every LOD.
- Stamps are LOD-aware automatically — they vanish at distant LODs.
- VoxelSize=50 (Goosey's bump for the mesh-stamp example) quadruples chunk
  count vs. VoxelSize=100. Use selectively for important regions only.

---

## 14. Common pitfalls

| Pitfall | Symptom | Fix |
|---|---|---|
| Hunting for a `VoxelGraph` slot on `AVoxelWorld` | Can't find where to assign the VHG | There isn't one. Place a stamp actor (Place → Volume/Height) and assign the graph to the stamp. The world is just the runtime container. |
| Ignored the "missing mega material entry" popup | Surface renders as checkerboard | Bottom-right popup → click Add → wait for shader compile. |
| Used a regular UE Material in the stamp's Material slot | Won't compile / appears unset | Wrap it in a `UVoxelMaterial` asset first. |
| `Bounds` pin on OutputHeight is empty / unwired | Stamp generates nothing; world appears flat | Wire `Make Box 2D From Radius` → Bounds, promote Radius to a parameter so designers can size it. |
| Built FBm by hand instead of using Advanced Noise 2D | Way more node spaghetti than necessary; hard to tune | Replace the chain with one `Advanced Noise 2D` node. |
| `Mix Seeds` per octave in a hand-built FBm | Wasted nodes, no benefit | Advanced Noise 2D handles octave decorrelation internally. Mix Seeds only between *independent* nodes. |
| Hand-built `1 − abs(perlin)` chain | Works but unnecessary | Set `DefaultOctaveType = RidgedPerlin` on the Advanced Noise node — same result. |
| Renamed the `Seed` exposed parameter | Save/load gives different terrain than new game | Name it back to `Seed`. |
| `bCreateRuntimeOnBeginPlay = true` with MOGameMode active | Voxel runtime starts before seed applies → wrong terrain | Set to false in shipping levels; let MOGameMode call CreateRuntime. |
| Stamp actor bounds smaller than play area | Terrain only inside a circle, ocean outside | Increase the Radius parameter, or scale up the stamp actor. |
| Multiple stamps overlapping without thought | Last stamp wins or values blend surprisingly | Pick an explicit Behavior per stamp (Override / Add / Smooth Min). Check the order in the LayerStack. |
| Mountains everywhere | No mountain mask applied | Multiply mountain output by a separate low-freq mask. |
| Cliff at water level | Coast is unusable | Apply Beach Flattening recipe (Terrain_Foundation_Plan §3). |
| Subtractive mesh stamp set to Additive by mistake | Stamps appear inside the player; player falls through | Set Behavior = Subtractive on cave-carving mesh stamps. |
| Character teleports / jitters on voxel terrain | Wrong base class | Use `AVoxelCharacter`, not `ACharacter`. |
| Voxel surface looks unlit in Lumen | Hardware ray tracing enabled | Project Settings → "Use Hardware Ray Tracing When Available" = false. Voxel 2.0 doesn't support HWRT. |
| PCG `Get Point Attribute <metadata>` returns nothing | Metadata not in Voxel Sampler's Query Metadata list | Add the metadata layer to Query Metadata explicitly. |
| Static Mesh Spawner attribute selection wildcard error | Attribute type defaulted to wildcard | In VPCG, explicitly cast the attribute to `Soft Object Path`. |
| Voxel mesh stamp looks blocky | World VoxelSize too coarse | Reduce VoxelSize (200→100→50). Trade-off: chunk count quadruples each halving. |
| Hand-built FBm in a hand-built ridged chain produces flat midriffs | Misuse of math | Don't combine — use one `Advanced Noise 2D` node with `RidgedPerlin` octave type. |
| **PCG graph error: "PCG graph is invalidating itself: Voxel Stamp Spawner -> Voxel Sampler"** | PCG points visible in debug but no terrain deformation lands | You're reading and writing the SAME voxel layer in the same graph (e.g. Voxel Sampler on `DefaultVolumeLayer` AND Voxel Stamp Spawner targeting `DefaultVolumeLayer`). Either remove the sampler (if you don't need it) or sample from an EARLIER layer than the one you write to. The LayerStack's order is load-bearing: layers later in the list can read from earlier ones, never the same one or later ones. |
| Voxel Stamp Spawner runs but no stamp visible | Most common: stamp template still set to `VoxelHeightmapStamp` (the default placeholder) | Set `NewTemplate` to the correct stamp type (`VoxelShapeStamp` for built-in shapes, `VoxelMeshStamp` for voxelized meshes, `VoxelVolumeGraphStamp` for graph-driven carving). The default template is essentially a no-op. |

---

## 15. Sandbox testing protocol

Goal: isolate one technique per level so we can see + tune it without
other variables confounding.

### Graph editor preview shortcuts

- **Select any node + press D** — previews that node's output in the
  preview window (distance-field shading by default; can toggle to grayscale).
- **Open the Viewport tab** in the graph editor — shows the graph's full
  result without needing to use the graph in a level. Drastically faster
  iteration than playing every change.
- Make tunables (Amplitude, FeatureScale, Radius) into **parameters** —
  they're then editable both in the Viewport tab AND from the stamp
  actor's Details panel.

### Sandbox level setup (do once)

1. **Create level**: `Content/Penumbra/Sandbox/VoxelSandbox.umap`. Empty
   default lighting. Traditional level (not Open World) is fine.
2. **Place a stamp** (Place → Volume → Plane is the lightest option for
   "is this set up?"). The editor auto-creates an `AVoxelWorld` at origin
   and shows two popups:
   - "Voxel world added to the level" — informational.
   - "Voxel world has no mega material assigned" — click through, set
     `DefaultMegaMaterial`.
3. **AVoxelWorld settings**:
   - `bCreateRuntimeOnBeginPlay = true` (sandboxing — no MOGameMode).
   - VoxelSize: 100 (default; raise to 50 only when testing mesh stamps).
   - LayerStack: leave at `/Voxel/Default/DefaultStack.DefaultStack`.
   - MegaMaterial: assign `/Voxel/Default/DefaultMegaMaterial`. Click
     "Add" on the popup for each new material.
   - **Enable Lumen** button — click it.
4. **Game Mode Override** on the level: use the third-person sample game
   mode or any pawn-providing one so you can walk around. Verify the
   pawn class inherits from `AVoxelCharacter`.

### Per-test protocol

1. **Author** a new VHG: `VHG_Test_NN_Description.uasset` in
   `Content/Penumbra/Sandbox/`. (Right-click → Voxel → Voxel Height
   Graph.) The new asset opens with a default `Advanced Noise 2D` +
   `Make Box 2D From Radius` → `OutputHeight`. Edit from there.
2. **Add a `Seed` parameter** if you'll mix-seeds anything. Right-click
   in the graph → Add Parameter → `FVoxelExposedSeed` → name it `Seed`.
3. **Wire the Material pin** too if you're testing materials; otherwise
   surface defaults to DefaultMegaMaterial layer 0.
4. **Place the stamp**: drag the VHG asset into the viewport → auto-creates
   a stamp actor. Move / scale it to cover the test area. Tune the Radius
   parameter from the stamp actor's Details panel without re-opening the
   graph.
5. **Press Play** (or use the in-editor preview), fly around, take notes.
6. **Update §16** with what worked and what didn't.

### Avoid

Running tests in the gameplay level (`MOPCGScattering`). The PCG scatter
+ ambient spawn + voxel readiness subsystem all create noise in the logs
and obscure the terrain-only behaviour you're testing.

---

## 16. Test progression

Track each test here as we run it. Append entries; don't rewrite history.

### Test 0 — Pure Flat (sanity check)

**Goal:** confirm sandbox setup works. No noise variables yet.

**Graph:** `VHG_Test_00_Flat.uasset`.

- Open the new VHG; find the `Advanced Noise 2D` node.
- Set `Amplitude = 0`. Node still computes but returns 0 everywhere.
- Verify `Make Box 2D From Radius` is wired into `OutputHeight.Bounds`,
  Radius promoted to parameter, parameter default at least 5000.
- Don't wire Material (defaults to MegaMaterial layer 0).

**Expected:** flat plane at Z=0 across the stamp's Radius. Confirms
sandbox + stamp actor + LayerStack + MegaMaterial chain alive.

**Status:** ⬜ not yet built.

---

### Test 1 — Single Octave Perlin (baseline)

**Graph:** `VHG_Test_01_SinglePerlin.uasset`.

- `Advanced Noise 2D` with `NumOctaves=1`, `DefaultOctaveType=SmoothPerlin`,
  `Amplitude=5000`, `FeatureScale=100000`.
- `Seed` exposed parameter, wired in.

**Expected:** rolling uniform hills. Same look across the whole stamp.
Equivalent to the project's current `VHG_Flat`.

**Status:** ⬜ not yet built.

---

### Test 2 — Multi-Octave FBm

**Graph:** `VHG_Test_02_FBm.uasset`.

- Test 1 settings + `NumOctaves=4` (or 6). Keep Lacunarity=2, Gain=0.5.

**Expected:** terrain reads as having structure at multiple scales — big
shapes with hills with bumps. **No graph rewiring** vs. Test 1 — just
bump one number.

**Status:** ⬜ not yet built.

---

### Test 3 — Ridged Mountains, no mask

**Graph:** `VHG_Test_03_RidgedNoMask.uasset`.

- `Advanced Noise 2D` with `DefaultOctaveType=RidgedPerlin`, `NumOctaves=4`,
  `Amplitude=12000`, `FeatureScale=30000`.

**Expected:** sharp peaks + valleys everywhere. Probably unwalkable.
Confirms the ridged octave type works.

**Status:** ⬜ not yet built.

---

### Test 4 — Ridged + Mountain Zone Mask

**Graph:** `VHG_Test_04_RidgedMasked.uasset`.

Two Advanced Noise nodes:
1. **MountainShape** — `RidgedPerlin`, Test 3 settings.
2. **MountainMask** — `SmoothPerlin`, `NumOctaves=2`, `FeatureScale=600000`,
   `Amplitude=1.0`, piped through `Saturate` (and optional `±0.3` bias).

`MountainShape × MountainMask → OutputHeight`. Each node's Seed pin
through its own `Mix Seeds` (salts 1 and 2).

**Expected:** distinct mountain RANGES with flatter areas between.

**Status:** ⬜ not yet built.

---

### Test 5 — Beach Flattening

**Graph:** `VHG_Test_05_Beaches.uasset`.

Test 4's output + Beach Flattening (Terrain_Foundation_Plan §3):
```
RawHeight = ContinentBase + (MountainShape × MountainMask)
DeltaFromWater = RawHeight − WaterLevel
BeachWeight = Smoothstep(0, 1, Saturate(|Δ| / BeachWidth))
FinalHeight = Lerp(WaterLevel, RawHeight, BeachWeight)
```

ContinentBase = a third Advanced Noise (Smooth Perlin, FeatureScale 500000,
amplitude 6000–8000) via salt 3.

**Expected:** walkable shorelines.

**Status:** ⬜ not yet built.

---

### Test 6 — Combined Foundation (= candidate `VHG_Realistic`)

**Graph:** `VHG_Test_06_Foundation.uasset`. Test 5 + tuned parameters.

When happy: promote to `VHG_Realistic.uasset`. Swap the level's stamp
to reference it (Details → Stamp Component → Stamp Ref → Stamp → Graph),
or delete the old stamp and drag VHG_Realistic in.

**Status:** ⬜ not yet built.

---

### Test 7 — Auto Material slope blend

**Graph:** `VMG_BasicSlopeBlend.uasset` (a Material Graph), wrapped in
`VAM_BasicSlopeBlend.uasset` (a Voxel Auto Material).

VMG:
- Two `UVoxelMaterial` slots (grass + rock).
- `Get Vertex Normal → break → Z component → Multiply 2 → Subtract 1 →
  Saturate` → into `Blend Materials` Alpha.

Wire the Auto Material into Test 6's graph's `OutputHeight.Material` pin.

**Expected:** rock on slopes, grass on flats. Confirms Auto Material
pipeline.

**Status:** ⬜ not yet built.

---

### Test 8 — Metadata mask for foliage clearings

**Asset:** `VFM_ClearingMask.uasset` (Voxel Float Metadata).

Sandbox: place a sphere stamp at origin, disable shape + material, add
Metadata Override `VFM_ClearingMask = 1.0`, set smoothness high.

In the sandbox's PCG VPCG graph: Query Metadata = [VFM_ClearingMask],
`Get Point Attribute VFM_ClearingMask → Density Filter (1 - x)` so points
spawn everywhere except inside the metadata sphere.

**Expected:** circular foliage clearing centered on the stamp. Confirms
metadata write + PCG read pipeline.

**Status:** ⬜ not yet built.

---

## 17. MO57's voxel integration — what's already wired

We've built more on top of Voxel Plugin than is immediately obvious from
the plugin docs. Anything not listed here is either Goosey-vanilla or
not implemented yet.

### 17a. Sculpting / terraforming

| Class | Where | Role |
|---|---|---|
| `UMOTerraformingComponent` | on `AMOPlayerController` | Player-facing sculpt component. Modes: Dig, Raise, Flatten, Smooth, RemoveFoliage. Has both **Height** (2D) and **Volume** (3D) paths via `bUseVolumeSculpting`. Five-second timed actions with movement-interrupt integration. |
| `UMOTerrainModificationSubsystem` | world subsystem | Tracks **modified zones** (spheres). Spatial grid lookups for O(1)-ish `IsLocationModified` queries. Periodic + post-sculpt-burst sweep removes PCG-respawned grass inside zones. Save/load via `UMOPersistenceSubsystem`. Configurable per-tag auto-sweep (default: grass only; harvestable resources persist). |
| `AVoxelHeightSculptActor` / `AVoxelVolumeSculptActor` | placed in level | Voxel-Plugin-supplied actors that hold modifier history. The Terraforming Component finds them via `AutoFindSculptActors` and forwards modifiers to them. |

### 17b. PCG node suite (custom)

Built on top of Voxel Plugin's PCG `Voxel Sampler` node. Add these to
PCG graphs as needed:

| Node | Purpose |
|---|---|
| `MO Terrain Mod Filter` (`UMOPCGTerrainModificationFilterSettings`) | Drops points inside modified zones (or only inside, with `bInvert`). Inserts between Voxel Sampler and any spawner. |
| `MO Resource Spawner` (`UMOPCGResourceSpawnerSettings`) + Tree / Bush / Rock subclasses | DataTable-driven resource spawning. Pulls definitions from `DT_ResourceNodes` (`FMOResourceNodeDefinitionRow`). Auto-tags HISMs and registers with `UMOPCGInteractionSubsystem` so the interaction/harvest system finds them. |
| `MO Mesh Spawner` (`UMOPCGMeshSpawnerSettings`) | Generic mesh spawner. |
| `MO Item Spawner` (`UMOPCGItemSpawnerSettings`) | Spawns world items (`MOWorldItemFactory` integration). |
| `MO Spawn Point` (`UMOPCGSpawnPointSettings`) | Pawn spawn point markers. |
| `MO Elevation Bands` (`UMOPCGElevationBandsSettings`) | Splits input points into up to 8 elevation bands with configurable falloff. Use for "this tree at 0–500m, this tree at 500–1500m, snow grass above 1500m". |
| `MO Distance Culling` (`UMOPCGDistanceCullingSettings`) | Distance-based point culling. |
| `MO HISM Tagger` (`UMOPCGHISMTaggerSettings`) | Adds tags to HISM components. |
| `MO Force HISM Tree Build` (`UMOPCGForceHISMTreeBuild`) | Forces HISM acceleration-structure rebuild after large changes. |

### 17c. Subsystems that touch voxel

| Subsystem | Voxel coupling |
|---|---|
| `UMOPCGInteractionSubsystem` | Tag registry that maps PCG-spawned HISM instances to interaction/harvest behavior. |
| `UMOHarvestSubsystem` | Reads HISM tags via the interaction subsystem to identify what's harvestable. |
| `UMOHISMCullingSubsystem` | Culls HISM instances at distance. |
| `UMOIdentityRegistrySubsystem` | Maps voxel-spawned actors to stable GUIDs for save/load. |

### 17d. Voxel application of Engineering Principle #11 (realism)

See **CLAUDE.md Engineering Principle #11: "The simulation IS the design
— every system reflects real-world phenomena."** This is the voxel-side
manifestation of that rule.

- The Terraforming Component's default 5-second progress timer is the
  *minimum* sensible duration for any sculpt action. Most actions
  should be longer; 5 seconds is a placeholder we'll lengthen during
  balancing.
- Mining keeps the same pattern: a swing of the pickaxe is a timed
  action with interrupt-on-movement. Tool quality reduces the duration
  (flint pick → iron pick → steel pick) but the floor is non-zero.
- Cave-mining uses the existing `BeginTerraform → CompleteTerraform`
  flow, not the legacy `Dig(WorldLocation)` immediate-apply API. The
  legacy API stays for tools, automation, and AI controllers — never
  the player.
- If a sculpt feels too slow during playtesting, the answer is better
  feedback (visible incremental yield, audio cues, environmental
  change) — not a shorter timer, and certainly not zero.

### 17e. What's NOT yet wired (open work)

- **Procedural caves** (Volume Graph). See §18 below. Task #118.
- **Ore placement inside caves** via metadata or volume sampling. See §18.
  Task #119.
- **Mining mode for the Terraforming Component** — directional dig
  (carve into the cliff face, not just chip the surface) + ore yields
  via material query at the brush center, keeping the timed-action
  pattern. Task #120.
- **AMOCharacter parent class** — currently `ACharacter`, should be
  `AVoxelCharacter`. Task #116.
- **Lumen / HWRT config** — verify HWRT off so voxel surfaces participate.
  Task #117.

---

## 18. Caves & mining playbook

This section is design-forward — it captures the approach we plan to take
to add procedural caves and ore mining, given what we already have in
§17. Update this section as we land actual implementations.

> **Architectural note (read first):** caves, ore veins, rivers, POIs,
> landmarks, ore deposits, and player-built shelters are all instances
> of one abstraction — see `Docs/World_Features_Architecture.md`. This
> §18 covers the voxel-specific *how* (volume graphs, metadata bands,
> stamp behaviors). The World Features doc covers the *catalog +
> placement + registry + persistence* layer that all these things share.
> Don't build caves as a bespoke "cave system"; build them as a
> feature row that happens to use a `VolumeGraphStamp` kind.

### 18a. Two cave sources

Caves come from two directions in MO57:

1. **Player-mined caves** — the player swings a pickaxe and the world
   subtractively sculpts inward. Already supported in principle: the
   Terraforming Component's Dig mode with `bUseVolumeSculpting=true` does
   exactly this. **Mining keeps the existing timed-action pattern** —
   per §17d, every world-affecting action takes realistic time. A swing
   of the pickaxe is a timed action with interrupt-on-movement, not an
   instant click. Open items:
   - Make `Dig` aim into the cliff face (use the trace normal as the
     direction the brush extends, not just the center). The current Dig
     centers the brush on the hit point — for cave-mining you want the
     brush offset *into* the wall by half the brush radius.
   - **Tool-quality duration override**: better picks → shorter timer
     within `BeginTerraform`. The floor is non-zero (primitive bone pick
     on limestone may take 30s+ per ore chunk). Tool quality also
     affects brush radius (better pick → more removed per swing).
   - Yield items from the dug-out volume — pickaxe should produce
     stone/ore based on the material that was removed (read the
     material under the brush via `Query Voxel Layer` before applying
     the modifier in `CompleteTerraform`).

2. **Procedural caves** — generated alongside continental terrain so
   players can find them by exploring. Not yet implemented. The standard
   approach for Voxel Plugin 2.0:

### 18b. Procedural cave recipe

**Stamp type:** `UVoxelGraph` (volume graph, not height graph). Stamped
onto the world's **Volume Layer** via an `AVoxelStampActor` whose stamp
is a `FVoxelVolumeGraphStamp`. The graph outputs a *density* — negative
inside the surface, positive outside. Where density goes negative, the
voxel mesher carves out emptiness.

**Graph composition for caves:**

```
                              ┌─► [3D Perlin: large feature] ──► density baseline (rolling cave system)
                              │
[Get Voxel Position 3D] ──────┼─► [3D Perlin: worm noise + threshold] ──► thin tunnels (the connecting passages)
                              │
                              └─► [Smoothstep by Y depth] ──► "no caves above sea level" mask
                                                             (or "more caves deeper", whatever the rule)

  Combine: ((Baseline + Worms) × DepthMask) → density output → carve out where < 0
```

The simplest cave that reads as a cave is **3D Ridged Perlin** stamped as
volume — ridges become tunnel surfaces, valleys-between-ridges become
empty cave volume. `Advanced Noise 3D` with `DefaultOctaveType=RidgedPerlin`
is the equivalent of the 2D ridged-mountain recipe but in 3D.

**Depth gating** is important — you don't want caves intersecting the
surface randomly (creates ugly holes in plains). Mask the cave density
by `(SeaLevelZ - WorldZ) / CaveDepthRange` so caves only carve out at
or below some depth. The mask lets you have shallow caves near
mountain bases and deep, more-frequent caves underground.

### 18c. Splines vs procedural

**Don't use splines for the bulk of MO57's caves.** Spline-authored
caves are deterministic and ideal for hand-placed set pieces (dungeons,
campaign locations, story dungeons in a future DLC). For the procedural
open-world goal — "explore and find your own caves" — you want
3D-noise-driven Volume Graphs.

Splines might still earn their keep for:
- **Cave entrances** — a designer-placed spline shapes the entrance
  arch, where it meets a procedural noise-driven cave volume.
- **Rivers** that go through caves (a spline carves both the river bed
  on the surface and continues as a tube into a cave system).
- **Set-piece dungeons** added later — placed by hand inside the procedural
  cave layer so they read as "natural" but have authored geometry.

### 18d. Ore placement inside caves — the metadata pattern

**Problem:** PCG's `Voxel Sampler` is built around sampling 2D points on
the height surface. Caves are 3D — they have walls, floors, and ceilings,
not a single height value. We need a way for PCG to "spawn an iron vein
on a cave wall".

**Solution: metadata layer that flags "this voxel is cave-adjacent".**

1. **In the Volume Graph that carves caves**, also write a metadata layer
   like `VFM_CaveSurface = 1.0` for voxels within ±N units of the cave's
   density-zero isosurface. Anywhere else, leave the default (0).
2. **Voxel Sampler in the ore PCG graph** has Query Metadata configured
   to include `VFM_CaveSurface`.
3. **Density Filter** on the sampled points keeps only those where the
   metadata is high — ie. on cave walls.
4. **`MO Resource Spawner`** drops an "IronOre" resource at filtered
   points using its existing DataTable-driven row picker.

This is the cleanest path because:
- The cave shape and the ore distribution are independent — you can tune
  ore density without re-rolling cave geometry.
- It composes with the existing `MO Terrain Mod Filter` and elevation
  bands nodes — you can say "iron ore only on cave walls below 500m".
- Player-mined caves can also write `VFM_CaveSurface` (via a metadata
  override on the Dig sculpt), so PCG ore can appear in player-carved
  tunnels too — emergent self-consistency.

**Alternative for early prototyping: spline-along-cave-axis spawning.**
If we go the spline route for cave authoring, the simpler path is "scatter
ore points along each spline within a radius". This works without metadata
but doesn't generalize to player-mined caves.

### 18e. Different ore types via different metadata layers

Each ore type writes its own metadata layer with its own depth gating:

| Metadata layer | Written where | PCG samples for |
|---|---|---|
| `VFM_CaveSurface_Shallow` | Cave walls between -500 and 0 (sea level) | Iron, copper |
| `VFM_CaveSurface_Deep` | Cave walls below -1500 | Silver, gold |
| `VFM_CaveSurface_VeryDeep` | Cave walls below -5000 | Gemstones, rare ores |
| `VFM_VeinSpot` | Sparse points throughout cave volumes | Mother lode ore stamps (authored mesh stamps with custom material) |

Authoring these as separate layers (vs one numeric layer with a magic
threshold) lets the metadata write logic stay simple and lets the PCG
filters be readable.

### 18f. Bridging "found ore" → "mined yields"

This part already works through our existing systems — we just need to
wire it up:

1. Cave volume graph stamps an "OreMother" volume mesh stamp at vein
   spots. The stamp has a custom MegaMaterial layer (e.g.
   `VMI_IronOre`) so it visibly looks different from cave wall.
2. Player begins a timed Dig action (volume sculpt) at that location.
   The 5+ second timer runs as usual — mining is a realistic-time
   action, not an instant click (see §17d).
3. Inside `CompleteTerraform` (when the timer expires), **before**
   applying the modifier, call `Query Voxel Layer` for the material
   at the brush's center. If the material is `VMI_IronOre`, yield
   "IronOre" items to the player's inventory. Quantity should scale
   with the brush radius and the tool quality, so a single 5-second
   swing of a primitive pick produces 1–2 ore chunks; an iron pick
   on the same vein might produce 3–4 in 3 seconds.
4. Apply the modifier (the voxel material disappears as the volume is
   removed).

This means `CompleteTerraform` needs a tiny pre-modifier read step.
Easy add to `UMOTerraformingComponent::ApplyPendingTerraform` (which
already dispatches to `VolumeDig` / etc.).

### 18g. Open questions / things to figure out at implementation time

- **3D Voxel Sampler in PCG** — does it actually sample volume positions
  in 3D, or only height surface? If only height, ore placement in caves
  needs the spline-along-cave-axis path instead of true volume sampling.
  Need to test in a sandbox before committing to the metadata approach.
- **Cave performance** — Volume Graphs are 3–5× the cost of Height
  Graphs. Caves at scale (whole-world cave layer) may need LOD strategy
  (no cave generation past N units from player).
- **Save/load of cave-state metadata** — player-modified metadata needs
  to persist. Check what Voxel Plugin's sculpt save covers.
- **Cave-aware character/AI navigation** — the navigation system needs
  to generate navmesh inside caves. `AVoxelWorld.bEnableNavigation` is
  default true, but `bGenerateNavigationInsideNavMeshBounds` only
  generates inside designer-placed bounds — for caves we'd want a
  bigger bounds volume, or rely on dynamic generation when chunks load.

---

## Glossary

| Term | Meaning |
|---|---|
| **FBm** | Fractal Brownian motion. Sum of multiple noise octaves at decreasing amplitude + increasing frequency. The standard way to add multi-scale detail. |
| **Octave** | One layer of an FBm sum. |
| **Lacunarity** | Frequency multiplier between octaves. 2.0 = double the frequency each layer. |
| **Persistence / Gain** | Amplitude multiplier between octaves. 0.5 = half the strength each layer. (`Advanced Noise 2D` calls this `Gain`.) |
| **Ridged noise** | Transform `1 − |noise|` that turns smooth peaks into sharp ridges. Exposed as `RidgedXxx` octave types on Advanced Noise. |
| **Billowy noise** | Transform `abs(noise)` that turns smooth gradients into bulbous bumps. Exposed as `BillowyXxx` octave types. |
| **Stamp** | Per-instance application of a graph / heightmap / mesh / spline to the voxel world. Lives on an `AVoxelStampActor` in the level. |
| **Layer (Height/Volume)** | A bucket in the world's `LayerStack`. Stamps target layers; layers themselves are nearly empty containers. |
| **LayerStack** | The asset assigned to `AVoxelWorld.LayerStack`. Lists Height/Volume layers + ordering. Default `/Voxel/Default/DefaultStack`. |
| **VoxelMaterial** | A `UVoxelMaterial` asset wrapping a regular UE Material. Required by stamps and graphs. |
| **MegaMaterial** | Voxel's per-world material registry that blends N layered VoxelMaterials. Needs a popup "Add" click whenever a new VoxelMaterial appears in the level. |
| **VMG (Material Graph)** | Voxel graph type that outputs a material. Runs over the whole world based on position / normal / metadata. |
| **Auto Material** | Wrapper asset around a VMG. What you drop into a Height Graph's Material pin. |
| **VPCG (PCG Graph)** | Voxel graph type that processes PCG points (snap, filter, sample metadata, randomize). Called from PCG via `Call Voxel Graph`. |
| **Metadata** | Per-position data layer (float / vector / int) attached to the world. Stamps write it, materials/PCG/gameplay read it. |
| **Per-Instance Custom Data** | UE's mechanism for per-instance shader data. Used by VPCG → foliage to colour bushes by ground metadata. |
| **InstanceStampComponent** | The component used for runtime sculpting. Stamps added live in world space. |
| **VoxelCharacter** | `AVoxelCharacter` — required parent class for player/AI on voxel terrain. Regular `ACharacter` teleports. |
| **Mega material** | (alias for MegaMaterial above) |
| **Chunk** | The unit voxel data loads/streams in. Configurable; default ~3200³ voxel units. |
| **LOD** | Level of detail. Voxel auto-downsamples distant chunks. |
| **HWRT vs SWRT** | Hardware vs Software ray tracing. Voxel 2.0 P7 supports only SWRT for Lumen. |
