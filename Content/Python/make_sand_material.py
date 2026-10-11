"""Build the SAND voxel material (for sea-level shores) and a dark JUNGLE FLOOR variant (for the rainforest), and register both in the voxel mega material.

    python Tools/gen_sand_textures.py                                      (once: makes Saved/GeneratedTextures/sand/*.png with the system Python)
    python Tools/ue.py py --file Content/Python/make_sand_material.py      (editor open, PIE NOT running)

What a voxel ground material is here (found by reading the existing ones, 2026-10-10): a UVoxelSurfaceTypeAsset (VST_*) pointing at a material instance of M_QuixelBase (Diffuse / Height /
Roughness / Normal textures + tiling scalars), and the VoxelMegaMaterial (NaniteMegaMaterial) lists every surface type the terrain may paint. The MO Terrain node paints a biome's
GroundSurfaceType (DT_Biomes) -- so "sand on the shore" is: texture set -> MI_Sand -> VST_MI_Sand -> in the mega material -> ShoreStrand's GroundSurfaceType.
Both new materials are duplicates of an existing sibling (same parent, same tiling/dither settings, same blend smoothness) with their own textures / tint. Idempotent. Output lines start with SAND.
"""
import unreal

SRC = "D:/UEProjects/MO57/Saved/GeneratedTextures/sand/"
BASE = "/Game/VoxelExamples/NaniteMaterials/Materials"
SAND_DIR = BASE + "/Sand"
JUNGLE_DIR = BASE + "/JungleFloor"
MEGA = BASE + "/NaniteMegaMaterial"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("SAND " + msg)


def import_texture(png, name, like):
    """Import one PNG as `name` in SAND_DIR with the same texture settings as the existing texture `like`."""
    path = SAND_DIR + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.filename = SRC + png
    task.destination_path = SAND_DIR
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    ref = unreal.load_asset(like)
    for prop in ("compression_settings", "srgb", "mip_gen_settings", "lod_group", "filter", "address_x", "address_y", "flip_green_channel", "never_stream"):
        try:
            tex.set_editor_property(prop, ref.get_editor_property(prop))
        except Exception as e:   # a property this engine version does not expose: the import default stands
            log("  (texture property %s not copied: %s)" % (prop, str(e)[:50]))
    eal.save_loaded_asset(tex)
    log("imported %s like %s" % (path, like))
    return tex


def duplicate(src, dst):
    if eal.does_asset_exist(dst):
        return unreal.load_asset(dst)
    out = eal.duplicate_asset(src, dst)
    log("duplicated %s -> %s" % (src, dst))
    return out


def main():
    # --- sand
    ref = BASE + "/Gravel/"
    diff = import_texture("sand_diff_2k.png", "sand_diff_2k", ref + "rocky_trail_diff_2k")
    nor = import_texture("sand_nor_gl_2k.png", "sand_nor_gl_2k", ref + "rocky_trail_nor_gl_2k")
    rough = import_texture("sand_rough_2k.png", "sand_rough_2k", ref + "rocky_trail_rough_2k")
    height = import_texture("sand_height_2k.png", "sand_height_2k", ref + "rocky_trail_disp_2k_Heightmap")

    mi_sand = duplicate(ref + "MI_Gravel", SAND_DIR + "/MI_Sand")
    for name, tex in (("Diffuse", diff), ("Normal", nor), ("Roughness", rough), ("Height", height)):
        mel.set_material_instance_texture_parameter_value(mi_sand, name, tex)
    for name, value in (("TextureSize", 300.0), ("NormalStrength", 0.6), ("HeightMultiplier", 0.6), ("TilingSizeExponent", 2.0), ("TilingSwitchFrequency", 2000.0)):
        mel.set_material_instance_scalar_parameter_value(mi_sand, name, value)
    mel.set_material_instance_vector_parameter_value(mi_sand, "Color", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mel.update_material_instance(mi_sand)
    eal.save_loaded_asset(mi_sand)

    vst_sand = duplicate(BASE + "/VST_MI_Gravel", SAND_DIR + "/VST_MI_Sand")
    vst_sand.set_editor_property("material", mi_sand)
    vst_sand.set_editor_property("blend_smoothness", 0.35)   # a beach grades into its neighbours rather than ending at a line
    eal.save_loaded_asset(vst_sand)

    # --- jungle floor: the muddy-leaves material, darker and greener (wet, shaded, mossy)
    mi_jungle = duplicate(BASE + "/MuddyLeaves/MI_MuddyLeaves", JUNGLE_DIR + "/MI_JungleFloor")
    mel.set_material_instance_vector_parameter_value(mi_jungle, "Color", unreal.LinearColor(0.50, 0.62, 0.40, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi_jungle, "NormalStrength", 1.3)
    mel.update_material_instance(mi_jungle)
    eal.save_loaded_asset(mi_jungle)
    vst_jungle = duplicate(BASE + "/VST_MI_MuddyLeaves", JUNGLE_DIR + "/VST_MI_JungleFloor")
    vst_jungle.set_editor_property("material", mi_jungle)
    eal.save_loaded_asset(vst_jungle)

    # --- the mega material must list every surface type the terrain may paint
    mega = unreal.load_asset(MEGA)
    types = list(mega.get_editor_property("surface_types"))
    have = [t.get_path_name() for t in types if t]
    changed = False
    for vst in (vst_sand, vst_jungle):
        if vst.get_path_name() not in have:
            types.append(vst)
            changed = True
            log("added %s to the mega material" % vst.get_name())
    if changed:
        mega.set_editor_property("surface_types", types)
        eal.save_loaded_asset(mega)
    log("DONE: mega material lists %d surface types" % len(mega.get_editor_property("surface_types")))


main()
