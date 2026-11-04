"""Crop plants in a real -game world: one crop sown and grown through its stages, or candidate meshes side by side (A/B).

    python Tools/check_crop_plants.py grow [--crop WildGrain] [--seed-item GrainStore01] [--keep] [--reuse]
        a creative world; a field plot (MO.Colony.PlaceBuilding TillFieldPlot); the crop's seed given and sown through the plot's own
        menu (MO.Test.WorldAction Sow:<Crop>); aged with MO.Test.AgeCrops to Sprouted, Growing and Ripe (8 / 50 / 102 days: DT_Crops
        WildGrain sprouts at 7, ripens at 100); at each stage its "[MOCrop] ... now <Stage>" line and shots from 4 m and close up.
    python Tools/check_crop_plants.py ab <label>=<mesh path>:<height cm, 0 = as imported>[:<material path>] ... [--keep] [--reuse]
        one fallow plot per mesh, side by side, each dressed with that mesh and ITS OWN materials (196 plants, 14 x 14, jittered,
        traced onto the worked ground, as AMOCropPlotActor::RefreshPlants lays them); shots of each from 4 m and close up.

    python Tools/check_crop_plants.py perf <label>=<mesh path>:<height>[:<material>] [--plots 6]
        a row of plots all dressed with that mesh; MO.Perf.Bench over the field twice each with the plants shown and hidden -> the GPU
        ms the crop costs. Run with the editor CLOSED (Docs/Performance.md: the GPU is shared).

Shots: Saved/CropTest/<mode>/ at 10:00 (the clock is moved forward to it: set backwards, the sky stayed dark, Oct 30).

Why (Nov 3 2026, DECISIONS "Crop plants are made with TRELLIS ... A/B tested until photorealistic"). Lessons from the Oct 30 test, built in:
  - game Python cannot spawn actors or add components (no deferred spawn, no add_component_by_class), so A/B patches are real plots;
    a fallow plot redraws its plants only when it first fits the ground, then never again, so the plants put on its Plants stay;
  - the meshes must be SAVED before the instance starts: a running instance locks the packages it loaded and an editor save then
    silently fails (TRAPS "Asset saves silently don't happen"). On Oct 30 the plants rendered near-black because of exactly this: the
    meshes' new slot materials never reached disk and the game drew them with the glTF import's material, which cannot draw Nanite or
    instanced meshes. Each slot is set to the mesh's own material anyway (DRESSED prints it: check it is the MI_Crop_* one);
  - other sessions' instances share Saved/Screenshots and move files out of it: a vanished shot is retaken.
"""
import argparse
import glob
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "croptest"
SPACING = 520.0   # A/B plot centres across the row (a plot is 4 m)
AHEAD = 900.0     # the A/B row's distance in front of the player
OUT = ""


def q(code, key, timeout=120):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    lines = [ln.split(key + " ", 1)[1] for ln in out.splitlines() if (key + " ") in ln]
    return lines[-1].strip() if lines else ("ERR " + out[-1500:])


def shot(label, tries=3):
    shots = os.path.join(ue.ROOT, "Saved", "Screenshots")
    for _ in range(tries):
        before = set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True))
        ui.console(NAME, "shot")
        deadline, new = time.time() + 25, []
        while time.time() < deadline and not new:
            new = sorted(set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True)) - before)
            time.sleep(0.3)
        if new:
            time.sleep(0.6)
            os.makedirs(OUT, exist_ok=True)
            try:
                shutil.move(new[-1], os.path.join(OUT, label + ".png"))
                print("shot", label, flush=True)
                return True
            except OSError:
                pass
    print("shot FAILED", label, flush=True)
    return False


def cam(c, t, fov=75, settle=6.0):
    ui.console(NAME, "MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f %d" % (c[0], c[1], c[2], t[0], t[1], t[2], fov))
    time.sleep(settle)


def shoot_patch(tag, c, f):
    r = (-f[1], f[0])
    cam((c[0] - f[0] * 420, c[1] - f[1] * 420, c[2] + 165), (c[0], c[1], c[2] + 45), fov=70)
    shot("%s_4m" % tag)
    cam((c[0] - f[0] * 280 + r[0] * 40, c[1] - f[1] * 280 + r[1] * 40, c[2] + 85), (c[0] - f[0] * 140, c[1] - f[1] * 140, c[2] + 55), fov=55)
    shot("%s_close" % tag)


HEAD = r'''
import unreal, random
p = unreal.GameplayStatics.get_player_pawn(world, 0)
'''
WHERE = HEAD + r'''
o = p.get_actor_location(); f = p.get_actor_forward_vector()
out("WHERE %.1f,%.1f,%.1f %.4f,%.4f" % (o.x, o.y, o.z, f.x, f.y))
'''
MOVE = HEAD + r'''
p.set_actor_location(unreal.Vector(X, Y, Z), False, True)
out("MOVED ok")
'''
PLOTS = HEAD + r'''
o = p.get_actor_location()
ps = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCropPlotActor))
out("PLOTS %s" % " ; ".join("%s@%.0f,%.0f,%.0f d=%.0f" % (a.get_name(), a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z, (a.get_actor_location() - o).length()) for a in ps))
'''
GIVE = HEAD + r'''
inv = p.get_component_by_class(unreal.MOInventoryComponent)
inv.add_item_by_guid(unreal.GuidLibrary.new_guid(), ITEM, COUNT)
out("GIVEN %s %d" % (ITEM, inv.get_item_count_by_definition_id(ITEM)))
'''
DRESS = HEAD + r'''
p.set_actor_hidden_in_game(True)
o = unreal.Vector(OX, OY, OZ); f = unreal.Vector(FX, FY, 0.0); r = unreal.Vector(-FY, FX, 0.0)
plots = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCropPlotActor))
plots.sort(key=lambda a: (a.get_actor_location() - o).dot(r))
ign = [p]
def ground(x, y, z):
    g = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 400.0), unreal.Vector(x, y, z - 800.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ign, unreal.DrawDebugTrace.NONE, True)
    t = g.to_tuple() if g else None
    return t[4].z if (t and t[0]) else None
specs = SPECS
res = []
for k, plot in enumerate(plots[:len(specs)]):
    label, path, height, mat = specs[k]
    m = unreal.load_asset(path)
    ism = plot.get_component_by_class(unreal.InstancedStaticMeshComponent)
    if not m or not ism:
        res.append("%s=missing(mesh=%s ism=%s)" % (label, bool(m), bool(ism))); continue
    h = m.get_bounds().box_extent.z * 2.0
    s = (height / h) if height > 0 else 1.0
    ism.clear_instances()
    ism.set_static_mesh(m)
    for si in range(len(m.static_materials)):   # the mesh's own material (or the given one) on every slot, whatever the plot had
        ism.set_material(si, unreal.load_asset(mat) if mat else m.get_material(si))
    c0 = plot.get_actor_location()
    rnd = random.Random(7 + k)
    n = 0
    for i in range(14):
        for j in range(14):
            d = r * (-200.0 + 28.57 * (i + 0.5) + rnd.uniform(-8.0, 8.0)) + f * (-200.0 + 28.57 * (j + 0.5) + rnd.uniform(-8.0, 8.0))
            z = ground(c0.x + d.x, c0.y + d.y, c0.z)
            if z is None:
                continue
            sc = s * rnd.uniform(0.85, 1.1)
            ism.add_instance(unreal.Transform(unreal.Vector(c0.x + d.x, c0.y + d.y, z), unreal.Rotator(roll=rnd.uniform(-4.0, 4.0), pitch=rnd.uniform(-4.0, 4.0), yaw=rnd.uniform(0.0, 360.0)), unreal.Vector(sc, sc, sc)), True)
            n += 1
    res.append("%s=%d@%.0f,%.0f,%.0f h=%.0fcm mat=%s" % (label, n, c0.x, c0.y, c0.z, h * s, ism.get_material(0).get_name()))
out("DRESSED %d plots: %s" % (len(plots), " ; ".join(res)))
'''


def boot(reuse):
    if not reuse:
        ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1600, 900), nosteam=True)
        if not ui.wait_ready(NAME, 300):
            sys.exit("FAIL: the instance did not boot")
        ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.NewGame 777")
        if not ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420):
            sys.exit("FAIL: no character creator")
        time.sleep(5)
        ui.console(NAME, "MO.Test.ClickWidget BeginButton")
        time.sleep(45)
    ui.console(NAME, "MO.Weather.Director 0")   # else a front can roll in mid-test (Nov 3: the A/B came out in fog)
    ui.console(NAME, "MO.Weather.SetPreset Clear_Skies")
    ui.console(NAME, "MO.Clock.SkipToDay")
    time.sleep(3)
    ui.console(NAME, "MO.Clock.SetTime 10 0")
    time.sleep(3)
    a, b = q(WHERE, "WHERE").split(" ")
    o = [float(v) for v in a.split(",")]
    f = [float(v) for v in b.split(",")]
    n = (f[0] ** 2 + f[1] ** 2) ** 0.5
    return o, [f[0] / n, f[1] / n]


def grow(args):
    o, f = boot(args.reuse)
    results = []
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.World.Creative 1")
    ui.console(NAME, "MO.Colony.PlaceBuilding TillFieldPlot 300")
    time.sleep(25)   # the plot fits the ground on its 10 s tick
    plots = q(PLOTS, "PLOTS")
    m = re.search(r"@(-?\d+),(-?\d+),(-?\d+) d=(\d+)", plots)
    if not m:
        sys.exit("FAIL: no plot: " + plots)
    c = [float(m.group(1)), float(m.group(2)), float(m.group(3))]
    print("plot", plots)
    print(q(GIVE.replace("ITEM", repr(args.seed_item)).replace("COUNT", "20"), "GIVEN"))
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.WorldAction Sow:%s" % args.crop)
    sown = ui.wait_log(NAME, r"\[MOCrop\][^\r\n]*(?:sow|Sown|%s)[^\r\n]*" % args.crop, since=off, timeout=360)
    results.append(("sown through the plot's menu", bool(sown), sown.group(0)[-160:] if sown else ""))
    q(HEAD + "\np.set_actor_hidden_in_game(True)\nout('HID ok')", "HID")
    for days, stage in ((8, "Sprouted"), (42, "Growing"), (52, "Ripe")):
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Test.AgeCrops %d 2000" % days)
        m = ui.wait_log(NAME, r"\[MOCrop\][^\r\n]*now (\w+)", since=off, timeout=30)
        got = m.group(1) if m else "?"
        results.append(("%s reached" % stage, got == stage, m.group(0)[-120:] if m else "no stage line"))
        time.sleep(4)
        shoot_patch("%s_%s" % (args.crop, stage), c, f)
    ui.console(NAME, "MO.Cam.Reset")
    for label, ok, detail in results:
        print("%s  %s  %s" % ("PASS" if ok else "FAIL", label, detail))
    return 0 if all(r[1] for r in results) else 1


def ab(args, shoot=True):
    specs = []
    for a in args.patches:
        label, rest = a.split("=", 1)
        bits = rest.split(":")
        specs.append((label, bits[0], float(bits[1]), bits[2] if len(bits) > 2 else ""))
    o, f = boot(args.reuse)
    r = (-f[1], f[0])
    for k in range(len(specs)):
        side = (k - (len(specs) - 1) / 2.0) * SPACING
        q(MOVE.replace("X", "%.1f" % (o[0] + r[0] * side)).replace("Y", "%.1f" % (o[1] + r[1] * side)).replace("Z", "%.1f" % (o[2] + 30.0)), "MOVED")
        time.sleep(1.5)
        ui.console(NAME, "MO.Colony.PlaceBuilding TillFieldPlot %d" % AHEAD)
        time.sleep(2.5)
    q(MOVE.replace("X", "%.1f" % o[0]).replace("Y", "%.1f" % o[1]).replace("Z", "%.1f" % (o[2] + 30.0)), "MOVED")
    time.sleep(35)
    code = (DRESS.replace("OX", "%.1f" % o[0]).replace("OY", "%.1f" % o[1]).replace("OZ", "%.1f" % o[2])
            .replace("FX", "%.5f" % f[0]).replace("FY", "%.5f" % f[1]).replace("SPECS", repr(specs)))
    got = q(code, "DRESSED", timeout=300)
    print("DRESSED", got, flush=True)
    centres = {}
    for part in got.split(": ", 1)[-1].split(" ; "):
        if "=" in part and "@" in part:
            label, rest = part.split("=", 1)
            centres[label] = [float(v) for v in rest.split("@")[1].split(" ")[0].split(",")]
    if not centres:
        return 1
    time.sleep(15)   # a 10 s tick passes: the plants must still be there
    if shoot:
        for label, c in centres.items():
            shoot_patch(label, c, f)
        ui.console(NAME, "MO.Cam.Reset")
    return 0


HIDE = HEAD + r'''
n = 0
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCropPlotActor):
    ism = a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    if ism:
        ism.set_visibility(SHOW, False)
        n += 1
out("SHOWN %s %d" % (SHOW, n))
'''


def bench(label, seconds=12):
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Perf.Bench %d %s" % (seconds, label))
    m = ui.wait_log(NAME, r"\[MOPERF\] %s\s+frames=(\d+) fps=([\d.]+) avg=([\d.]+)ms p50=([\d.]+) p95=([\d.]+)[^|]*\| gpu=([\d.]+)ms" % re.escape(label),
                    since=off, timeout=seconds + 60)
    if not m:
        print("BENCH %s: no result line" % label)
        return None
    print("BENCH %s: avg %s ms, p95 %s ms, gpu %s ms (%s frames)" % (label, m.group(3), m.group(5), m.group(6), m.group(1)), flush=True)
    return float(m.group(6))


def perf(args):
    """A row of plots all dressed with one mesh; GPU ms over the field with the plants shown and hidden (editor closed: Performance.md)."""
    label, rest = args.patches[0].split("=", 1)
    args.patches = ["%s%d=%s" % (label, k, rest) for k in range(args.plots)]
    ab(args, shoot=False)
    o, f = boot(True)
    mid = q(PLOTS, "PLOTS")
    pts = [[float(v) for v in m] for m in re.findall(r"@(-?\d+),(-?\d+),(-?\d+)", mid)]
    c = [sum(p[i] for p in pts) / len(pts) for i in range(3)]
    cam((c[0] - f[0] * 900, c[1] - f[1] * 900, c[2] + 200), (c[0], c[1], c[2] + 30), fov=90, settle=10.0)
    shot("%s_field" % label)
    results = {}
    for rep in range(2):
        for show in (True, False):
            q(HIDE.replace("SHOW", str(show)), "SHOWN")
            time.sleep(4)
            results.setdefault(show, []).append(bench("%s_%s_%d" % (label, "on" if show else "off", rep)))
    on = [v for v in results[True] if v]
    off = [v for v in results[False] if v]
    if on and off:
        print("PERF %s: %d plots x 196 plants: gpu %.2f ms with, %.2f ms without -> %.2f ms" % (label, len(pts), sum(on) / len(on), sum(off) / len(off), sum(on) / len(on) - sum(off) / len(off)))
    ui.console(NAME, "MO.Cam.Reset")
    return 0


def main():
    global OUT
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=("grow", "ab", "perf"))
    ap.add_argument("--plots", type=int, default=6)
    ap.add_argument("patches", nargs="*")
    ap.add_argument("--crop", default="WildGrain")
    ap.add_argument("--seed-item", default="GrainStore01")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--reuse", action="store_true")
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    OUT = args.out or os.path.join(ue.ROOT, "Saved", "CropTest", args.mode)
    try:
        return {"grow": grow, "ab": ab, "perf": perf}[args.mode](args)
    finally:
        if not args.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
