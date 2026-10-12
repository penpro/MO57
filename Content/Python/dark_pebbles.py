"""Two derived voxel surfaces: DarkPebbles (the Nordic Strand beach: MI_Gravel, grey rocky trail, tinted dark and tiled larger) and ForestLitter (the Electric Forest floor: MI_MuddyLeaves tinted darker and greener).
Each is its own surface type in the mega material.

    python Tools/ue.py py --file Content/Python/dark_pebbles.py        (editor open, PIE NOT running, no instance running: it holds the files)

(The first Nordic palette used the Electric Dreams IcelandicQuarryRock scan: close up it is rust red, not the black volcanic beach the biome wants.) Idempotent. Output lines start with DARKPEB.
"""
import unreal

BASE = "/Game/VoxelExamples/NaniteMaterials/Materials"
MEGA = BASE + "/NaniteMegaMaterial"
SURFACES = [  # (name, source MI, source VST, tint, tile cm)
    ("DarkPebbles", BASE + "/Gravel/MI_Gravel", BASE + "/VST_MI_Gravel", (0.40, 0.41, 0.47, 1.0), 380.0),            # the gravel is light grey; this reads as wet dark basalt gravel
    ("ForestLitter", BASE + "/MuddyLeaves/MI_MuddyLeaves", BASE + "/VST_MI_MuddyLeaves", (0.34, 0.38, 0.26, 1.0), None),  # leaf litter under a canopy: darker, a little green
]

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def dup(src, dst):
    return unreal.load_asset(dst) if eal.does_asset_exist(dst) else eal.duplicate_asset(src, dst)


mega = unreal.load_asset(MEGA)
types = list(mega.get_editor_property("surface_types"))
have = [t.get_path_name() for t in types if t]
for name, src_mi, src_vst, tint, tile in SURFACES:
    mi = dup(src_mi, "%s/%s/MI_%s" % (BASE, name, name))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*tint))
    if tile:
        mel.set_material_instance_scalar_parameter_value(mi, "TextureSize", tile)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    vst = dup(src_vst, "%s/%s/VST_MI_%s" % (BASE, name, name))
    vst.set_editor_property("material", mi)
    vst.set_editor_property("blend_smoothness", 0.35)
    eal.save_loaded_asset(vst)
    if vst.get_path_name() not in have:
        types.append(vst)
        have.append(vst.get_path_name())
        unreal.log("DARKPEB added %s to the mega material" % vst.get_name())
    unreal.log("DARKPEB %s ready: %s" % (name, vst.get_path_name()))
mega.set_editor_property("surface_types", types)
eal.save_loaded_asset(mega)
unreal.log("DARKPEB done: mega material lists %d surface types" % len(types))
