"""Terraforming in a creative world (Wes, Nov 3: "make digging not produce dirt or require dirt for raising in creative mode and make it have
larger effects so creative players can mess around a lot more").

    python Tools/check_creative_dig.py [--seed 777]

One -game process, a new world. CONTROL first, in survival: a dig takes its time (developer mode's 1 s) and leaves its earth as Dirt.
Then `MO.World.Creative 1`: the brush is 2x the radius and 10x the strength (GetBrushRadius / GetBrushStrength), a dig completes at once,
lowers the ground ~50 cm at the centre (the height sculpt moves Radius / 10 x Strength) and leaves no Dirt; a Raise there builds it back up
with nothing spent (raising never takes dirt, in any world).
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_buildings as cb  # noqa: E402  (its q helper)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

PRE = r"""
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
comp = pawn.get_component_by_class(unreal.MOTerraformingComponent)
items = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem))
def down(x, y, z0, z1):
    r = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z0), unreal.Vector(x, y, z1), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn] + items, unreal.DrawDebugTrace.NONE, True)
    return r.to_tuple()[4].z if r else -1e9
"""
fails = []


def check(ok, label, detail=""):
    print("%s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
        fails.append(label)


def aim(yaw_add):
    cb.q(PRE + "\nrot = pc.get_control_rotation()\npc.set_control_rotation(unreal.Rotator(0.0, -40.0, rot.yaw + %f))\nout('TURN ok')\n" % yaw_add, "TURN")
    time.sleep(1.5)   # the camera takes the new rotation next frame
    r = cb.q(PRE + "\nout('AIM %r' % (comp.find_terraform_target(),))\n", "AIM")
    nums = [float(v) for v in re.findall(r"[-+]?\d+\.\d+", r)]
    return nums[:3] if len(nums) >= 3 else None


def act(mode, t, label):
    """One terraform action at the aimed spot (mode DIG / RAISE): (ground before, ground after, seconds to complete, earth logged)."""
    x, y, z = t
    before = float(cb.q(PRE + "\nout('G %%.1f' %% down(%f, %f, %f, %f))\n" % (x, y, z + 300, z - 1000), "G") or "nan")
    off = ui.log_size(cb.NAME)
    cb.q(PRE + "\ncomp.set_mode(unreal.MOTerraformMode.%s)\nout('BEGIN %%s' %% comp.begin_terraform())\n" % mode, "BEGIN")
    t0 = time.time()
    done = ui.wait_log(cb.NAME, r"\[MOTerraforming\] Timer completed", since=off, timeout=30)
    took = time.time() - t0
    time.sleep(4.0)
    after = float(cb.q(PRE + "\nout('G %%.1f' %% down(%f, %f, %f, %f))\n" % (x, y, z + 300, z - 1000), "G") or "nan")
    log = ui.read_log_from(cb.NAME, off)
    earth = re.findall(r"dug [\d.]+m3 at [^:]*: ([^\r\n(]*)", log)
    timing = re.search(r"developer mode: ([\d.]+) s instead of the simulated ([\d.]+) s", log)
    print("   %s %s: ground %.1f -> %.1f, completed %s after %.1f s, earth %s, timing %s" % (label, mode, before, after, bool(done), took, earth, timing.group(0) if timing else "-"), flush=True)
    return before, after, took if done else None, earth, log


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--seed", default="777")
    a = ap.parse_args()
    cb.NAME = "creativedig"
    ui.start(cb.NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1280, 720), nosteam=True)
    try:
        ui.wait_ready(cb.NAME, 300)
        ui.call(cb.NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(cb.NAME)
        ui.console(cb.NAME, "MO.NewGame " + a.seed)
        ui.wait_log(cb.NAME, r"Character creator opened", since=off, timeout=420)
        time.sleep(5)
        ui.console(cb.NAME, "MO.Test.ClickWidget BeginButton")
        time.sleep(40)

        # CONTROL: survival.
        brush = cb.q(PRE + "\nout('BRUSH %.1f %.3f' % (comp.get_brush_radius(), comp.get_brush_strength()))\n", "BRUSH")
        check(brush.startswith("250.0 0.100"), "survival: the brush is Config's (250 cm, 0.1)", brush)
        t = aim(0.0)
        b0, a0, took0, earth0, log0 = act("DIG", t, "survival")
        check("developer mode" in log0 and "simulated" in log0, "survival: the dig is timed (developer mode stands in for its simulated time)")
        check(bool(earth0), "survival: the dig leaves its earth (Dirt)", str(earth0))
        check(a0 < b0 - 1.0 and a0 > b0 - 6.0, "survival: one bite lowers the centre a few cm (Radius / 10 x Strength = 2.5)", "%.1f -> %.1f" % (b0, a0))

        # Creative.
        ui.console(cb.NAME, "MO.World.Creative 1")
        time.sleep(2.0)
        brush = cb.q(PRE + "\nout('BRUSH %.1f %.3f' % (comp.get_brush_radius(), comp.get_brush_strength()))\n", "BRUSH")
        check(brush.startswith("500.0 1.000"), "creative: the brush is 2x the radius and 10x the strength (500 cm, 1.0)", brush)
        t = aim(120.0)
        b1, a1, took1, earth1, log1 = act("DIG", t, "creative")
        check(took1 is not None and took1 < 1.0 and "developer mode" not in log1, "creative: the dig completes at once (no timer)", "%s s" % took1)
        check(not earth1, "creative: the dig leaves no Dirt", str(earth1))
        check(a1 < b1 - 35.0, "creative: one dig lowers the centre ~50 cm", "%.1f -> %.1f" % (b1, a1))
        b2, a2, took2, earth2, log2 = act("RAISE", t, "creative")
        check(took2 is not None and a2 > b2 + 35.0, "creative: a raise there builds it back up ~50 cm at once, nothing spent", "%.1f -> %.1f" % (b2, a2))
    finally:
        ui.stop(cb.NAME)
    print("CREATIVE DIG %s (%d fail)" % ("PASS" if not fails else "FAIL", len(fails)), flush=True)
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
