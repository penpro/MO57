"""A held bow, drawn and loosed (Nov 2 2026: the Blender bows, Tools/blender_items.py). One -game instance on a new world: the survivor gets
the bow into her left hand, then MO.Test.BowShot draws it with Sparrow's clips on her body (draw, hold at full draw, loose) while the game runs
slowed down, so screenshots catch each phase from the bow's side; the [MOBow] lines give the string's draw and its distance from her fingers
every 0.1 s of game time.

    python Tools/check_bow.py [--item SimpleBow01] [--seed 777] [--slomo 0.25] [--keep] [--reuse]

Prints each phase's string-to-fingers distance and "BOW PASS" when, while nocked, the string stays within 3 cm of the fingers and the release
brings it back to brace. Screenshots: Saved/Screenshots/bow/<item>_*.png. The editor may stay open (the instance builds its own world).
"""
import argparse
import glob
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "bowcheck"
SHOTS = os.path.join(ue.ROOT, "Saved", "Screenshots", "bow")


def q(code, key, timeout=60):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    m = re.search(key + r" ([^\r\n]*)", out)
    return m.group(1) if m else ""


def shot(label):
    root = os.path.join(ue.ROOT, "Saved", "Screenshots")
    before = set(glob.glob(os.path.join(root, "**", "*.png"), recursive=True))
    ui.console(NAME, "shot")
    deadline, new = time.time() + 20, []
    while time.time() < deadline and not new:
        new = sorted(set(glob.glob(os.path.join(root, "**", "*.png"), recursive=True)) - before)
        time.sleep(0.2)
    if new:
        time.sleep(0.4)
        os.makedirs(SHOTS, exist_ok=True)
        dst = os.path.join(SHOTS, label + ".png")
        shutil.move(new[-1], dst)
        return dst
    return None


EQUIP = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
inv = p.get_component_by_class(unreal.MOInventoryComponent)
eq = p.get_component_by_class(unreal.MOEquipmentComponent)
g = unreal.GuidLibrary.new_guid()
added = inv.add_item_by_guid(g, 'ITEM', 1)
ok = eq.equip_from_inventory(inv, g, unreal.MOEquipmentSlot.LEFT_HAND)
out('EQUIP added=%s equipped=%s' % (added, ok))
"""

HELD = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
b = p.get_held_bow()
out('HELDBOW bow=%s mesh=%s range=%s' % (b.get_name() if b else None, b.get_skeletal_mesh_asset().get_name() if b and b.get_skeletal_mesh_asset() else None, b.get_draw_range_cm() if b else None))
"""

CAM = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
l = p.get_actor_location(); f = p.get_actor_forward_vector(); r = p.get_actor_right_vector()
out('POSE %f %f %f %f %f %f %f %f %f' % (l.x, l.y, l.z, f.x, f.y, f.z, r.x, r.y, r.z))
"""


def look(view):
    v = [float(x) for x in q(CAM, "POSE").split()]
    if len(v) != 9:
        raise SystemExit("FAIL: the instance gave no pose (crashed? see its game.log and Saved/Crashes)")
    l, f, r = v[0:3], v[3:6], v[6:9]
    target = [l[i] + f[i] * 25.0 for i in range(3)]
    target[2] += 45.0
    if view == "side":       # her left (the bow's side), level with the bow: the limbs' bend in profile
        cam = [l[i] - r[i] * 260.0 + f[i] * 25.0 for i in range(3)]
        cam[2] += 50.0
        fov = 55
    elif view == "front":    # ahead of her and to the left: the bow's back, her face behind it
        cam = [l[i] + f[i] * 230.0 - r[i] * 110.0 for i in range(3)]
        cam[2] += 60.0
        fov = 55
    else:                    # close on the hands at full draw, from her left side
        cam = [l[i] - r[i] * 120.0 + f[i] * 10.0 for i in range(3)]
        cam[2] += 55.0
        target = [l[i] + f[i] * 20.0 for i in range(3)]
        target[2] += 45.0
        fov = 45
    ui.console(NAME, "MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f %d" % (cam[0], cam[1], cam[2], target[0], target[1], target[2], fov))
    time.sleep(0.3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--item", default="SimpleBow01")
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--slomo", type=float, default=0.06)   # each screenshot costs ~2 s of real time: slow enough that the plan keeps up
    ap.add_argument("--hold", type=float, default=1.0)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--reuse", action="store_true")
    a = ap.parse_args()
    try:
        if not a.reuse:
            ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1280, 720), nosteam=True)
            if not ui.wait_ready(NAME, 300):
                print("FAIL: the instance did not boot")
                return 1
            ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
            off = ui.log_size(NAME)
            ui.console(NAME, "MO.NewGame %d" % a.seed)
            if not ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420):
                print("FAIL: no character creator")
                return 1
            time.sleep(5)
            ui.console(NAME, "MO.Test.ClickWidget BeginButton")
            time.sleep(40)
        off = ui.log_size(NAME)
        eq = q(EQUIP.replace("ITEM", a.item), "EQUIP")
        print("EQUIP", eq, flush=True)
        time.sleep(1.5)   # the equipment change reaches the held visual a tick later
        print("HELDBOW", q(HELD, "HELDBOW"), flush=True)
        held = ui.wait_log(NAME, r"\[MOBow\][^\r\n]*(holds|no draw clip|does not move)[^\r\n]*", since=off, timeout=10)
        print("HELD", held.group(0) if held else "no '[MOBow] ... holds' line", flush=True)
        prefix = a.item
        for view in ("side", "front"):
            look(view)
            shot("%s_0_braced_%s" % (prefix, view))
        # the shot, slowed: Drawback is 2.47 s of game time, the hold a.hold, the loose's follow-through 0.9
        look("side")
        ui.console(NAME, "slomo %.3f" % a.slomo)
        off = ui.log_size(NAME)
        t0 = time.time()
        ui.console(NAME, "MO.Test.BowShot %.2f" % a.hold)
        k = 1.0 / a.slomo
        plan = [(1.2, "1_mid_draw", "side"), (2.47 + a.hold * 0.3, "2_full_draw", "side"), (2.47 + a.hold * 0.6, "2_full_draw_hands", "hands"),
                (2.47 + a.hold + 0.03, "3_released", "side"), (2.47 + a.hold + 0.12, "3_released_swing", "side"), (2.47 + a.hold + 0.8, "4_after", "front")]
        for at, label, view in plan:
            wait = t0 + at * k - time.time()
            if wait > 0:
                time.sleep(wait)
            look(view)
            shot("%s_%s" % (prefix, label))
        done = ui.wait_log(NAME, r"\[MOBow\] BowShot done", since=off, timeout=60)
        ui.console(NAME, "slomo 1")
        ui.console(NAME, "MO.Cam.Reset")
        text = ui.read_log_from(NAME, off)
        rows = re.findall(r"\[MOBow\] t=([\d.]+) (\w+): string ([\d.-]+) cm, fingers ([\d.-]+) cm, string to fingers ([\d.-]+) cm( \(nocked\))?", text)
        for r in rows:
            print("  t=%s %-5s string %5s fingers %5s  string->fingers %5s%s" % r, flush=True)
        # once in the fingers (the first 0.2 s blends it over to them)
        firsts = [i for i, r in enumerate(rows) if r[5]]
        nocked = [float(r[4]) for r in rows[firsts[0] + 2:] if r[5]] if firsts else []
        after = [float(r[2]) for r in rows if r[1] == "loose"]
        brace = re.search(r"\(brace ([\d.-]+)\)", held.group(0)) if held else None
        brace_cm = float(brace.group(1)) if brace else None
        worst = max(nocked) if nocked else None
        settled = after[-1] if after else None
        ok = bool(done) and worst is not None and worst <= 3.0 and brace_cm is not None and settled is not None and abs(settled - brace_cm) < 0.5
        print("BOW %s: %d samples nocked, string within %.1f cm of the fingers at worst; after the loose the string ends at %s cm (brace %s); "
              "screenshots %s" % ("PASS" if ok else "FAIL", len(nocked), worst if worst is not None else -1, settled, brace_cm, SHOTS), flush=True)
        return 0 if ok else 1
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
