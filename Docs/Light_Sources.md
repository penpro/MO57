# Light Sources: carried flames and seeing in the dark

> **Covers:** carried flames (torch, oil lamp), campfires as light, eye adaptation / exposure near flames, cave and night
> darkness, the light from a sun or moon below the horizon.
> **Current state (2026-10-17):** Torch01 craftable, lit at a fire, burns down (per-item wear waits on Codex's equipment commit, BACKLOG N-002; the co-op case of a
> client lighting its own torch is to be retested, N-054), goes out in water; campfires register
> as flames; UDS interior adjustments keep caves black; the eye-adaptation floor keeps a torch-lit cave readable.
> **Entry points:** `UMOCarriedLightComponent`, `UMOLightSourceSettings`, `UMOEyeAdaptationComponent`, `UMOFlameLightComponent`,
> `UMOSubterraneanLightingSubsystem`, `FMOFireActions::LightCarried` / `LightFromCarried`, `IMOLocalLightSource` on
> `UMOAmbientEnvironmentRegistry`. **Dev verbs:** `MO.Light.Carried [give|light|out]`, `MO.Light.Exposure`, `MO.Light.Test`,
> `MO.Light.Cube`, `MO.Light.Off`, `MO.Light.BlockFromBelow 0|1`. **Tests:** `MOFramework.Light.*`.
> **Open items:** `Docs/BACKLOG.md` § Water, light, weather. **Decisions:** DECISIONS 2026-10-17 ("leave night brightness alone for now").
> **How to extend:** a new carried light = one `UMOLightSourceSettings` profile + an equippable Held tool row with MaxDurability;
> a new flame (forge, kiln, brazier) = `UMOFlameLightComponent` on its Blueprint (or implement `IMOLocalLightSource` and register).

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
  45 min, inside the measured 21-61. It lives on the item (`UMOEquipmentComponent::ReduceEquippedDurability`, Codex, Oct 17), so a half-burnt torch put out and relit carries on
  where it was; at 0 the torch is used up.
- **Going out:**
  - it leaves the hand (put away, dropped, swapped): nobody pockets a burning torch;
  - the flame goes under the water surface, or the pawn swims (wading with it held up is fine);
  - it burns down (the owner is told);
  - **rain at the flame** (N-056, Oct 20): the weather's rain exposure AT the flame (a roof, an overhang or a cave keeps it off), looked at
    once a second. Up to the profile's `RainSafeIntensity` (0.4) nothing happens: a resin torch survives light rain. Above it a constant
    hazard, `(Rain - Safe) / (1 - Safe)` per `MeanSecondsInHeaviestRain` (120 s at rain 1): UDW "Rain" (0.7 in the open) puts a torch out
    after ~4 min on average, at random (`UMOCarriedLightComponent::RainDouseChance`, memoryless, so the check rate does not matter). The
    numbers are an estimate (no measurement found; DECISIONS Oct 20); 0 = a covered lantern. The owner is told ("goes out (rain)").
    Test speed-up: `MO.Test.RainOnFlameSpeedup` (not in Shipping).
- **The light:**
  - a point light in **candelas** at the far end of the held mesh's long axis, warm (`ColorTemperatureK`), with shadows;
  - flicker comes from two noise octaves;
  - **airflow:** intensity runs from `StillIntensityScale` at rest (torch 0.6) to 1.0 at walking pace, smoothed over ~1 s. Walking with a torch
    makes it flare; standing still it burns low. That is the experiment's result, and a reason to keep moving.
  - the visible flame is the profile's Niagara effect, kept upright in the world.
- **Carrying fire:** a laid fire is lit from a lit torch (`FMOFireActions::LightFromCarried`): 20 s, no drill and no tinder. It catches 95 %
  of the time dry, 60 % in rain. That is why people carried fire rather than drill a new one at every camp.
- **Torch from torch** (N-056, Oct 20): holding an unlit torch, a right-click on another pawn carrying a burning one offers "Light your
  Torch from their Torch" (`FMOFireActions::GetBearerActions`, in the pawn's world actions next to its wounds' treatments; greyed past
  2.5 m). It is the healer's timed action of the torch's `LightSeconds`, and it catches if the other flame still burns within reach. A
  recruited survivor's command menu lists it too. Co-op: the joiner's menu is built from the replicated flame; the pick runs on the host.
- **Co-op:** the lit item's GUID, id and hand replicate. Every machine with a renderer builds its own light and flame from them. Verified on real
  host + client processes (`nettest game --keep`):
  - the client sees the host's torch lit, and it light-adapts the client's eye;
  - the client lights its own torch at a campfire through the server RPC and renders the flame;
  - the host sees it lit.
- **Wear, verified:** a lit torch's own durability goes down 1 point per 30 s; sped up 60x it burnt out, the owner was told, and the spent torch was
  gone from the hand.

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

**Verified end to end in game (Oct 17, `flow_check.py` against an instance):**

1. MakeTorch crafted a Torch01.
2. Equipped, at a burning campfire the fire menu offered "Light the Torch in the fire" (the real server RPC); after 6 s it was lit at 6.1 cd,
   exposure floor -0.54.
3. At a laid, unlit fire "Light it from the Torch": caught (the 95 % roll).
4. Walking into a lake: "Torch01 went out (doused)".

Close-ups at dusk set the side carry: `HeldVisual.RightHandTransform` gripped near the butt, 15 deg forward and 35 deg outward, flame effect at
0.5. A torch-raising arm pose (animation) would improve it further.

**Still open:**

- ~~The campfire is not a registered light source.~~ Done: `UMOFlameLightComponent` on `BP_Campfire` registers the actor's visible point
  lights (units converted: the unitless 5000 is 8 cd). Any fire Blueprint (forge, kiln, brazier) gets it the same way, with no code change.
  Measured at 23:30 standing by a lit campfire: 0.58 lux at the camera, floor EV100 -2.1; the firelit ground reads and the sky beyond goes black.
- Night outdoors without a fire looks like dusk: the exposure adapts to EV100 -10, so starlight reads as twilight. After a fire is put out,
  its mesh's emissive embers glow white at that exposure. A darker night (a higher night minimum, or slower dark adaptation) is Wes's call. Numbers for it: starlight lights the ground to ~0.001 lux,
  which at albedo 0.2 is ~6e-5 cd/m2, EV100 ~ -11; a full moon ~0.25 lux is EV100 ~ -3. With a -10 floor a starlit night renders about one
  stop under mid-grey (dusk). With a night floor of about -6, moonlight still reads and a moonless night is properly dark: the dark-adapted
  eye sees shapes under starlight, colourless and soft, not a dusk landscape.
- ~~Fire-fear.~~ Done Oct 20 (N-096): a wolf will not close within its species' `FireFearRadiusCm` (Wolf01 400) of a registered flame -- a
  carried torch, a campfire -- around its prey: the chase stops short and no bite is made. One flee decision now
  (`AMOCreatureController::ShouldFlee`). `Docs/AI_And_Colony.md` section 1.
- Adaptation speed: UDS uses UE's 3 EV/s up, 1 EV/s down. Real dark adaptation takes minutes (pupil in seconds, cones 5-10 min, rods 20-30 min),
  and light adaptation takes seconds. A slower speed down would be more faithful; that changes every shade transition, so it is Wes's call.

## Under a closed canopy (Oct 23, N-202)

Wes, on the x3 rainforest: "now it's BLACK in the rain forest, this also makes me wonder if lumen is working properly as it should lighten it
up a bit, or we need to adjust HDR post processing". Measured on seed 3014 (`MO.Light.Exposure` now prints the sky light's live intensity
and UDS's player-occlusion component; mean brightness of 1280x720 shots, 0-255):

- **Lumen works.** `r.DynamicGlobalIlluminationMethod` 1. With it off the same view went from 88 to 108: without Lumen the sky light reaches
  the shade unshadowed. With it, a closed canopy is a roof: the sky cannot reach the floor and the bounce from sunlit gaps is little.
- **UDS does not take the forest for a cave.** Its interior adjustment trims the sky light under trees, not to zero: 0.89 open, 0.78 in a
  temperate stand, 0.71 under the rainforest canopy (a cave reads 0.03).
- **The fix is Lumen's Skylight Leaking**, a share of the sky light that reaches occluded places anyway (the light through and between the
  leaves): `UMOSceneLightingSubsystem` puts one unbound post process component (priority -1: under the sky's, the water's and any level
  volume) in every game world, `mo.Light.SkylightLeaking` (live). The pawn's view under the rainforest canopy: 0 -> 24, 0.02 -> 30,
  0.05 -> 36, 0.1 -> 46; at 0.1 (the default) the stand reads (trunks, leaves, ferns) and is still darker than the clearing. The leak alone
  left a cave black (155 m inside the cave behind `poi CaveEntrance`: sky light 0.000, the view 0.0 at both 0 and 0.1); since the interior
  floor below (Oct 24) a cave gets 0.35 of the sky light and reads dim blue-grey.

- **Oct 24 (Wes, BR24-13: "do more adjustment to 'enclosed' light levels, i'd rather things be a little unnaturally bright than to have
  it being like a lgiht switch turning off and on"):** UDS's interior adjustment cut the sky light to 0 wherever its occlusion test read
  enclosed, and in dense forest that test flips as you walk. Its *Sky Light Intensity Multiplier in Interiors* is now 0.35 (set at runtime by
  `UMOSceneLightingSubsystem`, `mo.Light.InteriorSkyLight`): enclosed places dim to about a third of the sky light instead of going black.
  Caves are no longer pitch black either -- Wes's call: a little unnaturally bright over a switch.

## Sun and moon below the horizon (cave walls lit from below)

Measured Oct 10 with `MO.Light.Test <pitch> <yaw> <lux>` in a sinkhole chamber: a light from ABOVE leaves enclosed tunnels pitch black
at noon (terrain blocks sun and sky correctly); a light from BELOW (pitch +40) lit cave walls like direct sun, shadows on or off. Not fixed by
the mega material's `bGenerateTwoSidedMaterial`, `bCastShadowAsTwoSided`, non-Nanite voxel meshes, VSM off, or a 180 km shadow quad under
the caves (Nanite's shadow raster takes two-sidedness only from the material; the one-sided voxel surface has no face for a light below).
**Fix:** `UMOSubterraneanLightingSubsystem` attenuates every directional light's intensity by how far below the horizon it points (1 at the
horizon, 0 at 5 deg below), keeping the intensity Ultra Dynamic Sky writes. A/B: `MO.Light.BlockFromBelow 0|1`. `ULightComponent::bAffectsWorld`
cannot change at runtime and `SetVisibility` is overwritten by the sky blueprint, which is why it drives intensity.

## Not modelled yet

- Smoke (a torch in a dead-end gallery fills it).
- Survivors carrying torches at night (one torch lights from another since Oct 20).
- Fuel for lamps: an oil lamp's durability stands in for its oil; refilling with fat or oil is not a recipe yet.
- A key to snuff the flame without putting it away (an equipped torch is not in the pack's item menu, and a key needs choosing).
- A lit torch as fire for other uses beyond lighting a campfire (scaring animals, fire-setting a rock face).
