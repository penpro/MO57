"""Every held item in every base's hand (Docs/Held_Grip.md; Nov 2 2026, Wes: "make sure it works on the other metahumans and make sure we have a
standard for how they grip other items"). One -game instance on a new world: for each base (MO.Test.Base) each item goes into its hand, and
the hand -- as the game poses it, idle -- is measured against the item's handle as the game holds it (AMOCharacter::GetHeldGripShape): each
finger's nearest approach and the palm's. Close-ups of the hand: Saved/Screenshots/grip/<base>_<item>_<view>.png.

    python Tools/check_grip.py [--bases Femme,Harvey] [--items Torch01,HaftedAxe01] [--shots all|first|none] [--keep] [--reuse]
                               [--clip A_2H_Idle_01_MH [--support] [--samples 6]]

Prints a line per base and item and "GRIP PASS" when every item is seated on every base, no finger is more than PEN_CM into a handle (the
palm PALM_PEN_CM), and each of the four fingers is within TOUCH_CM of it. --shots first: close-ups on the first base only (the default; all: every base).
The editor may stay open (the instance builds its own world).

--clip plays a clip on the body (a name under /MOFramework/Characters/Spear/ or a full path) instead of the idle, looping, and measures
--samples times through it (the worst counts). --support also measures the OTHER hand on the same handle (Docs/Held_Grip.md, two hands: a
spear's front hand in the two-hand stance): the game must hold it fully (AMOCharacter::GetHeldGripSupport 1), its four fingers within
TOUCH_CM of the shaft, none past PEN_CM into it, the palm not past PALM_PEN_CM. Close-ups of that hand: <base>_<item>_support_<view>.png.
A spear clip carries the spear's own track (Content/Python/spear_track.py, N-269): there the spear must lie within TRACK_DEG of the clip's
spear (AMOCharacter::GetHeldGripTrack), the carrying hand is measured against the whole shaft (it may slide up it), and the other hand must
hold it in the samples where the clip's hand holds it and let go where the clip's lets go. A clip's close-ups carry its name; --freeze 0.35
takes them with the clip held still at 35 % of its length (a moving attack carries the hand out of the shot); --verbose prints every sample.
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
TRACK_DEG = 1.0   # a spear on the clip's own track lies this close to the clip's spear (N-269)
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

# The hand against the handle: FMOHandGrip's finger capsules and palm (metacarpal -> knuckle), scaled by the body's hand, off the body's bones.
# HELD: which hand's item (its handle as the game holds it, turned through the other hand on a long handle); SIDE: which hand is measured;
# SHAFT: the handle as a long shaft (the other hand's grip is past the carrying hand's GripHalfSize.Z, on the same shaft).
MEASURE = """import math
p = unreal.GameplayStatics.get_player_pawn(world, 0)
m = p.get_editor_property('mesh')
r = p.get_held_grip_shape(HELD)   # None while the hand grips nothing (a bool return is Python's success flag)
if not r:
    out('GRIPM none')
else:
    T, half, style, scale = r[-4:]
    sphere = style == unreal.MOGripStyle.SPHERE
    h = (half.x, half.y, 1.0e6 if SHAFT else half.z)
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
    S = 'SIDE'
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
    q = T.inverse_transform_location(m.get_socket_location('middle_01_' + S))
    tr = p.get_held_grip_track(HELD)   # (weight, the clip's spear toward its head (world), the clip's other hand's hold)
    tw, tdir, thold = tr if isinstance(tr, tuple) and len(tr) == 3 else (0.0, unreal.Vector(0, 0, 0), 0.0)
    ax = unreal.MathLibrary.transform_direction(T, unreal.Vector(0, 0, 1))
    dd = math.degrees(math.acos(max(-1.0, min(1.0, ax.dot(tdir) / max(ax.length() * tdir.length(), 1e-9))))) if tdir.length() > 0.5 else 0.0
    ua, la, ha = (m.get_socket_location('%s_%s' % (b, S)) for b in ('upperarm', 'lowerarm', 'hand'))
    # The shaft in this hand's fist: its turn off the knuckle line (the seat's axis) and how much of that tips it out of the palm's plane
    kn = [m.get_socket_location('%s_01_%s' % (f, S)) for f in ('index', 'middle', 'ring', 'pinky')]
    mc = [m.get_socket_location('%s_metacarpal_%s' % (f, S)) for f in ('index', 'middle', 'ring', 'pinky')]
    e1 = (kn[0] - kn[3]); e1 = e1 * (1.0 / max(e1.length(), 1e-6))
    md = (kn[1] + kn[2]) * 0.5 - (mc[1] + mc[2]) * 0.5
    npm = e1.cross(md); npm = npm * (1.0 / max(npm.length(), 1e-6))
    hx = unreal.MathLibrary.transform_direction(T, unreal.Vector(0, 0, 1))
    pivot = math.degrees(math.acos(min(1.0, abs(hx.dot(e1)) / max(hx.length(), 1e-6))))
    tilt = math.degrees(math.asin(min(1.0, abs(hx.dot(npm)) / max(hx.length(), 1e-6))))
    reach = (ha - ua).length() / max((la - ua).length() + (ha - la).length(), 1e-6)   # 1: the arm straight (an IK target out of its reach)
    out('GRIPM %s palm=%.2f support=%.2f along=%.1f scale=%.2f track=%.2f clip=%.2f dirdeg=%.2f reach=%.3f pivot=%.1f tilt=%.1f style=%s at=%.1f,%.1f,%.1f' % (' '.join(res),
        palm, p.get_held_grip_support(HELD), q.z, scale, tw, thold, dd, reach, pivot, tilt, 'Sphere' if sphere else 'Power', c.x, c.y, c.z))
"""

PLAY = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
seq = unreal.load_asset('CLIP')
ai = p.get_editor_property('mesh').get_anim_instance()
mt = ai.play_slot_animation_as_dynamic_montage(seq, 'DefaultSlot', 0.2, 0.2, 1.0, 100000) if seq else None
out('PLAY %s %s %.2f' % (seq.get_name() if seq else 'missing', mt is not None, seq.get_play_length() if seq else 0.0))
"""

# The clip held still at a fraction of its length for close-ups (its play rate 0: an animation stopped, not the game paused); RATE 1 lets it go
FREEZE = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
ai = p.get_editor_property('mesh').get_anim_instance()
m = ai.get_current_active_montage()
if m:
    if RATE == 0.0:
        ai.montage_set_position(m, m.get_play_length() * AT)
    ai.montage_set_play_rate(m, RATE)
out('FREEZE %s' % (m is not None))
"""

HANDPOS = """p = unreal.GameplayStatics.get_player_pawn(world, 0)
h = p.get_editor_property('mesh').get_socket_location('middle_01_SIDE')
f = p.get_actor_forward_vector(); r = p.get_actor_right_vector()
out('HANDPOS %f %f %f %f %f %f %f %f %f' % (h.x, h.y, h.z, f.x, f.y, f.z, r.x, r.y, r.z))
"""


def q(code, key, timeout=60):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    m = re.search(key + r" ([^\r\n]*)", out)
    if m and "%" in m.group(1):   # the bridge echoed the code: it raised in the game
        err = [line for line in out.splitlines() if "Error" in line or "error" in line]
        print("  (the game's Python raised: %s)" % (err[-1][:240] if err else out[-400:]), flush=True)
        return ""
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


def measure(held_left, side_left, shaft):
    """One hand against one hand's held handle, as the game shows it: ({finger: cm}, palm cm, support weight, along cm, the raw row, and the
    clip's spear track: how far the item follows it, how far the clip's other hand holds it, the item's axis off the clip's spear in deg)."""
    row = q(MEASURE.replace("HELD", "True" if held_left else "False").replace("SIDE", "l" if side_left else "r")
            .replace("SHAFT", "True" if shaft else "False"), "GRIPM")
    vals = dict(kv.split("=") for kv in row.split() if "=" in kv) if row and row != "none" else {}
    fingers = {k: float(vals[k]) for k in ("thumb", "index", "middle", "ring", "pinky") if k in vals}
    return (fingers, float(vals.get("palm", 99)), float(vals.get("support", 0)), float(vals.get("along", 0)), row,
            float(vals.get("track", 0)), float(vals.get("clip", 0)), float(vals.get("dirdeg", 0)))


def judge(samples, support=False):
    """The worst of the samples: each finger's deepest and the four fingers' farthest, the palm's deepest; ok against PEN_CM, TOUCH_CM,
    PALM_PEN_CM (and, for the other hand, the game holding it fully in every sample). In a clip carrying its spear's own track (N-269) the
    item must lie on the clip's spear (within TRACK_DEG) wherever it follows it fully, and the other hand must hold it where the clip's hand
    does and let go where the clip's lets go (the thrust's middle); a sample between the two is not judged."""
    if not samples or any(not s[0] for s in samples):
        return False, "no answer (%s)" % (samples[-1][4] if samples else "")
    tracked = [s for s in samples if s[5] >= 0.99]
    if tracked and not support:
        dev = max(s[7] for s in tracked)
        ok, text = _judge_hand(samples, False)
        return ok and dev <= TRACK_DEG, "%s | on the clip's spear in %d/%d samples, at most %.2f deg off it" % (text, len(tracked), len(samples), dev)
    if tracked and support:
        held = [s for s in samples if s[5] >= 0.99 and s[6] >= 0.99]
        free = [s for s in samples if s[5] >= 0.99 and s[6] <= 0.01]
        let_go = all(s[2] <= 0.01 for s in free)
        ok, text = _judge_hand(held, True) if held else (True, "never held in the clip")
        return ok and let_go, "%s | the clip's hand holds in %d/%d samples, lets go in %d (%s)" % (
            text, len(held), len(samples), len(free), "this one too" if let_go else "this one does NOT: support %s" % [s[2] for s in free])
    return _judge_hand(samples, support)


def _judge_hand(samples, support):
    if not samples:
        return True, "no samples"
    deepest = min(min(s[0].values()) for s in samples)
    farthest = max(max(s[0][k] for k in ("index", "middle", "ring", "pinky")) for s in samples)
    palm = min(s[1] for s in samples)
    weight = min(s[2] for s in samples)
    ok = deepest >= -PEN_CM and farthest <= TOUCH_CM and palm >= -PALM_PEN_CM and (not support or weight >= 0.99)
    fingers = " ".join("%s=%.2f..%.2f" % (k, min(s[0][k] for s in samples), max(s[0][k] for s in samples))
                       for k in ("thumb", "index", "middle", "ring", "pinky"))
    along = " along=%.0f..%.0f" % (min(s[3] for s in samples), max(s[3] for s in samples)) if support else ""
    reach = [float(kv.split("=")[1]) for s in samples for kv in s[4].split() if kv.startswith("reach=")]
    along += " arm %.3f..%.3f straight" % (min(reach), max(reach)) if support and reach else ""
    return ok, "%s palm>=%.2f support>=%.2f%s" % (fingers, palm, weight, along)


def main():
    global NAME
    ap = argparse.ArgumentParser()
    ap.add_argument("--bases", default=",".join(BASES))
    ap.add_argument("--items", default=",".join(ITEMS))
    ap.add_argument("--shots", choices=["all", "first", "none"], default="first")
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--clip", default="", help="a clip on the body (a name under /MOFramework/Characters/Spear/ or a full path), looping")
    ap.add_argument("--support", action="store_true", help="also the other hand on the same handle (a two-hand clip)")
    ap.add_argument("--samples", type=int, default=0, help="measurements through the clip (default 6 with --clip, else 1)")
    ap.add_argument("--name", default=NAME, help="the instance's name (two sessions can check at once)")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--verbose", action="store_true", help="print every sample's measurement")
    ap.add_argument("--freeze", type=float, default=None, help="with --clip: the close-ups with the clip held at this fraction of its length")
    ap.add_argument("--reuse", action="store_true")
    a = ap.parse_args()
    NAME = a.name
    bases, items = a.bases.split(","), a.items.split(",")
    clip = a.clip if not a.clip or a.clip.startswith("/") else "/MOFramework/Characters/Spear/" + a.clip
    samples = a.samples or (6 if clip else 1)
    tag = "_" + clip.split("/")[-1].split(".")[0] if clip else ""   # a clip's close-ups are its own
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
                interval = 0.0
                if clip:
                    played = q(PLAY.replace("CLIP", clip), "PLAY").split()
                    if len(played) < 3 or played[1] != "True":
                        print("FAIL: %s did not play (%s)" % (clip, " ".join(played)), flush=True)
                        return 1
                    interval = max(0.1, float(played[2]) / samples)
                time.sleep(0.6)   # the fingers close over 0.15 s (the clip blends in over 0.2)
                held, other = [], []
                for _ in range(samples):
                    # a spear in a spear clip may slide in the carrying hand (the clip's own track, N-269): that hand on the whole shaft
                    held.append(measure(left, left, bool(clip) and "Spear" in item))
                    if a.support:
                        other.append(measure(left, not left, True))
                    time.sleep(interval)
                if a.verbose:
                    for h, o in zip(held, other or [None] * len(held)):
                        print("    carry:", re.sub(r" style=.*", "", h[4]))
                        if o:
                            print("    other:", re.sub(r" style=.*", "", o[4]))
                ok, text = judge(held)
                seat = seated.group(0)[-110:] if seated else "no seat line (%s)" % eq
                line = "%-7s %-20s %s | %s | %s" % (base, item, "ok  " if ok else "FAIL", text, seat)
                if a.support:
                    sok, stext = judge(other, support=True)
                    line += "\n%-7s %-20s %s | the other hand: %s" % ("", "", "ok  " if sok else "FAIL", stext)
                    ok = ok and sok
                print(line, flush=True)
                results.append(line)
                if not ok:
                    failures.append("%s %s" % (base, item))
                if a.shots == "all" or (a.shots == "first" and bi == 0):
                    still = clip and a.freeze is not None
                    if still:
                        q(FREEZE.replace("RATE", "0.0").replace("AT", "%.3f" % a.freeze), "FREEZE")
                        time.sleep(0.5)
                    shot_tag = tag + ("_at%02d" % round(a.freeze * 100) if still else "")
                    hand_shots("%s_%s%s" % (base, item, shot_tag), left)
                    if a.support:
                        hand_shots("%s_%s%s_support" % (base, item, shot_tag), not left)
                    if still:
                        q(FREEZE.replace("RATE", "1.0").replace("AT", "0"), "FREEZE")
        print("GRIP %s: %d base x item pairs%s, %d failing%s; close-ups %s" % (
            "PASS" if not failures else "FAIL", len(results), " in %s" % clip if clip else "", len(failures),
            (": " + ", ".join(failures)) if failures else "", SHOTS), flush=True)
        return 0 if not failures else 1
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
