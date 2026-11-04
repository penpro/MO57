# Farming and animal husbandry

> **Covers:** crops (seed, field, growth, harvest, processing) and domestic animals (capture, pens, taming, breeding, products): the
> design, what exists to build on, the build order and the forks. Wild animals as creatures: `Docs/AI_And_Colony.md` §1. Era and
> recipe placement: `Docs/PROGRESSION_ROADMAP.md` (the design authority; era 3 "Settlement" is where both belong).
> **Crop art (2026-11-04):** TRELLIS plants for barley, emmer, flax and pea at three stages each, Wild grain drawn as the barley, the crops'
> own flicker wind (section 2, "Crop art"); emmer / flax / pea wait on their rows (F44); the assets wait on the LFS budget.
> **Current state (2026-10-26):** from Wes's ask (Oct 26: "let's get farming, animal husbandry, and some other animals going. We will
> need horses, pigs, cows, etc plus bears, rabbits, birds, chickens, I'm sure there's more"). **Built:** the crop core (a field plot,
> sowing, growth on the game clock through its stages, harvest, saved, replicated) with one crop, wild grain; pig, fox, boar, rabbit and bear as wild species; goat, sheep, cow and
> chicken built as species (Blueprints and rows, from the Animals Full Pack) but not spawned until F43 decides how they become domestic;
> species are data, so new animals are cheap once their assets exist (F41). **Not yet:** the founder crops (F44), grinding / porridge,
> flax, wild stands to gather seed from, water / weeds / frost, husbandry (F43). Forks F42-F44.
> **Entry points:** `AMOCropPlotActor` (`BP_CropPlot`, recipe `TillFieldPlot`), `FMOCropDefinitionRow` / `FMOCropGrowth` (`MOCropTypes.h`),
> `DT_Crops`, the world's Growing pace (`EMOWorldPace::Growing`). **Dev verbs:** `MO.Test.AgeCrops <Days> [RadiusCm]`, `MO.Test.WorldAction
> list|Sow:<Crop>|Harvest|ClearPlot`. **Tests:** `MOFramework.Crop.*`.

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

### Built (Oct 26)

- A plot is a building (`TillFieldPlot`: a "Dig" action part, 2,700 s, a hoe optional at 3x without), so it is placed, worked, finished,
  saved and replicated like any other. Finished, right-click opens its menu (`IMOWorldActionProvider`): **Sow <crop>** for each crop whose
  seed the pawn carries (the seed is taken when the timed sowing ends), **Harvest** when ripe or overripe (a sickle, or a knife at half
  pace; what is left falls as it shatters: `FMOCropGrowth::HarvestableFraction`), **Pull up** (clears it). One worker at a time.
- The server keeps the crop and the game second it was sown; every 10 s it works out the stage (`FMOCropGrowth::StageAt`: Sown, Sprouted
  for the first fifth after the green shows, Growing, Ripe, Overripe) and the size, and replicates those two (a joiner never needs the
  host's clock). The plants are instanced meshes on the ground under the plot (each traced to the voxel ground), scaled from a fifth of
  full height to full at ripe; since Nov 4 Wild grain shows the TRELLIS barley (see "Crop art" below; before: Megascans grasses).
- The worked ground is a rigid 4 x 4 m slab: each machine lays it on the plane through the voxel ground under its four corners once that
  ground exists (`FitToGround`; a slope steeper than 60 degrees keeps it level), and a plant stands on the slab's top or the ground, whichever
  is higher. Before (first look, Oct 26) the slab stayed level, floated over the downhill side and hid the young plants under it.
- Saved as the days grown (`FMOPersistedBuildingRecord::CropId / CropDaysGrown`): a load picks the season up where it was.
- The world's **Growing season** knob (New Game panel, 0.1-4, default 1 = real seasons) stretches every crop (`GetDaysGrown` divides by it).
- Verified (Oct 26, one game instance, `verify_crops`): a creative world tills a plot at once; with no seed the menu says so; Sow Wild grain
  takes its time and 5 grain; aged 8 / 50 / 102 days it reads Sprouted / Growing / Ripe (screenshots: a dense patch on the slope);
  Harvest needs a sickle (the stone sickle did no tool work until the Oct 26 tool fix) and gives 4 sheaves; threshing one in the creative
  world finished in 0.1 s (6 grain); a second sowing grown 20 days came back from a save and reload at 20.0 days.

### Crop art: TRELLIS plants (Nov 3-4)

Wes picked F55 D ("TRELLIS crops"), then: "download any and all models you want for the best results and if results are questionable
refine things and do A/B testing until we have photorealistic assets that we like" (DECISIONS 2026-11-03). Barley, emmer, flax and pea,
three stages each (the plot's `StageMeshes`: Sprouted, Growing, Ripe), made by one repeatable pipeline, `Tools/crop_art.py`:

1. **Reference photos** (`images`): WanGP on DARKTOWER (a scheduled task: Windows OpenSSH kills a `Start-Process` child when the session
   ends). A/B on barley / flax / pea, two seeds each (`Saved/CropArt/ab1`): FLUX.2 Klein 4B drew lavender for flax and a generic plant for
   pea; Z-Image Turbo drew clean, true flax and pea but wheat for barley and oversized flowers; **Qwen-Image 2512** (Apache 2.0, ~3 min an
   image on the 3090) drew the true crops (two-row barley's long awns, emmer's red-brown awned ears, flax's 2 cm flowers and bolls, pea's
   stipules, tendrils and dry pods) and its photos survive TRELLIS best. The prompts in `CROPS` are botanical on purpose ("ripe barley"
   alone draws wheat); a negative ("no ears") was ignored until the prompt said what IS there ("leaves only, like young grass").
   FLUX.2 Klein 9B / dev are not used: their licence's 4(a) bars "data produced by the FLUX Model" from commercial use.
2. **Soil out** (`clean`, `Tools/crop_image_clean_blender.py`): Qwen stands every plant in a block of soil and roots whatever the prompt
   says, and TRELLIS builds it. The block is found by shape (solid dark rows, not thin dark stems) and painted the backdrop colour.
3. **TRELLIS.2** (`trellis --preset fine`): the template's remesh (a 768 grid, "solid" sign mode, 20 smoothing passes, small parts under
   1 % dropped) erased every awn; "fine" remeshes at 1536 in "udf" mode with 3 passes and keeps parts down to 0.05 %: awns and tendrils
   survive (~1.5M faces, ~8 min a plant).
4. **Prep** (`prep`): `glb_item_prep_blender.py` (orient keep, the crop's height, **40k triangles**: 150k looks the same; 3k tears the thin
   leaf shells apart) then `Tools/crop_finish_blender.py`: the bottom 2 % cut (soil left over), the Megascans vertex-colour convention
   baked in (A = height, the primary bend; R = blade gradient; G / B = smooth noise, the phase), the ORT mask built from TRELLIS's ORM
   (roughness remapped to 0.6-0.9: TRELLIS's is glossy), and the base colour matched to the photo (one gain per channel + one saturation
   scale: matching each channel's spread made rainbow heads). Previews use a copy without vertex colours: glTF multiplies the base colour
   by COLOR_0, Unreal does not.
5. **Import** (`import`, `Content/Python/crop_mesh_prepare.py`): each plant is an instance of the world grass's master,
   `/Game/Materials/FabOverrides/M_MS_Foliage` (two-sided foliage, the world's wind through `QMF_WindAnimation`, precision-safe far from
   the origin: `Docs/PCG.md` section 9), with the stand-in grass's settings, no twig mask, a WPO fade from 40 m and a camera fade of
   15-60 cm (the grass's 40-160 cm speckled heads 1-2 m away), and the leaf transmission capped at strength 0.5 like every foliage
   instance (`fix_foliage_transmission.py`, test `FoliageTransmissionCapped`: the copied grass values were 1.0). Nanite on, no collision.
6. **Wind** (Wes, Nov 4: "all the crops move WAY too much ... about 20% as much and a little faster ... little ripples"; then "small plants
   don't move like that ... non-uniform flickers here and there, not a constant slow swaying movement"): the grass master's speed is the
   world's (`Wind Speed Plants` in its MPC, shared by every plant), and its per-material knobs only scaled the slow sway (`motion_probe`:
   under 3 % of the motion above 0.5 Hz at any strength). So the crops have their own copy of the master, `/Game/ImageTo3D/Crops/
   M_MO_CropFoliage` (`Content/Python/crop_wind_material.py`): the same shading, its offset a fast flutter (2.0 and 3.3 Hz, phased per plant
   and per part) gated by a drifting noise field (only the plants in its patches move), times A^2 (heads, not stems) and the world's plant
   wind strength. World position reduced before time is added, Time wrapping on whole cycles (`Docs/PCG.md` section 9). Old against new:
   `Saved/CropTest/wind/wind_old_vs_new.mp4` (Wes judges it; the grass gets the same once the crops look right).
Checked in a real world with `Tools/check_crop_plants.py` (`grow`: one crop sown and aged through its stages; `ab`: candidates side by
side on fallow plots; `perf`: the GPU cost of a row of plots). Nov 4: `grow` 4/4 (Wild grain sown through the plot's menu and drawn as the
barley at Sprouted, Growing and Ripe); every stage at its in-game height in `Saved/CropTest/ab_capped/`. At 4 m each reads as its crop;
from ~1 m the ripe heads smear (TRELLIS's limit), flax flowers read pale and ripe flax too orange. Frame cost, first bench (Nov 4, an
empty machine, a -game instance at 1600x900, 4090): a 6-plot field of ripe barley (1,176 plants, 40k-triangle Nanite, the flicker wind) in
view, the whole frame's GPU at 7.5-10.3 ms. Its baseline was wrong (the plants HIDDEN opened the view to a costlier background: 17-31 ms),
so `perf` now swaps the same instances to the stand-in grass; the crop's own cost against the grass is still to measure. **The assets are local only:** the LFS budget refuses `.uasset` pushes (BACKLOG "Waiting on
Wes"); `crop_art.py` and `Docs/RowPatches/2026-11-04-crops-trellis-barley-DT_Crops.json` remake them.

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

1. Done (Oct 26): species as data; pig and fox wild; groups of one species. Then (N-221) boar, rabbit and bear wild; goat, sheep, cow
   and chicken built, with carcass yields (fleece, horn, feathers), and left out of the spawn lists (F43).
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
