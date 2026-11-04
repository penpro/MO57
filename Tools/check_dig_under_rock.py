"""Digging under rock (N-256 / BR1101-7, Nov 3): the volume sculpt works the spot itself, made at run time, saved.

    python Tools/check_dig_under_rock.py [--seed 777]

Two -game processes (developer mode: every terraform takes 1 s):
1. A new world. Aim 40 deg down at the ground ahead; add a 75 cm rock 3 m over the aimed spot (bUseVolumeSculpting for one Raise: that
   makes the volume sculpt, MO_VolumeSculpt, at run time) -- the spot is now under rock. The timed Dig there must not be refused, must
   lower the ground (a sphere, not a 2.5 cm skim) and leave the rock. CONTROL: a dig turned 120 deg away in the open stays the height
   sculpt. Then MO.Save.SaveAs.
2. A fresh process loads the save from the main menu (it travels to the map; MO.Save.LoadFrom reloads in place and keeps the old actor,
   so it cannot show this): the volume sculpt is made again, its record loads, the hole and the rock are there.
The ground is traced with Visibility minus the dropped items (the spoil lands on the spot).
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_buildings as cb  # noqa: E402  (its q / cmd helpers)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

SLOT = "MOTestDigUnderRock"
PRE = r"""
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
comp = pawn.get_component_by_class(unreal.MOTerraformingComponent) if pawn else None
ign = [pawn] if pawn else []
def down(x, y, z0, z1):
    r = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z0), unreal.Vector(x, y, z1), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ign + list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem)), unreal.DrawDebugTrace.NONE, True)
    return r.to_tuple()[4].z if r else -1e9
"""
fails = []


def check(ok, label, detail=""):
    print("%s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
        fails.append(label)


def boot():
    ui.start(cb.NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1280, 720), nosteam=True)
    ui.wait_ready(cb.NAME, 300)
    ui.call(cb.NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)


def ground_and_over(tx, ty, tz, key):
    r = cb.q(PRE + "\nout('%s ground=%%.1f over=%%.1f' %% (down(%f, %f, %f, %f), down(%f, %f, %f, %f)))\n"
             % (key, tx, ty, tz + 200, tz - 1000, tx, ty, tz + 5000, tz + 50), key, timeout=90)
    m = re.search(r"ground=([-\d.]+) over=([-\d.]+)", r)
    return (float(m.group(1)), float(m.group(2))) if m else (None, None)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--seed", default="777")
    a = ap.parse_args()
    cb.NAME = "digrock"
    boot()
    try:
        off = ui.log_size(cb.NAME)
        ui.console(cb.NAME, "MO.NewGame " + a.seed)
        ui.wait_log(cb.NAME, r"Character creator opened", since=off, timeout=420)
        time.sleep(5)
        ui.console(cb.NAME, "MO.Test.ClickWidget BeginButton")
        time.sleep(40)
        start = ui.log_size(cb.NAME)
        cb.q(PRE + "\nrot = pc.get_control_rotation()\npc.set_control_rotation(unreal.Rotator(0.0, -40.0, rot.yaw))\nout('TURN ok')\n", "TURN")
        time.sleep(1.5)   # the camera takes the new rotation next frame
        r = cb.q(PRE + "\nout('AIM %r' % (comp.find_terraform_target(),))\n", "AIM")
        nums = [float(v) for v in re.findall(r"[-+]?\d+\.\d+", r)]
        if len(nums) < 3:
            check(False, "an aimed spot on the ground", r)
            return 1
        tx, ty, tz = nums[:3]
        cb.q(PRE + "\ncomp.set_editor_property('use_volume_sculpting', True)\ncomp.raise_(unreal.Vector(%f, %f, %f))\n"
             "comp.set_editor_property('use_volume_sculpting', False)\nout('BLOB ok')\n" % (tx, ty, tz + 300.0), "BLOB")
        time.sleep(6)
        g0, over0 = ground_and_over(tx, ty, tz, "OVER")
        check(over0 is not None and over0 > tz + 200.0, "a rock stands 3 m over the spot", "ground %s, rock at %s" % (g0, over0))
        cb.q(PRE + "\ncomp.set_mode(unreal.MOTerraformMode.DIG)\nout('BEGIN %s' % comp.begin_terraform())\n", "BEGIN")
        time.sleep(8)
        g1, over1 = ground_and_over(tx, ty, tz, "AFTER")
        log = ui.read_log_from(cb.NAME, start)
        check("the volume sculpt works the spot itself" in log, "the dig under rock is not refused: the volume sculpt works it")
        check("spawned the volume sculpt" in log, "the volume sculpt is made at run time (the level has none)")
        check(g0 is not None and g1 is not None and g1 < g0 - 40.0, "the spot under the rock is dug (a sphere, not a 2.5 cm skim)", "%s -> %s" % (g0, g1))
        check(over1 is not None and over1 > tz + 200.0, "the rock over it is untouched (the height sculpt would have worked the surface above)", str(over1))
        mark = ui.log_size(cb.NAME)
        cb.q(PRE + "\nrot = pc.get_control_rotation()\npc.set_control_rotation(unreal.Rotator(0.0, -40.0, rot.yaw + 120.0))\nout('TURN2 ok')\n", "TURN2")
        time.sleep(1.5)
        cb.q(PRE + "\nout('OPEN %r begin=%s' % (comp.find_terraform_target(), comp.begin_terraform()))\n", "OPEN")
        time.sleep(8)
        log2 = ui.read_log_from(cb.NAME, mark)
        check("the volume sculpt works the spot itself" not in log2 and "Dig at" in log2, "CONTROL: a dig in the open stays the height sculpt")
        print(cb.cmd("MO.Save.SaveAs " + SLOT, SLOT, timeout=60), flush=True)
        time.sleep(3)
    finally:
        ui.stop(cb.NAME)

    boot()
    try:
        time.sleep(5)
        mark = ui.log_size(cb.NAME)
        cb.q("pc = unreal.GameplayStatics.get_player_controller(world, 0)\npc.load_game('%s')\nout('LOAD sent')\n" % SLOT, "LOAD")
        ui.wait_log(cb.NAME, r"\[MOVoxelSave\] (Restored volume|Sculpt actor|volume record)", since=mark, timeout=420)
        time.sleep(40)
        log = ui.read_log_from(cb.NAME, mark)
        check("spawned the volume sculpt" in log, "after a main-menu load: the volume sculpt is made again in the fresh world")
        check("Restored volume sculpt" in log, "... and its record loads into it")
        g2, over2 = ground_and_over(tx, ty, tz, "LOADED")
        check(g2 is not None and g2 < g0 - 40.0, "... the hole under the rock is there", "%s (dug %s)" % (g2, g1))
        check(over2 is not None and over2 > tz + 200.0, "... the rock over it is there", str(over2))
    finally:
        ui.console(cb.NAME, "MO.Save.Delete " + SLOT)
        time.sleep(2)
        ui.stop(cb.NAME)
    print("DIG UNDER ROCK %s (%d fail)" % ("PASS" if not fails else "FAIL", len(fails)), flush=True)
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
