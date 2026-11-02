"""Every held item in every base's hand (Docs/Held_Grip.md; Nov 2 2026, Wes: "make sure it works on the other metahumans and make sure we have a
standard for how they grip other items"). One -game instance on a new world: for each base (MO.Test.Base) each item goes into its hand, and
the hand -- as the game poses it, idle -- is measured against the item's handle as the game holds it (AMOCharacter::GetHeldGripShape): each
finger's nearest approach and the palm's. Close-ups of the hand: Saved/Screenshots/grip/<base>_<item>_<view>.png.

    python Tools/check_grip.py [--bases Femme,Harvey] [--items Torch01,HaftedAxe01] [--shots all|first|none] [--keep] [--reuse]

Prints a line per base and item and "GRIP PASS" when every item is seated on every base, no finger is more than PEN_CM into a handle (the
palm PALM_PEN_CM), and each of the four fingers is within TOUCH_CM of it. --shots first: close-ups on the first base only (the default; all: every base).
The editor may stay open (the instance builds its own world).
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

NAME = "gripcheck"
SHOTS = os.path.join(ue.ROOT, "Saved", "Screenshots", "grip")
PEN_CM = 0.3     # a finger this far into the handle fails
PALM_PEN_CM = 0.5   # the palm's soft side gives a little under a grip (and an animation's palm is not quite the fist's)
TOUCH_CM = 0.5   # each finger (index .. little) this close to it, or the hand is not holding it
BASES = ["Femme", "Jalena", "Zuri", "Harvey", "Trey"]
# Content/Python/held_grip.py's ITEMS that equip to a hand (not Apple01 / CookedMeat01: food is not equipped) and the bows; shields and bows in the left hand
ITEMS = ["HaftedAxe01", "PolishedAxe01", "IronAxe01", "Adze01", "StoneHoe01", "StoneSickle01", "SmithHammer01", "StoneMaul01", "CrudeClub01",
         "Torch01", "Atlatl01", "DropSpindle01", "BowDrill01", "FireDrill01", "SmithTongs01", "BronzeSword01", "IronSword01", "SteelSword01",
         "WoodenSpear01", "FireHardenedSpear01", "FlintSpear01", "BronzeSpear01", "IronSpear01", "FishSpear01", "WoodenShield01",
         "BronzeShield01", "IronShield01", "Hammerstone01", "CrudeChopper01", "StoneScraper01", "SimpleBow01",
         "SinewBackedBow01", "CompositeBow01"]
LEFT = ("Shield", "Bow01")

EQUIP = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
inv = p.get_component_by_class(unreal.MOInventoryComponent)
eq = p.get_component_by_class(unreal.MOEquipmentComponent)
for s in (unreal.MOEquipmentSlot.LEFT_HAND, unreal.MOEquipmentSlot.RIGHT_HAND):
    if eq.has_item_in_slot(s):
        old = eq.get_equipped_item(s).item_guid
        eq.unequip_to_inventory(s, inv)
        inv.remove_item_by_guid(old, 1)   # out of the pack too: a long run would fill it
if not inv.can_add_item_by_definition_id('ITEM', 1):   # a full pack (the starting kit, a long run): emptied, it is a test world
    for old in inv.get_all_item_guids():
        for _ in range(100):
            if not inv.remove_item_by_guid(old, 1):
                break
g = unreal.GuidLibrary.new_guid()
added = inv.add_item_by_guid(g, 'ITEM', 1)
ok = eq.equip_from_inventory(inv, g, unreal.MOEquipmentSlot.SLOT)
out('EQUIP added=%s equipped=%s' % (added, ok))
"""

# The hand against the handle: FMOHandGrip's finger capsules and palm (metacarpal -> knuckle), scaled by the body's hand, off the body's bones
MEASURE = """import math
p = unreal.GameplayStatics.get_player_pawn(world, 0)
m = p.get_editor_property('mesh')
r = p.get_held_grip_shape(LEFT)   # None while the hand grips nothing (a bool return is Python's success flag)
if not r:
    out('GRIPM none')
else:
    T, half, style, scale = r[-4:]
    sphere = style == unreal.MOGripStyle.SPHERE
    h = (half.x, half.y, half.z)
    def clear(pt, rad):
        q = T.inverse_transform_location(pt)
        x, y, z = q.x, q.y, q.z
        if sphere:
            rn = math.sqrt((x / h[0]) ** 2 + (y / h[1]) ** 2 + (z / h[2]) ** 2)
            return (math.sqrt(x * x + y * y + z * z) * (1.0 - 1.0 / rn) if rn > 1e-6 else -min(h)) - rad
        if abs(z) > h[2] + 2.0:
            return 99.0
        rn = math.sqrt((x / h[0]) ** 2 + (y / h[1]) ** 2)
        radial = math.sqrt(x * x + y * y)
        d = radial * (1.0 - 1.0 / rn) if rn > 1e-6 else -min(h[0], h[1])
        if abs(z) > h[2]:
            d = max(d, abs(z) - h[2])
        return d - rad
    S = 'l' if LEFT else 'r'
    R = {'thumb': (1.0, 0.92, 0.8), 'index': (0.85, 0.78, 0.68), 'middle': (0.86, 0.8, 0.7), 'ring': (0.8, 0.74, 0.65), 'pinky': (0.72, 0.66, 0.58)}
    res = []
    for f in ('thumb', 'index', 'middle', 'ring', 'pinky'):
        j = [m.get_socket_location('%s_%s_%s' % (f, k, S)) for k in ('01', '02', '03')]
        j.append(j[2] + (j[2] - j[1]) * 0.85)
        mn = 99.0
        for k in range(3):
            r0, r1 = R[f][k] * scale, (R[f][k + 1] if k < 2 else R[f][2] * 0.9) * scale
            for i in range(1, 5):
                mn = min(mn, clear(j[k] + (j[k + 1] - j[k]) * (i / 4.0), r0 + (r1 - r0) * i / 4.0))
        res.append('%s=%.2f' % (f, mn))
    palm = 99.0
    for f in ('index', 'middle', 'ring', 'pinky'):
        a, b = m.get_socket_location('%s_metacarpal_%s' % (f, S)), m.get_socket_location('%s_01_%s' % (f, S))
        for i in range(5):
            palm = min(palm, clear(a + (b - a) * (i / 4.0), 1.2 * scale))
    c = T.translation
    out('GRIPM %s palm=%.2f scale=%.2f style=%s at=%.1f,%.1f,%.1f' % (' '.join(res), palm, scale, 'Sphere' if sphere else 'Power', c.x, c.y, c.z))
"""

HANDPOS = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
h = p.get_editor_property('mesh').get_socket_location('middle_01_SIDE')
f = p.get_actor_forward_vector(); r = p.get_actor_right_vector()
out('HANDPOS %f %f %f %f %f %f %f %f %f' % (h.x, h.y, h.z, f.x, f.y, f.z, r.x, r.y, r.z))
"""


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


def hand_shots(label, left):
    """The holding hand close up from its outside (the back of the hand) and from in front of her (the fingers round the handle)."""
    v = [float(x) for x in q(HANDPOS.replace("SIDE", "l" if left else "r"), "HANDPOS").split()]
    h, f, r = v[0:3], v[3:6], v[6:9]
    out_side = -1.0 if left else 1.0
    for name, off in (("out", [r[i] * 40.0 * out_side + f[i] * 12.0 for i in range(3)]), ("front", [f[i] * 42.0 + r[i] * 12.0 * out_side for i in range(3)])):
        cam = [h[i] + off[i] for i in range(3)]
        cam[2] += 10.0
        ui.console(NAME, "MO.Cam.LookAt %.1f %.1f %.1f %.1f %.1f %.1f 45" % (cam[0], cam[1], cam[2], h[0], h[1], h[2]))
        time.sleep(0.5)
        shot("%s_%s" % (label, name))
    ui.console(NAME, "MO.Cam.Reset")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bases", default=",".join(BASES))
    ap.add_argument("--items", default=",".join(ITEMS))
    ap.add_argument("--shots", choices=["all", "first", "none"], default="first")
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--reuse", action="store_true")
    a = ap.parse_args()
    bases, items = a.bases.split(","), a.items.split(",")
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
        failures, results = [], []
        for bi, base in enumerate(bases):
            ui.console(NAME, "MO.Test.Base %s" % base)
            time.sleep(4)
            for item in items:
                left = any(k in item for k in LEFT)
                off = ui.log_size(NAME)
                eq = q(EQUIP.replace("ITEM", item).replace("SLOT", "LEFT_HAND" if left else "RIGHT_HAND"), "EQUIP")
                seated = ui.wait_log(NAME, r"seated [^\r\n]* hand of [^\r\n]*|cannot be seated[^\r\n]*", since=off, timeout=12)
                time.sleep(0.6)   # the fingers close over 0.15 s
                row = q(MEASURE.replace("LEFT", "True" if left else "False"), "GRIPM")
                vals = dict(kv.split("=") for kv in row.split() if "=" in kv) if row and row != "none" else {}
                fingers = {k: float(vals[k]) for k in ("thumb", "index", "middle", "ring", "pinky") if k in vals}
                palm = float(vals.get("palm", 99))
                ok = bool(fingers) and min(fingers.values()) >= -PEN_CM and palm >= -PALM_PEN_CM and \
                    max(fingers[k] for k in ("index", "middle", "ring", "pinky")) <= TOUCH_CM
                line = "%-7s %-20s %s | %s | %s" % (base, item, "ok  " if ok else "FAIL", row or "no answer",
                                                     seated.group(0)[-90:] if seated else "no seat line (%s)" % eq)
                print(line, flush=True)
                results.append(line)
                if not ok:
                    failures.append("%s %s" % (base, item))
                if a.shots == "all" or (a.shots == "first" and bi == 0):
                    hand_shots("%s_%s" % (base, item), left)
        print("GRIP %s: %d base x item pairs, %d failing%s; close-ups %s" % (
            "PASS" if not failures else "FAIL", len(results), len(failures), (": " + ", ".join(failures)) if failures else "", SHOTS), flush=True)
        return 0 if not failures else 1
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
