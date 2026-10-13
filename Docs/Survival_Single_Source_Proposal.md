# Survival state: one source of truth (H10) — proposal for Wes

Status: **decision needed**, prepared Oct 13 2026. One bug was fixed underneath it and is described first. Nothing about the HUD's pacing has changed.

## What the code does today
Two models of the same body run side by side:

| | `UMOSurvivalStatsComponent` (legacy) | `UMOMetabolismComponent` (+ vitals / mental state) |
|---|---|---|
| State | Hunger / Thirst / Energy / Stamina, 0-100 counters, replicated | glycogen (500 g), hydration %, protein balance, vitamins, minerals, digestion queue, body composition |
| Rates | fixed decay per **real** second: Hunger 0.5, Thirst 0.8, Energy 0.2 (no clock TimeScale). Critical thirst in about 2 minutes | physiological: BMR from body composition, activity (MET), digestion over ~1 h per meal, about 2500 ml of water a day |
| Read by | the HUD moodles (`MOCharacterUIController::RefreshSurvivalStatMoodle`), `MOPlayerStatusWidget`, the stamina gate in `AMOCharacter` (sprint) | the **Status panel** (`MOStatusPanel`: its Thirst row is `HydrationLevel`), vitals (glucose, dehydration -> HR / BP / temperature), mental state (confusion) |
| Fed by | `ConsumeItem` -> `ApplyNutrition` (100 kcal = 10 Hunger, 100 ml = 50 Thirst) | `ConsumeFood` -- **until today only the colony manager's villagers** |

So the player sees two different "Thirst" values (moodle vs. Status panel), and the moodles run on arcade clocks the realism pillar names as the anti-example ("hunger decreases 1/sec").

**Fixed today (not a pacing change):** the player's eat action (`UMOSurvivalStatsComponent::ConsumeItem`) now also calls `UMOMetabolismComponent::ConsumeFood`. Until then, the player's physiology received NOTHING the player ate or drank. Glucose, hydration and the cascade into vitals and mental state ran down however much the player ate; only the moodle counters moved. The AI path already fed the metabolism only, so nothing is applied twice in one model. Drinking from a lake or river still has no path into either model (`UMOMetabolismComponent::DrinkWater` has no gameplay caller); see "Also open" below.

## Proposal (option A from Oct 12, made concrete)
Make the metabolism the only state; keep the legacy component as a thin presentation adapter, so its consumers (HUD, status widget, sprint gate, tests, cheats) keep their API while it stops having state of its own.

| Legacy stat | Derived from (metabolism / vitals) | Notes |
|---|---|---|
| Hunger % | stomach content (digestion queue kcal) + `GlycogenStores / MaxGlycogen` (e.g. 0.4 stomach + 0.6 glycogen) | "Hungry" = glycogen under ~50 % with an empty stomach; "Starving" = glycogen spent and fat being burned |
| Thirst % | `HydrationLevel` | thresholds from the dehydration stages the vitals already use |
| Energy % | vitals `Exertion.Fatigue` + sleep debt | if sleep debt is not modelled yet, Energy = 100 - Fatigue |
| Stamina | vitals stamina / exertion (already exists: `OnStaminaChanged`) | the sprint gate reads the same number the vitals use |

Implementation outline (about one unit of work):
1. The adapter's getters compute from the metabolism (no Tick decay of its own), `OnStatChanged` re-broadcasts the metabolism's change events, `ModifyStat` / `SetStat` (cheats) write through to the metabolism.
2. Remove the per-second decay and its replicated counters; replication is the metabolism's (it already replicates the nutrient levels).
3. Save / load: the metabolism already saves its state; the legacy counters drop out of the save (old saves: ignore the field).
4. Tests: idle 2 real minutes (no critical moodle), eat, drink, sleep, possession swap, save / load, co-op client sees the host's values.

## What changes for the player (why this is your call)
- Hunger and thirst become **slow**: at clock TimeScale 1.0 a healthy adult gets hungry over hours and critically dehydrated over about a day. The current tutorial and early food / crafting loop assume the 2-minute clocks.
- If the game clock runs faster than real time (the clock supports a TimeScale), the moodles follow it, as everything else in the simulation does.
- Food values become meaningful (calories, water content), not "10 Hunger per 100 kcal".

## Decisions needed from Wes
1. **A** (above), **B** (keep the counters but scale them by the game clock and a slower rate), or **C** (keep the arcade pacing as an explicit difficulty choice).
2. The game clock's TimeScale for normal play (1.0 = real time?): it sets how often the player eats.
3. Whether "Energy" models sleep debt (a sleep system) or only exertion fatigue for now.

## Also open (found while mapping)
- Drinking from water bodies: no interaction feeds hydration (`DrinkWater` is unused). A "drink" action at a lake / river, timed and interruptible, is the realistic way to rehydrate.
- `MOStatusPanel` and the moodles disagree until the decision above is implemented.
