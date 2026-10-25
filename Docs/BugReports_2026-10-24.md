# Bug reports, Oct 24 2026 (Wes, PIE in the editor, build of d0292f9f + the vote batch, seed 10496, a NordicStrand start)

> **What this is:** the live tracker for the 22 reports Wes sent on Oct 24 (05:11-06:10 UTC, `mo-crashes-2026-10-24.tar.gz`; one standalone
> PIE session checking the round-2 vote batch). One row per report: what it asks, the layer responsible, the plan and the status.
> Backlog rows: BR24-n in `Docs/BACKLOG.md`. When every row is closed or moved to the backlog, archive this file (`Docs/Archive/`).
> **Crash in the same tarball:** 1, not Wes's: the harness's `probe` instance (Oct 24 03:50 UTC) ran `MO.WorldFeatures.Grounding`, whose
> `TraceRocks` line trace hit a degenerate triangle of an ED mesh's complex collision: a handled engine ensure
> (`CollisionConversions.cpp`: `OutResult.Normal.IsNormalized()`), the game went on. Dev-tool path only.
> **Read with:** `python -I Tools/crash_triage.py <dir>`; full texts and screenshots: `ue_crash_bundle.parse_bundle`.

## Reports

| Row | Report | Title (Wes) | Ask / diagnosis (layer) | Plan | Status |
|---|---|---|---|---|---|
| BR24-1 | 8d96a3e6 | these rocks need a flat spot to land on | open-geometry rock plates (ErodedRockyGround) on bumpy ground: one edge stands up with a shadow under it. "placement math for determining flatness of a spot versus the size of these open geometry assets" | the biome spawner: a plate is placed only where the ground under its footprint departs from a plane by less than a share of its thickness | VERIFY |
| BR24-2 | 52f28127 | open geometry rocks sticking out | NordicBeachRockyGround plate ~1 m proud on flat ground (scale 1.61) | with BR24-1 | VERIFY |
| BR24-3 | 0aeeee9d | cliffs? | a HugeSandstoneCliff slab (2.26x) at the shore: "10m of one sided cliff face" out of the ground | the slabs (BR23-12) | VERIFY |
| BR24-4 | d1408026 | smaller cliffs look better buttt.... | "larger cliffs are WAAAAAAYYYYYY out of the ground... maybe try laying them down and alligning them to the slope?" | slabs lie on the slope, face out of it (a rock outcrop), sunk to their face | VERIFY |
| BR24-5 | 2c5a72a7 | some smaller cliffs still not "sitting" in the terrain right | a slab seen side-on: a thin sheet out of the hill. "i may need to create some 'closed' geometry version of these cliffs" | with BR24-4; closed variants are Wes's art | VERIFY |
| BR24-6 | c75ec336 | huge random cliffs in alpine rock too | the x2 slabs standing upright on steep AlpineRock ground read as pillars | with BR24-4 | VERIFY |
| BR24-7 | e9854a84 | make all rocks in alpine rock biome much larger and sunk into ground better | AlpineRock's rocks are tiny on its summits | AlpineRock rocks x3, buried by size | VERIFY |
| BR24-8 | 20181523 | more rocks, bigger rocks | "make a lot of the closed geometry rocks larger, like 2-20 time larger ... set into the ground in a dynamic depth (based on size) ... small, medium, large, xlarge pipelines or ... 10-25% of mesh bounds deep" | size tiers of the closed rocks per biome (large x3-6, xlarge x8-20, rarer the bigger), each buried a share of its height | VERIFY |
| BR24-9 | cd52f446 | push the cliffs together closer | "overlapping the bases slightly would help blend them together, still need a lot more trees around the pois instead of just ferns" | cluster packing (CoreFraction) and trees round the POIs | VERIFY |
| BR24-10 | 03ba7773 | area around embankments still very bare | "push the embankments closer together and add a lot more trees" | with BR24-9 | VERIFY |
| BR24-11 | 932283fc | more floating assets | pieces meant to sit in an ED cliff assembly float over our ground (a grassy ledge over a hollow) | Epic's authored heights (N-040) | OPEN |
| BR24-12 | d4fa688c | ed forest asset out of ground | a mossy boulder plate of an ED assembly stands proud | with BR24-11 | OPEN |
| BR24-13 | 56b3e0e9 | rainforst/cave light levels weird | "i'd rather things be a little unnaturally bright than to have it being like a lgiht switch turning off and on"; the report frame is black | UDS's interior adjustment cuts the sky light to 0 when its occlusion test reads enclosed | VERIFY |
| BR24-14 | c9bd70b9 | height split not faded at all at a distance still | "but the variation looks much better" | the inland height bands blend over 25-50 m: far voxel LODs show that as a hard edge | VERIFY |
| BR24-15 | 57c68f06 | lakes are still little round things and the water is still too wavy | "huge swaths of water material rolling across the lake surface, dial them down a LOT still, they should be almost still" | the lake water material's layers (Oceanology); round lakes = BR23-13 | VERIFY |
| BR24-16 | d1ca4fa1 | lake movement/footsteps/swimming sounds | "just remove them until we find some that don't sound like shit" | the sound bank rows for water steps, wading, swimming, lake ambience | DONE-2026-10-24 |
| BR24-17 | 54c3a597 | terrain mesh offset needs dialed down a bit | (Wes, asked:) the ground material's Nanite displacement is high: great on the Nordic Strand rocks, odd elsewhere | a per-surface displacement scale in M_QuixelBase | VERIFY |
| BR24-18 | 94487cde | wetness affect is weird | "the wetness effect goes way too high on things and just looks metallic instead of wet, im assuming the effect is from oceanology??" -- beach rocks and the pawn's skin at the shore | find which wetness (Oceanology shore / character wetness, UDW) | VERIFY |
| BR24-19 | 98b8c640 | these little purple flowers look good here | "but they're weird and blurry in other places" | BR23-10 | FIXED Oct 25 with BR23-10 (by eye) |
| BR24-20 | 34096019 | purple flowers being weird here | blurry, some of them | BR23-10 | FIXED Oct 25 with BR23-10 (by eye) |
| BR24-21 | 1a77a86f | this grass still has no shadows and looks like it's glowing | the pawn's shadow lands on the ground under the blades, not on them | BR23-11 | OPEN |
| BR24-22 | 94980919 | cave entrances are still enormous with a lot of floating stuff at edges | | BR23-1 | OPEN |
