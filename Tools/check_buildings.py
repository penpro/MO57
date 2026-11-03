"""The building pieces in a game (Nov 2 2026; Docs/Building.md section 7). One -game instance on a new world: a small site is laid out through
the player's own placement path (MO.Test.PlacePiece: the preview, the snapping, an attaching piece's host, a floor's posts), pieces are shown
part way through their work (their parts popping in) and finished, then measured:

  - every piece lands where the snapping puts it: a wall on a floor's edge, a daub on its wall, a post on a wall's side, a rail on its top,
    a thatch on its roof, a wall on a foundation's edge, a door in a floor's edge;
  - a floor set 70 cm above the ground grows posts to the ground; the floor snapped beside it shares the posts at their common edge;
  - part way through only the first parts show; finished, all of them;
  - the door opens and closes (its leaf turns);
  - behind a bare wattle wall the wind passes, behind the same wall moss-daubed it does not (MO.Test.ShelterHere);
  - inside the hide tent: covered.

    python Tools/check_buildings.py [--seed 777] [--keep] [--reuse] [--shots]

Prints a line per check and "BUILDINGS PASS" / "BUILDINGS FAIL". Shots (--shots): Saved/Screenshots/buildings/<label>.png.
"""
import argparse
import glob
import math
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "buildcheck"
SHOTS = os.path.join(ue.ROOT, "Saved", "Screenshots", "buildings")
GROUND = """r = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(@X@, @Y@, @Z@ + 3000), unreal.Vector(@X@, @Y@, @Z@ - 3000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [unreal.GameplayStatics.get_player_pawn(world, 0)], unreal.DrawDebugTrace.NONE, True)
out('GROUND %f' % (r.to_tuple()[4].z if r else -1e9))
"""
PAWN = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
l = p.get_actor_location()
out('PAWN %f %f %f' % (l.x, l.y, l.z))
"""
fails = []
P = [0.0, 0.0, 0.0]


def q(code, key, timeout=60):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    m = re.search(key + r" ([^\r\n]*)", out)
    return m.group(1) if m else ""


def cmd(line, pattern, timeout=15):
    """Run a console line; the first log line matching pattern after it."""
    off = ui.log_size(NAME)
    ui.console(NAME, line)
    m = ui.wait_log(NAME, pattern + r"[^\r\n]*", since=off, timeout=timeout)
    return m.group(0) if hasattr(m, "group") else (m if isinstance(m, str) else "")


def check(ok, label, detail=""):
    print("%s %s %s" % ("PASS" if ok else "FAIL", label, detail.strip()), flush=True)
    if not ok:
        fails.append(label)
    return ok


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


SHOOT = False


def look(cam, target, label, fov=60):
    if not SHOOT:
        return None
    ui.console(NAME, "MO.Cam.LookAt %.1f %.1f %.1f %.1f %.1f %.1f %d" % (cam[0], cam[1], cam[2], target[0], target[1], target[2], fov))
    time.sleep(1.5)
    return shot(label)


def world_of(piece, local):
    """A point in a placed piece's frame (its yaw only: the pieces stand upright) in the world."""
    yaw = math.radians(piece["yaw"])
    c, s = math.cos(yaw), math.sin(yaw)
    x, y, z = local
    return (piece["at"][0] + c * x - s * y, piece["at"][1] + s * x + c * y, piece["at"][2] + z)


def place(recipe, aim, presses=0):
    """Aim at a world point and place through the player's path; presses: Q/E first (the edge of a floor a wall takes: 0 +X, 1 +Y, 2 -X, 3 -Y)."""
    line = cmd("MO.Test.PlacePiece %s %.1f %.1f %.1f @world %d" % (recipe, aim[0], aim[1], aim[2], presses), r"\[MOTEST\] (FAIL )?PlacePiece")
    m = re.search(r"valid=(\d) at=\(([-\d.]+),([-\d.]+),([-\d.]+)\) yaw=([-\d.]+) placed=(\d) (\S+)", line or "")
    if not m:
        return None, line
    return {"valid": m.group(1) == "1", "at": tuple(float(m.group(i)) for i in (2, 3, 4)), "yaw": float(m.group(5)), "placed": m.group(6) == "1",
            "name": m.group(7), "line": line}, line


def info_line(name):
    return cmd("MO.Test.PieceInfo 6000", r"\[MOTEST\] PieceInfo " + re.escape(name) + " ")


def progress(frac, name):
    return cmd("MO.Test.PieceProgress %s %s" % (frac, name), r"\[MOTEST\] (FAIL )?PieceProgress")


def shown(line):
    m = re.search(r"shown=(\d+)/(\d+)", line or "")
    return (int(m.group(1)), int(m.group(2))) if m else (-1, -1)


def posts(line):
    m = re.search(r"posts=\[([^\]]*)\]", line or "")
    return [float(v) for v in m.group(1).split(",")] if m and m.group(1) else []


def near(a, b, tol=2.0):
    return all(abs(a[i] - b[i]) <= tol for i in range(3))


def shelter(at, wind_yaw):
    ui.console(NAME, "MO.Player.Teleport %.1f %.1f %.1f" % at)
    time.sleep(1.5)
    line = cmd("MO.Test.ShelterHere %.1f" % wind_yaw, r"\[MOTEST\] ShelterHere")
    m = re.search(r"cover=([\d.]+) wind=([\d.]+) walls=(\d+)/(\d+) enclosure=([\d.]+) indoors=(\d)", line or "")
    return (float(m.group(1)), float(m.group(2)), int(m.group(3)), float(m.group(5)), m.group(6) == "1") if m else None, line


def ray(at, yaw, length, kind="wind", pitch=0.0):
    ui.console(NAME, "MO.Player.Teleport %.1f %.1f %.1f" % at)
    time.sleep(1.5)
    line = cmd("MO.Test.ShelterRay %.1f %.1f %s %.1f" % (yaw, length, kind, pitch), r"\[MOTEST\] ShelterRay")
    m = re.search(r"block=([\d.]+)", line or "")
    return float(m.group(1)) if m else -1.0


def build_stages(piece, label, cam, target):
    """A third and two thirds of the way through its work, then finished: the parts popping in."""
    seen = []
    for frac in ("0.34", "0.67", "done"):
        line = progress(frac, piece["name"])
        seen.append(shown(line))
        look(cam, target, "%s_%s" % (label, frac.replace(".", "")))
    s3, s6, sd = seen
    check(0 < s3[0] < s6[0] < sd[0] and sd[0] == sd[1], "%s pops in as the work goes on" % label, "shown %s -> %s -> %s" % (s3, s6, sd))


def main():
    global SHOOT
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--reuse", action="store_true")
    ap.add_argument("--shots", action="store_true")
    a = ap.parse_args()
    SHOOT = a.shots
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
        P[:] = [float(v) for v in q(PAWN, "PAWN").split()]
        ui.console(NAME, "MO.Building.ServerReach.Disable 1")   # the site is wider than one standing reach; the pawn is moved for the shelter readings

        def ground(x, y):
            v = q(GROUND.replace("@X@", repr(x)).replace("@Y@", repr(y)).replace("@Z@", repr(P[2])), "GROUND")
            return float(v) if v else P[2] - 90.0

        sx, sy = P[0] + 600.0, P[1]
        gz = ground(sx, sy)
        print("site at (%.0f, %.0f), ground %.1f, pawn %s" % (sx, sy, gz, P), flush=True)

        # ---- a stick floor 70 cm up: posts to the ground ----
        f1, line = place("BuildStickFloor", (sx, sy, gz + 70.0))
        check(f1 and f1["placed"], "stick floor placed 70 cm above the ground", line)
        if not f1:
            return 1
        il = info_line(f1["name"])
        p1 = posts(il)
        check(len(p1) == 4 and all(60.0 < v < 85.0 for v in p1), "the raised floor grows four posts to the ground", "posts %s" % p1)
        # ---- a plank floor snapped beside it: the shared edge's posts are the stick floor's ----
        f2, line = place("BuildPlankFloor", world_of(f1, (100.0, 0.0, 0.0)))
        check(f2 and f2["placed"] and near(f2["at"], world_of(f1, (100.0, 0.0, 0.0)), 3.0), "plank floor snapped beside it", line)
        p2 = posts(info_line(f2["name"])) if f2 else []
        check(len(p2) == 4 and sum(1 for v in p2 if v > 0) == 2, "the plank floor shares the two posts on the common edge", "posts %s" % p2)
        if f2:
            build_stages(f2, "plank_floor", world_of(f1, (60.0, -330.0, 160.0)), world_of(f2, (0.0, 0.0, -20.0)))
        progress("done", f1["name"])
        # ---- a wattle wall on the stick floor's far edge (-X), a moss daub on it, a post on its side, a rail along its top ----
        w1, line = place("BuildWattleWall", world_of(f1, (-46.0, 0.0, 6.0)), presses=2)
        check(w1 and w1["placed"], "wattle wall stands on the floor's edge", line)
        if w1:
            build_stages(w1, "wattle_wall", world_of(f1, (180.0, -120.0, 120.0)), world_of(w1, (-50.0, 0.0, 0.0)))
        # the wind through the bare wall: one shelter ray from the floor behind it, out through the wall and no farther (the hill beyond would
        # stop any ray that went on: the wind rays reach 15 m)
        inside = world_of(f1, (10.0, 0.0, 100.0))
        bare = ray(inside, (f1["yaw"] + 180.0) % 360.0, 110.0)
        daub, line = place("DaubWallMoss", world_of(w1, (-50.0, 0.0, 0.0))) if w1 else (None, "")
        check(daub and daub["placed"] and near(daub["at"], w1["at"], 0.5), "moss daub goes on the wattle wall (its frame)", line)
        if daub:
            build_stages(daub, "moss_daub", world_of(f1, (180.0, -120.0, 120.0)), world_of(w1, (-50.0, 0.0, 0.0)))
        daubed = ray(inside, (f1["yaw"] + 180.0) % 360.0, 110.0)
        check(0.45 <= bare <= 0.65 and daubed >= 0.95, "the bare wattle lets the wind through, the moss daub stops it",
              "the ray through the wall stops %.2f of the wind bare, %.2f daubed" % (bare, daubed))
        post, line = place("BuildPolePost", world_of(w1, (-50.0, 46.0, 0.0))) if w1 else (None, "")
        check(post and post["placed"] and near(post["at"], world_of(w1, (-50.0, 50.0, 0.0)), 1.0), "pole post on the wall's side edge", line)
        rail, line = place("BuildPoleRail", world_of(w1, (-50.0, 0.0, 46.0))) if w1 else (None, "")
        check(rail and rail["placed"] and near(rail["at"], world_of(w1, (-50.0, 0.0, 50.0)), 1.0), "pole rail on the wall's top edge", line)
        dup, line = place("DaubWallMoss", world_of(w1, (-50.0, 0.0, 0.0))) if w1 else (None, "")
        check(dup and not dup["placed"], "CONTROL: a second moss daub on the same wall is refused", line)
        for n in (post, rail):
            if n:
                progress("done", n["name"])
        # ---- a door in the stick floor's +Y edge ----
        door, line = place("BuildWattleDoor", world_of(f1, (0.0, 46.0, 6.0)), presses=1)
        check(door and door["placed"], "wattle door stands on the floor's edge", line)
        if door:
            progress("done", door["name"])
            o = cmd("MO.Test.PieceDoor open", r"\[MOTEST\] PieceDoor")
            time.sleep(1.5)
            look(world_of(f1, (0.0, 260.0, 150.0)), world_of(f1, (0.0, 46.0, 80.0)), "wattle_door_open")
            c = cmd("MO.Test.PieceDoor close", r"\[MOTEST\] PieceDoor")
            check("door=open" in o and "door=closed" in c, "the door opens and closes", "%s | %s" % (o[-80:], c[-80:]))
        # ---- a stick roof, free standing, and thatch on it ----
        rx, ry = sx, sy + 450.0
        roof, line = place("BuildStickRoof", (rx, ry, ground(rx, ry) + 60.0))
        check(roof and roof["placed"], "stick roof placed", line)
        if roof:
            progress("done", roof["name"])
            th, line = place("ThatchRoof", world_of(roof, (-50.0, 0.0, 0.0)))
            check(th and th["placed"] and near(th["at"], roof["at"], 0.5), "thatch goes on the stick roof (its frame)", line)
            if th:
                build_stages(th, "thatch", world_of(roof, (220.0, -160.0, 160.0)), world_of(roof, (-50.0, 0.0, 0.0)))
        # ---- a cobble foundation, a cobble wall on its edge; a cobble-and-daub infill framed in timber ----
        cx, cy = sx, sy - 450.0
        fnd, line = place("BuildCobbleFoundation", (cx, cy, ground(cx, cy) + 5.0))
        check(fnd and fnd["placed"], "cobble foundation placed", line)
        if fnd:
            build_stages(fnd, "cobble_foundation", world_of(fnd, (230.0, -170.0, 140.0)), world_of(fnd, (0.0, 0.0, -10.0)))
            cw, line = place("BuildCobbleWall", world_of(fnd, (46.0, 0.0, 6.0)))
            check(cw and cw["placed"], "cobble wall on the foundation's edge", line)
            if cw:
                build_stages(cw, "cobble_wall", world_of(fnd, (260.0, -170.0, 150.0)), world_of(cw, (-50.0, 0.0, 0.0)))
            fnd2, line = place("BuildCobbleFoundation", world_of(fnd, (0.0, 100.0, 0.0)))
            check(fnd2 and fnd2["placed"] and near(fnd2["at"], world_of(fnd, (0.0, 100.0, 0.0)), 3.0), "a second foundation joins beside it", line)
            if fnd2:
                progress("done", fnd2["name"])
                inf, line = place("BuildCobbleInfill", world_of(fnd2, (46.0, 0.0, 6.0)))
                check(inf and inf["placed"], "cobble-and-daub infill on the foundation's edge", line)
                if inf:
                    build_stages(inf, "cobble_infill", world_of(fnd2, (260.0, -150.0, 150.0)), world_of(inf, (-50.0, 0.0, 0.0)))
                    tp, line = place("BuildTimberPost", world_of(inf, (-50.0, 46.0, 0.0)))
                    check(tp and tp["placed"], "timber post on the infill's side", line)
                    tb, line = place("BuildTimberBeam", world_of(inf, (-50.0, 0.0, 46.0)))
                    check(tb and tb["placed"], "timber beam on the infill's top", line)
                    br, line = place("BuildTimberBrace", world_of(inf, (-50.0, 0.0, 0.0)))
                    check(br and br["placed"], "timber brace on the infill's face", line)
                    for n in (tp, tb, br):
                        if n:
                            progress("done", n["name"])
                    look(world_of(fnd2, (260.0, -150.0, 150.0)), world_of(inf, (-50.0, 0.0, 0.0)), "timber_frame")
        # ---- shelters ----
        tx, ty = sx + 900.0, sy
        tent, line = place("BuildHideTent", (tx, ty, ground(tx, ty)))
        check(tent and tent["placed"], "hide tent placed", line)
        if tent:
            build_stages(tent, "hide_tent", world_of(tent, (520.0, -380.0, 260.0)), world_of(tent, (0.0, 0.0, 130.0)))
            s, line = shelter(world_of(tent, (0.0, 0.0, 100.0)), 0.0)
            check(s and s[0] >= 0.8, "inside the hide tent: covered", line)
        lx, ly = sx + 900.0, sy + 600.0
        lean, line = place("BuildLeanTo", (lx, ly, ground(lx, ly)))
        check(lean and lean["placed"], "thatch lean-to placed", line)
        if lean:
            build_stages(lean, "thatch_leanto", world_of(lean, (420.0, -360.0, 220.0)), world_of(lean, (-80.0, 0.0, 80.0)))
        hx, hy = sx + 900.0, sy - 600.0
        hide, line = place("BuildHideLeanTo", (hx, hy, ground(hx, hy)))
        check(hide and hide["placed"], "hide lean-to placed", line)
        if hide:
            progress("done", hide["name"])
            look(world_of(hide, (420.0, -360.0, 220.0)), world_of(hide, (-80.0, 0.0, 80.0)), "hide_leanto")
        # the whole site
        look((sx + 450.0, sy - 1100.0, gz + 650.0), (sx + 350.0, sy, gz + 60.0), "site", 70)
        look(world_of(f1, (300.0, 250.0, 200.0)), world_of(f1, (0.0, 0.0, 30.0)), "floors_posts")
        print("BUILDINGS %s%s" % ("PASS" if not fails else "FAIL", "" if not fails else " (%s)" % "; ".join(fails)), flush=True)
        return 0 if not fails else 1
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
