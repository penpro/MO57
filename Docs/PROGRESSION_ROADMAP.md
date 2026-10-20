# Progression roadmap (design authority)

> **What this is:** the master document for MO57's technology and material progression. **It dictates the design; the
> DataTables implement it.** Created 2026-10-17 at Wes's request: "we need a master progression document ... I think we started off
> a bit blind and kept having to adjust, we're much further along now so we need a proper roadmap that dictates design."
> **Status markers:** **BUILT** (in the live data, verified by `MO.Test.ValidateData` / `MO.Test.ProgressionMap`), **APPROVED**
> (Wes decided; not built yet), **PROPOSED** (drafted from research; needs Wes), **GAP** (missing, no design yet).
> **Built on:** Wes's binding calls (`Docs/DECISIONS.md`), the research in `Docs/Technology_History.md` (heat ladder, materials
> ladder, mining), the February 9-era plan (`Docs/Archive/TechProgressionPlan.md`), the Oct 14-17 reachability passes
> (`Docs/Archive/Progression_Reachability_2026-10-14.md`, `Docs/Archive/Progression_FreshLook_2026-10-15.md`), and the live
> progression map of 2026-10-17 04:22 (`Saved/Logs/progression_map.json`, 126 of 136 recipes reachable).
> **How the code does it:** `Docs/Crafting_And_Progression.md` (recipes, stations, tool gates, discovery, item actions).

## 1. Rules that every progression change follows

1. **This document first.** A new item, recipe, station, knowledge id or producer is added here (era, why, how it is learned) in
   the same commit as the DataTable change, or before it. A DataTable change with no roadmap line is a defect. The DTs are edited
   live with `python Tools/ue.py rows set` (readback-verified); `ValidateData` and `ProgressionMap` must stay green.
2. **No hard locks; practical prerequisites** (the "genetic memory" pillar, `Docs/VISION.md`): a colonist may attempt any
   technique, but can't smelt without a furnace, or build a furnace without the tools and materials.
3. **Raw material is never the hard part; HEAT is** (Wes, 2026-10-17: "they should be able to collect pretty much any ore, the actual
   hurdle with iron is having fire hot enough, so smelting and forging technology"). Any ore can be mined with stone tools; what
   gates each metal is the fire's temperature, fuel, air supply and skill. Hard rock costs TIME, not a tool gate.
4. **Discovery is behind inspection** (Wes, 2026-10-17: "discovery should be locked behind 'inspect' if nothing else opens up to
   it, somewhere in the clay tech line humans would discover leftover metals etc, just make it realistic"). Every discovery
   knowledge has a realistic **precursor** a person would have learned it from (§6); a recipe's own output may teach more
   (up to level 3), but never be the only way in.
5. **Placement is the truth.** A resource exists only if a biome species or a PCG resource spawner places it, a carcass yields it,
   the geology yields it, or ground cover gives it (`FMOItemSourceAnalysis` counts only those).
6. **One id per real-world thing.** Retarget consumers to the produced id and delete the orphan row; never two rows for one thing.
7. **Start with nothing** (Wes, 2026-10-15: the starting kit "will ultimately be nothing"): the spine below is designed from bare
   hands. The current kit is a test tool.
8. **Every world thing gets an action** (Wes, 2026-10-15); plain cover defaults to Forage.
9. **Realism over convenience** (`Docs/ENGINEERING_PRINCIPLES.md` #11): real durations, real producers, real heat; tedium is
   batched or delegated to pawns, never skipped.
10. **Time gates come from real durations and skill, not artificial timers** (February plan principle; still in force).

## 2. The spine at a glance

| Era | Theme | Heat available | Gate to the next era | Status |
|---|---|---|---|---|
| 0. First hour | water, food, shelter, a fire | open fire (700 C) | a fire you can make | BUILT |
| 1. Stone and fibre (Paleolithic) | knapped stone, cordage, hafting, hides | open fire | wood and leather processing | BUILT |
| 2. Wood, hide, clay (Late Paleolithic / Mesolithic) | planks, bows, tanned leather, pottery, traps, canoe | open fire / pit firing (900 C, PROPOSED station) | a kiln and charcoal | BUILT (pottery fires in the campfire today) |
| 3. Settlement (Neolithic) | cloth, farming, storage, permanent buildings | kilns (1,000-1,100 C, PROPOSED) | fibre crops / animals; lime | GAP (no crops, no domestic animals, no flax/wool) |
| 4. Copper (Chalcolithic) | native copper, malachite prills, smelting, casting | charcoal + bellows (1,150-1,250 C) | tin for bronze | BUILT (gates PROPOSED) |
| 5. Bronze | alloying, cast tools and weapons | crucible hearth (1,100+ C) | iron bloom + forging skill | BUILT |
| 6. Iron | bloomery, wrought iron, forging | bloomery (1,250 C) + smith's forge (1,300 C) | steel know-how | BUILT |
| 7. Steel and later | carburizing, quenching; cast iron later | forge; blast furnace (1,400+ C, later) | (DLC eras) | BUILT (steel); later GAP |

The February plan's time budget (~1,000 hours single-player: Early Paleolithic 0-50 h ... Iron Age 950-1,000 h) is the last
stated intent; it has not been re-approved since and the current content is far shorter. **Needs Wes** (DECISIONS F16).

## 3. Eras in detail

Recipe and item ids are the live DataTable ids. "T<n>" is the tier in the live progression map (breadth-first steps from the bare
world with today's gates).

### Era 0: the first hour (BUILT)
- **Water:** drink from lakes / rivers / the sea (salt dehydrates; untreated water can make you ill), fill a bark container,
  boil with heated stones (20 min) or in a pot over the fire (12 min). `Docs/Water_And_Illness.md`.
- **Fire:** campfire (BuildCampfire, T1) on deadwood; friction fire (hand drill: a hazel rod + a dry branch; later the bow drill);
  carry fire with a torch (MakeTorch, T2). Fuel: Branch01, Firewood01, Charcoal01, Stick01 (green wood burns once laid, 32a87e46; it is refused only as the drill).
- **Food:** forage roots / mushrooms / berries / nuts; deer carcass parts (RawMeat01 -> CookMeat).
- **Shelter:** BuildLeanTo, stick walls / floors / roofs (T1).
- Producers (T0): ground pickups (sticks, bark, stones), cover gathering (dry grass, reeds, fibre, straight hazel rods,
  deadwood, tinder), digging (roots, mushrooms, river cobbles, flint, sand on strands, clay in lowland), carcass parts.

### Era 1: stone and fibre (BUILT)
- Knapping: SelectHammerstone / GatherHammerstones -> KnapFlintFlakes, KnapFlintKnife, KnapBifacialHandaxe (T1), StrikeFlakes,
  ShapeChopper, KnapMicroliths (T2), ShapeScraper (T3). Ground stone: MakeGrindingStone, MakePolishedAxe (basalt + sandstone).
- Fibre: StripBark / ShredBark / HarvestBark, TwistCordage, WeaveBasket (T1), WeaveFishTrap, MakeSpindle (T2).
- Hafting and weapons: SelectClub, ShapeDiggingStick (T1); HaftFlintSpear (T2); HaftSpear, CraftSimpleBow (T3); CraftAtlatl,
  MakeFishSpear (T4). **Arrows are blocked** (Feathers01: no birds) -- GAP.
- Hides and medicine: ScrapeHide, MakeBoneNeedle (T1); BrainTanHide, BarkTanHide, AssembleSutureKit, MakeSplint,
  MakeTourniquet, BrewAntiseptic / Painkiller / Antiparasitic, RenderBurnSalve (T2).
- Pitch: DistillBarkTar (Neanderthal birch-bark tar; spruce resin is not in the world). **HarvestSap is blocked** (no resource
  carries the Gives_Resin01 tag) -- GAP.

### Era 2: wood, hide and clay (BUILT, heat gates PROPOSED)
- Wood: ChopDownTree, FellWholeTrunk (alder), RivePlanks (axe + hammer), SplitFirewood, CarveBowStave (hardwood billet),
  ShaveArrowShafts; BuildWorkbench, CarveDugoutCanoe, MakeWoodenShield (T3).
- Leather: BarkTanHide (scraped hide + bark + water, 30 min) and BrainTanHide -> TannedLeather01; MakeSimplePouch, MakeQuiver,
  MakeBlanket, leather clothing and armour (T3).
- Clay: ShapeCrudePottery, MakeClayBowl (T1), MakeCookingPot, MakeStorageJar (T2), MakePottersWheel + ThrowWheelPot (T3).
  Temper: Sand01 (strands), clay: subsoil in lowland dig tables. **PROPOSED:** pottery needs >= 600 C (a campfire can, unreliably);
  a **pit kiln** (900 C) is the reliable step -- the station does not exist (GAP).
- Charcoal: BurnCharcoal (T2). Traps: MakeSnare, BuildDeadfall (T2).
- Fire-hardening, the bow drill (MakeBowDrill, T3).

### Era 3: settlement (mostly GAP)
- Built: BuildLoom / MakeSimpleLoom (T2), MakeHoe (T2), MakeSickle (T3), BuildSchool (T1), BuildBasketContainer01, storage jars.
- **GAP: agriculture** -- no seeds, crops or fields exist, though the hoe and sickle do (February plan Era 5 "Domestic Plants").
- **GAP: fibre crops and animals** -- FlaxFiber01 (flax / nettle), WoolFleece01 (sheep) have no producer, so SpinFlaxThread,
  SpinWoolThread, WeaveCloth, SewClothTunic and MakeBandage (WovenCloth01) are blocked. Domestic animals also give horn
  (MakeCompositeBow is blocked) and, with birds, feathers.
- **GAP: lime** -- burning limestone above 900 C (an updraft kiln) gives quicklime for mortar, plaster and whitewash; limestone is
  the commonest bedrock and the caves are in it (`Docs/Technology_History.md`).
- **PROPOSED station:** updraft kiln (1,000-1,100 C): fired ware, lime, crucible copper with a draught.

### Era 4: copper (BUILT; heat gates PROPOSED)
- Native copper worked cold: HammerNativeCopper -> CopperBead01 (T2; copper deposits yield NativeCopper01). Annealing (heat +
  hammer) is the first heat treatment.
- **How smelting is discovered (APPROVED + BUILT, Oct 17):** PaintPotteryMalachite (a crude pot painted with ground malachite,
  fired at the campfire) sometimes leaves CopperPrill01 (35 %, 3 beads); inspecting the prills teaches **Smelting**.
- MakeBellows needs Smelting (bellows were developed for metallurgy) -> BuildForge, BuildBloomery (T4).
- SmeltCopperOre, CastCopperAwl (T4). SmeltCopperOre sometimes (10 %) leaves an IronBloom01 (copper ore's iron-rich gangue: how
  iron was first noticed); inspecting it teaches IronSmelting.
- Glaze and glass: GrindGalenaGlaze -> MineralGlaze01 (medieval lead glaze), GlazePottery (T4); BurnPlantAsh -> SodaAsh01,
  MakeGlass (T4), ShapeGlassBeads (T5).

### Era 5: bronze (BUILT)
- SmeltTinOre -> TinIngot01 (T4; tin ore from granite veins; placers in rivers are a GAP); AlloyBronze (T5); ForgeBronze* (axe,
  knife, spearhead, sword, helmet, shield, greaves), MakeScaleArmor (T6). Bronze goods are CAST, then cold-worked and annealed.

### Era 6: iron (BUILT; heat gates PROPOSED)
- Ores: hematite in limestone fissures (cave walls, outcrops), bog iron dug in wetlands (Rainforest dig table, 12 %), magnetite
  (not modelled). IronOreDeposit takes a stone maul (0.5).
- SmeltIronOre (T5) -> IronBloom01; **the bloom is not iron yet**: RefineIronBloom (repeated forging at welding heat) ->
  WroughtIron01 (T5). MakeSmithHammer (T5); ForgeIron* (axe, spear, sword, helmet, shield, arrowheads, nails), ForgeHorseshoe,
  ForgeSmithTongs, MakeAnvil, MakeChainMail (T6).
- Coal: fuel for heating, lime and smithing; NOT a charcoal substitute for iron smelting until coke (sulphur spoils iron).

### Era 7: steel and later
- CarburizeSteel (charcoal bed ~900-950 C, quench), ForgeSteelSword (T7). BUILT.
- Later (GAP / DLC): cast iron + blast furnace (water-powered bellows), coke, crucible steel (`Docs/VISION.md` DLC pipeline:
  Medieval -> Industrial -> Modern -> Sci-fi).

### Realism checks on existing content (flagged 2026-10-17)
- **BuildChariot (T6) and ForgeHorseshoe** exist with no draft animals (the only fauna are deer and wolf). Keep, gate behind
  domestic horses, or cut: **Needs Wes** (DECISIONS F17).
- **MakeBandage** needs woven cloth (blocked): a realistic early bandage is plant fibre, moss or hide strips. PROPOSED to add an
  early dressing before cloth exists, so first aid does not wait for the loom.

## 4. The heat ladder (PROPOSED values; fields exist)

Codex's crafting batch C added the fields: a station building's `ProvidedProcessTemperatureCelsius`, the runtime
`AMOCraftingStationActor::AchievableProcessTemperatureCelsius` (fuel and air may lower it) and a recipe's
`MinimumProcessTemperatureCelsius` (0 = none). The values below are drafted from physics (`Docs/Technology_History.md`); **none is
authored yet** -- they wait on Wes and on Codex's fuel-to-temperature rule (DECISIONS F10).

| Station | Fuel / air | Reaches | Opens | Exists? |
|---|---|---|---|---|
| Campfire | wood | 700 C | cooking, fire-hardening, annealing native copper, crude earthenware (unreliable) | yes (BuildCampfire) |
| Pit kiln | wood | 900 C | reliable pottery; charcoal clamps (separate) | GAP |
| Updraft kiln | wood / charcoal | 1,000-1,100 C | fired ware, lime (900+), crucible copper with a draught | GAP |
| Crucible hearth | charcoal + blowpipes / bellows | 1,150 / 1,250 C | copper smelting, casting, bronze | folded into BuildForge today |
| Bloomery | charcoal + bellows | 1,250 C | iron bloom | yes (BuildBloomery) |
| Smith's forge | charcoal (coal later) + bellows | 1,300 C at the hearth | consolidating the bloom, forging, carburizing | yes (BuildForge) |
| Blast furnace | charcoal (coke later) + water-powered bellows | 1,400+ C | cast iron (late) | GAP |

| Recipe | Minimum C | Why |
|---|---|---|
| CookMeat | 150 | searing |
| ShapeCrudePottery, MakeCookingPot, MakeStorageJar, MakeClayBowl | 600 | clay becomes ceramic from ~550-600 |
| GlazePottery | 950 | low-fire glazes mature ~900-1,000 |
| SmeltCopperOre, CastCopperAwl, AlloyBronze | 1,100 | copper must melt (1,085) |
| ForgeBronze*, MakeScaleArmor | 1,050 | cast, then cold-worked and annealed |
| MakeGlass | 1,050 | soda-lime glass melts ~1,050-1,100 |
| ShapeGlassBeads | 900 | glass softens enough to wind |
| SmeltIronOre | 1,200 | bloomery slag must run |
| RefineIronBloom | 1,150 | forge welding heat |
| CarburizeSteel | 900 | carburizing in a charcoal bed |
| ForgeIron*, ForgeSteelSword | 1,000 | forging heat |

With these a campfire cooks and fires crude earthenware but melts no metal; wood in a forge should stay under ~1,000 C (no iron).

## 5. Materials and producers (where every raw material comes from)

| Material | Producer (BUILT unless marked) |
|---|---|
| Water, clean water | drink / fill at water; boil in a container (liquid MEASURES: Water01 / CleanWater01 are 250 ml drawn from carried containers) |
| Sticks, bark, stones | ground pickups (PCG item clutter) |
| Deadwood (Branch01), firewood, tinder (punk wood) | break up old logs / standing dead trees; Gather Deadwood on alder and locust |
| Straight rods | Cut Straight Rods on hazel; hazel shoots in cover |
| Dry grass, plant fibre, reeds | gather cover (grass; reeds in the water-edge band; bracken) |
| Logs, large logs, hardwood | ChopDownTree (locust, apple), Fell Whole Trunk (alder) |
| Planks, firewood, staves, shafts | RivePlanks, SplitFirewood, CarveBowStave, ShaveArrowShafts |
| Clay, sand | digging: subsoil clay in lowland dig tables, sand on strands (`FMOBiomeDefinitionRow::DigYields`) |
| Stone types, coal, ores | the geology (`FMOGeologyYield`): limestone, sandstone, granite, basalt, coal, hematite, copper, tin, flint, galena |
| Bog iron | Rainforest dig table (12 %, Foraging 2) |
| Native copper | copper deposits (biome-placed; to be replaced by geology outcrops) |
| Meat, hide, bone, sinew, brain, bladder | deer carcass |
| Resin | DistillBarkTar (birch-bark tar at a campfire) |
| Soda ash | BurnPlantAsh |
| Hide glue | BoilHideGlue |
| **GAP** | Feathers01 (birds), Horn01 (domestic animals / game with horns), FlaxFiber01 (flax / nettle), WoolFleece01 (sheep), tree sap (resin tap target), tin placers |
| **Unplaced on paper** | resource rows AppleTree, ClayDeposit, CommonSpruce, ReedBed, TallGrass exist but nothing places them (place or delete) |

## 6. How a player learns a technique (BUILT, Oct 17)

Each discovery knowledge is taught at level 1 by inspecting the material a person would really have learned it from; inspecting the
finished product still teaches up to 3. A new discovery = a realistic precursor here + the row's inspection grant. Table authored
with `ue.py rows set` (d6a3af69):

Trees can be studied too (Oct 18 2026): a resource node's Inspect studies what its harvest actions yield, so a tree teaches its bark and sticks
(`UMOHarvestSubsystem::GetStudyableItemIds`), and the tutorial "Study the Forest" -> "Harvest Bark" can be done.

| Inspect | Teaches (level 1) | Why |
|---|---|---|
| Clay01 | HandBuilding | wet clay holds the shape it is pressed into and hardens in a fire |
| CrudePottery01 | PotMaking, StorageVessels, WheelThrowing | a pot that holds water can stand in the fire; a bigger pot keeps grain; a pot turned as it is shaped comes out round |
| MineralGlaze01 | Glazing | the powder melts into a glassy skin in a hot firing |
| SodaAsh01 | GlassMaking | plant ash melted with sand runs into glass |
| CopperPrill01 | Smelting | copper came out of the green stone where the fire was hottest and smothered |
| CopperIngot01 | AnvilWork | metal is shaped by hammering it on a hard flat stone |
| WroughtIron01 | IronForging, IronArmor, MailMaking, SteelMaking | red-hot iron can be hammered; plates turn a blow; wire links into rings; iron left in glowing charcoal comes out harder |
| SteelIngot01 | SteelForging | steel takes and keeps an edge |
| TinIngot01 | BronzeCasting | a little tin melted into copper makes a harder metal |
| BronzeIngot01 | BronzeAxe, BronzeArmor, SwordFighting, ScaleArmor | bronze casts into an axe head; beaten thin makes a helmet; tough enough for a long blade; small plates sewn in rows |
| FlintFlake01 | ScraperUse, MicrolithMaking | a flake's edge scrapes a hide clean; tiny flakes set in a row in a haft |
| Microlith01 | SickleMaking | microliths set in a curved haft cut grain stalks |
| HandAxe01 | AxeHafting | a stone point bound to a shaft reaches farther |
| StraightBranch01 | FireHardening, BowMaking | a point charred and scraped is harder; a springy stave bends and throws back |
| WoodenSpear01 | FishSpearing, AtlatlMaking | a pronged spear holds a fish; a hooked stick throws a spear farther |
| Reeds01 | TrapFishing, BasketWeaving | woven reeds funnel fish; reeds woven over and under make a basket |
| SimpleBow01 | Fletching, CompositeBow | a light shaft with feathers flies straight; horn, wood and sinew glued together cast harder |
| FireDrill01 | BowDrill | a cord wrapped round the drill spins it faster |
| Branch01 | HandDrill | a dry stick twirled hard in a notch smokes |
| Stick01 | BarkFiber | a green stick's bark lifts off in long strips under a thumbnail (Wes, Oct 18: you had to find loose bark before you could strip a stick) |
| AnimalHide01 | BrainTanning | brains worked into a hide keep it soft |
| TannedLeather01 | LeatherArmor, LeatherClothing | thick leather softens a blow; leather can be cut and sewn |
| Log01 | CanoeMaking | a log hollowed by fire and adze floats a person |
| PottersWheel01 | ChariotBuilding | a wheel on an axle carries a load |
| Stone01 | DeadfallMaking | a heavy stone propped on a trigger stick falls |
| Cordage01 | SnareSetting, ThreadSpinning, LoomWeaving | a noose on a bent sapling; fibre twisted finer; threads held taut can be woven |
| Plank01 | ShieldUse | boards joined and held up stop a blow |
| DiggingStick01 | HoeFarming | a blade across a handle breaks soil faster |
| Basalt01 | GrainGrinding, GroundStone | seeds rubbed between hard stones; hard stone ground on sandstone takes an edge |
| WoolFleece01 | WoolSpinning | fleece drawn out and twisted makes yarn |

"Attempt anything" (an undiscovered recipe can be tried at a penalty, success teaches it: option C of Oct 14) was recommended as
the rule; Wes chose inspection precursors (option A) on Oct 17 and has not ruled on C (DECISIONS F18).

## 7. Open design questions (all in DECISIONS "Needs Wes")

- F10 heat-ladder values and the missing kiln stations;
- F16 the intended play time per era / overall pacing (February intent ~1,000 h);
- F17 chariot / horseshoe without draft animals;
- F18 attempt-anything as a rule on top of inspection;
- fauna and flora to add for the GAP materials (birds, sheep, flax/nettle, horned game) and agriculture (crops, fields, seasons).

## 8. How to verify the implementation matches this roadmap

```
python Tools/ue.py pie begin
python Tools/ue.py test --suite ValidateData      # Data:Sources: orphans, blocked recipes, unplaced resources
python Tools/ue.py run "MO.Test.ProgressionMap"    # writes Saved/Logs/progression_map.json (today vs attempt-anything tiers)
```
Why the metal path was blocked before Oct 17 (fixed in d6a3af69): 46 discovery knowledges were taught only by inspecting the recipe's
own output. Smelting was one: smelted output needed a forge, the forge needed bellows, and the bellows needed Smelting. Only 55 of 132
recipes were reachable. Rule since then: every knowledge has an inspect precursor reachable without it (§6).

Update the tier notes in §3 and the GAP lists after each content pass. Last check: 2026-10-17 04:22, 126 of 136 reachable; the 10
blocked are CraftAtlatlDart, FletchArrows, FletchIronArrows (feathers), HarvestSap (no resin tap target), MakeBandage and
SewClothTunic (woven cloth), MakeCompositeBow (horn), SpinFlaxThread (flax), SpinWoolThread (wool), WeaveCloth (wool thread).
