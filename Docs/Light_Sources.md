# Light Sources: carried flames and seeing in the dark

Caves are dark. No sky light reaches a gallery, and noon in a tunnel is black (correct: see `MOSubterraneanLightingSubsystem`). The night is dark too.
A primitive survivor has one light, fire, and has to carry it, feed it and keep it dry. This doc covers the measured numbers, how MO models a
carried flame, and what the camera has to do for a torch-lit cave to read.

## What real primitive lights give

Medina-Alcaide et al. 2021, *The conquest of the dark spaces: An experimental approach to lighting systems in Paleolithic caves*, PLOS ONE
16(6): e0250497 (doi 10.1371/journal.pone.0250497). They burned replica Paleolithic lights in Isuntza 1 cave and measured them:

| Light | Intensity | Working radius | Burn | Notes |
|---|---|---|---|---|
| Juniper torch (best of five) | 14 cd (21.9 lux at 40 cm) | 2.5 m (6 m lit diameter) | 61 min (others 21-41) | needs air: swinging it took 20 cm illuminance from 5 to 28 lux; resin burns fast; white smoke (juniper, oak), black (pine resin, birch bark) |
| Stone lamp, bone-marrow fat, 3 juniper wicks | 0.6 cd (0.7 lux at 1 m) | 1.6 m | > 1 h, best after ~20 min | no smoke; dazzles the carrier; throws light sideways, little on the floor |
| Small fireplace (0.9 kg oak + juniper) | 3 cd (19 lux at 40 cm) | 3.3 m (up to 4.5) | measured 30 min | smoke filled the space; static |

Their conclusion: torches are for moving and exploring (light all round, no dazzle, fast walking), lamps for staying put in small spaces. They
complement each other. A candle is ~1 cd (~12.6 lm). Wood and fat flames burn at ~1800-2000 K.

## The model (`UMOCarriedLightComponent`, `UMOLightSourceSettings`)

- **What can burn:** every carried light is an item with a profile in Project Settings > MO > Light Sources, keyed by item id. A new light
  source is one profile plus its item row (an equippable `Held` tool with `MaxDurability`). The profile holds intensity (cd), radius, colour
  temperature, flicker, the still-air scale, lighting time, burn time per durability point, flame position and effect.
- **Lighting:** from a burning fire. The fire's menu offers "Light the <torch> in the fire" (`FMOFireActions::LightCarried`) when the pawn
  holds an unlit light source: a timed action of the profile's `LightSeconds`, holding it in the flames.
- **Burning down:** one durability point per `BurnSecondsPerDurability` of game time, on the server clock. A torch is 90 points × 30 s =
  45 min, inside the measured 21-61. Durability lives on the item, so a half-burnt torch that is put out and relit carries on where it was.
- **Going out:**
  - it leaves the hand (put away, dropped, swapped): nobody pockets a burning torch;
  - the flame goes under the water surface, or the pawn swims (wading with it held up is fine);
  - it burns down (the owner is told).
- **The light:**
  - a point light in **candelas** at the far end of the held mesh's long axis, warm (`ColorTemperatureK`), with shadows;
  - flicker comes from two noise octaves;
  - **airflow:** intensity runs from `StillIntensityScale` at rest (torch 0.6) to 1.0 at walking pace, smoothed over ~1 s. Walking with a torch
    makes it flare; standing still it burns low. That is the experiment's result, and a reason to keep moving.
  - the visible flame is the profile's Niagara effect, kept upright in the world.
- **Carrying fire:** a laid fire is lit from a lit torch (`FMOFireActions::LightFromCarried`): 20 s, no drill and no tinder. It catches 95 %
  of the time dry, 60 % in rain. That is why people carried fire rather than drill a new one at every camp.
- **Co-op:** the lit item's GUID, id and hand replicate. Every machine with a renderer builds its own light and flame from them.

Calibration in code: `UMOLightSourceSettings` constructor (Torch01 10 cd, 1900 K; OilLamp01 0.8 cd, steady). Tests: `MOFramework.Light.*`.

Units trap: a UE point light defaults to *unitless*. A unitless value is ×16, a candela value is ×10000, so unitless 5000 (the campfire) is
**8 cd**. That is right for a fire, but anything that sets candelas must call `SetIntensityUnits(Candelas)` *before* `SetIntensity`. Changing
`intensity_units` through `set_editor_property` on a live light does not refresh its render proxy. That is why an early "8000 cd" test light
looked black: it was rendering far dimmer.

## What the camera must do: exposure

A 10 cd torch lights a wall 2 m away to 10 / 4 = 2.5 lux. At albedo 0.3 that wall's luminance is 2.5 × 0.3 / π ≈ 0.24 cd/m², which is
**EV100 ≈ 1**. Daylight is EV100 13-15. A human eye covers that range by adapting:

- the pupil opens in seconds (~4 EV);
- the cones adapt over 5-10 min;
- the rods take 20-30 min to reach full night vision.

Going from dark to bright takes seconds.

So the auto exposure must be allowed down to about EV100 0 or below, adapt down slowly (a cave mouth looks black from outside, and your eyes
adjust over many seconds inside), and adapt up fast. If a sky or post process volume clamps the minimum near daylight values, a torch-lit cave
renders black however bright the torch is. Ultra Dynamic Sky has its own exposure settings: *Apply Exposure Settings*, *Exposure Bias
Day/Night/In Interior*, *Exposure Brightness Range*, and *Apply Interior Adjustments* driven by player-occlusion traces. `MO.Light.Exposure`
dumps every exposure setting that can apply to the view: volumes, post process components, the camera, the sky actor's exposure / interior /
occlusion properties, and renderer defaults.

**Measured (Oct 17, seed 31415926, a gallery 90 m under the ground at noon; `Saved/Logs/torch_cave`):**

- **Exposure was not clamped.** Ultra Dynamic Sky's post process component (unbound, priority 1) sets histogram auto exposure, EV100 -10..20,
  bias 0, and its exposure compensation curve (about -1 EV at these levels). The camera had no overrides.
- **The cave glowed.** With no light the gallery read as a dim blue-grey night scene: the scene average was EV100 -7.3 and the eye adapted all
  the way down. The source was UDS's *Captured Scene Sky Light* (real-time capture, 0.89, **casts no shadow**), which lit the inside of the
  hill as if it were open sky. With it at 0 the cave measures 0.0 mean brightness: black.
  - **Fixed** with UDS's own cave feature on the level's sky actor (`MOPCGScattering`): *Apply Interior Adjustments* on, *Sky Light Intensity
    Multiplier in Interiors* 0. Its player-occlusion traces (12 x 4 rays, 45 m, every 0.55 s) measure how enclosed the view is, and work for
    dug tunnels and roofed huts too, not only generated caves.
  - Measured: deep in the cave occlusion 0.97, sky light 0.03, black; in the open 0.00 and 0.89, unchanged. Twelve random surface spots:
    10 read 0.00, 2 under canopy read ~0.25 (sky light ~25 % lower, as a forest floor is).
- **The torch blew out.** With the eye adapted to the darkness (~-7), a torch-lit floor at EV ~1 sat 8 stops over: white. A camera meters the
  whole frame; an eye near a flame stays light-adapted.
  - **Fixed** with `UMOEyeAdaptationComponent` (on every MO character, active only on the pawn a local player views through). It sums the
    illuminance of the registered flames at the camera (`IMOLocalLightSource` in `UMOAmbientEnvironmentRegistry`, inverse square). The camera's
    auto exposure may then not adapt darker than a 0.3 surround lit by that illuminance, -1 EV: `AdaptationFloorEV100`, `FloorBiasEV` -1.
  - Measured: a held torch (6 cd still, 10 walking) gives 1.6-1.7 lux at the third-person camera, floor EV100 -0.7. The ground and rocks within
    ~2.5 m read clearly, warm; past that it is black. That matches the study's 2.5 m working radius.
  - Bias sweep: +2 too dark, 0 / -0.5 / -1 close, -1 kept. With no flame near, nothing is written and the sky's range applies.

**Still open:**

- The campfire (`AMOCraftingStationActor`) is not yet a registered light source: its file is in Codex's batch C. A campfire at night will blow
  out the same way until it registers (8 cd).
- Adaptation speed: UDS uses UE's 3 EV/s up, 1 EV/s down. Real dark adaptation takes minutes (pupil in seconds, cones 5-10 min, rods 20-30 min),
  and light adaptation takes seconds. A slower speed down would be more faithful; that changes every shade transition, so it is Wes's call.

## Not modelled yet

- Smoke (a torch in a dead-end gallery fills it).
- Rain on a torch (resin torches survive light rain).
- Lighting one torch from another, and survivors carrying torches at night.
- Fuel for lamps: an oil lamp's durability stands in for its oil; refilling with fat or oil is not a recipe yet.
- A key to snuff the flame without putting it away.
- A lit torch as fire for other uses beyond lighting a campfire (scaring animals, fire-setting a rock face).
