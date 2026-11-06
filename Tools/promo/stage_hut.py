"""MO57 promo: the building shot's site (G14_wall; Wes, Nov 5: "the shot should be further away and show more of the building system"). Run on a live promo instance.

    python Tools/promo/stage_hut.py SX SY [--clear]     lay it out (--clear: first remove what stands within 25 m)
    python Tools/promo/stage_hut.py --job               only write the job again

Everything goes through the player's own placement path (MO.Test.PlacePiece: the preview, the snapping, the validity); server reach is off because the pawn is the hidden camera. Finished pieces are finished with
MO.Test.PieceProgress done. Left at nothing for the shot to raise, in real stages under its TIME-LAPSE tag: the near wall's wattle and then its moss, and the upper course behind it.
Prints the pieces and writes G14_wall's actions into Tools/promo/shots.json. The camera keys are the job's own: they are chosen by eye and left alone here.
"""
import json
import math
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import ue_inst as ui  # noqa: E402

NAME = "promo"
SHOTS = os.path.join(HERE, "shots.json")
FILM_FRAMES = 120
SIM_FRAMES = FILM_FRAMES * 4


def q(code, key, timeout=60):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    rows = re.findall(r"^%s (.*)$" % re.escape(key), out, re.M)
    return rows[-1] if rows else "ERR " + out[-500:]


def cmd(line, pat, timeout=30):
    off = ui.log_size(NAME)
    ui.console(NAME, line)
    m = ui.wait_log(NAME, pat + r"[^\r\n]*", since=off, timeout=timeout)
    return m.group(0) if m else ""


def ground(x, y):
    return float(q(r"""
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
g = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(%f, %f, 4000.0), unreal.Vector(%f, %f, -2000.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], unreal.DrawDebugTrace.NONE, True)
out('GZ %%.1f' %% (g.to_tuple()[4].z if g else -99999))
""" % (x, y, x, y), "GZ"))


def place(recipe, at, presses=0, quiet=False):
    line = cmd("MO.Test.PlacePiece %s %.1f %.1f %.1f @world %d" % (recipe, at[0], at[1], at[2], presses), r"\[MOTEST\] (FAIL )?PlacePiece")
    if not quiet:
        print(line[:170], flush=True)
    m = re.search(r"valid=(\d) at=\(([-\d.]+),([-\d.]+),([-\d.]+)\) yaw=([-\d.]+) placed=(\d) (\S+)", line)
    return {"recipe": recipe, "at": tuple(float(m.group(i)) for i in (2, 3, 4)), "yaw": float(m.group(5)), "placed": m.group(6) == "1", "name": m.group(7)} if m else None


def world_of(piece, local):
    yaw = math.radians(piece["yaw"])
    c, s = math.cos(yaw), math.sin(yaw)
    x, y, z = local
    return (piece["at"][0] + c * x - s * y, piece["at"][1] + s * x + c * y, piece["at"][2] + z)


def done(piece):
    return cmd("MO.Test.PieceProgress done %s" % piece["name"], r"\[MOTEST\] (FAIL )?PieceProgress")


def put(recipe, at, presses=0, finish=True):
    p = place(recipe, at, presses)
    if not p or not p["placed"]:
        print("REFUSED", recipe, at, flush=True)
        return None
    if finish:
        done(p)
    return p


def main():
    if "--job" in sys.argv:
        write_job()
        return
    sx, sy = float(sys.argv[1]), float(sys.argv[2])
    ui.console(NAME, "MO.Cam.Reset")
    if "--clear" in sys.argv:
        print(q(r"""
n = 0
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    c = a.get_class().get_name()
    if c.startswith('BP_Piece_') or c.startswith('BP_Buildable'):
        o = a.get_actor_location()
        if abs(o.x - %f) < 2500 and abs(o.y - %f) < 2500:
            a.destroy_actor(); n += 1
out('CLEARED %%d' %% n)
""" % (sx, sy), "CLEARED"), flush=True)
    print(q(r"""
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
pawn.set_actor_location(unreal.Vector(%f, %f, %f), False, True)
pawn.set_actor_hidden_in_game(True)
out('MOVED ok')
""" % (sx - 500, sy, ground(sx, sy) + 300.0), "MOVED"))
    time.sleep(10)
    ui.console(NAME, "MO.Building.ServerReach.Disable 1")
    gz = ground(sx, sy)
    print("ground", gz, flush=True)

    # the floor: three by three, stick floor at the back, planks at the front
    floors = {}
    f0 = put("BuildStickFloor", (sx, sy, gz + 45.0))
    if not f0:
        sys.exit("no floor")
    floors[(0, 0)] = f0
    for i in range(3):
        for j in range(3):
            if (i, j) == (0, 0):
                continue
            base = floors.get((i - 1, j)) or floors.get((i, j - 1))
            off = (100.0, 0.0, 0.0) if (i - 1, j) in floors else (0.0, 100.0, 0.0)
            floors[(i, j)] = put("BuildPlankFloor" if i == 2 else "BuildStickFloor", world_of(base, off))

    def wall(i, j, edge, recipe="BuildWattleWall", finish=True):
        off, presses = {"-X": ((-46.0, 0.0, 6.0), 2), "+Y": ((0.0, 46.0, 6.0), 1), "-Y": ((0.0, -46.0, 6.0), 3), "+X": ((46.0, 0.0, 6.0), 0)}[edge]
        return put(recipe, world_of(floors[(i, j)], off), presses=presses, finish=finish)

    def stack(below, recipe="BuildWattleWall", finish=True):
        plane = world_of(below, (-50.0, 0.0, 0.0))
        return put(recipe, (plane[0], plane[1], plane[2] + 50.0), finish=finish)

    def daub(w, recipe="DaubWallMoss"):
        return put(recipe, world_of(w, (-50.0, 0.0, 0.0))) if w else None

    # the far right side, two courses: moss, bare, clay
    b0 = wall(0, 0, "-X")
    b1 = wall(0, 1, "-X")
    b2 = wall(0, 2, "-X")
    b0u, b1u, b2u = stack(b0), stack(b1), stack(b2)
    daub(b0)
    daub(b0u)
    daub(b2, "DaubWallClay")
    daub(b2u, "DaubWallClay")
    # the far left side: wall, door, wall; the near end of its second course is the one the shot raises
    l0 = wall(0, 2, "+Y")
    door = wall(1, 2, "+Y", recipe="BuildWattleDoor")
    l2 = wall(2, 2, "+Y")
    l0u = stack(l0)
    daub(l0, "DaubWallClay")
    daub(l0u, "DaubWallClay")
    raised_upper = stack(l2, finish=False)
    # the near right side: one finished, the next is the shot's: stakes, rods, then a thin moss
    n0 = wall(0, 0, "-Y")
    daub(n0)
    focal = wall(1, 0, "-Y", finish=False)
    # a pole at each open corner and a rail along the open end
    posts = [put("BuildPolePost", world_of(w, (-50.0, 46.0, 0.0))) for w in (l2,) if w]
    rail = put("BuildPoleRail", world_of(floors[(2, 1)], (46.0, 0.0, 6.0)))
    # beyond it: a dry-stone wall running off, a hide tent, a lean-to
    dx, dy = sx + 620.0, sy + 200.0
    d1 = put("BuildDryStoneWall", (dx, dy, ground(dx, dy) + 50.0))
    d2 = put("BuildDryStoneWall", world_of(d1, (-50.0, 58.0, 0.0))) if d1 else None
    d3 = put("BuildDryStoneWall", world_of(d2, (-50.0, 58.0, 0.0))) if d2 else None
    tx, ty = sx - 700.0, sy - 620.0
    tent = put("BuildHideTent", (tx, ty, ground(tx, ty)))
    lx, ly = sx + 260.0, sy - 560.0
    lean = put("BuildLeanTo", (lx, ly, ground(lx, ly)))

    moving = {"focal": focal, "upper": raised_upper}
    print("MOVING", {k: v and v["name"] for k, v in moving.items()}, flush=True)
    for v in moving.values():
        if v:
            print(cmd("MO.Test.PieceProgress 0 %s" % v["name"], r"\[MOTEST\] (FAIL )?PieceProgress")[:140], flush=True)
    with open(os.path.join(ROOT, "Saved", "Promo", "shots", "hut.json"), "w", encoding="utf-8") as f:
        json.dump({"moving": moving, "moss_at": world_of(focal, (-50.0, 0.0, 0.0)) if focal else None, "focal_at": focal and focal["at"]}, f, indent=1)
    write_job()


def ramp(name, f0, f1, steps=20):
    return [[int(f0 + (f1 - f0) * i / steps), "MO.Test.PieceProgress %.3f %s" % (i / steps, name)] for i in range(steps + 1)] + [[int(f1) + 2, "MO.Test.PieceProgress done %s" % name]]


def write_job():
    """G14_wall's actions: the near wall's real stages, then its moss; the upper wall behind it. Fractions of a shot's 480 simulation frames, far faster than the game's pace (the shot's TIME-LAPSE tag)."""
    with open(os.path.join(ROOT, "Saved", "Promo", "shots", "hut.json"), encoding="utf-8") as f:
        hut = json.load(f)
    m = hut["moving"]
    moss = hut["moss_at"]
    actions = ramp(m["focal"]["name"], 0, 205) + ramp(m["upper"]["name"], 30, 290)
    actions.append([212, "MO.Test.PlacePiece DaubWallMoss %.1f %.1f %.1f @world 0" % tuple(moss)])
    actions += [[220 + int(250 * i / 20), "MO.Test.PieceProgress %.3f MossDaub" % (i / 20)] for i in range(21)]     # the pawn is at this wall: the nearest moss is the new one
    actions.sort(key=lambda a: a[0])
    d = json.load(open(SHOTS, encoding="utf-8"))
    job = d["G14_wall"]
    job["film_frames"] = FILM_FRAMES
    job["actions"] = actions
    job["pawn"] = {"hide": True, "fly": True, "at": [m["focal"]["at"][0], m["focal"]["at"][1], m["focal"]["at"][2] + 200.0]}
    json.dump(d, open(SHOTS, "w", encoding="utf-8", newline=chr(10)), indent=1)
    print("JOB G14_wall:", len(actions), "actions", flush=True)


if __name__ == "__main__":
    main()
