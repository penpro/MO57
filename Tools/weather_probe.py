"""Diagnostic (overnight plan item 5): which of Ultra Dynamic Weather's STATE variables does a co-op client actually share with the host?

Reflection of the Blueprint (Content/UltraDynamicSky/Blueprints/Ultra_Dynamic_Weather) shows 9 replicated variables (Cloud Coverage, Rain, Snow, Wind Direction, Transition Duration,
Transition Timer, Weather Speed, Time Random Offset, Season) out of 673 -- Fog, Dust, Thunder/Lightning, Wind Intensity, Material Wetness/Snow/Dust Coverage and the `Weather` settings
reference are NOT among them. This starts host + client, has the host apply presets through the MO bridge, and compares the live values on both machines.

    python Tools/weather_probe.py [Preset ...]        (default: Foggy_Weather-like names are taken from MO.Weather.ListPresets)
Leaves nothing running.
"""
import re
import sys
import time

sys.path.insert(0, __file__.replace("\\", "/").rsplit("/", 1)[0])
import ue_inst as ui  # noqa: E402

UPROJECT = r"D:\UEProjects\MO57\MO57.uproject"
EDITOR = r"D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
HOST, CLIENT = "wxhost", "wxclient"

VARS = ["Cloud Coverage", "Rain", "Snow", "Wind Direction", "Fog", "Dust", "Thunder/Lightning", "Wind Intensity", "Material Wetness", "Material Snow Coverage", "Material Dust Coverage",
        "Transition Duration", "Transition Timer"]

PROBE = r'''
import unreal
udw = None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if a.get_class().get_name() == "Ultra_Dynamic_Weather_C":
        udw = a
        break
if udw is None:
    out("WX none")
else:
    def val(name):
        for form in (name, name.replace(" ", "_"), name.replace(" ", "_").replace("/", "_"), name.lower().replace(" ", "_").replace("/", "_")):
            try:
                v = udw.get_editor_property(form)
                return "%.3f" % float(v) if isinstance(v, (int, float)) else str(v)[:40]
            except Exception:
                continue
        return "?"
    try:
        w = udw.get_editor_property("Weather")
        wname = w.get_name() if w else "None"
    except Exception:
        wname = "?"
    out("WX weather_var=%s " % wname + " ".join("%s=%s" % (n.replace(" ", "").replace("/", ""), val(n)) for n in VARS_PLACEHOLDER))
'''.replace("VARS_PLACEHOLDER", repr(VARS))


def sample(label):
    rows = {}
    for name in (HOST, CLIENT):
        code, out = ui.call(name, ["py", "-c", PROBE], timeout=60)
        m = re.search(r"WX (.*)", "\n".join(ln for ln in out.splitlines() if "WX " in ln and "import unreal" not in ln and "VARS" not in ln))
        rows[name] = m.group(1).strip() if m else out.strip()[-200:]
    print(f"--- {label}", flush=True)
    for name in (HOST, CLIENT):
        print(f"  {name:9} {rows[name]}", flush=True)
    return rows


def main():
    presets = sys.argv[1:]
    for n, pos in ((HOST, (0, 0)), (CLIENT, (680, 0))):
        ui.start(n, UPROJECT, EDITOR, pos=pos, nosteam=True)
    try:
        rep = ui.Report()
        if not ui._host_and_join(rep, HOST, CLIENT, "WeatherProbe", 300):
            print("[wx] could not set up the pair", flush=True)
            return 1
        time.sleep(10)
        off = ui.log_size(HOST)
        ui.console(HOST, "MO.Weather.ListPresets")
        time.sleep(2)
        listing = ui.read_log_from(HOST, off)
        names = re.findall(r"MOWeather\][^\n]*?\b([A-Za-z0-9_]+)\s*$", listing, re.M)
        print("[wx] presets:", sorted(set(n for n in re.findall(r"\b(Clear_Skies|Cloudy|Foggy|Light_Rain|Rain|Thunderstorm|Dust_Storm|Sand_Storm|Snow|Blizzard|Partly_Cloudy|Overcast)\b", listing)))[:20], flush=True)
        sample("before (both machines)")
        for preset in presets or ["Thunderstorm", "Foggy", "Dust_Storm", "Snow"]:
            off = ui.log_size(HOST)
            ui.console(HOST, f"MO.Weather.SetPreset {preset}")
            ok = ui.wait_log(HOST, r"dispatching to provider|not found", since=off, timeout=15)
            print(f"[wx] host SetPreset {preset}: {ok.group(0) if ok else 'no answer'}", flush=True)
            time.sleep(45)  # let the transition finish
            sample(f"{preset}, 45 s after")
        return 0
    finally:
        for n in (HOST, CLIENT):
            ui.stop(n)


if __name__ == "__main__":
    sys.exit(main())
