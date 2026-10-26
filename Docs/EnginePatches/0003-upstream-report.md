# Draft bug report for Epic: groom simulation jitters far from the world origin (UE 5.8.3)

Not posted. For https://issues.unrealengine.com (or the UE forums' Rendering / Groom section). Measurements are MO57's (Oct 25-26 2026).

**Summary:** A simulated groom (MetaHuman hair, local simulation in the `head` bone) shakes back and forth every frame when the character is
more than ~5 km from the world origin. The jitter grows with the distance; the same groom at the origin sways smoothly.

**Version:** Unreal Engine 5.8.3 (launcher), Windows 11, DX12, RTX 4090. Large World Coordinates on (the default).

**Repro:**
1. Any MetaHuman with a simulated hair groom (e.g. `Hair_S_UpdoBraids` with `Hair_L_StraightBangs`-style bangs), idle animation.
2. Place the character at (0, 0, 300000) and look at the bangs: they sway smoothly with the head.
3. Move the character to (1414214, -1414214, 300000) (20 km out, 3 km up, open sky): the bangs flutter back and forth every frame.
4. Disable the simulation (groom component `SimulationSettings.bOverrideSettings` with `SolverSettings.bEnableSimulation` off): steady anywhere.

**Measured** (the bangs tracked against the ear over 6 s of consecutive frames; rms per-frame step of hair minus face, and the lag-1
autocorrelation of that step: positive = smooth sway, negative = back-and-forth):

| Distance from origin (3 km up) | Step rms | Autocorrelation (y, x) |
|---|---|---|
| 0.05 km | 2.08 px | +0.80, +0.85 |
| 5 km | 2.30 px | +0.48, +0.45 |
| 7.5 km | 2.92 px | +0.11, +0.03 |
| 10 km | 3.19 px | +0.26, -0.01 |
| 15 km | 5.08 px | -0.12, -0.25 |
| 20 km | 5.28 px | -0.08, -0.07 |
| 30 km | 6.62 px | -0.13, -0.11 |

The break sits where a float's step doubles at 2^19 cm (5.24 km).

**Suspected cause:** `NiagaraDataInterfaceHairStrands.cpp`, `ComputeWorldTransform` / `ComputeBoneTransform`: the groom's world transform and
the simulation bone's world transform are converted to `FMatrix44f` first; the local-simulation branch then builds `FMatrix44d` from those
floats and takes `World * Bone^-1` with the comment "Due to large world coordinate we compute the relative world transform in double
precision" -- but the precision is already gone (~1-2 mm per step at 15-20 km). A patch computing the relative transform from the double
transforms is attached (`0003-hair-simulation-relative-transform.patch`). We could not build it to confirm (a launcher engine cannot override
the HairStrands plugin from a project: Interchange and MetaHumanSDKEditor reference HairStrandsCore). Simulating in the bone the groom is
attached to (so both world transforms round alike) did NOT reduce the jitter, so other absolute-float paths likely contribute too: the
absolute `BoneTransform` handed to the solver for local/world conversions (forces, collisions) and the physics asset collision shapes.

**Workaround in our game:** groom simulation only within 5 km of the origin on every coordinate (the start of a new world is the origin).
