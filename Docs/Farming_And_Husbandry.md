# Farming and animal husbandry

> **Covers:** crops (seed, field, growth, harvest, processing) and domestic animals (capture, pens, taming, breeding, products): the
> design, what exists to build on, the build order and the forks. Wild animals as creatures: `Docs/AI_And_Colony.md` §1. Era and
> recipe placement: `Docs/PROGRESSION_ROADMAP.md` (the design authority; era 3 "Settlement" is where both belong).
> **Current state (2026-10-26):** design only, from Wes's ask (Oct 26: "let's get farming, animal husbandry, and some other animals
> going. We will need horses, pigs, cows, etc plus bears, rabbits, birds, chickens, I'm sure there's more"). Built so far: pig and fox
> as wild species (the pig is the first animal that can become domestic); species are data now, so new animals are cheap once their
> assets exist (DECISIONS F41). Forks F42-F44 decide the crop timing, the domestication model and the first crops.
> **Entry points (to come):** a crop plot (buildable), `DT_Crops`, seed items; a pen (buildable), a creature's ownership / tameness.

## 1. What already exists to build on

- Tools and knowledge: `MakeHoe` (T2), `MakeSickle` (T3), `MicrolithSickle01`, `StoneSickle01`, `PlowShare01`, `DiggingStick01` (inspected:
  `HoeFarming`), `Basalt01` (inspected: `GrainGrinding`), `GrainStore01`, `Porridge01`, storage jars.
- Blocked by the gap: `FlaxFiber01` (no producer) -> SpinFlaxThread, WeaveCloth, SewClothTunic, MakeBandage; `WoolFleece01` (sheep);
  `Horn01` (MakeCompositeBow); `Feathers01` (birds) -- `PROGRESSION_ROADMAP.md` §3 era 3.
- Systems: the building system (plans, staged construction, server-driven progress) for fields and pens; `FMOWorldRules` paces for a
  Growing pace; the game clock (`UMOGameClockSubsystem`) and its saves; `UMOSurvivorJobQueueComponent` for pawns that tend fields and
  feed animals (tedium is delegated, Principle 11); creatures' perception and flee logic; carcass yields.

## 2. Crops (the simulation)

The real sequence, each step a timed action with a real duration (batched where a person would batch it):

1. **Seed.** Wild stands of each crop's wild ancestor grow in its biome (wild barley and emmer on grassland edges, wild flax, wild pea);
   ripe heads are gathered by hand in their season (F44). Seed is an item per crop (`Seeds_<Crop>01`).
2. **Ground.** A plot is cleared and broken: a digging stick (slow), a hoe (faster: `HoeFarming`), later an ard. A **field plot** is a
   buildable (a plan, staged like a building): the ground under it is worked, not a mesh dropped on grass.
3. **Sowing** a plot with seed (so much seed per area: ~150 kg/ha for barley, i.e. a 4 x 4 m plot takes ~250 g).
4. **Growth** on the game clock through stages (sprouted, tillering, heading, ripe, then overripe: grain shatters), its pace the world's
   **Growing** pace (F42). It needs water (rain from the weather director, or carried) and suffers from weeds and frost (later steps).
5. **Harvest** with a sickle (a knife or bare hands slower) -> sheaves; **threshing** (beating) and **winnowing** -> grain + straw +
   chaff; flax is pulled, retted (soaked weeks), broken and scutched -> `FlaxFiber01`.
6. **Storage and food:** grain stores keep (`GrainStore01`, jars); grinding on a quern (`GrainGrinding`) -> flour -> porridge / flatbread.
   Some grain is kept back as next year's seed.

`DT_Crops` (one row per crop): seed item, biome / climate range, sowing season, days to each stage, water need, yield per area, straw,
products. A `FMOCropPlot` state on the plot actor: crop, sown-at game time, stage, water, weeds; advanced on the clock (also while
nobody is near: computed from elapsed game time, like the survival stats), saved with the world, replicated as the stage only.

## 3. Domestic animals (the simulation)

The earliest domestic animals were taken young from the wild and raised among people; generations bred in captivity became docile
(dog from wolf 15,000+ years ago; sheep and goat ~11,000; pig and cattle ~10,500; chicken ~8,000-3,500; horse ~5,500).

1. **Capture.** An adult bolts (the flee logic already does that). The young can be caught: a snare or net, or a hand-raised piglet
   whose sounder was hunted. A caught animal is led on a rope (cordage).
2. **A pen** (buildable: posts and wattle, a stone wall later) holds animals; outside one, an animal not yet tame runs off.
3. **Taming** (F43 A): days of handling and feeding in the pen raise a captured young animal's tameness; adults stay wild (their flight
   distance); animals born in the pen are tame from birth; generations carry docility forward (a herd trait, slowly).
4. **Upkeep:** fodder (grazing inside the pen's area, hay, scraps), water; pawns do it as a job.
5. **Breeding:** a tame female and male in the same pen breed in season; real gestation (pig 114 days, sheep ~150, goat ~150,
   cow ~283, horse ~340) and litter sizes; the young follow their mother.
6. **Products:** meat, hide, bone, fat (all through the carcass system when slaughtered); living products: milk (goat, sheep, cow),
   wool (sheep, shorn yearly -> `WoolFleece01`), eggs and feathers (chicken -> `BirdEgg01`, `Feathers01`), horn, dung (fuel and
   manure for fields), and work: a draft ox or horse pulls an ard or a cart (F17: the chariot / horseshoe follow domestic horses).

A creature gets an owner (the settlement), a tameness (0-1) and a pen; its controller treats its owners as no threat above a
tameness, and the spawner never despawns an owned animal. Persistence: owned animals save with the world like survivors.

## 4. Build order

1. Done (Oct 26): species as data; pig and fox wild; groups of one species.
2. Crops core: `DT_Crops`, seed items, the field plot buildable, sowing, growth on the clock, harvest, save / load, co-op (stage
   replicated); the first crops by F44; wild stands for seed.
3. Processing: threshing, winnowing, grinding into the existing porridge; flax retting -> `FlaxFiber01` (unblocks cloth).
4. Husbandry core: capture and lead a young animal, the pen, tameness, feeding; the pig first (its assets exist).
5. Breeding and living products; pawn jobs for fields and animals.
6. More species as their assets arrive (F41): sheep, goat, cattle, chicken, horse; then bear, rabbit, birds (flight: BACKLOG N-223).

## 5. Open forks (DECISIONS)

- **F41** the animal assets to buy (no free realistic animated ones on Fab).
- **F42** how long crops take (real seasons + a Growing pace; F13 decides the play time).
- **F43** how an animal becomes domestic (captured young tamed in a pen; born tame; generations).
- **F44** the first crops (the founder crops by biome).
