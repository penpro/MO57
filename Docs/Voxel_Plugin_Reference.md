# Voxel Plugin Pro 2.0 — Reference & Lessons

> Living document. Add to it as we test more techniques and discover
> capabilities / pitfalls. Reference both for MO57 contributors and for
> any future modders building custom terrain.

> Companion to `Docs/Terrain_Foundation_Plan.md` (the actionable recipes).

## TOC

1. [Core concepts](#1-core-concepts)
2. [Graph types & when to use which](#2-graph-types--when-to-use-which)
3. [Node taxonomy](#3-node-taxonomy)
4. [Seed & determinism rules](#4-seed--determinism-rules)
5. [Performance characteristics](#5-performance-characteristics)
6. [Common pitfalls](#6-common-pitfalls)
7. [Sandbox testing protocol](#7-sandbox-testing-protocol)
8. [Test progression (we fill this in as we go)](#8-test-progression)

---

## 1. Core concepts

**`AVoxelWorld` actor** — the actor placed in the level that owns the
terrain. Holds a reference to a voxel graph (the recipe for what terrain
looks like) and a material. Per `CLAUDE.md`, set `bCreateRuntimeOnBeginPlay
= false` and let `MOGameMode` drive runtime creation so the seed gets
applied first. For pure sandboxing without MOGameMode, leave it `true`.

**Voxel Graph** — a `.uasset` containing the node graph that computes
terrain. Two flavors:
- **Height Graph** (2D, also called `VoxelHeightGraph`): one Z per (X,Y).
  Heightmap-style. Cheap, can't do caves/overhangs.
- **Volume Graph** (3D, `VoxelGraph`): a density function over 3D space.
  Supports caves, overhangs, floating islands. More expensive.

**Stamps** — pre-authored shapes (carved into voxels via `VoxelStampComponent`)
that get baked into terrain at fixed positions. Great for: handcrafted
points of interest, river beds, large structural features. Stamps live on
actors in the level, not in the graph.

**Materials & layers** — terrain surface is a "mega material" that blends
between layers (rock/dirt/grass/sand). Layer selection driven by graph
outputs (slope, altitude, biome value).

**PCG integration** — `VoxelPCG` module surfaces terrain query nodes to UE5's
PCG system. Foliage / rocks / props scatter via PCG using terrain conditions
(slope, height, biome).

---

## 2. Graph types & when to use which

| Need | Use |
|---|---|
| Standard outdoor terrain (heightmap) | **Height Graph** |
| Caves, overhangs, floating islands | **Volume Graph** |
| Both? | Layer them — Height Graph for surface, Volume Graph for caves carved into it. |

**MO57 currently uses Height Graph** (`VHG_Flat`). For paleolithic survival
this is the right call — caves are a future feature, and Volume Graph is
3–5× more expensive per chunk.

---

## 3. Node taxonomy

### 3a. Noise primitives (from `VoxelNoiseNodes.h`)
- `Perlin Noise 2D` / `Perlin Noise 3D` — classic smooth noise. Default choice.
- `Simplex Noise 2D` / `Simplex Noise 3D` — like Perlin, slightly faster +
  fewer directional artifacts. Use when you want isotropic features.
- `Value Noise 2D` / `Value Noise 3D` — older / blockier. Good for
  large-scale bias maps where you want patches rather than gradients.
- `Cellular Noise 2D` / `Cellular Noise 3D` — Worley/Voronoi distance.
  Outputs distance-to-nearest-cell-center. Good for "cracked earth" looks
  or generating region boundaries.
- `True Distance Cellular Noise 2D` / `3D` — variant that returns
  Euclidean distance instead of Manhattan. Smoother cell edges.

### 3b. Seed plumbing
- `Make Seeds` — split one seed into N derived seeds (extendable pin set).
  Use to give each octave / feature its own seed.
- `Mix Seeds` — combines two seeds via MurmurHash. The "salt" you pass as
  the second arg uniquifies what the same input produces.

### 3c. Math (from `VoxelMathFunctionLibrary`)
- `Add`, `Subtract`, `Multiply`, `Divide`, `Power`
- `Abs`, `Negate`, `Saturate` (clamp 0–1), `Clamp(min,max)`
- `Lerp(A, B, T)` — linear interpolation, the workhorse for blending
- `Smoothstep(A, B, T)` — Hermite blend, much nicer for visual transitions
- `Min`, `Max`

### 3d. Position
- `Get Voxel Position 2D` / `Get Voxel Position 3D` — the input you feed
  every noise function. Multiply by a small constant for noise frequency
  (smaller multiplier = bigger features).

### 3e. Curves (from `VoxelCurveFunctionLibrary`)
- `Apply Voxel Curve` — runs a value through an authored `UCurveFloat` /
  `UCurveLinearColor`. The cleanest way to shape a noise output without
  building math node chains. Author curves in `Content/Voxel/Curves/`.

### 3f. Box / spatial bounds (from `VoxelBoxFunctionLibrary`)
- `Make Box 2D From Radius` — used by chunk queries to define the bounds
  the graph is computing. **You normally don't author with these directly**
  — they're upstream of your graph entry points.

### 3g. Stamps
- `Voxel Height Graph Stamp` — applies a stamp to the height field.
  Useful for: rivers (negative stamp dipping height), mountain cores
  (positive Gaussian stamp), landmarks.
- Stamps live on `AVoxelStampActor` instances in the level, OR via
  `UVoxelStampComponent` on any actor. Each stamp has a position, a
  shape (curve / heightmap texture / sphere / etc.), and a strength.

---

## 4. Seed & determinism rules

**The single most important rule:** `Mix Seeds` between every noise input.
Without it, all your noise outputs correlate to the same seed and you get
visible repetition.

**Pattern:**
```
[Seed input] → [Mix Seeds: salt=1] → [first noise]
            → [Mix Seeds: salt=2] → [second noise]
            → [Mix Seeds: salt=3] → [third noise]
            ...
```

The integer "salt" can be anything — just unique per consumer.

**For multi-octave FBm specifically:** each octave needs its own seed.
Same noise type + same position + same seed = same output, every octave.

**Project seed pipeline (MO57 specific):**
- `MOGameSettings` holds `PendingWorldSeed` (int32, set by new-game UI).
- `MOGameMode::ApplySeedToHeightGraphParameter(WorldSeed)` finds the
  `FVoxelExposedSeed` parameter on the graph (param name `"Seed"`) and
  sets it.
- This MUST happen before `AVoxelWorld::CreateRuntime()`.
- The graph's `Seed` input pin must be named **exactly "Seed"** —
  MOGameMode looks it up by name.
- Same seed → same world. Load / save / regenerate all rely on this.

---

## 5. Performance characteristics

Order of magnitude per chunk (rough — depends on chunk size + LOD):

| Operation | Cost |
|---|---|
| Single Perlin/Simplex 2D | ~1 unit |
| Multi-octave FBm (4 octaves) | ~4 units (linear) |
| Cellular Noise 2D | ~3–5× Perlin (more lookups) |
| 3D noise (anything) | ~5–10× the 2D equivalent |
| Stamp application | constant per stamp; depends on stamp size |
| `Apply Voxel Curve` | very cheap |
| Math nodes | effectively free |

**Optimization hints:**
- Compute big-scale (continent) noise once and reuse for biome mask +
  base elevation.
- Push small details to LOD0 only — large-scale features are visible at
  every LOD and dominate the silhouette.
- Stamps are LOD-aware automatically — they vanish at distant LODs.

---

## 6. Common pitfalls

| Pitfall | Symptom | Fix |
|---|---|---|
| Using the same seed across octaves | Repeating diagonal patterns | `Mix Seeds` with unique salt per octave |
| Frequency multiplier too high | Pixelated / noisy terrain | Lower the position multiplier (e.g. 0.001 → 0.0005) |
| Frequency multiplier too low | Featureless / flat-looking | Raise the multiplier |
| Forgot to scale noise output to voxel units | Terrain is ±1 cm tall | Multiply noise (−1..1) by your max elevation in voxel units |
| Cliff at water level | Coast is unusable | Apply Beach Flattening recipe |
| `bCreateRuntimeOnBeginPlay = true` with MOGameMode active | Voxel runtime starts before seed applies → wrong terrain | Set to false; let MOGameMode call CreateRuntime |
| Renamed the `Seed` exposed parameter | Save/load gives different terrain than new game | Name it back to `Seed` — MOGameMode hardcodes the lookup |
| Ridged transform on signed noise without abs() | Half the world is below ground | `1 - abs(noise)`, not `1 - noise` |
| Forgot to mask mountains | Whole world is mountains | Multiply mountain output by a separate low-freq mask |

---

## 7. Sandbox testing protocol

Goal: isolate one technique per level so we can see + tune it without
other variables confounding.

**Sandbox level setup (do once):**

1. **Create level**: `Content/Penumbra/Sandbox/VoxelSandbox.umap`. World
   composition off. Empty default lighting.
2. **Add an `AVoxelWorld` actor** at origin.
   - `bCreateRuntimeOnBeginPlay = true` (sandboxing — no MOGameMode involved).
   - Voxel size: 200 (default).
   - World size: small enough to load fast — 64×64×8 chunks is fine.
3. **Set a default material** (`/Voxel/Default/DefaultMegaMaterial`).
4. **Add a default pawn or DefaultGameMode-style spawning** so you can
   fly around. Cheat console + spectator camera works too.
5. **Save level**, drop into the project's test level list.

**Per-test protocol:**

1. Duplicate the previous test's voxel graph: `VHG_Test_NN_Description.uasset`.
2. Author the change.
3. Assign to the sandbox's `AVoxelWorld`.
4. Press Play, fly around with the spectator cam.
5. Note what works + doesn't in this doc's [Test progression](#8-test-progression)
   section.

**Avoid:** running tests in the gameplay level (`MOPCGScattering`). The
PCG scatter + ambient spawn + voxel readiness subsystem all create noise
in the logs and make it harder to focus on terrain itself.

---

## 8. Test progression

Track each test here as we run it. Append entries; don't rewrite history.

### Test 0 — Pure Flat (sanity check)

**Graph:** `VHG_Test_00_Flat.uasset`. Single node graph that outputs a
constant. **No Seed input pin.**

**Recipe:**
```
[Constant Float 0.0] → Height Output
```

**Expected:** perfectly flat plane at Z=0. Confirms the sandbox setup
works at all.

**Status:** ⬜ not yet built. Run this first to confirm setup before
trying noisier graphs.

---

### Test 1 — Single Octave Perlin (baseline)

**Graph:** `VHG_Test_01_SinglePerlin.uasset`. Same as current `VHG_Flat`.

**Recipe:**
```
[Position 2D] → [× 0.001 (frequency)] → [Mix Seeds: Seed, 1] → [Perlin 2D] → [× 5000 (amplitude)] → Height Output
[Seed: FVoxelExposedSeed input]
```

**Expected:** rolling, uniform hills. Same look across the whole world.
This is the "single noise graph" the project currently ships.

**Status:** ⬜ not yet built.

---

### Test 2 — Multi-Octave FBm

**Graph:** `VHG_Test_02_FBm.uasset`. Add 3 more octaves at increasing
frequency + decreasing amplitude. Recipe in
`Docs/Terrain_Foundation_Plan.md` Recipe 1.

**Expected:** terrain reads as having structure at multiple scales —
big continental shapes with hills with small bumps.

**Status:** ⬜ not yet built.

---

### Test 3 — Ridged Mountains, no mask

**Graph:** `VHG_Test_03_RidgedNoMask.uasset`. Add `1 − |perlin|` chain
with power curve. NO mountain mask yet — see what "100% mountains" looks like.

**Expected:** sharp peaks + valleys everywhere. Probably oppressive,
unwalkable. Confirms the ridged transform works.

**Status:** ⬜ not yet built.

---

### Test 4 — Ridged + Mountain Zone Mask

**Graph:** `VHG_Test_04_RidgedMasked.uasset`. Add the separate low-freq
"is this a mountainous region" mask, multiply mountain output by it.

**Expected:** distinct mountain RANGES with flatter areas between. This
is the realistic-feeling outcome.

**Status:** ⬜ not yet built.

---

### Test 5 — Beach Flattening

**Graph:** `VHG_Test_05_Beaches.uasset`. Add the beach lerp on top of
Test 4's output.

**Expected:** walkable shorelines instead of cliffs into water.

**Status:** ⬜ not yet built.

---

### Test 6 — Combined Foundation (= candidate `VHG_Realistic`)

**Graph:** `VHG_Test_06_Foundation.uasset`. Test 5 + tuned parameters
for the final foundation pass. This becomes `VHG_Realistic` when it
feels right.

**Status:** ⬜ not yet built.

---

## Glossary

| Term | Meaning |
|---|---|
| **FBm** | Fractal Brownian motion. Sum of multiple noise octaves at decreasing amplitude + increasing frequency. The standard way to add multi-scale detail. |
| **Octave** | One layer of an FBm sum. |
| **Lacunarity** | Frequency multiplier between octaves. 2.0 = double the frequency each layer. |
| **Persistence** | Amplitude multiplier between octaves. 0.5 = half the strength each layer. |
| **Ridged noise** | Transform `1 − |noise|` that turns smooth peaks into sharp ridges. |
| **Stamp** | Pre-authored shape baked into the heightfield at a fixed world position. |
| **Mega material** | Voxel's surface material that blends N layered textures by mask outputs. |
| **Chunk** | The unit voxel data loads/streams in. Configurable; default ~3200³ voxel units. |
| **LOD** | Level of detail. Voxel auto-downsamples distant chunks. |
