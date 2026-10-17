# Weather, clock and time of day

> **Covers:** the game clock (time scale, start time, offline catch-up), Ultra Dynamic Sky / Weather and the MO weather bridge,
> the weather director, co-op sync of clock and weather, the time-of-day dialog and screenshot conditions.
> **Current state (2026-10-17):** the clock runs at real time (TimeScale 1.0) from 08:00 on a new game under Clear_Skies; offline
> catch-up is capped at 4 h for the clock AND crafting; the weather director changes weather on game time; the host's clock and
> preset are published through `AMOGameState`; UDS/UDW native replication stays ON.
> **Entry points:** `UMOGameClockSubsystem` (MOFrameworkCore; saves through `UMOClockSaveDomainAdapter`),
> `UMOWeatherIntegrationSubsystem` (+ `IMOWeatherProviderInterface`, `BP_WeatherBridge` in `Plugins/MOFramework/Content/WorldSystems`),
> `UMOWeatherDirectorSubsystem`, `UMOWorldSyncSubsystem`, `UMOTimeOfDayDialog` / `SMOClockDial`, `UMOQuickPickDialog`.
> **Dev verbs:** `MO.Clock.Info`, `MO.Clock.SetTimeScale <x>`, `MO.Clock.SetTime`, `MO.Weather.SetPreset <name>`, `.Director 0|1`,
> `.Info`, `.Next`, `.SkyTime`; console words `weather` / `time` (popups; `weather rain`, `time 2pm` skip them).
> **Open items:** `Docs/BACKLOG.md` § Water, light, weather. **Decisions:** DECISIONS 2026-10-08 (use the bridge), F6 (client-side apply),
> F13 (default time scale). Temperature effects on the body: `Docs/Medical_And_Survival.md`.

## 1. The clock

- `UMOGameClockSubsystem` (Core). Default **TimeScale 1.0 = real time**, so a weather spell or a day lasts hours of real time;
  `MO.Clock.SetTimeScale 24` = a day per hour. Changing the default is Wes's call (DECISIONS F13).
- New game starts at **08:00** (`DefaultStartDateTime`) under **Clear_Skies** (`UMOWeatherIntegrationSubsystem::ApplyNewGameStartConditions`,
  new game only; queued if the provider registers late).
- **Time never rewinds:** `AdvanceToTimeOfDay(Hour, Minute)` moves FORWARD to the next occurrence, so nothing timed on the DateTime
  sees a negative age. The dev `time` popup and the time dial use it (server RPCs on the player controller: works from a client).
- **Offline catch-up** (Wes, July 8; H36): on load the clock jumps forward by the real offline gap, capped at
  `OfflineAdvanceMaxHours` = 4 h (`FMOGameClockSaveData.SavedAtRealTime`; `ComputeOfflineAdvanceSeconds` pure-tested). The crafting
  queue uses the SAME cap (was 7 days: a queue could bank a year). Vitals do NOT fast-forward (4 h never starves a fed pawn).
- Survival stats decay on the world clock (`UMOSurvivalStatsComponent::AdvanceStats`, Oct 15). Crafting progress ticks on real
  `UtcNow` (not the game clock), so tests can't speed it with TimeScale (`Docs/Crafting_And_Progression.md`).

## 2. Weather

- **Ultra Dynamic Sky + Ultra Dynamic Weather** (`Content/UltraDynamicSky`) register the weather provider
  (`IMOWeatherProviderInterface`) that drives ambient temperature. Feels-like temperature is based on the GLOBAL temperature: UDS
  location queries blend canopy toward interior warmth (see `MOWeatherIntegrationSubsystem`).
- Every MO system reads and writes weather through `IMOWeatherProviderInterface` / `UMOWeatherIntegrationSubsystem`; the host's
  changes go through `BP_WeatherBridge` into UDW `Change Weather` (server-authoritative).
- **Weather director** (`UMOWeatherDirectorSubsystem`): holds a weather for a spell of GAME time, then follows a Markov chain over
  the UDS presets (~45 % fair / 13 % wet / 0.5 % thunderstorm over a long run); a wet front falls as snow below ~0.5 C. It respects
  a weather set by anyone else (adopts it, holds it 6 game hours). `MO.Weather.Director 0|1` (cvar), `.Info`, `.Next`.
- Snow on peaks and aspect snow are a material rule (`Docs/Terrain_Generation_Exploration.md`, M_QuixelBase); weather per biome
  (rain / snow line at altitude) is open.

## 3. Co-op sync

- The **clock** needs the MO sync: the host publishes `FMOWorldClockInfo` (+ the weather preset path) in `AMOGameState` through
  `UMOWorldSyncSubsystem`; a mid-day joiner otherwise runs its own 08:00 (`nettest game --withhold-sync` control: host 14:01 vs
  client 08:00). Check the SKY too (`MO.Weather.SkyTime` asks the bridge), not just the clock label.
- **Weather** visibly rides UDS/UDW native replication. Wes's rule (Oct 8): "use the bridge not the native UDS stuff" -- measured:
  UDW `Change Weather` is server-only, so turning native replication off made the client stop following. **Never switch native
  UDS/UDW replication off** without first giving the bridge a client-side apply (open fork F6, spec in §4 Recommendation 3).

## 4. Co-op weather: bridge vs native Ultra Dynamic Weather replication (investigation, 2026-10-09, verbatim)

**Question (Wes, Oct 8):** "we have a whole weather bridge for UDS and all that, so use the bridge not the native UDS stuff" -- can co-op weather go through `BP_WeatherBridge` only, with
native UDS/UDW replication switched off? **Scope of this note:** investigation + spec. No Blueprint was edited, native replication is still ON, nothing about the game changed.

### What is established (each line has its evidence)

| Fact | Evidence |
|------|----------|
| `Ultra_Dynamic_Sky_C` and `Ultra_Dynamic_Weather_C` actors replicate (`replicates=True`); `BP_WeatherBridge_C` does not | live host process, `Tools/weather_probe.py` / earlier `nettest game` |
| UDW replicates **9 of its 673** member variables: Cloud Coverage, Rain, Snow, Wind Direction, Transition Duration, Transition Timer, Weather Speed, Time Random Offset, Season | Blueprint asset reflection (`get_blueprint_variable_replication`) |
| UDS replicates 12 of 974: Moon Phase, Day/Night Length, Time Speed, Simulation Speed (+ night multiplier), Clouds Position, Simulated Sunrise/Sunset Time, Transitioning Time, Clouds B Time, Initial Replication. (Time of Day itself is NOT in the list: the MO clock sync, `UMOWorldSyncSubsystem`, exists for that reason) | same |
| NOT replicated: Fog, Dust, Thunder/Lightning, Wind Intensity, Material Wetness / Snow Coverage / Dust Coverage, and the `Weather` settings reference | same |
| Turning native replication OFF made the client stop following the host's weather even though the MO sync reached the client's bridge | Oct 8 experiment (reverted), memory `weather-bridge-not-native-uds` |
| UDW's `Change Weather` is server-authoritative, so the bridge's `SetWeatherPreset` cannot start a transition on a client | same experiment; `MOWorldSyncSubsystem` log "client following the host's weather preset ... dispatching to provider" with no visible change |
| The client's `Weather` variable (the preset reference) DOES end up equal to the host's: after `MO.Weather.SetPreset Foggy` / `Snow` on the host, the client's UDW `Weather` = Foggy / Snow | `Tools/weather_probe.py` (both machines printed `weather_var=Foggy`) |
| The bridge's event dispatchers / graph are not readable through the Python API (no function-graph access); the C++ side of the bridge is `IMOWeatherProviderInterface` (`MOWeatherProviderInterface.h`) | asset reflection, header |

### What is NOT established (said plainly)

- Whether a client really misses **Fog / Dust / Thunder / Wind Intensity / wetness** when the host changes weather. `Tools/weather_probe.py` read the live variables on both machines after the host applied presets: they were
  **identical to the "before" values on BOTH machines, including the host** (Snow preset -> `Snow=0.000` on the host after 45 s). So either those Blueprint variables are not the live state in a `-game` test process, or the
  transition does not run there; **the probe therefore proves nothing about a client gap in either direction**. Do not cite it as evidence for one.
- The exact names of the UDW functions a client may call locally (the graph is not readable from Python). Needs someone to open `Ultra_Dynamic_Weather` and list its Functions panel.

### Recommendation

1. **Keep native UDS/UDW replication ON.** It is the only transport that works today (measured), and it carries the visible core of weather (clouds, rain, snow, wind direction, the transition timer, season).
2. **The bridge is already the single path where it matters:** every MO system reads and writes weather through `IMOWeatherProviderInterface` / `UMOWeatherIntegrationSubsystem`, on every machine; the host's changes go
   through the bridge into UDW. "Use the bridge" is satisfied for the game's code; replication is UDW's transport underneath it, not a second API.
3. **A client-side apply in the bridge is only worth building if a real gap is shown** (a client that visibly lacks fog/dust/lightning/wetness that the host has). To show it: two real windows (`python Tools/ue.py inst`),
   host applies a preset with fog or thunder, **look at both screens** (the numeric probe above cannot). If there is a gap, the minimal Blueprint change is:
   in `BP_WeatherBridge`, an `ApplyHostWeatherState` that runs on **non-authority** machines and sets UDW's *local* state from the host's published preset (`AMOGameState` already carries `PresetPath`) through a UDW function
   that is **not** server-gated (find it in UDW's Functions panel; `Change Weather` is the server-gated one) -- then native replication of those actors can be turned off safely. Until a gap is shown, this is churn in Wes's Blueprint.
4. Never switch native replication off first (the Oct 8 experiment shows what happens).

### Reproduce

```
python Tools/ue.py editor start / wait      # reflection of the assets (python Tools/ue.py py --file <script>, see this note's table for the API: list_member_variable_names, get_blueprint_variable_replication)
python Tools/weather_probe.py [Preset ...]  # host + client, host applies presets through the bridge, both machines' UDW variables are printed
```

## 5. Screenshots and sky conditions (traps)

- The weather bridge syncs only FORWARD in game time: `MO.Clock.SetTime` backwards leaves the old sky; wait ~60 s after a forward set.
- For clean shots: `MO.Weather.Director 0` + `MO.Weather.SetPreset Clear_Skies`; `r.VolumetricRenderTarget 0` before a 4K shot or
  clouds are blocky; HighResShot excludes the HUD/UMG.
- Time dialog (Oct 15): `SMOClockDial` (midnight at the bottom, drag the hand), `WBP_TimeOfDayDialog` (`kit_time_dialog.py`);
  `PreviewSkyAt` / `EndSkyPreview` preview the sky while dragging; Dawn / Dusk buttons use `GetGoldenHourMinutesOfDay` (sunrise /
  sunset +- 30 min). Wes wants clean screenshots / video and golden-hour shots.

