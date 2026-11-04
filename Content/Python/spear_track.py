"""The spear's own track baked into the retargeted spear clips (Nov 3 2026, N-269; Docs/Held_Grip.md, two hands, "The spear's track").

The Mega Spear pack does not keep its spear in either hand: the spear (SK_Spear) rides the character's ik_hand_gun bone (BP_SpearFighter
attaches it there turned -90 deg in roll; no spear clip moves the spear's own bone), and ABP_SpearAnimations_IK pulls the hands onto it.
Measured Nov 3 on the pack's own clips: whenever a hand holds the spear the character clip already has it there (its bone 9-14 cm off the
shaft's axis; the pack's IK moves it 2 cm), the rear hand stays at one place on the shaft while the shaft pivots in it (24 deg in the jab,
43 in the horizontal swing, 66 in the thrust), the front hand slides along it (34 cm in the swing) or lets go (the middle of the thrust),
and changing to one hand the rear hand slides 89 cm up it. Our retarget carries the arms but not ik_hand_gun (it would have to scale the
spear off our hands), so the track comes over as curves on each <name>_MH clip, read by UMOBodyRetargetAnimInstance (FMOHandGrip::Track):

  MO_SpearTrack           1 through the clip (the other curves are weighted by the clip's blend; this says by how much)
  MO_SpearDirX / Y / Z    the spear, toward its head, in the clip's component space (unit)
  MO_SpearHoldL / R       how far that hand holds it: 1 with its bone within HOLD_FULL_CM of the shaft's axis, 0 past HOLD_NONE_CM or off the spear
  MO_SpearSlideL / R      how far that hand is up the shaft (toward the head, cm) from where it holds it in the two-hand stance (A_2H_Idle_01)

Usage (MO57's editor): python Tools/ue.py py --file Content/Python/spear_track.py --timeout 900
retarget_weapon_sets.py runs it after its pelvis repair (it removes every curve the retarget copies). Prints "SPEARTRACK ..." lines; the last
says how many clips have the track and which have no source. Local only, like the clips (LFS budget).
"""
import math
import unreal

TARGET = "/MOFramework/Characters/Spear/"
SOURCE = "/Game/SpearAnimationsV2/Animations/Character/"
STANCE = SOURCE + "2Hand/Pose01/A_2H_Idle_01"
SPEAR_IN_GUN = unreal.Transform(unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(roll=-90.0, pitch=0.0, yaw=0.0), unreal.Vector(1.0, 1.0, 1.0))
SPEAR_HALF_CM = 91.0      # SK_Spear's half length along its +Y (its bounds: 90.9)
HOLD_FULL_CM = 15.0       # a hand bone this close to the shaft's axis holds it (the pack's: 4-14 cm while it does)
HOLD_NONE_CM = 21.0       # ... this far, not (the pack's: 22 cm and more once it lets go)
RATE = 30.0               # keys per second (the pack's clips are 30 fps)
CURVES = ["MO_SpearTrack", "MO_SpearDirX", "MO_SpearDirY", "MO_SpearDirZ", "MO_SpearHoldL", "MO_SpearHoldR", "MO_SpearSlideL", "MO_SpearSlideR"]


def log(s):
    unreal.log_warning("SPEARTRACK " + s)


def pose(anim, t):
    return unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, unreal.AnimPoseEvaluationOptions())


def at(p, bone):
    return unreal.AnimPoseExtensions.get_bone_pose(p, bone, unreal.AnimPoseSpaces.WORLD)


def spear(p):
    """The pack's spear in a character pose: its grip point (the spear's origin) and its axis toward the head, component space."""
    s = unreal.MathLibrary.compose_transforms(SPEAR_IN_GUN, at(p, "ik_hand_gun"))
    return s.translation, unreal.MathLibrary.transform_direction(s, unreal.Vector(0.0, 1.0, 0.0)).normal()


def on_shaft(p, origin, axis, hand):
    """How far up the shaft a hand is (cm from the spear's origin toward its head) and how far its bone is off the axis."""
    v = at(p, hand).translation - origin
    along = v.dot(axis)
    return along, (v - axis * along).length()


def hold(along, perp):
    if abs(along) > SPEAR_HALF_CM:
        return 0.0
    return max(0.0, min(1.0, (HOLD_NONE_CM - perp) / (HOLD_NONE_CM - HOLD_FULL_CM)))


def sources():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    found = {}
    for a in reg.get_assets_by_path(SOURCE, recursive=True):
        if str(a.asset_class_path.asset_name) == "AnimSequence":
            found[str(a.asset_name)] = str(a.package_name)
    return found


def bake(clip, src, ref):
    L = src.get_play_length()
    Ld = clip.get_play_length()
    n = max(2, int(round(L * RATE)) + 1)
    times, keys = [], {c: [] for c in CURVES}
    for i in range(n):
        t = L * i / (n - 1)
        p = pose(src, t)
        origin, axis = spear(p)
        times.append(min(t, Ld))
        keys["MO_SpearTrack"].append(1.0)
        for c, v in zip(("MO_SpearDirX", "MO_SpearDirY", "MO_SpearDirZ"), (axis.x, axis.y, axis.z)):
            keys[c].append(v)
        for side, hand in (("L", "hand_l"), ("R", "hand_r")):
            along, perp = on_shaft(p, origin, axis, hand)
            keys["MO_SpearHold" + side].append(hold(along, perp))
            keys["MO_SpearSlide" + side].append(max(-SPEAR_HALF_CM, min(SPEAR_HALF_CM, along)) - ref[side])
    for c in CURVES:
        if unreal.AnimationLibrary.does_curve_exist(clip, c, unreal.RawCurveTrackTypes.RCT_FLOAT):
            unreal.AnimationLibrary.remove_curve(clip, c, False)
        unreal.AnimationLibrary.add_curve(clip, c, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
        unreal.AnimationLibrary.add_float_curve_keys(clip, c, times, keys[c])
    unreal.EditorAssetLibrary.save_loaded_asset(clip, False)

    # Read back: the keys as stored, and what they say
    back = {}
    for c in CURVES:
        kt, kv = unreal.AnimationLibrary.get_float_keys(clip, c)
        back[c] = list(kv)
    ok = all(len(back[c]) == n and max(abs(a - b) for a, b in zip(back[c], keys[c])) < 1e-3 for c in CURVES)
    first = (keys["MO_SpearDirX"][0], keys["MO_SpearDirY"][0], keys["MO_SpearDirZ"][0])
    pivot = max(math.degrees(math.acos(max(-1.0, min(1.0, keys["MO_SpearDirX"][i] * first[0] + keys["MO_SpearDirY"][i] * first[1]
                                                        + keys["MO_SpearDirZ"][i] * first[2])))) for i in range(n))
    log("%-32s %3d keys %s | hold L %.2f..%.2f R %.2f..%.2f | slide L %4.0f..%4.0f R %4.0f..%4.0f cm | the spear turns up to %3.0f deg from its first key" % (
        clip.get_name(), n, "stored" if ok else "NOT STORED", min(keys["MO_SpearHoldL"]), max(keys["MO_SpearHoldL"]), min(keys["MO_SpearHoldR"]),
        max(keys["MO_SpearHoldR"]), min(keys["MO_SpearSlideL"]), max(keys["MO_SpearSlideL"]), min(keys["MO_SpearSlideR"]), max(keys["MO_SpearSlideR"]), pivot))
    return ok


def main():
    stance = unreal.load_asset(STANCE)
    if not stance:
        log("no %s: copy the Mega Spear pack first (Content/Python/retarget_weapon_sets.py's header)" % STANCE)
        return
    p = pose(stance, 0.0)
    origin, axis = spear(p)
    ref = {"L": on_shaft(p, origin, axis, "hand_l")[0], "R": on_shaft(p, origin, axis, "hand_r")[0]}
    log("the two-hand stance's grips: left %.1f, right %.1f cm up the shaft" % (ref["L"], ref["R"]))
    found = sources()
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    done, missing, failed = 0, [], []
    for a in reg.get_assets_by_path(TARGET, recursive=False):
        name = str(a.asset_name)
        if str(a.asset_class_path.asset_name) != "AnimSequence" or not name.endswith("_MH"):
            continue
        src_path = found.get(name[:-3])
        if not src_path:
            missing.append(name)
            continue
        if bake(unreal.load_asset(str(a.package_name)), unreal.load_asset(src_path), ref):
            done += 1
        else:
            failed.append(name)
    log("done: %d clips carry the spear's track; no source: %s; not stored: %s" % (done, missing or "none", failed or "none"))


try:
    main()
except Exception:
    import traceback
    log("ERROR " + traceback.format_exc().replace("\n", " | "))
