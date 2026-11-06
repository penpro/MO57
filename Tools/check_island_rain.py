"""Under a floating island nothing falls (N-272, Nov 6): the pawn's exposure, its wetting and the rain in the air, against the open ground beside the same island.

    python Tools/check_island_rain.py [--seed 777] [--shots DIR]

A -game instance (the editor closed) hosts a new world under "Rain" at noon and stands the pawn on the ground (a) under the island's middle and (b) well outside its rim. PASS: under it the exposure's
rain is 0 and the cleared air reaches its 30 m, the wetting stays still; beside it the exposure's rain is not 0, the air is not cleared and the wetting rises. The streak measure is the fine vertical
detail of one frame with the camera held still (the rain's streaks) against the control with `mo.Weather.ClearAirUnderIslands 0` in the same spot: that the air looks right is Wes's.
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_forest_light as cfl  # noqa: E402
import check_save_roundtrip as base  # noqa: E402
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "islandrain"


def info():
    mark = ui.log_size(NAME)
    ui.console(NAME, "MO.Weather.Info")
    m = ui.wait_log(NAME, r"exposure rain=([\d.]+) snow=([\d.]+) wind=([\d.]+) sheltered=(\d) feels=(-?[\d.]+)C underIsland=(\d) clearAir=([\d.]+)cm", since=mark, timeout=15)
    return (float(m.group(1)), int(m.group(6)), float(m.group(7))) if m else (-1.0, -1, -1.0)


def wetness():
    code = """
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
v = pawn.get_component_by_class(unreal.MOVitalsComponent)
out('WET %.4f' % v.get_wetness_level())
"""
    m = re.search(r"WET ([\d.]+)", ui._probe(NAME, code, timeout=30))
    return float(m.group(1)) if m else -1.0


def streaks(path):
    """Mean fine vertical-line detail of the frame's upper half."""
    from PIL import Image, ImageChops, ImageFilter
    im = Image.open(path).convert("L")
    im = im.crop((0, 0, im.width, im.height // 2))
    blurred = im.filter(ImageFilter.BoxBlur(4))
    brighter = ImageChops.subtract(im, blurred)    # a streak is brighter than what is round it
    return sum(1 for v in brighter.getdata() if v >= 5)


def stand(x, y, z_above, label, out_dir, started, look):
    ui.console(NAME, f"MO.Player.Teleport {x} {y} {z_above}")
    time.sleep(35)
    # Over the trees, looking out at a shallow angle: blank sky behind the rain, the rock out of frame.
    ui.console(NAME, "MO.Cam.LookAt %d %d %d %d %d %d 70" % (x, y, z_above, x + look, y, z_above + 500))
    time.sleep(6)
    return cfl.shoot(out_dir, label, started)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--shots", default=os.path.join(ue.ROOT, "Saved", "SkyFeatures", "island_rain"))
    a = ap.parse_args()
    os.makedirs(a.shots, exist_ok=True)
    base.NAME = NAME
    cfl.NAME = NAME
    rep = ui.Report()
    started = time.time()
    ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, nosteam=True, res=(1920, 1080))
    try:
        ui.wait_ready(NAME, 300)
        if not base.host_fresh_world(rep, a.seed):
            return 1
        time.sleep(6)
        mark = ui.log_size(NAME)
        ui.console(NAME, "MO.WorldGen.FindIsland 1500000")
        m = ui.wait_log(NAME, r"Island #0 dist=\S+ x=(-?\d+) y=(-?\d+) top=(-?\d+) ground=(-?\d+) radius=(\d+) thickness=(\d+)", since=mark, timeout=20)
        if not m:
            return rep.step("an island near the start", False, "none in 15 km") or 1
        ix, iy, top, ground, radius = (int(m.group(k)) for k in (1, 2, 3, 4, 5))
        print(f"island at {ix} {iy}: top {top}, ground {ground}, radius {radius}", flush=True)
        for line in ("MO.Weather.Director 0", "weather Rain", "MO.Clock.SetTime 12 0", "fly 1"):
            ui.console(NAME, line)
        time.sleep(25)   # the front comes in

        under_at = (ix, iy, ground + 6000)
        beside = (ix + int(radius * 2.2) + 3000, iy, ground + 6000)

        shot_under = stand(*under_at, "under_cleared", a.shots, started, 2000)
        rain, under, clear = info()
        rep.step("under the island: no rain on the pawn, the generator says it is under the rock", rain == 0.0 and under == 1, f"rain {rain:.2f}, underIsland {under}")
        rep.step("under the island: the air round the camera is cleared to the rain's reach", clear >= 2900.0, f"{clear:.0f} cm")
        w0 = wetness()
        time.sleep(20)
        w1 = wetness()
        rep.step("under the island: the pawn does not get wetter", 0.0 <= w0 and w1 <= w0 + 1e-4, f"{w0:.4f} -> {w1:.4f}")

        ui.console(NAME, "mo.Weather.ClearAirUnderIslands 0")
        time.sleep(7)
        shot_control = cfl.shoot(a.shots, "under_control_rain_in_the_air", started)
        ui.console(NAME, "mo.Weather.ClearAirUnderIslands 1")
        if shot_under and shot_control:
            cleared, rained = streaks(shot_under), streaks(shot_control)
            rep.step("the rain's streaks are gone from the cleared air, against the control in the same spot", cleared * 3 < rained, f"{cleared} bright fine pixels cleared, {rained} control")

        stand(*beside, "beside_control", a.shots, started, 2000)
        rain, under, clear = info()
        rep.step("CONTROL beside the island: rain on the pawn, not under the rock, the air not cleared", rain > 0.1 and under == 0 and clear == 0.0, f"rain {rain:.2f}, underIsland {under}, clear {clear:.0f}")
        w0 = wetness()
        time.sleep(20)
        w1 = wetness()
        rep.step("CONTROL beside the island: the pawn gets wetter", 0.0 <= w0 < w1, f"{w0:.4f} -> {w1:.4f}")
        print(f"shots in {a.shots}", flush=True)
    finally:
        ui.stop(NAME)
    print("ISLAND RAIN %s" % ("PASS" if rep.ok else "FAIL"), flush=True)
    return 0 if rep.ok else 1


if __name__ == "__main__":
    sys.exit(main())
