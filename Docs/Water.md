# Water: ocean, lakes, swimming, breath

> **Covers:** the ocean and upland lakes (Oceanology), how a point knows it is in water, swimming / wading / drowning, the
> underwater look, shore planting. Drinking, salt water and illness: `Docs/Water_And_Illness.md`.
> **Current state (2026-10-17):** Oceanology replaced Epic Water on 2026-10-11 (ocean in both levels, per-machine upland lakes
> with a registered wave solver, capped underwater distortion). Swimming is stock CMC swimming driven by our own water query;
> drowning comes from the medical sim. Floating objects / boats are not built.
> **Entry points:** `MOOceanologyIntegration.h/.cpp` (`UMOOceanologyIntegrationSubsystem` + `MOOceanology::` helpers: read its
> pitfalls first), `UMOUplandLakeSubsystem` (spawns `AOceanologyLake`), `UMOWaterSubsystem` (`GetSurfaceZ`, `GetGroundZ`),
> `FMOWorldGen::GetWaterSurfaceCm` / `ClassifyWaterBody`, `UMOCharacterMovementComponent`, `UMORespirationComponent`
> (Medical module), `AMOCharacter::TickWater` / `UpdateSwimAnimation` / `UpdateWaterSounds`, `UMOBreathMeterWidget`.
> **Dev verbs:** `MO.Water.Underwater`, `.FixUnderwaterVolume 0|1`, `.UnderwaterDistortion`, `.Swim`, `.GoToLake [rank] [dry]`,
> `.Stroke [s] [up|down]`, `.SetBreath`, `.Meter`, `.Here`, `.LookAtWater`, `.GoToShore`, `MO.WorldGen.FindWater`; cvars
> `mo.Lakes.Absorption`, `mo.Lakes.Scattering`. **Tests:** `MOFramework.Water.*`, `MOFramework.Lakes.QueryCacheFreshness`,
> `MOFramework.WorldGen.UplandLakes` / `.UplandLakePlane` / `.UplandLakeBed`.
> **Open items:** `Docs/BACKLOG.md` § Water, light, weather. **Traps:** `Docs/TRAPS.md` § 11 (Water). **Decisions:** DECISIONS (Oct 11 Oceanology,
> Oct 16 hair / prepass).

## 1. Where water is (the generator decides)

- **Sea level water** = everything below `SeaLevelCm` (the infinite ocean plane). Rivers are channels whose bed is below sea
  level (zero contour of a 2-octave field, ~24 m wide, fade out above 12 m of ground); ponds and lakes at sea level are bowls in
  lowland. Deep ocean since v5.1: the continent spline drops to -14 / -45 / -80 m (x2 VerticalScale), sea > 100 m deep 67 %.
- **Upland lakes** (generator v4+): one site per 1.3 km cell; the surface = lowest ground around the lake minus a freeboard, so
  the shore is always a dry bank; ~1 per 5-6 km2, radius 70-180 m. The generator also buries the lake's axis-aligned plane
  rectangle (+3 m, 60 cm freeboard) because a lake plane cannot rotate.
- `FMOWorldGen::GetWaterSurfaceCm` (upland ellipse = its own surface, else the sea plane) is what `UMOWaterSubsystem::GetSurfaceZ`
  answers; `ClassifyWaterBody` gives sea / river / lake / pond for drinking and illness. `GetGroundZ` + `BuriedMarginCm` (300):
  feet that far under the generator ground are NOT in water (the ocean plane fills everything under z = 0, underground too).
- Lake bed: `FMOWorldGen::UplandLakeBedWeight` drives `UMOWorldGenSettings::UplandLakeBedSurfaceType` (now `VST_MI_DarkPebbles`).

## 2. Oceanology in this engine (each fact cost a diagnosis)

- **Plugin:** `Plugins/Marketplace/Oceanology_Plugin` ("Oceanology Legacy" 5.9.0 for UE 5.8, source + Win64 binaries), enabled
  in MO57.uproject, a dependency of MOFramework. Levels `MOPCGScattering` and `LoadingLevel` each hold one
  `BP_OceanologyInfiniteOcean` (preset `03_Beaufort_GentleBreeze`); Epic `WaterBodyCustom` / `BuoyancyManager` are gone (a
  BuoyancyManager in a loaded world is a transient the Water plugin spawns; tests skip RF_Transient).
- **Underwater look on the infinite ocean:** the plugin's box volume has NO collision and its post process is a bounded volume on
  it; in game the box has a physics object but no physics STATE, so `EncompassesPoint` answers FLT_MAX. Fix: a query-only,
  ignore-everything body (`MOOceanology::EnsureUnderwaterVolumeQueryable`, at world begin play and on spawn). The headless test
  world builds a body anyway, so that control is information only; reproduce in play with `MO.Water.FixUnderwaterVolume 0`.
- **Lakes can't rotate:** the water mesh is a quad tree over WORLD-aligned bounds; plane = the ellipse's AABB
  (`FLake::PlaneHalfSizeCm`, margin 2 %). The plane is whole tiles from a grid-snapped centre. **Never scale a lake actor to a big
  factor**: the quad tree pre-allocates ~(2 x extent x scale)^2 x 4/3 nodes (a 3 km lake ~270 MB); use a bigger TileSize.
- **Lakes are spawned per machine from the seed:** `SetReplicates(false)` before `FinishSpawning`, or the host's copy doubles them
  on clients. Copy the `SpawnLake` pattern for any per-machine water actor.
- **Frozen lakes (fixed Oct 16):** Oceanology never registers a RUNTIME-spawned water body's wave solver (`SetupWaveSolvers` uses
  `NewObject` without `RegisterComponent`); `SpawnLake` now registers + activates it.
- **Distortion cap:** presets overwrite `DistortionIntensity` / `VignetteDistortion` on every load, so the 1/4 cap (Wes: "really
  strong and looks pretty bad") is re-enforced every 0.5 s (`MO.Water.UnderwaterDistortion`, default 0.0125).
- **Lake look** (Oct 13): set at spawn from `mo.Lakes.Absorption` ("2.5 4.5 3 50") / `mo.Lakes.Scattering`. Absorption is how far
  each channel CARRIES (bigger = clearer, not more absorbed); plugin default 10/25/30 = turquoise pool; ours = dark teal + green
  shallows. Cost: a lake filling the view ~1.2 ms GPU of 10; the 4 km spawn radius stays.
- **Lake waves** (Oct 18, Wes: "make lakes less splashy, smaller waves"; Oct 23, BR23-16: "these are all small lakes so they should be almost
  still"): as each lake spawns, `mo.Lakes.WaveScale` (0.1; 0.4 until Oct 23) scales the preset's four Gerstner wave heights,
  `mo.Lakes.WavelengthScale` (0.2) their wavelengths, and each wave's speed follows by deep-water dispersion (x sqrt(0.2): short waves are
  slow); `mo.Lakes.TurbulenceScale` (0.1) scales the procedural surface noise (`Procedural.Turbulance`). All lakes share the elected lake's
  wave parameters, so they all get the same scales. The lake's SOUND is the Water ambience (Audio.md): x0.2 since Oct 23.
- **Caustics** are only the projected `T_Caustics` texture (cheap, on by default).
- **Cooking:** Oceanology loads assets through soft paths in `UOceanologyRuntimeSettings` the cooker does not follow (first
  packaged run crashed in `InitBubbles`). `Config/DefaultGame.ini` always-cooks the needed `/Oceanology_Plugin/` dirs, narrowed
  on Oct 16 (the Gerstner Niagara effects must NOT cook: DX11 can't compile them). Test
  `MOFramework.Water.OceanologyAssetsAreCooked` (has an explicit NotCooked map). After touching water or plugin content run the
  packaged co-op test (`Tools/ov.bat` recipe / `nettest packaged`). See `Docs/Packaging.md`.
- **Not integrated:** Oceanology buoyancy (`UOceanBuoyancyComponent`) and swimming (`UOceanSwimmingComponent`, timer-death
  drowning) need an `AOceanologyWaterVolume` (an APhysicsVolume brush that can't be built at run time). We use our own query.

## 3. Swimming, wading, breath (commits cce2524a, b0c989b5)

- `UMOCharacterMovementComponent` (every MO pawn; creatures wade only, `bCanSwim=false`) computes the capsule's immersion from
  `GetSurfaceZ`: wading drag (~half speed chest deep); swim at 0.70 immersion; stop only below 0.50 AND with walkable ground
  under the feet (holding Space at the surface cannot pop in/out); buoyancy 1.27 floats the head (settles at 0.79, eyes ~10 cm
  above). The capsule stays upright (`ShouldRemainVertical`), the mesh lies down. `GetPhysicsVolume()` returns ONE brushless
  `APhysicsVolume` with `bWaterVolume` while swimming (PhysSwimming flips to falling if the volume is not water).
- `AMOCharacter::TickWater`: Space = rise, steering by camera pitch with a +-12 deg dead zone, hustle = crawl; stamina / MET /
  exertion / XP / wetness. Swim poses play single-node on the body mesh (`Plugins/MOFramework/Content/Characters/Swim/*_MH`, the
  plugin's Manny set retargeted: `Docs/TOOLING.md` § animation retarget); the ABP is restored on exit (crouch and swim never overlap).
- **Drowning is the medical sim:** `UMORespirationComponent` (oxygen reserve, `SetAirwayClosedBy(source, bool)`; a closed airway
  drains it, 110 s at rest, /4 at full exertion) caps SpO2 via `UMOVitalsComponent::SetAirwayOxygenReserve`; asphyxia death 75 s
  after empty -> `DieOf(Drowned)`. Pure math `FMOBreathMath`; `EMOActivityLevel::Swimming` (appended last).
- Splash / wading / stroke sounds (`UpdateWaterSounds`, `SFX.Footsteps.WaterSplash1/2`); HUD breath meter (`UMOBreathMeterWidget`,
  full-screen with the bar aligned bottom-centre: see `Docs/UI.md`); cold-water heat exchange
  (`UMOVitalsComponent::ApplyWaterToExposure` / `WaterTemperatureForAir`, `SetWaterImmersion`).
- **Cold water is far too fast and that is the THERMAL MODEL, not the water coupling** (a 13 C lake: core 36.6 -> 33.4 in 36 s;
  real ~1 C per 15-20 min). Logged as a fork (DECISIONS F2); do not paper over it with a constant.
- Verify by using it: `MO.Water.GoToLake` -> `MO.Water.Stroke` -> `MO.Water.Swim` lines + screenshots, and `MOFramework.Water`.

## 4. Underwater look, hair and depth of field (engine-level, open)

- Hair buns (strands) vanish under the water effect and blur under the character creator's depth of field: with Single Layer
  Water the deferred renderer swaps `SceneTextures.Depth` for the SLW prepass depth (no hair) and the hair composition skips its
  depth write. `r.Water.SingleLayer.DepthPrepass=0` proves it (hair depth back) but Wes REJECTED it on Oct 16 (Oceanology water
  loses its look). The fix is engine patch #1 (`Docs/Engine_Patches.md`), which needs a source engine; the creator's DOF is off
  (`UMOCharacterCreatorPanel::bPreviewDepthOfField = false`, 0573ab31) until Epic fixes it. Upstream report drafted, not posted
  (DECISIONS F11).
- DISPROVEN (don't retry): SceneDepthWithoutWater copy, Min(SceneDepth, CustomDepth) with groom custom depth, hair cards,
  DepthPrepass 0 (rejected), HW raster, aperture, `DepthOfFieldUseHairDepth` (its tooltip is inverted vs the code),
  `r.HairStrands.ComposeAfterTranslucency`.

## 5. Shores

- Shore planting is the PCG shore tier (`WaterEdgeSpecies` per biome, `Docs/PCG.md` §3); boulders / fallen logs / lakeside
  saplings were added by `Tools/shore_dressing.py` (bug #29). The reeds are stand-ins; real reed / rush / cattail meshes need a
  source (DECISIONS F5). Electric Dreams has Taro / ferns / mud-bank assemblies but no reed asset.

## 5a. Lakes and the shore look (Oct 24, Wes's BR24-15 / 18)

- **Lakes nearly still:** "still huge swaths of water material rolling across the lake surface, dial them down a LOT still, they should be
  almost still". Waves 0.03 of the lake preset's height (`mo.Lakes.WaveScale`, was 0.1; length 0.2, noise 0.1 as before) and the surface's
  animated normal maps (Oceanology Flipbook: near / mid / far strength and frame rate) a quarter (`mo.Lakes.NormalScale`), set as each lake
  spawns (`UMOUplandLakeSubsystem`). The round lakes are BR23-13.
- **Shore wetness:** the ocean lays a 1 km shoreline wetness decal; its preset made everything within 4 m of the water 75 % metallic at
  roughness 0.1 -- beach rocks and wet skin read as chrome ("looks metallic instead of wet"). `UMOSceneLightingSubsystem` sets it a tick after
  the ocean loads its preset: metallic 0, roughness 0.25, a 1 m band (`mo.Water.ShoreWetMetallic` / `ShoreWetRoughness` / `ShoreWetRadius`).

## 6. Superseded

- Epic Water era (Oct 10-11): a dedicated `WaterTrace` channel (`ECC_GameTraceChannel2`) + `WaterBodyCollision` profile fixed the
  Epic underwater look (it ran only on a BLOCKING hit on its trace channel). **Superseded by Oceanology (Oct 11 night): dead config,
  left in place.** `MO.Water.UnderwaterTrace` was removed with it.
- `AMOLakeActor` / `AMOWaterActorBase` (Gerstner, no material) and `MOBuoyancyComponent` are unused legacy.
- "The pawn walks on the seabed" (Oct 11 morning): superseded by swimming the same day.
