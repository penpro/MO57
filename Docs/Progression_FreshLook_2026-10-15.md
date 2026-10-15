# Progression from bare hands — a fresh look (Oct 15 2026)

Wes, Oct 15: "take a fresh look at the progression" (instead of picking options C+A from
`Progression_Reachability_2026-10-14.md`), and "the starting kit will ultimately be nothing". So this review starts from a
survivor with empty hands.

Data: `MO.Test.ProgressionMap` (new) writes `Saved/Logs/progression_map.json`. It runs the producer analysis
(`FMOItemSourceAnalysis`) twice, once with the gates as they are **today** and once with every discovery and knowledge gate
removed (**attempt anything**). It gives each item and recipe a tier: the number of breadth-first steps from the bare world.
Rerun it after any recipe, item or resource change.

## The short answer

**The recipe tree is starved of materials, not locked by knowledge.**

| | Recipes a player can perform |
|---|---|
| Today | **37 of 124** |
| Every discovery and knowledge gate removed | **45 of 124** |

Removing the discovery problem the Oct 14 review was about (C+A) adds 8 recipes. The other 79 stay out of reach because
**24 raw materials have no producer**, and a handful of them sit at the root of everything past the stone age:

* **Water** — nothing produces `Water01` or `CleanWater01`, and no action lets a survivor drink from a lake or river.
  With thirst now warning after 30 minutes, every player hits this in their first hour.
* **Wood products** — no hardwood, softwood, planks, large logs, straight branches, bow staves or arrow shafts. Trees give
  only `Log01` and green sticks. That blocks the Workbench, the Loom, the bellows, the fire drill, the bow, arrows, spears,
  the canoe and shields.
* **Leather** — `TannedLeather01` comes only from brain tanning, and carcasses never yield a brain. The item
  `BarkTannedLeather01` exists but no recipe makes or uses it. Leather + planks make the bellows, and the bellows gate the
  Forge: **all 29 metal recipes** hang on these two.
* **Sand** — pottery needs `Sand01` as temper; nothing produces it, though the beaches are sand.
* **Plant materials** — reeds, dried grass, flax and birch bark have no source (baskets, fish traps, tinder, thread, bark
  containers).

So the first job is the material economy, mostly data, and water first because the new pacing makes it urgent. Discovery
(C+A) is still worth doing, but it is the smaller lever.

## What a survivor with empty hands can do today

| Tier | Obtainable |
|---|---|
| 0 (by hand, in the world) | Green Branch, Stripped Branch, Tree Bark, Stone, River Cobble, Flint Nodule, Edible Root, Wild Mushroom; from a carcass: Raw Meat, Raw Hide, Animal Bone, Animal Sinew |
| 1 | Flint Flake, Flint Knife, Acheulean Hand Axe, Hammerstone, Crude Club, Digging Stick, Plant Fiber, Tree Resin, Clay, Dry Tinder, Scraped Hide, Bone Needle, Roasted Meat, Hazelnuts, Wild Berries, Red Apple, Burn Salve, Native Copper; buildings: Campfire, Lean-to, stick floor / walls / roofs, School |
| 2 | Simple Cordage, Charcoal, Log, Crude Chopper, Sharp Stone Flake, Copper Bead, Suture Kit |
| 3 | Splint, Tourniquet, Activated Charcoal, Basket |

That is the whole tree today: a stone toolkit, cordage, a fire, a lean-to, and first aid. It stops before water,
pottery, leather, woodworking, the bow, the Workbench and every metal.

The campfire's fuel is `Firewood01`, `Charcoal01` or `Stick01`. Firewood has no producer, so today the only fuel is green
branches, which burn badly in reality. The station's menu also has no visible way to add fuel.

## Where it dead-ends, and what would produce each missing material

Attempt-anything blockers, grouped by root cause (first gate per recipe):

| Missing material | Recipes it blocks | A realistic producer to add | Kind |
|---|---|---|---|
| Water01, CleanWater01 | Boil Water, three medicine brews; and drinking itself | **Drink** at a lake / river (timed), **Fill** a container there; Boil Water makes clean water | code (interaction) |
| Plank01, Hardwood01, Softwood01, LargeLog01 | Workbench, Loom, Bellows (→ Forge), Fire Drill, Smith Hammer, Atlatl, Potter's Wheel, Wooden Shield, Chariot, Deadfall, Canoe | species yields (locust / alder / oak → hardwood, spruce / pine → softwood, large trees → large log); **Split Log** with axe + wedge → planks and billets | data |
| StraightBranch01, BowStave01, ArrowShaft01 | Haft Spear, Haft Flint Spear, Fish Spear, Atlatl Dart, Simple Bow, Composite Bow, arrows | hazel and saplings yield straight rods (hazel coppice is the classic source); **Carve Stave** / **Shave Shafts** recipes | data |
| TannedLeather01 (via AnimalBrain01) | Bellows (→ Forge), Pouch, Quiver, Blanket, leather clothing and armour | deer carcass yields a brain; wire **bark tanning** (scraped hide + bark + water, a long timed soak) to the same leather id | data |
| Sand01 | Crude Pottery, Clay Bowl, Cooking Pot, Storage Jar | dig sand on beaches and riverbanks (the forage dig table by ground material) | data / small code |
| Reeds01, DriedGrass01, FlaxFiber01, BirchBark01 | Basket, Fish Trap, Gather Tinder, Spin Flax Thread, Bark Container | reed beds on shores (the reed mechanism exists), dry grass from grassland forage, flax or nettle in meadows, birch trees | data |
| Bellows01 (consequence) | Forge, Bloomery → **all 29 metal recipes** | falls out of planks + leather | — |
| WoolFleece01, WoolThread01, TinIngot01, SodaAsh01, MineralGlaze01, Greenstone01, Sandstone01, Feathers01, Horn01, AnimalBladder01, HideGlue01 | wool cloth, bronze, glass, glaze, polished axe, grinding stone, arrows' fletching, water skin | sheep (domestication), tin ore deposit, plant-ash leaching, outcrops, bird and carcass parts | later ages; park |

## A progression spine

The order a real survivor would go in, with the producers each band needs. The bands are ordered by need, so nothing in a
later band is required earlier.

| Band | What the survivor needs | What it takes | New producers |
|---|---|---|---|
| **First hour** | water, food, shelter, a fire | drink at water; forage; sticks, bark, stones, flint; knap flakes; lean-to; campfire on **deadwood**; friction fire (hand drill) | Drink / Fill; firewood from deadwood; straight rod + softwood board for the drill |
| **First days** | a stone toolkit | cordage; hafting (rod + resin + cordage → hafted axe, spear); chop trees → logs; bark and woven containers; scraped hides; cooking | straight rods; reeds / withies |
| **First week** | wood and hide | split logs → planks; Workbench; bow and arrows; bark-tanned leather; pottery (clay + sand, open firing); snares and traps; water skin | split log; staves and shafts; bark tanning; sand; bladder |
| **Settlement** | cloth, crops, storage | flax / nettle fibre → spindle → loom → cloth; hoe, sickle, planting; storage jars; school | flax / nettle; (later) sheep |
| **Metal** | copper → bronze → iron | native copper (cold-hammered, works today) → furnace + bellows + charcoal → cast copper → bronze (tin) → bloomery iron → steel | bellows (planks + leather); tin ore |

## How a player learns techniques

Unchanged from Oct 14, but sequenced after the materials: **attempt anything** (an undiscovered recipe can be tried at a
penalty; success teaches it) as the rule, and **precursor grants** (handling the materials "remembers" the technique) as
the fast path. It matches the pillar ("no hard locks; genetic memory"). Today it adds 8 recipes; once the materials above
exist it is what keeps the whole tree open without a grind.

## Recommended order

1. **Drink and fill at water** (code: a timed, interruptible action on lakes / rivers / the ocean, feeding thirst and the
   metabolism; fill a container). Thirst now bites at 30 minutes and nothing answers it.
2. **Woodworking producers** (data + three recipes): species wood yields, Split Log, rods from hazel and saplings, staves and
   shafts. Unlocks the Workbench, spears, the bow, and half the bellows.
3. **Leather** (data): brain from deer, bark tanning to the one leather id. With step 2 it unlocks the Forge and the metal age.
4. **Sand, reeds, dried grass, flax / nettle, birch** (data + the dig table): pottery, baskets, tinder, thread.
5. **Firewood from deadwood** and an **add fuel** control on the station.
6. **Discovery: attempt anything + precursor grants.**
7. Later ages: wool, tin, glass and glaze, outcrop stones, bird parts.

Steps 2–5 are a data pass of about a day with `ue.py rows set`; step 1 is a few hours of code. Rerun
`MO.Test.ProgressionMap` after each: the target is every recipe reachable in attempt-anything mode, with the tiers in the
order above.
