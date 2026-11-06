"""Do an AI-walked survivor's legs walk? (Wes, Nov 6, watching the promo's G19: "the player is like drag floating instead of walking".) On a live instance (default "promo"), this measures:

  1. the player, walking by real input: the control, the gait a walk should have;
  2. a recruited survivor sent somewhere by the real order (AMOSurvivorController::SetStayAtLocation), as the game leaves her;

    python Tools/check_ai_walk.py [--name promo] [--seconds 6] [--survivor BP_MOMetaHuman_Female_C_N] [--no-control]

A line per case: speed, foot passes a second, how far the feet swing. PASS when the survivor's feet pass about as often as the player's for her speed. Nothing here changes a file; the instance's world is changed.
"""
import argparse
import json
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

SEQ = os.path.join(ue.ROOT, "Content", "Python", "walk_probe.py")
JOB = os.path.join(ue.ROOT, "Saved", "Promo", "shots", "walk_probe.json")


def q(name, code, key, timeout=60):
    _, out = ui.call(name, ["py", "-c", code], timeout=timeout)
    rows = re.findall(r"^%s (.*)$" % re.escape(key), out, re.M)
    return rows[-1] if rows else "ERR " + out[-600:]


def probe(name, job):
    os.makedirs(os.path.dirname(JOB), exist_ok=True)
    with open(JOB, "w", encoding="utf-8") as f:
        json.dump(job, f)
    _, out = ui.call(name, ["seq", SEQ, "--timeout", str(int(job["seconds"]) * 40 + 120)], timeout=int(job["seconds"]) * 40 + 180)
    rows = re.findall(r"WALKPROBE (\{.*\})", out)
    if not rows:
        return {"error": out[-500:]}
    return json.loads(rows[-1])


def show(label, r):
    print("%-26s %s" % (label, json.dumps(r)), flush=True)
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="promo")
    ap.add_argument("--seconds", type=float, default=6.0)
    ap.add_argument("--survivor")
    ap.add_argument("--no-control", action="store_true")
    a = ap.parse_args()
    n = a.name
    player = q(n, "p = unreal.GameplayStatics.get_player_pawn(world, 0); o = p.get_actor_location(); y = p.get_actor_rotation().yaw; out('ME %s %f %f %f %f' % (p.get_name(), o.x, o.y, o.z, y))", "ME").split()
    me, x, y, z, yaw = player[0], float(player[1]), float(player[2]), float(player[3]), float(player[4])
    suffix = me.rsplit("BP_MOMetaHuman_", 1)[-1]
    control = None
    if not a.no_control:
        ui.console(n, "MO.Test.InjectInput IA_MOMove 0 1 9999")
        time.sleep(1.0)
        control = show("player, by input", probe(n, {"pawn": suffix, "seconds": a.seconds}))
        ui.console(n, "MO.Test.InjectInput IA_MOMove 0 0 0.1")
        time.sleep(1.0)
        player = q(n, "p = unreal.GameplayStatics.get_player_pawn(world, 0); o = p.get_actor_location(); y = p.get_actor_rotation().yaw; out('ME %s %f %f %f %f' % (p.get_name(), o.x, o.y, o.z, y))", "ME").split()
        x, y, z, yaw = float(player[1]), float(player[2]), float(player[3]), float(player[4])
    name = a.survivor
    if not name:
        before = set(q(n, "out('PS ' + ' '.join(a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)))", "PS").split())
        ui.console(n, "MO.Colony.SpawnSurvivor 300")
        time.sleep(3.0)
        after = set(q(n, "out('PS ' + ' '.join(a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)))", "PS").split())
        fresh = [p for p in after - before if "MetaHuman" in p]
        if not fresh:
            sys.exit("no survivor spawned: %s" % (after - before))
        name = fresh[0]
        ui.console(n, "MO.Colony.Recruit " + name)
        time.sleep(3.0)
    suffix = name.rsplit("BP_MOMetaHuman_", 1)[-1]
    print("survivor", name, flush=True)
    import math
    a_rad = math.radians(yaw)
    goal = [x + math.cos(a_rad) * 3000.0, y + math.sin(a_rad) * 3000.0, z]
    got = show("survivor, AI order", probe(n, {"pawn": suffix, "seconds": a.seconds, "goal": goal}))
    if control and "foot_passes_per_s" in got and "foot_passes_per_s" in control and control["speed"] and got["speed"]:
        per_m_control = control["foot_passes_per_s"] / (control["speed"] / 100.0)
        per_m_got = got["foot_passes_per_s"] / (got["speed"] / 100.0)
        ok = per_m_got >= 0.7 * per_m_control and got["speed"] > 20.0
        print("foot passes per metre: player %.2f, survivor %.2f -> %s" % (per_m_control, per_m_got, "AI WALK PASS" if ok else "AI WALK FAIL"), flush=True)


if __name__ == "__main__":
    main()
