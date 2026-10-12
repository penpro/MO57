"""Stage 2: import the unpacked Electric Dreams ground surfaces as voxel terrain materials (see Tools/ed_surfaces_prep.py).

    python Tools/ue.py py --file Content/Python/ed_surfaces_import.py        (editor open, PIE NOT running)

Per surface: four textures in /Game/VoxelExamples/NaniteMaterials/Materials/<Surface>/ (same texture settings as the gravel set), a material instance duplicated from MI_Gravel (same parent, same
tiling / dither settings) with its own maps, a surface type VST_MI_<Surface>, and an entry in the NaniteMegaMaterial (it must list every surface the terrain may paint). Idempotent.
Output lines start with EDSURF.
"""
import unreal

SRC = "D:/UEProjects/MO57/Saved/EDSurf/out/"
BASE = "/Game/VoxelExamples/NaniteMaterials/Materials"
MEGA = BASE + "/NaniteMegaMaterial"
GRAVEL = BASE + "/Gravel/"

# surface -> (tile size cm, normal strength, height multiplier, tint, normal map flipped (GL))
SURFACES = {
    "NordicMoss": (260.0, 1.0, 0.5, (1.0, 1.0, 1.0, 1.0), False),
    "MossyRockyGround": (300.0, 1.2, 0.7, (1.0, 1.0, 1.0, 1.0), False),
    "IcelandicQuarryRock": (320.0, 1.0, 0.7, (1.0, 1.0, 1.0, 1.0), True),
    "MossyGrass": (260.0, 1.0, 0.4, (1.0, 1.0, 1.0, 1.0), False),
    "DriedGrassOnDampSoil": (260.0, 1.0, 0.5, (1.0, 1.0, 1.0, 1.0), False),
}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(m):
    unreal.log("EDSURF " + m)


def import_texture(png, dest, name, like, flip_green):
    path = dest + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.filename = SRC + png
    task.destination_path = dest
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    ref = unreal.load_asset(like)
    for prop in ("compression_settings", "srgb", "mip_gen_settings", "lod_group", "filter", "address_x", "address_y", "never_stream"):
        try:
            tex.set_editor_property(prop, ref.get_editor_property(prop))
        except Exception:
            pass
    try:
        tex.set_editor_property("flip_green_channel", flip_green)
    except Exception:
        pass
    eal.save_loaded_asset(tex)
    return tex


def duplicate(src, dst):
    if eal.does_asset_exist(dst):
        return unreal.load_asset(dst)
    return eal.duplicate_asset(src, dst)


def main():
    mega = unreal.load_asset(MEGA)
    types = list(mega.get_editor_property("surface_types"))
    have = [t.get_path_name() for t in types if t]
    for s, (size, nstr, hmul, tint, glnormal) in SURFACES.items():
        dest = BASE + "/" + s
        diff = import_texture(s + "_diff.png", dest, "T_" + s + "_Diffuse", GRAVEL + "rocky_trail_diff_2k", False)
        nor = import_texture(s + "_nor.png", dest, "T_" + s + "_Normal", GRAVEL + "rocky_trail_nor_gl_2k", glnormal)
        rough = import_texture(s + "_rough.png", dest, "T_" + s + "_Roughness", GRAVEL + "rocky_trail_rough_2k", False)
        height = import_texture(s + "_height.png", dest, "T_" + s + "_Height", GRAVEL + "rocky_trail_disp_2k_Heightmap", False)
        mi = duplicate(GRAVEL + "MI_Gravel", dest + "/MI_" + s)
        for pname, tex in (("Diffuse", diff), ("Normal", nor), ("Roughness", rough), ("Height", height)):
            mel.set_material_instance_texture_parameter_value(mi, pname, tex)
        for pname, val in (("TextureSize", size), ("NormalStrength", nstr), ("HeightMultiplier", hmul)):
            mel.set_material_instance_scalar_parameter_value(mi, pname, val)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*tint))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        vst = duplicate(BASE + "/VST_MI_Gravel", dest + "/VST_MI_" + s)
        vst.set_editor_property("material", mi)
        vst.set_editor_property("blend_smoothness", 0.35)
        eal.save_loaded_asset(vst)
        if vst.get_path_name() not in have:
            types.append(vst)
            have.append(vst.get_path_name())
            log("added %s to the mega material" % vst.get_name())
        log("%s: textures + MI + VST ready" % s)
    mega.set_editor_property("surface_types", types)
    eal.save_loaded_asset(mega)
    log("DONE: mega material lists %d surface types" % len(mega.get_editor_property("surface_types")))


main()
