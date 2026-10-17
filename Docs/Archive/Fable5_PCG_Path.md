# Fable 5 Goal — The Path to Robust PCG (a world worth settling)

**To:** Fable 5
**From:** the 2026-07-04 planning pass
**Reads with:** `MO57_Fable5_Charter.md` (Pillar 6 = Game Feel/World), `Fable5_Village_Handoff.md` (the village that sits in this world), World Features tasks #115/#117–#123.

The village is only as convincing as the world around it. MO57's current PCG scatter is functional but sparse; the goal is **a layered, biome-driven, density-graded procedural world** that reads as a real place — the tier of the Fab PCG showcase the brief points at. This doc sets that goal, names the routes, and flags the one hard constraint that makes MO57's PCG path *different* from every off-the-shelf asset.

> **Note on the reference link:** the Fab share URL `fab.com/s/7ee8c5704aaa` is bot-walled (403) so I couldn't resolve the exact product. The leading biome-PCG assets in that space are **Calysto World 2.0**, **Biomes in a Box**, **PCG Layered Biomes**, **Massive World**, and **Procedural World Generator** (sources below). The path here is **asset-agnostic** — it holds whichever one you meant; please confirm the exact asset when convenient so I can tune the integration notes. Critically, **UE 5.8 ships Epic's own native PCG + "PCG Biome Core / Biome Sample" framework**, which MO57 already has.

---

## 1. Current State — what MO57's PCG does today

MO57 has a **custom PCG spawner set that runs on the voxel surface** (not on a UE Landscape):
- `UMOPCGResourceSpawnerSettings` + subclasses (`Tree`/`Bush`/`Rock`), `UMOPCGItemSpawnerSettings`, `UMOPCGMeshSpawnerSettings`.
- Supporting nodes: `UMOPCGElevationBandsSettings` (height-banded placement), `UMOPCGDistanceCullingSettings` (perf), `UMOPCGHISMTaggerSettings` + `UMOPCGForceHISMTreeBuild` (instanced-mesh tagging for the harvest system), `UMOPCGSpawnPointSettings`.
- Terrain foundation in flight: `VHG_Realistic` height graph (#115, multi-octave + ridged mountains + beach flattening).
- World Features architecture designed (#121): a catalog + subsystem + PCG spawner skeleton for POIs (springs #122/#123, ore veins #119, caves #118).

**What it lacks vs. the end goal:** biome *layering* (one flat scatter everywhere vs. distinct forest/meadow/rocky/wetland zones with their own palettes and rules), density *grading* (uniform vs. clustered/thinned by terrain + biome), ecosystem *variety* (few species), and the visual *richness* (ground cover, LOD-cheap detail, transitions) that sells "a real world."

---

## 2. The End Goal

A world where **where you are** is legible from what grows there, and **founding a village somewhere** is a choice with texture:
- **Layered biomes:** distinct zones (temperate forest, meadow, rocky highland, wetland, beach, alpine) each with its own species palette, ground cover, and scatter rules — driven by terrain (height/slope/moisture/temperature from the existing sims) and a low-frequency biome mask.
- **Density grading:** clustered undergrowth, thinned canopy, bare rock, dense reeds at water — not uniform noise.
- **Ecosystem variety:** enough species + ground cover + detail meshes per biome to avoid the "copy-paste tree" read.
- **Runs on the voxel terrain, and regenerates as it changes** (MO57's superpower — you dig a valley, the scatter respects the new surface). This is the one thing no off-the-shelf asset does out of the box (§4).
- **Scales:** Nanite + HISM + distance culling + World Partition/HLOD so a settle-able open world holds frame.

---

## 3. Two Routes (use the native pattern, harvest the asset's ideas)

**Route A — Epic's native PCG Biome framework (UE 5.8, already installed).**
UE 5.7/5.8 ship **PCG Biome Core + Biome Sample** plugins: hierarchical, **data-asset-driven** biome generation on top of the base PCG graph system (points, mesh spawning, metadata domains, graph instances/parameters, World-Partition + HLOD integration). This is the *architecture* to adopt — it's native (no third-party dependency/licensing), it's data-asset-driven (which matters for autonomy, §5), and it's the pattern the whole ecosystem is converging on.

**Route B — a Fab biome-PCG asset (Calysto World 2.0 / Biomes in a Box / PCG Layered Biomes / Massive World / Procedural World Generator).**
These get you to a *rich look* faster and are worth studying for their biome-layering logic, density curves, and palettes. **But (§4) almost all of them sample the UE Landscape heightfield** — which MO57 does not use — so you adopt their *ideas and data structures*, not their sampling front-end.

**Recommendation:** build on **Route A's native, data-asset-driven biome architecture**, re-targeted to sample MO57's **voxel surface** (reuse the existing spawner sampling + `MOTerrainSweep`), and **harvest Route B assets** for biome definitions, species palettes, and density curves. You get native tech + autonomy-friendly data + the richness of a curated asset, without inheriting a Landscape dependency.

---

## 4. The One Hard Constraint (why this isn't a plug-in-an-asset job)

**MO57's ground is Voxel Plugin Pro 2.0 destructible terrain, NOT a UE Landscape.** Nearly every PCG biome asset — and Epic's Biome samples — sample a **Landscape heightfield** (`Get Landscape Data`, landscape layer weights) to decide where/what to place. **Those nodes return nothing on a voxel world.**

MO57 already solved voxel-surface scatter (the existing `UMOPCG*SpawnerSettings` sample the voxel surface, and the harvest/HISM tagging works). So the integration is **not** "drop in the asset" — it's:
1. Keep MO57's **voxel-surface sampling** as the placement source.
2. Adopt the biome asset's / native framework's **layering + density + palette + data-asset** logic *on top of* those voxel samples.
3. Preserve MO57's **runtime regen on terrain edit** — the thing that makes a destructible world feel alive and that no Landscape-based asset supports.

Budget real integration time here; this is the crux and the reason MO57's PCG is a *custom* path, not a purchase.

---

## 5. Autonomy Note — PCG is a build-tool gap too

Like the village's art (see `Fable5_Village_Handoff.md` §5), **PCG-graph authoring is largely outside the autonomous loop today**: wiring PCG graph nodes is visual editor work, and the MCP has no proven PCG-graph node-authoring API. Two implications:
- **Prefer the data-asset-driven (native Biome) route precisely because it's more autonomous-friendly** — biomes defined as **data assets / DataTable rows** (species lists, density curves, height/slope/moisture bands) can be authored and iterated via the MCP `rows set` loop, exactly like recipes and medical chains were this session. Push as much biome *definition* as possible into data the loop can write, and keep the hand-wired graph a thin, stable executor over that data.
- **Flag the graph-authoring gap** when you hit it: if a biome needs new graph topology (not just new data), that's editor work — surface it as a tooling gap (candidate: an MCP PCG-graph authoring capability, or accept it as human/editor work) rather than a silent manual detour. Log PCG hit/miss in `AUTONOMOUS_TOOLING.md`.

---

## 6. Phased Goal

Sequenced so each phase is verifiable and the autonomous-buildable parts (data + code) lead:

1. **Biome catalog (data, autonomous)** — a `DT_Biomes` (or the World-Features catalog, #121): per-biome species palette, ground-cover set, density curves, and terrain bands (height/slope/moisture/temperature — moisture/temperature can seed off the existing weather/exposure sims). Author + iterate via MCP.
2. **Biome mask + voxel-surface layered scatter** — a low-frequency biome mask over the world; extend the existing voxel-surface spawners to read the biome catalog and place per-biome palettes with graded density. Native PCG Biome pattern, voxel sampling.
3. **Density + variety + transitions pass** — clustering, thinning, ground cover, biome-edge blending; enough species to kill the copy-paste read. (Harvest Fab palettes/curves here.)
4. **Runtime regen on edit** — confirm scatter respects terraforming (dig/raise/flatten) and regenerates coherently; the destructible-world superpower.
5. **Perf + look** — Nanite on scatter, HISM batching + distance culling (partly done), World Partition/HLOD for scale, and the Lumen/HWRT tuning for voxel visibility (#117). This is where it becomes A-rated *world*.

**Verification:** boot to a seed, fly the world, capture viewport shots (the `ue.py asset shot` / `EditorAppToolset` CaptureViewport capability from the village handoff §5B is the visual-QA rung PCG needs too), and grade biome distinctness + density + transitions against reference.

---

## 7. First Moves

1. **Confirm the reference asset** (`fab.com/s/7ee8c5704aaa`) so integration notes can be exact — but don't block on it; the path is asset-agnostic.
2. **Stand up `DT_Biomes` + one biome end-to-end** (data → voxel-surface scatter of one distinct palette) — the PCG vertical slice, mostly autonomous.
3. **Prove runtime regen** on that one biome (dig a hole, watch scatter update) — the constraint that defines MO57's PCG.
4. Then layer biomes, grade density, and do the perf/look pass toward the end-goal world.

---

**Sources:** [Calysto World 2.0 — Fab](https://www.fab.com/listings/8631308a-67a3-4e20-b3e4-74be19813f77) · [Biomes in a Box — Fab](https://www.fab.com/listings/9de2e94e-df06-4d36-ab54-097806c3968a) · [PCG Layered Biomes — Fab](https://www.fab.com/listings/762ad275-f56b-4275-9c24-6da1025508fa) · [Procedural World Generator — Fab](https://www.fab.com/listings/7c9e76c9-68ed-4a02-997e-2ac8c7a113fd) · [UE PCG Biome Core & Sample Plugins — Epic Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-overview-guide-in-unreal-engine) · [UE PCG Overview — Epic Docs](https://dev.epicgames.com/documentation/unreal-engine/procedural-content-generation-overview)
