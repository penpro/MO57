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

## Current state (`VHG_Flat`)

- Single Perlin/Simplex noise → height.
- Uniform variation everywhere — no large-scale structure, no peaks vs plains.
- Seed pipeline (`MOGameMode → ApplySeedToHeightGraphParameter →
  FVoxelExposedSeed param`) works correctly; **preserve the Seed input pin
  shape** on the new graph so MOGameMode keeps applying seeds without code
  changes.

---

## Voxel Plugin Pro 2.0 — what's available

These are the actual node class names you'll see in the graph editor search:

| Category | Nodes you'll use |
|---|---|
| **Noise primitives** | `Perlin Noise 2D`, `Simplex Noise 2D`, `Value Noise 2D`, `Cellular Noise 2D`, `True Distance Cellular Noise 2D` (Worley) |
| **Seed plumbing** | `Mix Seeds`, `Make Seeds` (split one seed into N derived ones — keeps octaves uncorrelated) |
| **Math** | `Add`, `Multiply`, `Lerp`, `Abs`, `Saturate`, `Smoothstep`, `Power`, `Clamp` |
| **Position** | `Get Voxel Position 2D`, `Get Voxel Position 3D` (input to all noise) |
| **Curves** | `Apply Voxel Curve` (remap noise output through an authored curve asset — best way to shape mountain falloff or beach transition) |
| **Stamps** | `Voxel Height Graph Stamp` — bakes pre-authored shapes (river beds, mountain cores) into the heightfield |

**Note on Ridged + FBm**: Voxel Plugin doesn't ship "Ridged Noise" or "FBm"
as single nodes — you compose them from primitives. Recipes below.

---

## Recipe 1 — Multi-Octave Noise (FBm)

**What:** sum N Perlin layers, each at half the amplitude and double the
frequency of the previous one. Gives natural detail at every scale.

**Why:** a single noise looks artificial — too uniform, no large-scale
features. FBm is the standard solution.

**Graph pattern** (4 octaves, but tune N):

```
[Position 2D] → [Multiply × 0.00005] → [Mix Seeds: seed, 1] → [Perlin 2D] → [× 1.0   ]
              → [Multiply × 0.00020] → [Mix Seeds: seed, 2] → [Perlin 2D] → [× 0.50  ]
              → [Multiply × 0.00080] → [Mix Seeds: seed, 3] → [Perlin 2D] → [× 0.25  ]
              → [Multiply × 0.00320] → [Mix Seeds: seed, 4] → [Perlin 2D] → [× 0.125 ]
                                                                        \\
                                                                 [Add all 4]
                                                                        ↓
                                                              ContinentBase (-1..+1ish)
```

**Tunables:**
- **Lacunarity** (frequency multiplier per octave): 2.0 default. Higher = bigger
  jump in scale between octaves. Lower = smoother blend.
- **Persistence** (amplitude multiplier per octave): 0.5 default. Higher = more
  detail visible. Lower = first octave dominates.
- **Base frequency** (the first × 0.00005): controls continent size. Lower =
  bigger continents. The 0.00005 value gives ~20km-wide features at default
  voxel scale; tune for your world size.

**Critical: derive a unique seed per octave** via `Mix Seeds`. If every
octave uses the same seed, the layers correlate and you get repeating
patterns instead of noise. The integer 2nd arg to `Mix Seeds` is the salt —
just pick unique ints per octave.

**Multiply the final sum by your max elevation** (e.g. 8000 voxel units for
a world that goes 80m above sea level). That's your `ContinentBase`.

---

## Recipe 2 — Ridged Mountain Ranges

**What:** transform a noise output so high values become sharp ridges
(mountain peaks) and low values become smooth (valleys). Mask it by a
separate low-freq "mountainous zone" noise so mountains only appear in
parts of the world.

**Why:** plain Perlin gives rolling hills. Ridged gives shapes that read as
mountains — narrow ridges with sharp valleys between them.

**Ridge transform (single node chain):**
```
[Perlin 2D output] → [Abs] → [× -1] → [+ 1]   = ridge value (0..1, 1 at the ridge)
```
Equivalent formula: `1 - |perlin|`.

**Recipe (composes a mountain field):**

```
// Step 1: build a 2-octave ridged noise — the actual mountains
[Pos × 0.0003] → [Mix Seeds: seed, 10] → [Perlin 2D] → [Abs] → [×-1] → [+1] → [× 1.0]
[Pos × 0.0008] → [Mix Seeds: seed, 11] → [Perlin 2D] → [Abs] → [×-1] → [+1] → [× 0.4]
                                                                            \\
                                                                       [Add] = MountainShape

// Step 2: power-curve the result so only HIGH values become real peaks
[MountainShape] → [Power × 3.0] = MountainShapeSharp

// Step 3: separate mask — low-frequency Perlin that says "this region IS mountainous"
[Pos × 0.00003] → [Mix Seeds: seed, 12] → [Perlin 2D] → [Saturate]   = MountainMask (0..1)
                                                       [+ 0.3 first to bias toward more mountains]

// Step 4: multiply mountain shape by mountain mask, scale by mountain peak height
[MountainShapeSharp] × [MountainMask] × 12000   = MountainContribution (in voxel units)
```

**Tunables:**
- Power exponent (Step 2): higher = sharper, more isolated peaks. 2.0–4.0 is
  the useful range.
- MountainMask bias (Step 3): how much of the world has mountains. 0.0 ≈ 50%
  mountainous; +0.3 ≈ 80%; -0.3 ≈ 20%. Most worlds want -0.3 to 0.0 (mountains
  are special).
- Mountain peak height (Step 4): max elevation a mountain adds on top of
  ContinentBase. 12000 voxel units ≈ 120m. Real mountains in the player's
  scale of view want to feel imposing but not unscalable.

**Combine with ContinentBase:**
```
[ContinentBase] + [MountainContribution] = RawHeight
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
RawHeight = ContinentBase + MountainContribution   // from above
WaterLevel = 0.0                                    // your project's water Z
BeachWidth = 1500.0                                 // ~15m above and below

// Distance from water level
[RawHeight] → [- WaterLevel] = DeltaFromWater  // signed; negative = below water

// 0 at waterline, 1 outside BeachWidth (above or below)
[DeltaFromWater] → [Abs] → [/ BeachWidth] → [Saturate] = BeachFalloff

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
Position 2D ──┬─→ [Multi-Octave FBm]     ─→ ContinentBase
              │
              ├─→ [Ridged Mountain Field] ─→ MountainContribution
              │      ↑
              │   masked by [Mountain Zone Mask]
              │
              └─→ (used everywhere as input)

ContinentBase + MountainContribution = RawHeight
RawHeight → Beach Flatten → FinalHeight (output)
```

Plus the input pins:
- `Seed: FVoxelExposedSeed` — **must keep this exact name** so
  `MOGameMode::ApplySeedToHeightGraphParameter` finds it.

---

## Acceptance criteria

Visual check after building VHG_Realistic + repointing the level at it:

- [ ] **No tiling**: flying the camera high reveals organic continent shapes,
      not a uniform noise plane.
- [ ] **Distinct regions**: some chunks are clearly mountainous, others are
      gentle hills. You can tell visually which biome you're standing in.
- [ ] **Visible peaks**: mountains have sharp ridges, not just bumps.
- [ ] **Walkable coasts**: every shoreline has a gradual beach, no flying
      buttresses dropping into the sea.
- [ ] **Same seed = same world**: load a save and the terrain matches.
      (Requires the existing seed pipeline to still find the `Seed` pin.)

---

## Repointing levels at the new graph

Once VHG_Realistic is authored:

1. Open the test level (currently `MOPCGScattering`).
2. Select the `AVoxelWorld` actor.
3. In Details → `Voxel Graph`, swap from `VHG_Flat` to `VHG_Realistic`.
4. Save level. Test new-game-start — terrain should regenerate with the
   foundation features.

If you want the swap to persist across all levels via Project Settings,
expose `DefaultHeightGraphPath` on `MOGameSettings` and have `MOGameMode`
apply it programmatically. Likely a follow-on, not needed for this pass.

---

## Future ladder rungs (separate tasks, build on this foundation)

| Rung | Task | Notes |
|---|---|---|
| 4 | Biome zones | Temperature + Moisture maps → biome lookup → per-biome elevation modifier (sand dunes, swamp basins). |
| 5 | Procedural rivers | Voxel `Heightfield Stamp` along peak→coast curves. |
| 6 | Hydraulic erosion | Check Voxel Plugin Pro 2.0 docs — they ship erosion in some 2.x. Otherwise CPU pre-pass at chunk init. |
| 7 | Landmarks | Hand-authored stamps (giant boulder, dolmen, cliff line) scattered via PCG with min-spacing constraints. |

---

## Reference: useful tunable curves

For the `Apply Voxel Curve` node — author UCurveFloat assets with these
shapes for common transitions:

| Curve | Use |
|---|---|
| `S-curve` (smoothstep) | Beach falloff, biome blending |
| `Sharp peak` (gauss-like) | Mountain shape — boost the high end |
| `Slow-then-fast` (concave-up) | Hills that get steeper as they rise |
| `Fast-then-slow` (concave-down) | Cliffs that flatten on top |

Keep authored curves in `Content/Voxel/Curves/` for reuse.
