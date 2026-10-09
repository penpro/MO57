# Co-op weather: bridge vs native Ultra Dynamic Weather replication (investigation, 2026-10-09)

**Question (Wes, Oct 8):** "we have a whole weather bridge for UDS and all that, so use the bridge not the native UDS stuff" -- can co-op weather go through `BP_WeatherBridge` only, with
native UDS/UDW replication switched off? **Scope of this note:** investigation + spec. No Blueprint was edited, native replication is still ON, nothing about the game changed.

## What is established (each line has its evidence)

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

## What is NOT established (said plainly)

- Whether a client really misses **Fog / Dust / Thunder / Wind Intensity / wetness** when the host changes weather. `Tools/weather_probe.py` read the live variables on both machines after the host applied presets: they were
  **identical to the "before" values on BOTH machines, including the host** (Snow preset -> `Snow=0.000` on the host after 45 s). So either those Blueprint variables are not the live state in a `-game` test process, or the
  transition does not run there; **the probe therefore proves nothing about a client gap in either direction**. Do not cite it as evidence for one.
- The exact names of the UDW functions a client may call locally (the graph is not readable from Python). Needs someone to open `Ultra_Dynamic_Weather` and list its Functions panel.

## Recommendation

1. **Keep native UDS/UDW replication ON.** It is the only transport that works today (measured), and it carries the visible core of weather (clouds, rain, snow, wind direction, the transition timer, season).
2. **The bridge is already the single path where it matters:** every MO system reads and writes weather through `IMOWeatherProviderInterface` / `UMOWeatherIntegrationSubsystem`, on every machine; the host's changes go
   through the bridge into UDW. "Use the bridge" is satisfied for the game's code; replication is UDW's transport underneath it, not a second API.
3. **A client-side apply in the bridge is only worth building if a real gap is shown** (a client that visibly lacks fog/dust/lightning/wetness that the host has). To show it: two real windows (`python Tools/ue.py inst`),
   host applies a preset with fog or thunder, **look at both screens** (the numeric probe above cannot). If there is a gap, the minimal Blueprint change is:
   in `BP_WeatherBridge`, an `ApplyHostWeatherState` that runs on **non-authority** machines and sets UDW's *local* state from the host's published preset (`AMOGameState` already carries `PresetPath`) through a UDW function
   that is **not** server-gated (find it in UDW's Functions panel; `Change Weather` is the server-gated one) -- then native replication of those actors can be turned off safely. Until a gap is shown, this is churn in Wes's Blueprint.
4. Never switch native replication off first (the Oct 8 experiment shows what happens).

## Reproduce

```
python Tools/ue.py editor start / wait      # reflection of the assets (python Tools/ue.py py --file <script>, see this note's table for the API: list_member_variable_names, get_blueprint_variable_replication)
python Tools/weather_probe.py [Preset ...]  # host + client, host applies presets through the bridge, both machines' UDW variables are printed
```
