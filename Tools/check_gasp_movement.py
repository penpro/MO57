"""The survivor's movement since Wes's Oct 28 calls ("match the animation project settings", "fit the capsule to somewhere in that range",
the body's sex: "add it"), in one -game instance after the creator (Docs/Animation_GASP.md):

- the capsule is 185 cm (half height 92.5) and the feet stand on its floor;
- the gaits move at GASP's speeds: walk 200, jog 500, sprint 700 cm/s; a jump rises about 1.27 m (500 cm/s);
- an attack that turns a STANDING survivor to its aim turns her body too (GASP holds a standing body's facing: the driver's hold,
  AMOCharacter::HoldBodyToCapsule) -- the head's world yaw follows the capsule;
- the body's sex comes from the base: Female on the femme and Jalena, Male on Harvey.

    python Tools/check_gasp_movement.py [--keep]

Prints each step and "GASP MOVEMENT PASS" / "FAIL".
"""
import re
import sys
import time

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402
import check_creator_bases as cb  # noqa: E402

NAME = cb.NAME
PAWN = "p = unreal.GameplayStatics.get_player_pawn(world, 0); "


def q(code, key):
    _, out = ui.call(NAME, ["py", "-c", PAWN + code], timeout=30)
    m = re.search(key + r" ([^\r\n]*)", out)
    return m.group(1) if m else ""


def speed_during(cmd, seconds=2.5):
    ui.console(NAME, cmd)
    time.sleep(seconds)
    return float(q("v = p.get_velocity(); out('SPEED %.0f' % ((v.x * v.x + v.y * v.y) ** 0.5))", "SPEED") or -1)


def yaw_gap(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


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
        cb.query("MO.Test.ClickWidget BeginButton", r"ClickWidget: '[^']*BeginButton")
        time.sleep(8)

        cap = q("c = p.get_editor_property('capsule_component'); m = p.get_editor_property('mesh'); "
                "floor = p.get_actor_location().z - c.get_scaled_capsule_half_height(); "
                "out('CAP %.2f %.1f %.3f %.2f' % (c.get_unscaled_capsule_half_height(), min(m.get_socket_location('foot_l').z, m.get_socket_location('foot_r').z) - floor, "
                "p.get_actor_scale3d().z, c.get_scaled_capsule_half_height()))", "CAP")
        half, feet, zscale, scaled = (float(x) for x in cap.split()) if cap else (-1, -1, -1, -1)
        rep.step("the capsule is 185 cm tall, unscaled (the actor's scale 1), and the feet stand on its floor (within 15 cm)",
                 abs(half - 92.5) < 0.1 and abs(zscale - 1.0) < 0.001 and abs(feet) < 15, f"half {half}, feet {feet}, actor z scale {zscale}, scaled half {scaled}")
        sex = q("out('SEX %s' % p.get_body_sex())", "SEX")
        rep.step("the femme's body is female", "FEMALE" in sex, sex)

        walk = speed_during("MO.Footstep.Walk 6")
        time.sleep(4)
        ui.console(NAME, "MO.Test.InjectInput IA_Hustle 1 0 0.1")   # a tap toggles the jog
        time.sleep(0.5)
        jog = speed_during("MO.Footstep.Walk 6")
        time.sleep(4)
        ui.console(NAME, "MO.Test.InjectInput IA_Hustle 1 0 0.1")
        time.sleep(0.5)
        sprint = speed_during("MO.Footstep.Walk 6 sprint", 4.0)
        time.sleep(3)
        rep.step("walk / jog / sprint at GASP's 200 / 500 / 700 cm/s", abs(walk - 200) <= 15 and abs(jog - 500) <= 25 and abs(sprint - 700) <= 30,
                 f"walk {walk:.0f}, jog {jog:.0f}, sprint {sprint:.0f}")

        jump = q("m = p.get_movement_component(); out('JUMP %.0f %.2f %.0f' % (m.get_editor_property('jump_z_velocity'), "
                 "m.get_editor_property('air_control'), m.get_editor_property('braking_deceleration_falling')))", "JUMP")
        rep.step("jump 500 cm/s (a 1.27 m rise; it was 700: 2.5 m), air control 0.25, no air braking", jump == "500 0.25 0", jump)

        # A standing survivor turned by the game: GASP holds the body's facing (control), unless the turn is an attack's (the driver's hold).
        head = "out('HEAD %.1f %.1f' % (p.get_editor_property('mesh').get_socket_rotation('head').yaw, p.get_actor_rotation().yaw))"

        def offset():
            h, a = (float(x) for x in q(head, "HEAD").split())
            return (h - a + 180.0) % 360.0 - 180.0, a
        base_off, a0 = offset()
        ui.call(NAME, ["py", "-c", PAWN + "r = p.get_actor_rotation(); p.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=r.yaw + 90.0), False); out('TURNED')"], timeout=30)
        time.sleep(2.0)
        held_off, a1 = offset()
        rep.step("CONTROL: the capsule turned 90 degrees with no action -> the standing body keeps its facing (GASP's root offset)",
                 yaw_gap(a1, a0) > 60 and yaw_gap(held_off, base_off) > 50, f"capsule {a0:.0f} -> {a1:.0f}, head-to-capsule {base_off:.0f} -> {held_off:.0f}")
        # The camera aims back where the body faces (90 degrees off the capsule): an attack turns her to it, and the hold lets the body follow.
        ui.call(NAME, ["py", "-c", "pc = unreal.GameplayStatics.get_player_controller(world, 0); c = pc.get_control_rotation(); "
                       "pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=c.pitch, yaw=c.yaw - 90.0)); out('AIMED')"], timeout=30)
        time.sleep(1.0)
        ui.console(NAME, "MO.Test.Attack")
        time.sleep(2.0)
        att_off, a2 = offset()
        rep.step("an attack turns the standing survivor to her aim, and her body lines up with the capsule (the driver's hold)",
                 yaw_gap(a2, a1) > 45 and yaw_gap(att_off, base_off) < 25, f"capsule {a1:.0f} -> {a2:.0f}, head-to-capsule {held_off:.0f} -> {att_off:.0f} (standing {base_off:.0f})")

        for base, want in (("Jalena", "FEMALE"), ("Harvey", "MALE")):
            cb.query(f"MO.Test.Base {base}", r"\[MOTEST\] Base.*")
            time.sleep(5)
            sex = q("out('SEX %s' % p.get_body_sex())", "SEX")
            rep.step(f"{base}'s body is {want.lower()} (from the base)", want in sex and (want != "MALE" or "FEMALE" not in sex), sex)
        return rep.ok
    finally:
        print("GASP MOVEMENT " + ("PASS" if rep.rows and rep.ok else "FAIL"), flush=True)
        if not keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(0 if main(keep="--keep" in sys.argv) else 1)
