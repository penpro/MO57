"""The survivor's legs, measured (Oct 27: Wes -- "the leg animations are broken now that the new character work has been done ... something to
do with the foot IKs", and the crouch tears her backside). One -game instance: the femme as her Blueprint makes her, then the Lillian base;
for each, standing, walking and crouched: the feet's spacing across the body and their height over the capsule's floor (the foot IK rig
CR_MH_F plants them: it needs the skeleton's virtual bones VB root_root / VB VB root_root_foot_l / _r), and screenshots from behind and
from the side in Saved/Screenshots/legs/ for Wes's eye.

    python Tools/check_legs.py [--keep]

Prints each measurement and "LEGS PASS" / "FAIL". The editor may stay open (an instance builds its own world).
"""
import os
import re
import statistics
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402
import check_creator_bases as cb  # noqa: E402

NAME = cb.NAME
SHOTS_OUT = os.path.join(ue.ROOT, "Saved", "Screenshots", "legs")
# Feet across the body (along the actor's right vector) and over the capsule's floor, the pelvis over the floor, the virtual bones.
SAMPLE = ("p = unreal.GameplayStatics.get_player_pawn(world, 0); m = p.get_editor_property('mesh'); c = p.get_editor_property('capsule_component'); "
          "floor = p.get_actor_location().z - c.get_scaled_capsule_half_height(); r = p.get_actor_right_vector(); "
          "fl = m.get_socket_location('foot_l'); fr = m.get_socket_location('foot_r'); pv = m.get_socket_location('pelvis'); "
          "d = fl - fr; out('LEGS across=%.1f lz=%.1f rz=%.1f pelvis=%.1f speed=%.0f crouched=%s vb=%d' % (abs(d.x * r.x + d.y * r.y), fl.z - floor, "
          "fr.z - floor, pv.z - floor, p.get_velocity().length(), p.get_editor_property('is_crouched'), m.get_bone_index('VB VB root_root_foot_l')))")
LEGS_RE = re.compile(r"LEGS across=([\d.-]+) lz=([\d.-]+) rz=([\d.-]+) pelvis=([\d.-]+) speed=(\d+) crouched=(\w+) vb=(-?\d+)")


def samples(n, gap=0.25):
    rows = []
    for _ in range(n):
        code, out = ui.call(NAME, ["py", "-c", SAMPLE], timeout=30)
        m = LEGS_RE.search(out)
        if m:
            rows.append(tuple(float(x) if i not in (5,) else x for i, x in enumerate(m.groups())))
        time.sleep(gap)
    return rows


def summary(rows):
    if not rows:
        return {}
    return {"across": statistics.median(r[0] for r in rows), "feet_z": statistics.median(min(r[1], r[2]) for r in rows),
            "pelvis": statistics.median(r[3] for r in rows), "speed": statistics.median(r[4] for r in rows), "vb": int(rows[-1][6])}


def side_shot(label):
    """A shot from behind, then one from the side: the pawn turned 90 degrees under the same camera (and back)."""
    cb.SHOTS_OUT = SHOTS_OUT
    cb.screenshot(label + "_back")
    ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); r = p.get_actor_rotation(); "
                   "p.set_actor_rotation(unreal.Rotator(0.0, r.yaw + 90.0, 0.0), False); out('TURNED')"], timeout=30)
    time.sleep(1.0)
    cb.screenshot(label + "_side")
    ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); r = p.get_actor_rotation(); "
                   "p.set_actor_rotation(unreal.Rotator(0.0, r.yaw - 90.0, 0.0), False); out('TURNED')"], timeout=30)


def measure(rep, who):
    stand = summary(samples(6))
    rep.step(f"{who}: the foot IK's virtual bones are on the skeleton", stand.get("vb", -1) >= 0, str(stand))
    # GASP's idle (Oct 27) stands relaxed, the weight on one leg and that foot set out: 31 cm across on the femme, 46 on Lillian, both
    # planted and natural by eye. Crossed or collapsed legs (the foot IK failing) read under 15.
    rep.step(f"{who} standing: feet under the hips (15-50 cm apart), on the floor (within 15 cm)",
             bool(stand) and 15 <= stand["across"] <= 50 and abs(stand["feet_z"]) <= 15, str(stand))
    side_shot(f"{who}_stand")
    ui.console(NAME, "MO.Footstep.Walk 20 walk")   # long enough for the samples (each a bridge call of a second or more)
    time.sleep(1.0)
    walk = summary(samples(6, 0.2))
    rep.step(f"{who} walking: the feet track under the hips (median 8-30 cm apart), the lower foot on the floor (within 15 cm)",
             bool(walk) and walk["speed"] > 100 and 8 <= walk["across"] <= 30 and abs(walk["feet_z"]) <= 15, str(walk))
    cb.SHOTS_OUT = SHOTS_OUT
    cb.screenshot(f"{who}_walk_back")
    time.sleep(8)   # the walk ends
    ui.console(NAME, "MO.Test.InjectInput IA_Crouch 1 0 0.3")
    time.sleep(3)
    crouch = summary(samples(6))
    # The crouch idle holds the pelvis ~35 cm over the floor; under 25 is the Oct 12 retarget's pelvis on the floor (Oct 27: "her ass falls off").
    rep.step(f"{who} crouched: the feet on the floor (within 15 cm), the pelvis down and off the floor (25-70 cm)",
             bool(crouch) and abs(crouch["feet_z"]) <= 15 and 25 <= crouch["pelvis"] < 70, str(crouch))
    side_shot(f"{who}_crouch")
    ui.console(NAME, "MO.Test.InjectInput IA_Crouch 1 0 0.3")
    time.sleep(3)
    return stand, walk, crouch


def main(keep=False):
    rep = ui.Report()
    try:
        ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1600, 900), nosteam=True)
        if not rep.step("the instance boots", ui.wait_ready(NAME, 300)):
            return False
        ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.NewGame 777")
        if not rep.step("a new game opens the character creator", bool(ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420))):
            return False
        time.sleep(5)
        cb.query("MO.Test.ClickWidget BeginButton", r"ClickWidget: '[^']*BeginButton")   # the femme as her Blueprint makes her
        time.sleep(8)
        measure(rep, "femme")
        cb.query("MO.Test.Base Lillian", r"\[MOTEST\] Base.*")
        time.sleep(6)
        measure(rep, "lillian")
        return rep.ok
    finally:
        print("LEGS " + ("PASS" if rep.rows and rep.ok else "FAIL"), flush=True)
        print("screenshots: " + SHOTS_OUT, flush=True)
        if not keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(0 if main(keep="--keep" in sys.argv) else 1)
