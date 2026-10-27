"""The foot IK's virtual bones on the MetaHuman body skeleton (Oct 27 2026).

The survivor's foot IK rig (/MOFramework/Characters/MetaHumans/Female/Animations/CR_MH_F, run by ABP_MH_F) plants the feet on targets the
skeleton carries as virtual bones: VB root_root (root -> root), and from it VB VB root_root_foot_l / _r (-> foot_l / foot_r). They are a
project addition: the MetaHuman Creator's own metahuman_base_skel has none, so copying the Creator's Common content over ours (Oct 27,
for the Lillian base) wiped them -- the legs stopped walking. Run after any refresh of /Game/MetaHumans/Common (MO57 and MO_Tools):

    python Tools/ue.py py --file Content/Python/ensure_foot_ik_virtual_bones.py

Adds whichever are missing (UAnimationBlueprintLibrary::AddVirtualBone through any clip on the skeleton), saves the skeleton, prints
"VIRTUAL BONES <added> added, <kept> already there". The test MOFramework.Animation.FootIKVirtualBones checks them.
"""
import unreal

SKELETON = "/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel"
# (source, target) in the order they build on each other; the engine names each "VB <source>_<target>".
CHAIN = [("root", "root"), ("VB root_root", "foot_l"), ("VB root_root", "foot_r")]


def _emit(msg):
    try:
        out(msg)  # noqa: F821 -- the bridge's output, when run through it
    except NameError:
        unreal.log(msg)


def main():
    skeleton = unreal.load_asset(SKELETON)
    if not skeleton:
        _emit("VIRTUAL BONES FAILED: no skeleton at " + SKELETON)
        return
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    clips = [a for a in registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "AnimSequence"))
             if SKELETON in str(a.get_tag_value("Skeleton"))]
    clip = unreal.load_asset(str(clips[0].package_name)) if clips else None
    if not clip:
        _emit("VIRTUAL BONES FAILED: no clip on the skeleton to add them through")
        return
    existing = {str(n) for n in unreal.AnimationLibrary.get_animation_virtual_bones(clip)} if hasattr(unreal.AnimationLibrary, "get_animation_virtual_bones") else set()
    added, kept = [], []
    for source, target in CHAIN:
        name = "VB %s_%s" % (source, target)
        if name in existing:
            kept.append(name)
            continue
        made = unreal.AnimationLibrary.add_virtual_bone(clip, source, target)
        (added if str(made) == name else kept).append(name)
    if added:
        unreal.EditorAssetLibrary.save_asset(SKELETON, only_if_is_dirty=False)
    _emit("VIRTUAL BONES %d added (%s), %d already there (%s)" % (len(added), ", ".join(added), len(kept), ", ".join(kept)))


main()
