# Night report, Oct 17 2026 (Claude, alongside Codex)

Your direction: "caves only in limestone, mining yields from the geology ... they should be able to collect pretty much any ore, the actual hurdle
with iron is having fire hot enough ... research how technology actually advanced", then "keep working with codex for the next 12 hours".
Codex shares the tree. We coordinated in `claude-codex-coop.md`, and I staged only my own files in every commit. Codex worked the audit
batches (crafting, inventory, equipment) and was idle from 00:33 to 03:44; I held the build window in that gap and gave it back when Codex
resumed.

## Built and verified

| Commit | What | How it was checked |
|---|---|---|
| ba37d659 | Caves only in limestone (generator v11); digging yields what the geology holds (soil, the bedrock's stone, coal / hematite / copper / tin / flint); ore outcrops on steep bare rock; iron ore with a stone pick | WorldGen 43/43; a coal outcrop mined in game |
| 0c323d7e | Icon studio no longer crashes on HDR textures; icons for Coal, Limestone, Granite, Basalt (granite and basalt off the mossy boulder) | icons rendered and looked at |
| 4b1e1265 | **Torch** (see below), **eye adaptation**, **caves no longer glow**, **galena** (lead ore) in limestone, which also gives glazing a source | tests below; screenshots in `Saved/Logs/torch_cave` |
| 20586cce | Torch carried at the side (it was gripped mid-shaft with the flame at the face); flame effect half size | close-ups from four sides at dusk |
| 9eddbf01 | End-to-end checks recorded | -- |
| 61722e7c | The torch recipe used BirchBark01, which nothing produces, so the torch was unobtainable; now common bark (Bark01) + resin | progression map: Torch01 reachable at tier 2 |
| 057e525a | Campfires light-adapt the eye too (`UMOFlameLightComponent` on BP_Campfire) | at night by a lit fire: floor EV100 -2.1; clears when put out |
| a9a4de5d | Dev verbs: MO.Fire.Do / cover gather send their RPC next tick (on a joiner they ran locally) | build; co-op retest pending |

Test totals: 264/264 over every group except Codex's Crafting/Inventory/Equipment (theirs to run), `MOFramework.Light.*` 8/8 (7 new tests),
`nettest game` PASS with the new components in.

### The torch (`Docs/Light_Sources.md`)

- **Calibration:** measured Paleolithic lights (Medina-Alcaide et al. 2021, PLOS ONE): a juniper torch is ~14 cd, burns 21-61 min, and lights
  ~2.5 m; a fat lamp ~0.6 cd. Torch01 is 10 cd for 45 min.
- **Recipe:** MakeTorch = a branch + bark x2 + resin; the mesh is a kitbashed branch with a bark bundle.
- **Lighting it:** "Light the Torch in the fire" at a burning fire (6 s, timed action).
- **Carrying fire:** "Light it from the Torch" lights a laid fire with no drill or tinder (95 % dry, 60 % rain).
- **It goes out** when put away, under water, or burnt down.
- **Airflow:** it burns brighter while moving (held still it starves of air: the study's 5 -> 28 lux when swung).
- **Verified in game:** crafted -> lit at a campfire via the real server RPC -> lit a laid fire -> doused wading into a lake.
- **Co-op:** a host's lit torch is seen lit on the client.
- **Pending:** the torch does not wear down its item yet. Nothing could wear an EQUIPPED item; Codex is adding `ReduceEquippedDurability`
  after their gate, then it is a one-line swap (marked PENDING in `MOCarriedLightComponent.cpp`).

### Why caves were not dark, and why a torch blew out white

- Ultra Dynamic Sky's real-time *Captured Scene Sky Light* casts no shadow, so it lit the inside of every hill like a moonlit night. Auto
  exposure goes down to EV100 -10, so the cave looked dim blue instead of black. **Fixed** with UDS's own *Apply Interior Adjustments* (its
  occlusion traces; sky light x0 inside): the cave is black (measured 0.0), open ground unchanged. It also covers dug tunnels and huts.
- With the cave black, a torch-lit floor sat ~8 stops over: a camera meters the whole frame, an eye near a flame stays light-adapted.
  **Fixed** with `UMOEyeAdaptationComponent`: the illuminance of nearby flames at the camera floors the auto exposure. Result: ~2.5 m of warm
  lit ground, black beyond, matching the measured working radius.

## Your calls (nothing built on these)

1. **ANSWERED and BUILT (morning):** inspect-based discovery, with metal through the clay line. 126 of 136 recipes reachable (was 55); see `Docs/Archive/Progression_Reachability_2026-10-14.md`. Original note: **the metal path was blocked by discovery gates, not by heat.** `MO.Test.ValidateData`: 55 of 132 recipes reachable.
   - Smelting, IronSmelting, AnvilWork (and ~45 more) are knowledge levels nothing grants.
   - So Bellows01 can't be made, so no Forge or Bloomery can be built, so nothing smelts.
   - This is the Oct 14 fork (`Docs/Archive/Progression_Reachability_2026-10-14.md`; recommended: "attempt anything", as the genetic-memory pillar in
     CLAUDE.md says, plus precursor grants).
   - Your "the hurdle with iron is fire hot enough" reads as attempt-anything; the heat ladder can only be the gate once these are open.
2. **Digging time by rock hardness** -- ANSWERED ("yes, different times for different rocks/minerals") and BUILT (see Technology_History.md).
   Before, a cubic metre of granite dug as fast as soil.
   - Evidence: Grimes Graves chalk ~6x soil with haulage; fire-set hard rock ~40-60x.
   - Proposed: limestone x5, granite and basalt x40, soil unchanged; ore nodes keep their own harvest times.
   - Choices: (a) as proposed, (b) gentler (limestone x2, granite x6), (c) leave it.
3. **Heat ladder data**: station temperatures and per-recipe minimum temperatures are drafted from the physics in `Docs/Technology_History.md`
   (campfire 700 C, forge 1,300, bloomery 1,250; earthenware 600, copper 1,100, iron bloom 1,200 ...). Codex's fields exist; the values wait
   on their gate and on you.
4. **Night look** -- ANSWERED: leave it for now. outdoors at night with no fire the exposure adapts to EV100 -10, so a moonless night reads as dusk. A darker night means
   a higher night minimum or slower dark adaptation. By a fire the new eye floor already makes the dark beyond the firelight black.

## Waiting on a commit

- **Torch wear** is built and verified: the item's own durability burns down, burn-out notifies and empties the hand, and co-op passes 5/5.
- It is UNCOMMITTED on purpose: it calls Codex's `UMOEquipmentComponent::ReduceEquippedDurability`, which is in Codex's uncommitted equipment
  files, so committed alone it would not build.
- Codex commits only when you authorize it. Once their equipment work is committed, commit mine (MOCarriedLightComponent.h/.cpp +
  Docs/Light_Sources.md) right after.

## Still open (mine)

- Co-op: a client lighting its own torch at a fire. The first try hit the dev-verb RPC bug fixed in a9a4de5d; the retest runs after Codex's
  window.
- Orphan items found by ValidateData that a realistic recipe could feed: SodaAsh01 (plant ash for glass), HideGlue01 (boiled hide),
  TinIngot01 (smelting tin), Greenstone01 (basalt serves for ground axes).
- A torch-raising arm pose; smoke in dead-end galleries; rain on torches.
