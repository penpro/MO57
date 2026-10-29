"""The crouch clips' pelvis, put back where the body is (Oct 27 2026; Wes: crouching "makes her ass fall off").

The retargeted Manny crouch set (/MOFramework/Characters/Crouch/MM_Unarmed_Crouch_*_MH, Oct 12: root translation zeroed on the sources, then
the 5.8 op-stack retargeter) came out with the pelvis ON THE FLOOR (its height over the root ~ -5 cm, standing 85) and the crouch's ~40 cm
carried by the pelvis's children instead: spine_01 and both thighs each translated by the same offset. The torso and legs land where they
should; everything skinned to the pelvis -- the backside -- stays on the floor.

The repair moves that offset back where it belongs, frame by frame: d = spine_01's translation minus its reference (in the pelvis's space);
the pelvis takes +d (rotated into its parent's space), every child of the pelvis carrying the same offset takes -d. No bone moves in the
world except the pelvis itself (and what is skinned to it). Usage, in the editor (MO57):

    REPAIR_DRY_RUN=1 python Tools/ue.py py --file Content/Python/repair_crouch_pelvis.py     # measure only
    python Tools/ue.py py --file Content/Python/repair_crouch_pelvis.py                      # repair + save
    REPAIR_CLIPS=/MOFramework/Characters/Climbing/AS_Climb_Up_MH,... python Tools/ue.py py --file ...   # other clips from the same retargeter

The same retargeter (RTG_MoverMannyToMetaHuman) did it again to Titan Climbing's climbing set (Oct 29, /MOFramework/Characters/Climbing):
any clip it makes gets this repair.

Prints "CROUCH <clip>: d=(...) pelvis z <before> -> <after>, children fixed [...]" per clip. Idempotent: a clip whose spine_01 already sits at
its reference is left alone. Applied Oct 27 to all four (crouch idle pelvis -5.2 -> 34.7 cm over the root; in game crouched 31-34 cm over the
floor, Tools/check_legs.py).
"""
import os

import unreal

CLIPS = ["/MOFramework/Characters/Crouch/MM_Unarmed_Crouch_Idle_MH", "/MOFramework/Characters/Crouch/MM_Unarmed_Crouch_Walk_Fwd_MH",
         "/MOFramework/Characters/Crouch/MM_Unarmed_Crouch_Entry_MH", "/MOFramework/Characters/Crouch/MM_Unarmed_Crouch_Exit_MH"]
_OTHER = os.environ.pop("REPAIR_CLIPS", "")   # popped, like REPAIR_DRY_RUN
if _OTHER:
    CLIPS = [c.strip() for c in _OTHER.split(",") if c.strip()]
# A standing clip on the same skeleton: its spine_01 / thigh translations are the skeleton's reference ones (checked Oct 27: MM_Idle 2.0,-0.1,0.0).
REFERENCE_CLIP = "/MOFramework/Characters/MetaHumans/Female/Animations/Animations/MM_Idle"
BODY = "/Game/MetaHumans/MH_F_1/Body/SKM_MH_F_1_BodyMesh"
DRY = os.environ.pop("REPAIR_DRY_RUN", None) == "1"   # popped: the editor's process keeps os.environ between bridge calls
L = unreal.AnimationLibrary


def _emit(msg):
    try:
        out(msg)  # noqa: F821
    except NameError:
        unreal.log(msg)


def write_track(clip, bone, translations, rotations, scales):
    """One bone's keys in one undoable step. All built before the bracket opens (a failure inside it would leave the editor's transaction
    open); the setter Python reaches takes double-precision keys (FVector / FQuat arrays; the float overload is not exposed, Oct 27)."""
    positions = [unreal.Vector(t.x, t.y, t.z) for t in translations]
    quats = [unreal.Quat(q.x, q.y, q.z, q.w) for q in rotations]
    sizes = [unreal.Vector(v.x, v.y, v.z) for v in scales]
    ctrl = clip.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("Repair the crouch pelvis"), True)
    try:
        return ctrl.set_bone_track_keys(bone, positions, quats, sizes, True)
    finally:
        ctrl.close_bracket(True)


def pelvis_children():
    comp = unreal.SkeletalMeshComponent()
    comp.set_skeletal_mesh_asset(unreal.load_asset(BODY))
    names = [comp.get_bone_name(i) for i in range(comp.get_num_bones())]
    return [str(n) for n in names if str(comp.get_parent_bone(n)) == "pelvis"]


def main():
    ref = unreal.load_asset(REFERENCE_CLIP)
    children = pelvis_children()
    ref_t = {b: L.get_bone_pose_for_frame(ref, b, 0, False).translation for b in children}
    for path in CLIPS:
        clip = unreal.load_asset(path)
        if not clip:
            _emit("CROUCH %s: missing" % path)
            continue
        frames = L.get_num_frames(clip)
        keys = range(frames + 1)
        pel = [L.get_bone_pose_for_frame(clip, "pelvis", f, False) for f in keys]
        kids = {b: [L.get_bone_pose_for_frame(clip, b, f, False) for f in keys] for b in children}
        d0 = kids["spine_01"][0].translation - ref_t["spine_01"]
        if d0.length() < 2.0:
            _emit("CROUCH %s: already right (spine_01 %.1f cm off its reference)" % (path.split("/")[-1], d0.length()))
            continue
        # The children that carry the pelvis's offset (within 6 cm of spine_01's at frame 0); the rest are left as they are.
        carry = [b for b in children if (kids[b][0].translation - ref_t[b] - d0).length() < 6.0]
        new_pel, new_kids = [], {b: [] for b in carry}
        for i in range(len(pel)):
            d = kids["spine_01"][i].translation - ref_t["spine_01"]
            p = pel[i]
            new_pel.append(unreal.Transform(p.translation + p.rotation.rotate_vector(d), p.rotation.rotator(), p.scale3d))
            for b in carry:
                k = kids[b][i]
                new_kids[b].append(unreal.Transform(k.translation - d, k.rotation.rotator(), k.scale3d))
        mid = len(pel) // 2
        _emit("CROUCH %s: %d frames, d=(%.1f,%.1f,%.1f), pelvis z %.1f -> %.1f (mid %.1f -> %.1f), children fixed %s, left %s%s" % (
            path.split("/")[-1], len(pel), d0.x, d0.y, d0.z, pel[0].translation.z, new_pel[0].translation.z, pel[mid].translation.z,
            new_pel[mid].translation.z, carry, [b for b in children if b not in carry], " (dry run)" if DRY else ""))
        if DRY:
            continue
        # A key reads back with a constant added (the skeleton's translation retargeting: the pelvis +(0, 0.11, -5.25), Oct 27): each bone's
        # constant is measured (keys of zero, read back) and taken off, so what reads back -- and plays -- is the target.
        for bone, tracks in [("pelvis", new_pel)] + [(b, new_kids[b]) for b in carry]:
            rotations = [t.rotation for t in tracks]
            scales = [t.scale3d for t in tracks]
            if not write_track(clip, bone, [unreal.Vector(0, 0, 0)] * len(tracks), rotations, scales):
                _emit("CROUCH %s: %s's keys refused" % (path.split("/")[-1], bone))
                break
            offsets = [L.get_bone_pose_for_frame(clip, bone, f, False).translation for f in keys]
            write_track(clip, bone, [t.translation - o for t, o in zip(tracks, offsets)], rotations, scales)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        after = L.get_bone_pose_for_frame(clip, "pelvis", 0, False).translation.z
        _emit("CROUCH %s: saved, pelvis z now %.1f" % (path.split("/")[-1], after))


try:
    main()
except Exception as e:  # the bridge shows a raised exception as "not responding": say it instead
    import traceback
    _emit("CROUCH FAILED: %s\n%s" % (e, traceback.format_exc()[-1500:]))
