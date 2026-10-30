"""The bow and spear animation sets on the MetaHuman body (Oct 29 2026, F54; Wes: "let's start with sparrow", "i added sparrow and 'mega spear
animation pack' to mo tools for you to go through").

Sources (Fab, local only, gitignored; copy them from a project that has them to the same /Game paths, editor closed):
  /Game/ParagonSparrow       Epic's Paragon: Sparrow (free): the bow set. Her bow is bones under hand_l (bow_base, shaft, string), the nock under hand_r.
  /Game/SpearAnimationsV2    the Mega Spear Animation Pack: one- and two-hand spear on the UE4 mannequin.
Made here (tracked): /MOFramework/Characters/Bow|Spear/ -- IK_<Kind>Source (GASP's UE4 mannequin IK rig, on the pack's mesh: both skeletons
carry every bone it names), RTG_<Kind>ToMetaHuman (the crouch set's RTG_MoverMannyToMetaHuman with that source, chains re-mapped) and the clips
as <name>_MH. The source's curves are removed (Paragon's crashed a -game process on load). The retargeter puts the pelvis on the floor
(TRAPS: "her backside drops to the floor"): every clip then gets repair_crouch_pelvis.py. Finally each clip is measured against its source,
scaled by the bodies' head heights (pelvis, thigh, feet).

Usage (MO57's editor):  python Tools/ue.py py --file Content/Python/retarget_weapon_sets.py --timeout 1800
Prints "WEAPONS ..." lines; the last says which clips are off their source by more than 12 cm anywhere (Oct 29: the three two-hand thrusts,
15 cm: a shallower lunge on her body; everything else within 4-11 cm).
"""
import os
import unreal

SPARROW = "/Game/ParagonSparrow/Characters/Heroes/Sparrow/Animations/"
SPEAR = "/Game/SpearAnimationsV2/Animations/Character/"
SETS = {
    "Bow": ("/Game/ParagonSparrow/Characters/Heroes/Sparrow/Meshes/Sparrow", [
        SPARROW + n for n in ["idle", "idle_relaxed", "Travel_mode_Idle", "Travel_Mode_Idle_BowDown", "RMB_Drawback", "RMB_Loop", "RMB_Fire",
                              "Primary_Fire_Slow", "Primary_Fire_Med", "Primary_Fire_Fast"]] + [
        SPARROW + "AimOffset/" + n for n in ["idle_AO_CU", "idle_AO_CC", "idle_AO_CD"]]),
    "Spear": ("/Game/SpearAnimationsV2/Mannequin/Character/Mesh/SK_Mannequin", [
        SPEAR + "2Hand/Pose01/" + n for n in ["A_2H_Idle_01", "A_2H_walk_01", "A_2H_run_01", "A_2H_Crouch_01", "A_2H_01_Thrust_High",
                                               "A_2H_01_Thrust_Mid", "A_2H_01_Thrust_Low", "A_2H_01_Jab_Mid", "A_2H_01_Swing_HorizontalMid",
                                               "A_2H_01_Swing_VerticalDown", "A_2H_Block_01", "A_2H_HitHigh_01", "A_2H_HitLow_01", "A_2H01_to_1H01"]] + [
        SPEAR + "1Hand/Pose01/" + n for n in ["A_1H_Idle_01", "A_1H_walk_01", "A_1H_run_01", "A_1H01_to_2H01"]] + [
        SPEAR + "1Hand/A_1H_Throw"] + [SPEAR + "1Hand/Pose02/" + n for n in ["A_1H_02_Thrust_Mid", "A_1H_02_Jab_Mid"]]),
}
UE4_RIG = "/Game/Characters/UE4_Mannequin/Rigs/IK_UE4_Mannequin_Retarget"   # GASP's (Tools/import_gasp.py)
TEMPLATE_RTG = "/MOFramework/Characters/Crouch/RTG_MoverMannyToMetaHuman"
TARGET_MESH = "/Game/MetaHumans/MH_F_1/Body/SKM_MH_F_1_BodyMesh"
REPAIR = os.path.join(unreal.Paths.project_content_dir(), "Python", "repair_crouch_pelvis.py")


def log(s):
    unreal.log_warning("WEAPONS " + s)


def dup(src_path, dst_path):
    if unreal.EditorAssetLibrary.does_asset_exist(dst_path):
        return unreal.load_asset(dst_path)
    return unreal.EditorAssetLibrary.duplicate_asset(src_path, dst_path)


def make_rigs(kind, mesh):
    folder = "/MOFramework/Characters/" + kind
    rig = dup(UE4_RIG, "%s/IK_%sSource" % (folder, kind))
    unreal.IKRigController.get_controller(rig).set_skeletal_mesh(mesh)
    rtg = dup(TEMPLATE_RTG, "%s/RTG_%sToMetaHuman" % (folder, kind))
    c = unreal.IKRetargeterController.get_controller(rtg)
    c.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rig)
    c.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, mesh)
    c.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    unreal.EditorAssetLibrary.save_loaded_asset(rig, False)
    unreal.EditorAssetLibrary.save_loaded_asset(rtg, False)
    return rtg


def retarget(kind, mesh, rtg, paths):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = [reg.get_asset_by_object_path("%s.%s" % (p, p.split("/")[-1])) for p in paths]
    missing = [p for p, a in zip(paths, assets) if not a.is_valid()]
    if missing:
        log("%s: MISSING sources %s" % (kind, missing))
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", [a for a in assets if a.is_valid()])
    inputs.set_editor_property("source_mesh", mesh)
    inputs.set_editor_property("target_mesh", unreal.load_asset(TARGET_MESH))
    inputs.set_editor_property("ik_retarget_asset", rtg)
    inputs.set_editor_property("suffix", "_MH")
    inputs.set_editor_property("target_path", "/MOFramework/Characters/" + kind)
    inputs.set_editor_property("use_source_path", False)
    inputs.set_editor_property("overwrite_existing_files", True)
    inputs.set_editor_property("include_referenced_assets", False)
    out = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    for a in out:
        clip = a.get_asset()
        if not clip:
            continue
        # Paragon's clips carry its own curves (DissolveAmount -- a material curve for its dissolve effect --, DistanceCurve, mask_geo); copied
        # over, a -game process crashed loading the clip (EXCEPTION_ACCESS_VIOLATION in UnrealEd, Oct 29). None of them means anything here.
        curves = [str(c) for c in unreal.AnimationLibrary.get_animation_curve_names(clip, unreal.RawCurveTrackTypes.RCT_FLOAT)]
        if curves:
            unreal.AnimationLibrary.remove_all_curve_data(clip)
            log("%s: removed the source's curves %s" % (clip.get_name(), curves))
        unreal.EditorAssetLibrary.save_loaded_asset(clip, False)
    log("%s: retargeted %d of %d" % (kind, len(out), len(paths)))
    return ["/MOFramework/Characters/%s/%s_MH" % (kind, p.split("/")[-1]) for p in paths]


def world(anim, bone, t):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, unreal.AnimPoseEvaluationOptions())
    return unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation


def wz(anim, bone, t):
    return world(anim, bone, t).z


def measure(src_path, dst_path):
    src, dst = unreal.load_asset(src_path), unreal.load_asset(dst_path)
    Ls, Ld = src.get_editor_property("sequence_length"), dst.get_editor_property("sequence_length")
    bones = ("pelvis", "thigh_l", "foot_l", "foot_r", "head")
    rows = [([wz(dst, b, Ld * i / 8.0) for b in bones], [wz(src, b, Ls * i / 8.0) for b in bones]) for i in range(9)]
    k = rows[0][0][4] / rows[0][1][4] if rows[0][1][4] else 1.0   # the bodies' scale: head heights at the start
    worst = max(abs(d[j] - s[j] * k) for d, s in rows for j in range(4))
    d, s = rows[4]

    def travel(anim, L, bone):   # how far the bone gets from where it starts, across the ground (cm)
        p0 = world(anim, bone, 0.0)
        return max(((world(anim, bone, L * i / 8.0) - p0) * unreal.Vector(1.0, 1.0, 0.0)).length() for i in range(9))
    log("%-32s x%.2f | mid pelvis %3.0f (src %3.0f) thigh %3.0f (%3.0f) feet %3.0f/%3.0f (%3.0f/%3.0f) | worst %4.1f cm | travel: root %3.0f pelvis %3.0f (src root %3.0f pelvis %3.0f)" % (
        dst_path.split("/")[-1], k, d[0], s[0] * k, d[1], s[1] * k, d[2], d[3], s[2] * k, s[3] * k, worst,
        travel(dst, Ld, "root"), travel(dst, Ld, "pelvis"), travel(src, Ls, "root"), travel(src, Ls, "pelvis")))
    return worst


try:
    made = []
    for kind, (mesh_path, paths) in SETS.items():
        mesh = unreal.load_asset(mesh_path)
        if not mesh:
            log("%s: no source mesh %s -- copy the pack first (see the header)" % (kind, mesh_path))
            continue
        made += list(zip(paths, retarget(kind, mesh, make_rigs(kind, mesh), paths)))
    os.environ["REPAIR_CLIPS"] = ",".join(d for _, d in made)
    exec(open(REPAIR, encoding="utf-8").read(), {"__name__": "repair"})
    off = [d.split("/")[-1] for s, d in made if measure(s, d) > 12.0]
    log("done: %d clips; off their source by more than 12 cm somewhere: %s" % (len(made), off or "none"))
except Exception:
    import traceback
    log("ERROR " + traceback.format_exc().replace("\n", " | "))
