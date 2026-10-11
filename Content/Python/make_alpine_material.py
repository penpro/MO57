"""Build the ALPINE HEATH ground (a dark olive-brown peaty soil) as a voxel surface type and register it in the voxel mega material.

    python Tools/ue.py py --file Content/Python/make_alpine_material.py      (editor open, PIE NOT running)

Why: AlpineHeath's ground was VST_MI_Gravel (pale grey-white gravel). Seen from any distance past the grass it read as bare desert, and the lush alpine grass seemed to sit on sand. A heath is peaty
soil with low vegetation: the muddy-leaves texture set, tinted darker and more olive than the jungle floor (same recipe as make_sand_material.py: duplicate an existing sibling so the parent material,
tiling / dither settings and blend smoothness are inherited). Then `DT_Biomes.AlpineHeath.groundSurfaceType` -> VST_MI_AlpineHeath (done by the caller / documented in the exploration doc).
Idempotent. Output lines start with ALPINE.
"""
import unreal

BASE = "/Game/VoxelExamples/NaniteMaterials/Materials"
DIR = BASE + "/AlpineHeath"
MEGA = BASE + "/NaniteMegaMaterial"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("ALPINE " + msg)


def duplicate(src, dst):
    if eal.does_asset_exist(dst):
        return unreal.load_asset(dst)
    out = eal.duplicate_asset(src, dst)
    log("duplicated %s -> %s" % (src, dst))
    return out


def main():
    mi = duplicate(BASE + "/MuddyLeaves/MI_MuddyLeaves", DIR + "/MI_AlpineHeath")
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(0.46, 0.47, 0.30, 1.0))   # olive-brown peat: darker than the jungle floor, no red
    mel.set_material_instance_scalar_parameter_value(mi, "NormalStrength", 1.1)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)

    vst = duplicate(BASE + "/VST_MI_MuddyLeaves", DIR + "/VST_MI_AlpineHeath")
    vst.set_editor_property("material", mi)
    eal.save_loaded_asset(vst)

    mega = unreal.load_asset(MEGA)
    types = list(mega.get_editor_property("surface_types"))
    if vst.get_path_name() not in [t.get_path_name() for t in types if t]:
        types.append(vst)
        mega.set_editor_property("surface_types", types)
        eal.save_loaded_asset(mega)
        log("added %s to the mega material" % vst.get_name())
    log("DONE: mega material lists %d surface types" % len(mega.get_editor_property("surface_types")))


main()
