# Audio

> **Covers:** the audio subsystem (volumes, ambience states, music, one-shots), the heartbeat / output meter that diagnoses audio
> reports, the NaN guard, the Windows backend, footsteps and water sounds, and the focus policy.
> **Current state (2026-10-22):** the Oct 16 buzz / silence was NaN and absurd samples from Ultra Dynamic Weather MetaSounds latched by
> the engine's master reverb; `MOAudioNaNGuard` fixes it. Oct 22: two leaks closed (the master's dry path, garbage around invalid samples):
> a poisoned stretch is now ~15 s of silence after a menu load, not a buzz (section 2, BACKLOG N-189); measured, not yet heard by Wes. The Windows backend is XAudio2 (`Config/Windows/WindowsEngine.ini`; every editor log since Oct 21 reads "Initializing audio mixer using platform API: 'XAudio2'"); Wes's relaunch test is left. The game is silent while unfocused (Wes's policy).
> **Entry points:** `UMOAudioSubsystem` (`HandleWorldAudioContext`, `PreloadOneShots`, ambient state / layers, `PlayOneShotAtLocation`
> with volume / pitch scales), `MOAudioDiagCommands.cpp` (heartbeat + `FOutputMeter`), `MOAudioNaNGuard`, `UMOAmbientEnvironmentSubsystem`
> (Cave / Water / Outdoor from the generator), `UMOFootstepComponent`, `AMOCharacter::UpdateWaterSounds`, `DT_MOSound.json`,
> `DT_AmbientLayers`. **Dev verbs / cvars:** `mo.Audio.Heartbeat`, `MO.Audio.Diag`, `MO.Audio.Tone`, `MO.Footstep.Probe|Log|Heights|Walk|Count`;
> grep `[MOAudio] HB`. **Tests:** `MOFramework.Audio.*` (`MasterVolumeAppliedOnce`, `AmbientEnvironment`), `MOFramework.Footsteps.*`.
> Harness instances are `-nosound` by default: `ue.py inst start <name> --sound`. **Open items:** `Docs/BACKLOG.md` § Audio.

## 1. Rule: instrument, don't guess

Audio "fixes" went back and forth on guesses (Oct 12: buffers 1 -> 2). The engine logs nothing for these faults (1,504 saved logs: no
underrun, no device swap, no warning; `LogAudio=Warning`, set to hide UDW's RecursiveApplyAdjuster spam, also hides Display lines). On any
audio report, run with the heartbeat and read `[MOAudio] HB` before changing settings. The heartbeat (non-Shipping `mo.Audio.Heartbeat`;
the meter is compiled into Shipping too and feeds the bug report's `Audio.*`) samples the game world's audio device each second and logs
only changes / alarms: output device name + id, primary volume / mute, app volume (focus), restart storms (a sound started 4+ times/s),
voice limit, and an `FOutputMeter` submix listener on the main submix (OUTPUT BROKEN (NaN / Inf), STUCK, CLIPPING, SILENT while sounds
play, NOT RENDERING; a summary every 30 s). Submix listeners get the POST-effect buffer.

## 2. What was found (keep)

- **The buzz and the silence (Oct 16 night):** Ultra Dynamic Weather's MetaSounds emit NaN AND absurd finite samples (1e8..1e24) for ~20 s
  after a map load; the engine's MASTER REVERB (MasterReverbSubmixDefault, feedback) latched the NaN -> the whole mix NaN until restart
  (Wes's "no audio"); the rest hit the device -> driver clamps -> buzz. Fix = `MOAudioNaNGuard` (a submix guard first in the master reverb
  chain + on master: non-finite or > 16 -> 0; a 1 Hz watchdog restarts procedural sounds and re-applies the reverb chain). Bisect tools
  that worked: `au.BypassAllSubmixEffects 1`, `unreal.AudioMixerLibrary.clear_submix_effects(world, submix)`, stopping AudioComponents
  (muting is useless: NaN x 0 = NaN). ~20 s of thin menu audio after a map load remains (UDW DSP).
- **The buzz came back past the guard (Oct 22, Wes in PIE).** Measured with sound instances on the silent renderer (`-DeterministicAudio`:
  the whole mix runs, the meter reads it, nothing reaches the speakers, focused or not) and the guard's per-second counts:
  - After most menu loads (14 of 20 runs; 2 more briefly) the mix holds invalid samples for ~15 s while ONLY UDW's three weather MetaSounds play
    (UDS_Global_WeatherSounds, UDS_Directional_WeatherSounds, UDS_WeatherSoundMixer; positions and multipliers sane). Their generated
    audio is clean: a source guard on them (`FMOSourceNaNGuard`) processed 400-560 blocks a second and found nothing invalid, while the
    master's and the reverb's input guard each found every block poisoned. So the garbage is made in the mixer's per-source stages AFTER
    source effects (volume fade, channel map, send levels), driven by UDW; turning the reverb effect off changes nothing
    (`mo.Audio.NaNGuard.ReverbOff 1`). The watchdog's restarts end it after ~5 recoveries.
  - Two leaks let it reach the speakers: the master submix's DRY path (a submix's default dry level, -96 dB, converts to 1.6e-5 linear,
    not 0, and the mixer keeps it live: `FMixerSubmix::ProcessAudio`, `IsNearlyZero` at 1e-8) carried 1e24 samples around the guard at
    ~1e19; and zeroing only the invalid samples left the garbage around them (output still 13.6-15 peak, rms 0.5-0.7).
  - Now: the guard sets the master's dry level to exactly 0 ("dry path closed"; `mo.Audio.NaNGuard.CloseDry`), a block with ANY invalid
    sample goes out silent whole (`SanitizeBlock`, threshold +12 dBFS, unit test `MOFramework.Audio.NaNGuard`), and the source guard rides
    on every procedural sound (it must declare 8 channels: a source chain whose presets support fewer channels than the source is never
    instantiated, default 2). Measured: no invalid sample and no peak over 0.24 at the output in any run since. The cost: while UDW
    poisons the mix, the mix is silent (~15 s after a menu load) instead of buzzing (BACKLOG N-189). The guard logs per second "silenced
    N master + N reverb-input of N mix block(s), and N of N source block(s)" and, once per stretch, every sound playing.
- **WASAPI vs XAudio2:** UE 5.8 switched the Windows default backend to WASAPI; in its render callback a block the mixer declines (device
  swap in flight, stream not Running) is released unwritten, so the device replays stale memory ("dddd"). The project uses XAudio2 (a
  declined block starves the voice = silence, never a replay); delete the key in `Config/Windows/WindowsEngine.ini` to return to WASAPI.
  An engine-side Memzero patch was proposed and NOT applied (launcher engine).
- **Master volume applied twice** (Oct 12): class volumes = category x master AND the device transient volume = master -> everything
  ~-28 dB with Wes's saved settings. Device-only master now (test `MasterVolumeAppliedOnce`); `AudioNumBuffersToEnqueue` 1 -> 2.
- **`UEngine::LoadMap` resets the device transient primary volume to 1.0 on every map load:** `UMOAudioSubsystem::HandleWorldAudioContext`
  re-applies the master volume.
- The intro video's `UMediaSoundComponent` had no sound class (full volume, ignored the sliders): it plays at music x master.
- The ambience driver flapped Water <-> Outdoor every second (rings depended on the previous state): `DistanceToWaterCm` measures once on
  fixed 7 m rings + a 4 s minimum dwell. Ambience states: Cave (> 12 m under the generator surface; leaves at 8 m), Water (within 35 m of
  water; leaves beyond 56 m), else the clock's Day / Night; Indoor is left to buildings.
- First-use loads after READY are held at the LOADER (`PreloadOneShots`); the debug file logger `MOHARVEST_LOG` cost 8-11 ms per spawn
  (now opt-in `MO.Harvest.Enable 1`). Rule: any new debug file logger defaults off, has a pre-format gate, and compiles out in Shipping.
- `TObjectIterator<USoundClass>` at 1 Hz walked every UObject (~30 ms): removed (`Docs/TECHNICAL_REFERENCE.md` "Periodic work").
- **Focus policy (Wes, Oct 16):** silent when unfocused: `[Audio] UnfocusedVolumeMultiplier=0.0` in `Config/DefaultEngine.ini`. A launch
  that comes up behind another window is silent until clicked: policy, not a fault. Exclusive-fullscreen mode switches re-enumerate monitor
  audio endpoints (a plausible swap trigger); Wes's machine has 5 active endpoints, default Speakers (Dell USB Audio).
- Not explained: the first `--sound` instance was silent to Wes while 8-10 sounds played at full primary volume; the next one was audible.
  If it recurs, read the HB lines around the time.

## 3. Footsteps and water sounds

**Footsteps** (`UMOFootstepComponent`, one per `AMOCharacter`, dormant without `ball_l` / `ball_r`; TECHNICAL_REFERENCE Oct 2026, verbatim):
- **When**: the animation decides. A foot's WORLD horizontal speed against the body's: swing = faster than 0.65 x max(body speed, 100) cm/s
  for at least 0.2 s, plant = back under 0.30 x. Measured on the MO walk / sprint / crouch clips (walk 2.65 steps/s alternating L-R; a height
  signal was tried first and is useless here). Fallback: walked distance; landing: a heavy step. No steps in water (the water sounds own wading).
- **What**: `IMOFootstepSurfaceInterface` on the actor stood on, else the world generator (floor vs the generated surface: structure /
  underground / the BIOME, `FMOBiomeDefinitionRow::FootstepSet`). The set name is the middle of the sound-bank ids `SFX.Footsteps.<Set>1` /
  `<Set>2`: a new surface = bank rows + a data value, never code. Crouched = Sneak gait (quiet).
- Dev verbs: `MO.Footstep.Probe|Log|LogAll|Heights <s> [full]|Walk [s] [sprint|sneak]|Count`. Tests: `MOFramework.Footsteps.*`.
- Biome sets (DT_Biomes `FootstepSet`; Temperate / Locust had drifted to SandTrail in the data, restored Oct 18, BR18-8): Temperate / Locust LeafCrunch, Meadow GrassTread, Rocky RockScuff, Shore / DryScrub SandTrail, AlpineHeath MossPad,
  Rainforest MuddyStep, AlpineRock StoneStep.
- **Props underfoot** (N-132, Oct 20): a placed mesh stood on (PCG rocks and logs, props) names its set by its asset path,
  `UMOFootstepComponent::PropRules`, first match (`FMOFootstepMath::DefaultPropRules`): rocky ground / riverbed / rock / boulder / cliff /
  stone / cobble / shelf / pebble = StoneStep; stump / tree / branch / log / trunk / roots / wood / plank / stick = BareWood; ground / terrain /
  embankment / path patches defer to the biome below. Before, every prop rang as BareWood (no physical materials). Probe reason: `prop <mesh>`.
- **Creatures** (N-132, Oct 20): the wolf and deer skeletons have no ball-of-foot bones, so their footstep component was silent. A pawn
  without the bones now steps by the ground it covers when `BoneFreeStepLengthCm` is set (AMOCreature: 70 cm at a walk, longer strides
  past 3 m/s, `FMOFootstepMath::BoneFreeStepCm`), on the same surface sets, at `StepVolumeScale` 0.5 (a padded paw, a light hoof),
  set per species in `DT_Creatures` (`FootstepStrideCm` / `FootstepVolume`, 0 = those defaults; Oct 20: Wolf01 60 cm / 0.35, a padded
  paw; Deer_Doe 75 cm / 0.55, a hoof -- estimates). `MO.Footstep.LogAll 1` logs every pawn's steps (`pawn=` on each line); host: the
  wolf case hears it.

**Water:** splash / wading / stroke sounds (`AMOCharacter::UpdateWaterSounds`, ids `SFX.Footsteps.WaterSplash1/2`).
