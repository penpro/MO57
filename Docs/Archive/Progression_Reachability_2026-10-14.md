# Progression reachability — what a player can actually make (Oct 14 2026)

Source: `MO.Test.ValidateData` → **Data:Sources** (`FMOItemSourceAnalysis`, commit 271830a8). Starting from nothing
but the world, it works out what a player can obtain (resource harvesting, harvest recipes, carcass parts, digging,
ground pickups) and which recipes can ever pass their real gates (ingredients and build parts, station plus fuel,
tools, discovery knowledge, required knowledge).

**Result: 124 recipes, 36 reachable, 88 blocked, 25 orphan items.** After the duplicate-row merge the same evening
(12 items that existed twice under two ids, e.g. Snare01/SnareWire01, folded into the id recipes use): **37 reachable,
87 blocked, 24 orphan items** (the copper deposit now yields NativeCopper01).

**Re-check Oct 17 2026 (night): 132 recipes, 55 reachable, 77 blocked, 9 orphan items** (BirchBark01, Feathers01, FlaxFiber01, Greenstone01,
HideGlue01, Horn01, SodaAsh01, TinIngot01, WoolFleece01). Circular discovery is now almost the whole story, and it blocks the entire metal path:
- MakeBellows needs AnvilWork, so Bellows01 can't be made, so neither BuildForge nor BuildBloomery can be built.
- SmeltCopperOre / SmeltIronOre / RefineIronBloom need Smelting / IronSmelting; AlloyBronze needs BronzeCasting. None of these is ever reached.
Wes's Oct 17 framing ("the actual hurdle with iron is having fire hot enough") reads as option C below: anyone may attempt smelting, and
heat (station temperature, fuel, bellows) is the real gate. The heat values are drafted in `Docs/Technology_History.md`.

**Built Oct 17 morning** (Wes: "discovery should be locked behind 'inspect' if nothing else opens up to it; somewhere in the clay tech line humans
would discover leftover metals etc, just make it realistic"). This is option A: precursor grants by inspection. Each circular knowledge (granted only by
inspecting the recipe's own output) is also taught at level 1 by inspecting the material a person would really have learned it from; the finished
product still teaches up to 3.

**Metal through the clay line:**
- PaintPotteryMalachite (Campfire, Pottery): a crude pot painted with ground malachite and fired in the embers sometimes leaves copper beads.
- CopperPrill01 (35 %, 3 beads): inspecting the prills teaches **Smelting**.
- MakeBellows now needs Smelting, not AnvilWork: bellows were developed for metallurgy.
- SmeltCopperOre sometimes (10 %) leaves an iron bloom: copper ore's iron-rich gangue, how iron was first noticed. Inspecting the bloom teaches IronSmelting.

New producers:
- BurnPlantAsh -> SodaAsh01 (forest-glass ash)
- BoilHideGlue -> HideGlue01
- SmeltTinOre -> TinIngot01
- MakePolishedAxe takes Basalt01 + Sandstone01 instead of the sourceless Greenstone01

**Result (ValidateData): 136 recipes, 126 reachable, 10 blocked, 0 orphan knowledge.** The 10 lack fauna or flora: feathers, horn, flax, wool (and
cloth from them), and the resin-tap target tag.

Precursor table (authored with `ue.py rows set`):

| Inspect | Teaches (level 1) | Why |
|---|---|---|
| Clay01 | HandBuilding | wet clay holds the shape it is pressed into and hardens in a fire |
| CrudePottery01 | PotMaking | a pot that holds water can stand in the fire |
| CrudePottery01 | StorageVessels | a bigger pot keeps grain and water |
| CrudePottery01 | WheelThrowing | a pot turned as it is shaped comes out round |
| MineralGlaze01 | Glazing | the powder melts into a glassy skin in a hot firing |
| SodaAsh01 | GlassMaking | plant ash melted with sand runs into glass |
| CopperPrill01 | Smelting | copper came out of the green stone where the fire was hottest and smothered |
| CopperIngot01 | AnvilWork | metal is shaped by hammering it on a hard flat stone |
| WroughtIron01 | IronForging | red-hot iron can be hammered into a blade |
| WroughtIron01 | IronArmor | iron plates turn a blow |
| WroughtIron01 | MailMaking | iron drawn into wire can be linked into rings |
| WroughtIron01 | SteelMaking | iron left long in glowing charcoal comes out harder |
| SteelIngot01 | SteelForging | steel takes and keeps an edge |
| TinIngot01 | BronzeCasting | a little tin melted into copper makes a harder metal |
| BronzeIngot01 | BronzeAxe | bronze casts into an axe head |
| BronzeIngot01 | BronzeArmor | bronze beaten thin makes a helmet |
| BronzeIngot01 | SwordFighting | bronze is tough enough for a long blade |
| BronzeIngot01 | ScaleArmor | small bronze plates sewn in rows make armour |
| FlintFlake01 | ScraperUse | a flake's edge scrapes a hide clean |
| FlintFlake01 | MicrolithMaking | tiny flakes can be set in a row in a haft |
| Microlith01 | SickleMaking | microliths set in a curved haft cut grain stalks |
| HandAxe01 | AxeHafting | a stone point bound to a shaft reaches farther |
| StraightBranch01 | FireHardening | a point charred in the embers and scraped is harder |
| StraightBranch01 | BowMaking | a springy straight stave bends and throws back |
| WoodenSpear01 | FishSpearing | a pronged spear holds a slippery fish |
| WoodenSpear01 | AtlatlMaking | a hooked stick throws a spear farther |
| Reeds01 | TrapFishing | woven reeds funnel fish into a trap |
| Reeds01 | BasketWeaving | reeds woven over and under make a basket |
| SimpleBow01 | Fletching | a light shaft with feathers flies straight from a bow |
| SimpleBow01 | CompositeBow | horn, wood and sinew glued together cast harder |
| FireDrill01 | BowDrill | a cord wrapped round the drill spins it faster |
| Branch01 | HandDrill | a dry stick twirled hard in a notch smokes |
| AnimalHide01 | BrainTanning | brains worked into a hide keep it soft |
| TannedLeather01 | LeatherArmor | thick leather layers soften a blow |
| TannedLeather01 | LeatherClothing | leather can be cut and sewn to fit |
| Log01 | CanoeMaking | a log hollowed by fire and adze floats a person |
| PottersWheel01 | ChariotBuilding | a wheel on an axle carries a load |
| Stone01 | DeadfallMaking | a heavy stone propped on a trigger stick falls on what disturbs it |
| Cordage01 | SnareSetting | a noose on a bent sapling catches small game |
| Cordage01 | ThreadSpinning | fibre twisted finer makes thread |
| Cordage01 | LoomWeaving | threads held taut on a frame can be woven |
| Plank01 | ShieldUse | boards joined and held up stop a blow |
| DiggingStick01 | HoeFarming | a blade set across a handle breaks soil faster |
| Basalt01 | GrainGrinding | seeds rubbed between hard stones turn to flour |
| Basalt01 | GroundStone | hard stone ground on sandstone takes a polished edge |
| WoolFleece01 | WoolSpinning | fleece drawn out and twisted makes yarn |

Two causes accounted for nearly all of the original blocking. Both are content/design problems, not code bugs.

## 1. Circular discovery (49 knowledge ids → most of the 88)

A recipe with `bRequiresDiscovery` is hidden until the player has its `DiscoveryKnowledgeId` at the required level.
Knowledge comes from inspecting items. For **49** of those knowledge ids the only item that grants it is:

* **the recipe's own output (32)**. To learn to make a scraper you must inspect a scraper; same for the simple bow,
  crude pottery, cooking pot, storage jar, basket, microliths, every bronze/iron/steel item, and more.
* **an item nobody can make or find (17)**, e.g. `HandDrill` from `HandDrillSpindle01`, `FireHardening` from
  `FireHardenedSpear01`. Before the merge `ScraperUse`, `BowDrill` and `SnareSetting` were granted only by unmakeable
  twin rows (`SimpleScraper01`, `BowDrillKit01`, `SnareWire01`); the merge moved those grants onto the craftable ids,
  so they are now own-output cases. `CopperWorking` now has a source: the copper deposit yields `NativeCopper01`.

So unless a survivor teaches it, the player can never discover these recipes. The pillar in CLAUDE.md says the
opposite: *"No hard locks; player has 'genetic memory' allowing attempts at any tech, but practical prerequisites
make skipping difficult."* The charter (`MO57_Fable5_Charter.md`) lists the discovery fantasy as "only half-wired".

| Knowledge | Granted only by | Unlocks | Kind |
|---|---|---|---|
| ScraperUse | StoneScraper01 (was SimpleScraper01) | ShapeScraper | own output (since the merge) |
| HandBuilding | CrudePottery01 | ShapeCrudePottery, MakeClayBowl | own output |
| BowMaking | SimpleBow01 | CraftSimpleBow | own output |
| HandDrill | HandDrillSpindle01 | MakeFireDrill | item nobody can make |
| BowDrill | BowDrill01 (was BowDrillKit01) | MakeBowDrill | own output (since the merge) |
| FireHardening | FireHardenedSpear01 | HaftSpear | item nobody can make |
| AxeHafting | HaftedAxe01 | HaftFlintSpear | item nobody can make |
| MicrolithMaking | Microlith01 | KnapMicroliths | own output |
| SnareSetting | Snare01 (was SnareWire01) | MakeSnare | own output (since the merge) |
| BasketWeaving | WovenBasket01 | WeaveBasket | own output |
| PotMaking / StorageVessels | CookingPot01 / StorageJar01 | MakeCookingPot / MakeStorageJar | own output |
| …34 more (bronze, iron, steel, leather, loom, glass, armour) | | | 24 own output, 10 unmakeable |

The full list is printed by `MO.Test.ValidateData` (`Blocked recipe '…': discovery: … is never reached`).

### Design fork (Wes) — how does a player first learn a technique?

| Option | What changes | Cost | Fits the pillar? |
|---|---|---|---|
| **A. Precursor grants** ("remembering" when handling the materials) | Move each grant one step earlier: inspecting `SharpFlake01` teaches ScraperUse, `Clay01` teaches HandBuilding, `StraightBranch01` + `Cordage01` teach BowMaking, … | Data only (`ue.py rows set`), ~49 rows | Yes, reads as genetic memory |
| **B. Skill-level discovery** | Fill `DiscoverySkillLevel` so practising the parent skill reveals the recipe (Stoneworking 2 → ShapeScraper) | Data only; the engine path already exists (`IsRecipeAvailable`) | Partly: progression stays, but no "remembering" moment |
| **C. Attempt anything** | An undiscovered recipe can still be attempted at a penalty (×3 time, poor quality, chance to waste materials); a success grants the knowledge | Code: crafting availability + UI state "you can attempt this" | Yes, this is the pillar as written |

**Recommendation: C as the rule, A as the fast path.** C makes every recipe reachable (no hard locks) and makes
discovery a learning curve rather than a gate. A gives the "I remember how to do this" moment when the player
handles the right materials. B alone keeps recipes invisible until a grind and does not match the lore. Nothing has
been changed; this needs your call.

## 2. Raw materials with no producer (25 orphan items)

Items some recipe consumes that nothing in the world produces:

* **Raw materials the world should yield**: `Sand01` (blocks all pottery), `Water01` (blocks boiling and every
  brew), `StraightBranch01`, `Hardwood01`, `Softwood01` (block the workbench, loom, fire drill and smith hammer),
  `DriedGrass01`, `Reeds01`, `BirchBark01`, `Greenstone01`, `Sandstone01`, `NativeCopper01`, `FlaxFiber01`,
  `WoolFleece01`, `SodaAsh01`, `MineralGlaze01`.
* **Animal parts the carcass should give**: `AnimalBladder01`, `AnimalBrain01`, `Horn01`, `Feathers01` (needs a bird).
* **Intermediates with no recipe**: `ArrowShaft01`, `BowStave01`, `HideGlue01`, `Plank01`, `LargeLog01`,
  `TinIngot01` (no tin smelting recipe).

Most have an obvious realistic source, and some are already designed elsewhere: sand and clay are dig/spoil
material in the terraform excavation design; water comes from a container at a lake. Each needs a placement
decision (which resource, biome or carcass gives it), so they are listed here rather than invented.

## How to re-check

```
python Tools/ue.py pie begin
python Tools/ue.py test --suite ValidateData
```
`Data:Sources` passes when no recipe is blocked. Root causes (orphan items and orphan knowledge) are logged first,
then each blocked recipe with the first gate that stops it.
