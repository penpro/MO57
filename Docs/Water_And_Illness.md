# Drinking water, salt water and waterborne illness (Oct 15 2026)

Wes, Oct 15: "make it so you can drink from the lakes and rivers but not the ocean, or make it so you can drink from the
ocean but it spikes sodium and causes dehydration etc, make it realistic. Give drinking directly from natural sources a chance
of causing illness, and make a 'first hours' path to being able to boil water ... check online for actual primitive survival
methods to see how humans actually do it instead of game logic."

## What the research says

* **Untreated wild water is a real but modest risk.** The only before/after-trip studies of backcountry campers found
  5.7 % and 24 % infected (Giardia), and fewer than half of the infected became ill. US surveillance (Benedict et al. 2019)
  puts untreated drinking and recreational water at the highest odds of giardiasis among exposures (~4x). Filter makers
  and sceptics agree the per-drink risk is "not especially high"; nobody serious says it is zero.
  ([bucktrack.com summary of the trip studies](https://bucktrack.com/Giardia_Hiking_Water.html),
  [MSR: Understanding Giardia](https://cascadedesigns.com/blogs/msr-gear-guides/water-treatment-101-understanding-giardia))
* **Flowing water is safer than still water; small still pools used by animals are worst.** Hunter-gatherers chose springs
  and fast streams and moved camp to water (secondary sources; ethnographic primary sources not pulled).
* **Course of the illness:** Campylobacter incubates 2-10 days and lasts 2-5; Cryptosporidium ~7 days (2-10), illness about a
  week; Giardia 7-10 days (3-25) and can drag on for weeks. Diarrhoea is the danger: fluid loss and dehydration.
  ([Colorado CDPHE giardiasis](https://cdphe.colorado.gov/dcphr/communicable-disease-manual/giardiasis),
  [cryptosporidiosis](https://cdphe.colorado.gov/dcphr/communicable-disease-manual/cryptosporidiosis))
* **Boiling works:** a rolling boil for 1 minute (3 above 2,000 m) kills bacteria, viruses and protozoa (CDC boil-water
  guidance). It does nothing for salt.
* **Sea water costs water.** It is saltier (~35 g/L, ~600 mmol Na/L) than the kidney can concentrate urine (~1,200 mOsm/kg,
  roughly 300 mmol Na/L of urine), so every litre drunk needs about two litres of urine to clear its sodium: a net LOSS of
  about a litre, plus a sodium spike (hypernatremia: thirst, confusion, then seizures and coma).
* **Boiling without a pot (the first hours):** a bark or hide container can hang over the fire -- the water keeps the
  container below its ignition point ("you can boil in just about anything as long as you take it off the flame pretty
  quickly", Speth) -- or, the classic: **stone boiling**, heating stones in the fire and dropping them into water in a bark
  basket, hide or wooden bowl. Fuel-hungry, slow, and the stones crack (archaeologists find the "fire-cracked rock").
  Stones from a river bed can burst from trapped water.
  ([Wikipedia: Stone boiling](https://en.wikipedia.org/wiki/Stone_boiling),
  [Speth et al., When Did Humans Learn to Boil?](https://paleoanthropology.org/ojs/index.php/paleo/article/view/732))

## The model

**Where the water comes from** is the generator's own knowledge (`FMOWorldGen::ClassifyWaterBody`): sea (the ocean basin),
river, pond (tier-0 basin), lake (sea-level tier-1 basin), upland lake. Each kind has a profile in Project Settings > Water
Sources (`UMOWaterSourceSettings`): salinity (g/L), the chance per litre drunk untreated of a symptomatic illness, and a name.

| Source | Salinity | Illness per litre (untreated) | Why |
|---|---|---|---|
| Sea | 35 g/L | 0.2 % | the salt is the problem, not the bugs |
| River | 0 | 0.5 % | flowing |
| Lake / upland lake | 0 | 1.0 % | still, animals drink and wade |
| Pond | 0 | 2.5 % | small, still, warm, trampled |

Calibration: a typical trip drinks ~10 L untreated; 0.5-1 %/L gives a 5-10 % chance of being ill per trip -- the trip
studies' 5.7-24 % infected, under half symptomatic.

**Drinking** (`UMOSurvivalStatsComponent::IngestWater`, the one intake path for drinkable liquid): fresh water fills the thirst
counter (0.5 points / ml, as items already did) and the metabolism's body water. Sea water: the net water is
`ml - ml x salinity / 21` (two litres of urine per litre of sea water at 35 g/L) -- negative -- so thirst gets WORSE, body water
falls, and sodium jumps (~920 % of a day's need per litre). The illness roll is made at the drink, hidden.

**Reach** (`AMOPlayerController::FindWaterAtCrosshair`, the one query behind the "Pond water" prompt and E): the crosshair finds the water,
the drink happens within `UMOWaterSourceSettings::ReachCm` (250) of the pawn. The shoulder camera meets the water 3-4 m out even when the
pawn stands at the edge, so since Oct 19 a crosshair point out of reach falls back to the nearest water toward it within reach (before, the
prompt showed and E said "Get closer to the water to drink." a step from a pond). The server re-checks reach + 1 m
(`FMOWaterActions::ReachableWaterAt`). Verified: 1 m from a river bank, Drink runs (20 s, 250 ml).

**Illness course** (the condition system, data in DT_Conditions): an incubation with no symptoms, then a self-limiting
illness whose severity rises to a peak and falls. While ill: fluid loss (diarrhoea) through the same intake path with a
negative volume, fatigue, and a moodle ("Stomach bug"). Times are compressed like the survival needs (Wes's 30-minutes-to-
thirsty pacing; the real body runs about 8x slower): incubation 6-24 game hours (real 1-10 days), illness 8-24 game hours
(real 2-7 days). Rest and drinking (clean water) carry you through it; untreated dehydration is what kills.

**The action** is the pawn's, not the UI's (`UMOTimedActionComponent`): the server times it on the game clock, cancels it if the
pawn moves or is hurt, and replicates its progress so the owner sees a progress bar. The same component lets a survivor pawn
drink, and every later timed body action (fill, boil, wash) plugs into it.

## First hours to safe water (phase 2)

1. Strip bark (Tree Bark, by hand) -> **Bark Container** (bark + cordage or split-stick pins; holds ~1 L). Any pliable fresh bark,
   not only birch.
2. Fill it at a river / lake (timed). The contents remember their source (untreated, its illness chance).
3. At a fire: **Boil (stone boiling)** -- heat 4-6 dry stones (not river-bed stones: they can burst) in the fire, drop them in
   with green-stick tongs; ~20-30 min including heating; some stones crack and are lost. Or hang the bark container over coals.
   Boiled water carries no illness chance.
4. Later: fired clay pots (Crude Pottery) boil directly on the fire.

The container needs per-item state (its contents), carried by the inventory entry with durability -- and through world-item drop
and pickup, which today loses durability as well.
