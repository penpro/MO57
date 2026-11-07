# Wes's votes on the Nov 6 look checks

Review page: https://claude.ai/artifact/LRzmvdbHmXDxw5scCjT8je (votes in its `verdicts` collection, read Nov 6, 22:00 PST). No comment threads. Notes word for word.

| Item | Vote | Wes's note | What it means |
|---|---|---|---|
| Gravel patches closed and pushed into the ground (`closed-gravel`) | down | "keep the gravel from the edges and bias more closed geometry rocks around the edges, so same pcg scheme as i've been saying large distance between pcg hot spots (the cave in this case) with smaller nodes scattered around the poi center in cascades, keeping the larger "gravel pit" look towards the center and larger rocks biased towards the outside" | N-354: rework where the bowl's dressing goes. The closing and the seat themselves stand ("they do look significantly better"). |
| The new overhangs (`overhang`) | down | "as with lakes, make them non uniform, right now they're like perfect little circles, chain some together, give then non-uniformity, random scatter of like 2-5 of them" | N-355: bowls that aren't circles, in chains of 2-5. A generator change. |
| Deep branching caves (`deep-cave`) | down | "i will have to run through them myself, add "branching cave" to pois" | N-356: the POI. Not a verdict on the caves: he walks them. |
| No rain under a floating island (`island-rain`) | down | "i will have to inspect in game" | Not a verdict: the row stays for him to look in game. |
| The black paint under the plants (`forest-floor`) | down | "still looks fucked from the photos but i may not be reading them correctly, will look in game" | N-333 stays on his list, with his doubt recorded. If it still looks wrong in game, N-333 reopens. |
| An option: sky light at each ray hit (`hit-sky-option`) | down | "im not sure what im looking at here" | The page failed to say what it showed. The option stays off; it comes back only if the forest needs it, shown plainly. |
| Rocks on the ravine walls (`ravines`) | down | "i'll have to look in game" | Not a verdict: the row stays for him to look in game. |
| The rainforest's dark trees (`rainforest`) | up | | Done. |
| Open ground in low sun (`open-ground`) | up | | Done. |
| A torch in a cave (`cave-torch`) | up | | Done. |
| Floating islands: smoother rock, large rocks for the crags (`islands`) | up | | N-331 done. |
| The Wounds tab (`wound-row`) | up | | N-315 done. |

## After the votes (Nov 6, Wes, in chat)

"actually i just realized you're just using the same template for the overhangs as the caves, the overhangs should be "native" to their biome, maybe some rocks and stuff exposed under the overhang but the native biome vegetationa and such should go right up to the overhang, it's suppose to give the existing biome some nice variation not be its own little poi type thing"

"so like grass and shit on top of the overhang, dirt/rocks/vines underneath"

Crash reports, `mo-crashes-2026-11-07.tar.gz` (Wes: "pretty sure all of these are ours"): 11 uploads, all from agents' runs on DARKSIDE, none from a player. 6 x an ensure in `RockUnder` under `IslandPlantsCommand` (a dev check verb's trace hit a bad voxel triangle), 2 x a stale ray tracing flag in `inst_lumenfix` runs (N-333's experiments), 1 x a voxel stamp hashing a transient object in `MO_WorldLab`, 1 x the known Python crash on `CLOSE_SLATE_MAINFRAME` (TRAPS), 1 x an access violation in `FMOWorldGenParams::Finalize` at a test launch at 12:28 Nov 6, between 1acfef4b and 05374b56 while the unity build was being fixed: Finalize runs on every launch and hundreds have passed since, so most likely a launch against a half rebuilt set of DLLs; not proven. Nothing to fix for players.

So the overhangs lost their bowls altogether (N-357), and his "chain 2-5" for them became shelters scattered along their slope. The caves keep their bowls: N-354 and N-355 stand for them.

## A reply for the Steam thread "Flame on?" (N-358; Wes posts it, when he wants)

You weren't missing anything, this was our bug. The game told you dry grass or shredded bark would work, then only accepted one specific item. The next update fixes it: dried grass and bark fibre will light a fire as they are, and the menu will say what you're using. Until then, breaking up an old fallen log sometimes gives Dry Tinder, and that's what the drill wants. Thanks for reporting it.

## Round 3 votes (Nov 7, the same page)

| Item | Vote | Wes's note | What it means |
|---|---|---|---|
| Overhangs, part of the hillside (`overhang-native`) | up | "these look good but i want them chained together so they're not so uniform... i think you thought i was talking about caves" | His Nov 6 "chain 2-5" was about the overhangs. They chain now: 2.9 overlapping domes each, blended into one uneven shelter (N-357). |
| Cave bowls (`cave-bowls`) | up | | N-354, N-355 done. |
