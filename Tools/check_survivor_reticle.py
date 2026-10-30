"""Survivors, their dialogue and the context reticle, in a real game (Wes, Oct 29 2026).

  "let's make it so there's always one within 200m with the campfire going"
  "when you interact with a survivor have a dialogue pop up ... 'If you can do better than me I'll join your village.' and prompts the
   'build a camp fire' quest line or, if it's already done have the quest complete right away with like 'Ohh you already built one!?'"
  "change the target reticle out to a small circle centered ... green to interact or red to attack or blue when it wants a right click"

One -game instance, a new world:
  1. a wandering survivor with a lit campfire within 200 m of the player; teleported 800 m away, another one within 200 m there
  2. the reticle on her is green; E -> her dialogue (greeting, camp line, the campfire ask) and the campfire quest line on the player's machine
  3. a campfire built, E again -> she joins ("You built one!"); a colonist under the reticle is blue
  4. the next survivor (the rule keeps one near) -> E -> "Ohh, you already built one!?" and she joins on the spot
  5. the reticle: an unfinished building blue, a wolf 12 m out red, the sky white (each checked in the screenshot's pixels too); the closing circle
Screenshots of each in Saved/Screenshots/reticle/ (the colours and the popup are Wes's to judge by eye).

Usage: python Tools/check_survivor_reticle.py [--seed 777] [--keep] [--reuse]
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

NAME = "survret"
SHOTS = os.path.join(ue.ROOT, "Saved", "Screenshots", "reticle")
RESULTS = []

NEAREST = '''
import unreal
p = unreal.GameplayStatics.get_player_pawn(world, 0)
best = None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter):
    if a == p or isinstance(a, unreal.MOCreature):
        continue
    rc = a.get_component_by_class(unreal.MORecruitmentComponent)
    if not rc or rc.is_possessable() or a.is_dead:
        continue
    d = a.get_distance_to(p)
    if best is None or d < best[0]:
        best = (d, a)
if best:
    l = best[1].get_actor_location()
    out("NEAREST %s %.0f %.0f %.0f %.0f" % (best[1].get_name(), best[0] / 100.0, l.x, l.y, l.z))
else:
    out("NEAREST none")
'''

# Stand Distance cm from (X, Y, Z) on the player's side, facing it.
STAND = '''
import unreal, math
p = unreal.GameplayStatics.get_player_pawn(world, 0)
t = unreal.Vector(__X__, __Y__, __Z__)
pl = p.get_actor_location()
dx, dy = pl.x - t.x, pl.y - t.y
n = math.hypot(dx, dy) or 1.0
spot = unreal.Vector(t.x + dx / n * __D__, t.y + dy / n * __D__, t.z + 20.0)
p.set_actor_location(spot, False, True)
yaw = math.degrees(math.atan2(t.y - spot.y, t.x - spot.x))
p.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), True)
unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(unreal.Rotator(0.0, 0.0, yaw))
out("STAND at %.0f %.0f %.0f yaw %.0f" % (spot.x, spot.y, spot.z, yaw))
'''

# Point the view (the camera) at (X, Y, Z): run it twice, the camera follows the control rotation a frame later.
AIM = '''
import unreal
pc = unreal.GameplayStatics.get_player_controller(world, 0)
cam = pc.player_camera_manager.get_camera_location()
rot = unreal.MathLibrary.find_look_at_rotation(cam, unreal.Vector(__X__, __Y__, __Z__))
pc.set_control_rotation(rot)
out("AIM pitch %.1f yaw %.1f" % (rot.pitch, rot.yaw))
'''

ACTOR_AT = '''
import unreal
p = unreal.GameplayStatics.get_player_pawn(world, 0)
found, best = None, None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.__CLASS__):
    if __FILTER__:
        d = a.get_distance_to(p)
        if best is None or d < best:
            found, best = a, d
l = found.get_actor_location() if found else None
out("ACTOR %s %.0f %.0f %.0f" % (found.get_name(), l.x, l.y, l.z) if found else "ACTOR none")
'''


def q(code, key, timeout=120):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    m = re.search(key + r" ([^\r\n]*)", out)
    return m.group(1) if m else ""


def check(label, ok, detail=""):
    RESULTS.append((label, ok, detail))
    print("%s  %s  %s" % ("PASS" if ok else "FAIL", label, detail))
    return ok


def shot(label):
    shots = os.path.join(ue.ROOT, "Saved", "Screenshots")
    before = set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True))
    ui.console(NAME, "shot showui")
    deadline, new = time.time() + 20, []
    while time.time() < deadline and not new:
        new = sorted(set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True)) - before)
        time.sleep(0.3)
    if new:
        time.sleep(0.5)
        os.makedirs(SHOTS, exist_ok=True)
        shutil.move(new[-1], os.path.join(SHOTS, label + ".png"))


def _png(path):
    """(width, height, bytes per pixel, rows) of an 8-bit RGB / RGBA PNG (the console's screenshots)."""
    import struct
    import zlib
    data, i, idat = open(path, "rb").read(), 8, b""
    while i < len(data):
        n = struct.unpack(">I", data[i:i + 4])[0]
        kind, chunk = data[i + 4:i + 8], data[i + 8:i + 8 + n]
        if kind == b"IHDR":
            w, h, _, colour = struct.unpack(">IIBB", chunk[:10])
        elif kind == b"IDAT":
            idat += chunk
        i += 12 + n
    raw, bpp = zlib.decompress(idat), (4 if colour == 6 else 3)
    stride, rows, prev, o = w * bpp, [], bytearray(w * bpp), 0
    for _ in range(h):
        f, line = raw[o], bytearray(raw[o + 1:o + 1 + stride])
        o += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        rows.append(line)
        prev = line
    return w, h, bpp, rows


RING_RGB = {"Interact": (0x8F, 0xBF, 0x6A), "Secondary": (0x7E, 0xA8, 0xDE), "Threat": (0xEE, 0x65, 0x50), "None": (0xEE, 0xE7, 0xD7)}


def ring_drawn(label, intent):
    """How many of 8 sectors round the screen's centre hold the intent's colour (+-30 per channel) on the ring's band (2.5-9 px out) in
    screenshot <label>: what was DRAWN, not what the widget was told (the first build reported green and drew white). A ring scores 7-8; a
    background of about that colour scores too, so test a colour against a background that is not it (blue on the sky would pass)."""
    import math
    path = os.path.join(SHOTS, label + ".png")
    if not os.path.exists(path):
        return -1
    w, h, bpp, rows = _png(path)
    want, sectors = RING_RGB[intent], set()
    for y in range(h // 2 - 10, h // 2 + 11):
        for x in range(w // 2 - 10, w // 2 + 11):
            dx, dy = x - w // 2, y - h // 2
            if 2.5 <= math.hypot(dx, dy) <= 9.0:
                px = rows[y][x * bpp:x * bpp + 3]
                if all(abs(px[k] - want[k]) <= 30 for k in range(3)):
                    sectors.add(int((math.degrees(math.atan2(dy, dx)) + 360.0) % 360.0 // 45))
    return len(sectors)


def query(verb, key):
    off = ui.log_size(NAME)
    ui.console(NAME, verb)
    ui.wait_log(NAME, re.escape("[MOQUERY] " + key), since=off, timeout=15)
    m = re.findall(r"\[MOQUERY\] " + re.escape(key) + r"[^\r\n]*", ui.read_log_from(NAME, off))
    return m[-1] if m else ""


def reticle():
    return query("MO.UI.Reticle", "RETICLE")


def aim_at(x, y, z):
    for _ in range(3):
        q(AIM.replace("__X__", str(x)).replace("__Y__", str(y)).replace("__Z__", str(z)), "AIM")
        time.sleep(0.35)
    time.sleep(0.3)   # the reticle follows within its 0.1 s focus update


def nearest():
    s = q(NEAREST, "NEAREST")
    m = re.match(r"(\S+) (\d+) (-?\d+) (-?\d+) (-?\d+)", s)
    return (m.group(1), float(m.group(2)), float(m.group(3)), float(m.group(4)), float(m.group(5))) if m else None


def wait_survivor_near(timeout):
    deadline, n = time.time() + timeout, None
    while time.time() < deadline:
        n = nearest()
        if n and n[1] <= 200.0:
            return n
        time.sleep(5)
    return n


def meet(n, label):
    """Stand 1.6 m from survivor n, look at her, read the reticle; press E; return (reticle line, dialogue line, log)."""
    q(STAND.replace("__X__", str(n[2])).replace("__Y__", str(n[3])).replace("__Z__", str(n[4])).replace("__D__", "160"), "STAND")
    time.sleep(1.0)
    aim_at(n[2], n[3], n[4] + 40)
    r = reticle()
    shot(label + "_reticle")
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.InjectInput IA_Interact 1 0 0.1")
    ui.wait_log(NAME, r"\[MODialogue\]|\[MOCharacter\]", since=off, timeout=10)
    time.sleep(1.0)
    d = query("MO.UI.Dialogue", "DIALOGUE")
    shot(label + "_dialogue")
    return r, d, ui.read_log_from(NAME, off)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--reuse", action="store_true", help="the instance is already in a world (a --keep run)")
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
        start_off = ui.log_size(NAME)

        # 1. A survivor near the player, her fire lit; far away, another one there.
        n = wait_survivor_near(90)
        survivors = query("MO.Spawn.Survivors", "SURVIVORS")
        lit = re.findall(r"\[MOQUERY\] SURVIVOR \S+ dist=(\d+)m state=\S+ wandering=1 fire=(\w+)", ui.read_log_from(NAME, max(0, ui.log_size(NAME) - 20000)))
        near_fires = [f for d, f in lit if float(d) <= 200.0]
        check("a wandering survivor within 200 m at the start", bool(n) and n[1] <= 200.0, "%s | %s" % (n, survivors))
        check("her campfire burns", "lit" in near_fires, "near fires: %s" % near_fires)
        before = n[0] if n else ""
        q("p = unreal.GameplayStatics.get_player_pawn(world, 0); l = p.get_actor_location(); "
          "p.set_actor_location(unreal.Vector(l.x + 80000.0, l.y, l.z + 3000.0), False, True); out('MOVED 800 m east')", "MOVED")
        far_off = ui.log_size(NAME)
        n2 = wait_survivor_near(120)
        spawn_line = re.findall(r"No survivor within 200 m of [^\r\n]*", ui.read_log_from(NAME, far_off))
        check("800 m away: a survivor within 200 m there too", bool(n2) and n2[1] <= 200.0 and n2[0] != before, "%s | %s" % (n2, spawn_line[-1:] or "no spawn line"))
        check("... spawned for the player with her fire lit", any("her campfire lit" in s for s in spawn_line), str(spawn_line[-1:]))

        # 2-3. Meet her: green reticle, her dialogue, the campfire ask; build one, she joins; a colonist is blue.
        if n2:
            r, d, log = meet(n2, "1_first_meeting")
            check("the reticle on a stranger is green (Interact)", "intent=Interact" in r, r)
            check("... and DRAWN green", ring_drawn("1_first_meeting_reticle", "Interact") >= 7, "%d of 8 sectors green" % ring_drawn("1_first_meeting_reticle", "Interact"))
            check("her dialogue pops up: greeting, camp, the ask", "showing=1" in d and "join your village" in d, d[:300])
            quest = re.findall(r"Tutorial_BuildCampfire asked for by a survivor: [^\r\n]*", log)
            check("the ask starts (or finds) the campfire quest line", bool(quest) and "could not start" not in quest[-1] and "no such quest" not in quest[-1], str(quest))
            check("she has the campfire quest", "NeedsBuilding (recipe BuildCampfire)" in log, (re.findall(r"\[Recruitment\][^\r\n]*", log) or ["no line"])[-1][:200])
            ui.console(NAME, "MO.Colony.PlaceBuilding BuildCampfire 400")
            time.sleep(2.0)
            r2, d2, log2 = meet(n2, "2_campfire_built")
            check("with a campfire built she joins: 'You built one!'", "You built one" in d2 and "has been recruited" in log2, d2[:200])
            aim_at(n2[2], n2[3], n2[4] + 40)
            r3 = reticle()
            shot("3_colonist_reticle")
            check("a colonist under the reticle is blue (Secondary: her orders are a right-click)", "intent=Secondary" in r3, r3)
            check("... and DRAWN blue", ring_drawn("3_colonist_reticle", "Secondary") >= 7, "%d of 8 sectors blue" % ring_drawn("3_colonist_reticle", "Secondary"))

            # 4. The next survivor the rule brings: the campfire already stands.
            n3 = None
            deadline = time.time() + 90
            while time.time() < deadline:
                n3 = nearest()
                if n3 and n3[0] != n2[0] and n3[1] <= 200.0:
                    break
                time.sleep(5)
            check("after she joined, another wandering survivor within 200 m", bool(n3) and n3[0] != n2[0] and n3[1] <= 200.0, str(n3))
            if n3 and n3[0] != n2[0]:
                r4, d4, log4 = meet(n3, "4_already_built")
                check("'Ohh, you already built one!?' and she joins on the spot", "already built one" in d4 and "was already met -> joined" in log4, d4[:300])
                # Three settlers now (the player and two recruits): the trust gate (V2.3) decides a stranger, not the campfire.
                n4 = None
                deadline = time.time() + 90
                while time.time() < deadline:
                    n4 = nearest()
                    if n4 and n4[0] not in (n2[0], n3[0]) and n4[1] <= 200.0:
                        break
                    time.sleep(5)
                if n4 and n4[0] not in (n2[0], n3[0]):
                    r8, d8, log8 = meet(n4, "4b_colony_of_three")
                    check("with three settlers the campfire ask is dropped and the trust gate decides",
                          "no building ask, the standing gate decides" in log8 and "get to know your village" in d8 and "already built one" not in d8,
                          (re.findall(r"\[Recruitment\][^\r\n]*", log8) or ["no line"])[-1][:200])

        # 5. The reticle on the rest: an unfinished building (blue), a wolf (red), the sky (white); the closing circle. On open ground 60 m from
        # everyone's camp first (a survivor's fire beside the ghost is a finished building: green).
        q("p = unreal.GameplayStatics.get_player_pawn(world, 0); l = p.get_actor_location(); "
          "p.set_actor_location(unreal.Vector(l.x - 6000.0, l.y + 6000.0, l.z + 2000.0), False, True); out('OPEN moved')", "OPEN")
        time.sleep(6.0)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Test.PlaceBuildingRPC BuildCampfire 350")
        time.sleep(3.0)
        ghost = q(ACTOR_AT.replace("__CLASS__", "MOBuildableActor").replace("__FILTER__", "not a.is_complete()"), "ACTOR")
        g = re.match(r"\S+ (-?\d+) (-?\d+) (-?\d+)", ghost)
        if check("an unfinished campfire placed", bool(g), ghost):
            aim_at(int(g.group(1)), int(g.group(2)), int(g.group(3)) + 10)
            r5 = reticle()
            shot("5_unfinished_building_reticle")
            check("an unfinished building is blue (Secondary: add materials with a right-click)", "intent=Secondary" in r5, r5)
            check("... and DRAWN blue", ring_drawn("5_unfinished_building_reticle", "Secondary") >= 7, "%d of 8 sectors blue" % ring_drawn("5_unfinished_building_reticle", "Secondary"))
        ui.console(NAME, "MO.Test.SpawnCreature /MOFramework/MOBS/Wolf/Wolf.Wolf_C 1200")   # 12 m: in sight (at 25 m it ran behind a rise: no threat through a hill)
        time.sleep(2.0)
        wolf = q(ACTOR_AT.replace("__CLASS__", "MOCreature").replace("__FILTER__", "'olf' in a.get_name() and not a.is_dead"), "ACTOR")
        w = re.match(r"\S+ (-?\d+) (-?\d+) (-?\d+)", wolf)
        if check("a wolf 12 m out", bool(w), wolf):
            r6 = ""
            for _ in range(10):   # it moves (to the player, or away from a fire): aim where it is now
                wolf = q(ACTOR_AT.replace("__CLASS__", "MOCreature").replace("__FILTER__", "'olf' in a.get_name() and not a.is_dead"), "ACTOR")
                w = re.match(r"\S+ (-?\d+) (-?\d+) (-?\d+)", wolf)
                if not w:
                    break
                q(AIM.replace("__X__", w.group(1)).replace("__Y__", w.group(2)).replace("__Z__", str(int(w.group(3)) + 40)), "AIM")
                time.sleep(0.25)
                r6 = reticle()
                if "intent=Threat" in r6:
                    shot("6_wolf_reticle")
                    break
            check("a wolf is red (Threat)", "intent=Threat" in r6, r6)
            check("... and DRAWN red", ring_drawn("6_wolf_reticle", "Threat") >= 7, "%d of 8 sectors red" % ring_drawn("6_wolf_reticle", "Threat"))
            check("the tutorial heard the circle turn red", "the reticle turned red" in ui.read_log_from(NAME, off), "")
        q("pc = unreal.GameplayStatics.get_player_controller(world, 0); r = pc.get_control_rotation(); "
          "pc.set_control_rotation(unreal.Rotator(0.0, 60.0, r.yaw + 180.0)); out('SKY')", "SKY")
        time.sleep(0.8)
        r7 = reticle()
        shot("7_sky_reticle")
        check("the sky is white (nothing)", "intent=None" in r7, r7)
        check("... and DRAWN white", ring_drawn("7_sky_reticle", "None") >= 7, "%d of 8 sectors white" % ring_drawn("7_sky_reticle", "None"))
        ui.console(NAME, "MO.UI.ReticleAim spread 1")
        time.sleep(0.4)
        shot("8_closing_circle_open")
        ui.console(NAME, "MO.UI.ReticleAim 2.0")
        diam = []
        for _ in range(4):
            m = re.search(r"diameter=([\d.]+)", reticle())
            diam.append(float(m.group(1)) if m else -1.0)
            time.sleep(0.45)
        check("the closing circle closes (diameters %s)" % diam, len(diam) == 4 and diam[0] > diam[-1] and diam[-1] <= 13.0 and all(x >= y - 0.01 for x, y in zip(diam, diam[1:])), "")

        failed = [r for r in RESULTS if not r[1]]
        print("RESULT %d of %d checks pass%s" % (len(RESULTS) - len(failed), len(RESULTS), "" if not failed else ": FAILED " + "; ".join(r[0] for r in failed)))
        print("screenshots:", SHOTS)
        return 0 if not failed else 1
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
