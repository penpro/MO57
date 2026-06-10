# Terrain Foundation Plan

**Audience:** designer/programmer building the voxel height graph in-editor.
**Goal:** move from `VHG_Flat`'s single-noise terrain to a foundation that
supports rich biomes / rivers / erosion later. Implements rungs 1–3 of the
realism ladder:

1. **Multi-octave noise composition** (continent + hills + detail)
2. **Ridged-noise mountain ranges** masked by a low-frequency "mountainous zone"
3. **Beach flattening** near water level

After this pass the world has macro shape (continents), distinct mountain
regions, and clean coasts. Biomes / rivers build on this canvas.

---

## Architectural note — read this first

`AVoxelWorld` does **not** hold a graph reference. The chain is:

```
AVoxelWorld (level actor)
  └─ LayerStack: UVoxelLayerStack  ─►  HeightLayers / VolumeLayers (containers)

AVoxelStampActor (level actor)
  └─ UVoxelStampComponent
        └─ FVoxelHeightGraphStamp
              └─ Graph: UVoxelHeightGraph   ←  THE FILE WE AUTHOR
```

To put a graph into the world: **place a stamp** (editor's Place menu →
Volume / Height) — this creates an `AVoxelStampActor` and, if the level
doesn't already have one, an `AVoxelWorld` at origin too. Set the stamp's
Kind to `Height → Graph`, then assign or Create-New a `UVoxelHeightGraph`
asset on it. Alternatively, **drag an existing VHG asset into the
viewport** — same outcome, auto-creates the stamp actor.

Move / resize the stamp actor to define the area the graph covers.

This means the original "swap from VHG_Flat to VHG_Realistic on AVoxelWorld"
instruction in earlier drafts was wrong — there's no graph slot to swap.
The actual swap is on the stamp actor's `Graph` field (or you can just
delete the old stamp and drag the new VHG in).

---

## Current state (`VHG_Flat`)

- A single `Advanced Noise 2D` node — actually already FBm-capable (it sums
  N octaves internally), but currently configured as the equivalent of a
  single-Perlin output: uniform variation everywhere, no large-scale
  structure, no peaks vs plains.
- Seed pipeline (`MOGameMode → ApplySeedToHeightGraphParameter →
  FVoxelExposedSeed param`) works correctly; **preserve the Seed input pin
  shape** on the new graph so MOGameMode keeps applying seeds without code
  changes.

---

## Voxel Plugin Pro 2.0 — what's available

These are the actual node class names you'll see in the graph editor search:

| Category | Nodes you'll use |
|---|---|
| **Composite noise (workhorse)** | `Advanced Noise 2D` — multi-octave FBm + ridged/billowy in one node |
| **Noise primitives (rare)** | `Perlin Noise 2D`, `Simplex Noise 2D`, `Value Noise 2D`, `Cellular Noise 2D` — only needed for one-off layered tricks the Advanced Noise can't express |
| **Seed plumbing** | `Mix Seeds` — decorrelate INDEPENDENT noise nodes (e.g. base height vs. mountain mask). Not needed inside one Advanced Noise. |
| **Math** | `Add`, `Multiply`, `Lerp`, `Abs`, `Saturate`, `Smoothstep`, `Power`, `Clamp` |
| **Position** | `Get Voxel Position 2D` — input to Advanced Noise 2D / any other noise |
| **Curves** | `Apply Voxel Curve` — remap noise output through an authored curve asset, useful for shaping mountain falloff or beach transition |
| **Stamps** | Not needed for base height — we are the stamp. Stamps are for later (rivers, landmarks). |

**Key fact:** `Advanced Noise 2D` exposes `Lacunarity`, `Gain` (= persistence),
`NumOctaves`, `Seed`, and a `DefaultOctaveType` enum with `SmoothPerlin`,
`BillowyPerlin`, `RidgedPerlin`, plus Cellular/Simplex/Value variants.
This collapses the entire "FBm chain" and "ridged transform" recipes into
parameter changes on one node.

---

## Recipe 1 — Multi-Octave Noise (FBm)

**What:** sum N noise layers, each at half the amplitude and double the
frequency of the previous one. Gives natural detail at every scale.

**Why:** a single noise looks artificial — too uniform, no large-scale
features. FBm is the standard solution.

**Implementation — one node:**

```
[Get Voxel Position 2D] ──► Position pin of [Advanced Noise 2D #1: "ContinentBase"]
                                            ├─ Amplitude       = 6000           (target relief in voxel units)
                                            ├─ FeatureScale    = 500000         (5 km between big bumps; adjust to world size)
                                            ├─ Lacunarity      = 2.0
                                            ├─ Gain            = 0.5
                                            ├─ NumOctaves      = 6
                                            ├─ Seed (pin)      ◄── [Mix Seeds: Seed, salt=1]
                                            └─ DefaultOctaveType = SmoothPerlin

Output: ContinentBase (≈ −Amplitude .. +Amplitude in voxel units)
```

**Tunables:**
- **`FeatureScale`**: world distance one big feature takes. Lower = smaller
  continents (more variation per kilometer). With VoxelSize=200 and world
  scale 1cm/unit, FeatureScale=500000 ≈ 5km per major feature.
- **`Lacunarity`** (2.0 default): higher = bigger jump in scale between
  octaves. Lower = smoother blend.
- **`Gain`** (0.5 default — the "persistence"): higher = more detail
  visible. Lower = first octave dominates.
- **`NumOctaves`**: 4–6 is the right zone for base terrain. 10 (the node
  default) is overkill at LOD0 and hurts perf without visible benefit.
- **`Amplitude`**: max relief in voxel units. 6000 ≈ 60m above/below mid level.

**Seed wiring:** `[Seed exposed param] → [Mix Seeds: salt=1] → ContinentBase
Seed pin`. The salt uniquifies this noise vs. other Advanced Noise nodes in
the graph (mountain shape, mountain mask, etc.).

**Do NOT** insert per-octave `Mix Seeds` — `Advanced Noise 2D` already
decorrelates its own octaves internally. Salting per-octave is the legacy
"stack of Perlins" pattern and just creates clutter.

---

## Recipe 2 — Ridged Mountain Ranges

**What:** sharp ridges + sharp valleys. Mask it by a separate low-frequency
"mountainous zone" noise so mountains only appear in parts of the world.

**Why:** plain Perlin gives rolling hills. Ridged gives shapes that read as
mountains — narrow ridges with sharp valleys between them.

**Implementation — two nodes:**

```
// Step 1: the mountain shape itself — ridged FBm
[Pos 2D] ──► [Advanced Noise 2D #2: "MountainShape"]
                ├─ Amplitude       = 12000          (~120 m peaks above base)
                ├─ FeatureScale    = 30000          (300 m between ridges)
                ├─ Lacunarity      = 2.0
                ├─ Gain            = 0.55           (a bit higher than base — keeps the small ridges visible)
                ├─ NumOctaves      = 5
                ├─ Seed (pin)      ◄── [Mix Seeds: Seed, salt=10]
                └─ DefaultOctaveType = RidgedPerlin     ← this IS the ridge transform
                                                          (no hand-built abs/1-x chain)

// Step 2: optional power curve for sharper isolated peaks
[MountainShape output] → [Power: Exp = 1.5..3.0] = MountainShapeSharp

// Step 3: separate low-freq mask — "this region IS mountainous"
[Pos 2D] ──► [Advanced Noise 2D #3: "MountainMask"]
                ├─ Amplitude       = 1.0
                ├─ FeatureScale    = 1500000        (very large — 15 km mask features)
                ├─ NumOctaves      = 2
                ├─ Gain            = 0.5
                ├─ Lacunarity      = 2.0
                ├─ Seed (pin)      ◄── [Mix Seeds: Seed, salt=11]
                └─ DefaultOctaveType = SmoothPerlin

[MountainMask output] → [Add: +0.0 to +0.3 bias] → [Saturate] = MountainMaskClamped (0..1)

// Step 4: combine
MountainContribution = MountainShapeSharp × MountainMaskClamped
```

**Tunables:**
- **Power exponent (Step 2)**: higher = sharper, more isolated peaks. 1.5–3
  is the useful range. 1.0 = no shaping. Skip the Power node if Ridged
  alone reads well enough.
- **MountainMask bias (Step 3)**: how much of the world has mountains.
  - 0.0 ≈ 50% mountainous (default Perlin distribution)
  - +0.3 ≈ 80%
  - −0.3 ≈ 20%
  - Most worlds want −0.2 to 0.0 (mountains are special, not the norm).
- **`Amplitude` on MountainShape**: how tall peaks get on top of
  ContinentBase. 12000 voxel units ≈ 120m. Tune for "imposing but not
  unscalable" — about 2× the player's view distance through a valley.

**Combine with ContinentBase:**
```
RawHeight = ContinentBase + MountainContribution
```

---

## Recipe 3 — Beach Flattening

**What:** smoothly flatten the terrain in a vertical band near sea level
so coasts look like beaches instead of cliffs that drop straight into the
water.

**Why:** raw noise crossing water level creates either underwater cliffs or
unwalkable shore cliffs. Real coasts have a gradient — wave-flat zone at the
waterline, then steeper as you go inland.

**Recipe:**
```
RawHeight = ContinentBase + MountainContribution   // from Recipes 1+2
WaterLevel = 0.0                                    // your project's water Z
BeachWidth = 1500.0                                 // ~15m above and below

// Distance from water level
[RawHeight] → [Subtract: WaterLevel] = DeltaFromWater  // signed; negative = below water

// 0 at waterline, 1 outside BeachWidth (above or below)
[DeltaFromWater] → [Abs] → [Divide: BeachWidth] → [Saturate] = BeachFalloff

// Smoothstep makes the transition curve gentle, not linear
[BeachFalloff] → [Smoothstep 0 1] = BeachWeight (0..1)

// Lerp toward WaterLevel where BeachWeight is small (near water)
[Lerp(WaterLevel, RawHeight, BeachWeight)] = FinalHeight
```

**Tunables:**
- `BeachWidth`: half-width of the smoothed band, in voxel units.
  - 1000 (10m): tight beaches, dramatic coastlines
  - 1500 (15m): natural-feeling beaches with gentle slope
  - 3000 (30m): very wide beaches, tropical-island feel
- `WaterLevel`: should match your project's `WaterLevelZ` (currently on
  `AMOGameMode`). Either hardcode here, or expose as a graph parameter and
  set from C++ at startup.

---

## Putting it all together — `VHG_Realistic`

The full chain:

```
[Get Voxel Position 2D] ──┬─► [Advanced Noise 2D: ContinentBase  (SmoothPerlin)]
                          ├─► [Advanced Noise 2D: MountainShape  (RidgedPerlin)] → optional Power
                          └─► [Advanced Noise 2D: MountainMask   (SmoothPerlin, very large scale)] → Saturate

[Seed param] ──► [Mix Seeds 1] ──► ContinentBase.Seed
            ──► [Mix Seeds 10] ──► MountainShape.Seed
            ──► [Mix Seeds 11] ──► MountainMask.Seed

ContinentBase + (MountainShape × MountainMask) = RawHeight
RawHeight → Beach Flatten chain → FinalHeight → OutputHeight
```

Plus the input parameter:
- `Seed: FVoxelExposedSeed` — **must keep this exact name** so
  `MOGameMode::ApplySeedToHeightGraphParameter` finds it.

Three Advanced Noise 2D nodes + four math nodes (two Mix Seeds extras +
combine math + beach chain). Far less spaghetti than a hand-built FBm
graph would be.

---

## Wiring it into a level

1. **Author** `VHG_Realistic.uasset` in `Content/Penumbra/Maps/` (or wherever
   you keep terrain graphs).
2. **Open the test level** (currently `MOPCGScattering`).
3. **Find the existing stamp actor** that holds the current `VHG_Flat`
   reference. (In the World Outliner, sort by class — look for an
   `AVoxelStampActor` near the world origin.)
4. **Either:**
   - **A) Replace the graph reference**: select the stamp actor → in
     Details, find `Stamp Component → Stamp Ref → Stamp → Graph`, swap from
     `VHG_Flat` to `VHG_Realistic`. Cheapest option, preserves the stamp's
     position/scale.
   - **B) Delete the old stamp, drag the new VHG into the viewport.**
     Editor auto-creates a fresh `AVoxelStampActor` pointing at
     `VHG_Realistic`. Move/scale it to cover the play area.
5. **Save level.** Test new-game-start — terrain should regenerate with the
   foundation features.

---

## Acceptance criteria

Visual check after building VHG_Realistic + repointing the level's stamp:

- [ ] **No tiling**: flying the camera high reveals organic continent shapes,
      not a uniform noise plane.
- [ ] **Distinct regions**: some chunks are clearly mountainous, others are
      gentle hills. You can tell visually which biome you're standing in.
- [ ] **Visible peaks**: mountains have sharp ridges, not just bumps. (If
      they look like bumps, switch the MountainShape node from
      SmoothPerlin to RidgedPerlin and/or raise the Power exponent.)
- [ ] **Walkable coasts**: every shoreline has a gradual beach, no flying
      buttresses dropping into the sea.
- [ ] **Same seed = same world**: load a save and the terrain matches.
      (Requires the existing seed pipeline to still find the `Seed` pin.)

---

## Future ladder rungs (separate tasks, build on this foundation)

| Rung | Task | Notes |
|---|---|---|
| 4 | Biome zones | Temperature + Moisture maps (more Advanced Noise nodes, very low frequency) → biome lookup → per-biome elevation modifier (sand dunes, swamp basins). |
| 5 | Procedural rivers | Voxel `Heightfield Stamp` along peak→coast curves. Place via new `AVoxelStampActor`s positioned by a separate generator. |
| 6 | Hydraulic erosion | Check Voxel Plugin Pro 2.0 docs — they ship erosion in some 2.x. Otherwise CPU pre-pass at chunk init. |
| 7 | Landmarks | Hand-authored stamps (giant boulder, dolmen, cliff line) scattered via PCG with min-spacing constraints. |

---

## Reference: useful tunable curves

For the `Apply Voxel Curve` node — author UCurveFloat assets with these
shapes for common transitions:

| Curve | Use |
|---|---|
| `S-curve` (smoothstep) | Beach falloff, biome blending |
| `Sharp peak` (gauss-like) | Mountain shape — boost the high end if Ridged alone isn't sharp enough |
| `Slow-then-fast` (concave-up) | Hills that get steeper as they rise |
| `Fast-then-slow` (concave-down) | Cliffs that flatten on top |

Keep authored curves in `Content/Voxel/Curves/` for reuse.
