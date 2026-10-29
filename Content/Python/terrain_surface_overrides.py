"""The ground surfaces' values tuned in the World look lab (D:/UEProjects/MO_WorldLab), kept in terrain_surface_overrides.json beside this file
as {surface: {parameter: value}} (a surface is a SURFACES key of upgrade_terrain_material.py: MuddyLeaves, MossyGrass, Cliff ...).

Applied LAST: upgrade_terrain_material.py's apply_surface_overrides() calls apply() after its own SURFACES table, so rebuilding the terrain
material keeps what was tuned in the lab; Tools/apply_worldlab_patch.py merges a lab export into the JSON and runs this file (editor open):

    python Tools/ue.py py --file Content/Python/terrain_surface_overrides.py

Prints TERRAINOVR lines (each value set, then how many instances were saved).
"""
import json
import os

import unreal

BASE = "/Game/VoxelExamples/NaniteMaterials/Materials"
JSON = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "Python", "terrain_surface_overrides.json")


def instance_path(surface):
    return "%s/%s/MI_%s" % (BASE, surface, surface)


def apply(log):
    data = json.load(open(JSON, encoding="utf-8")) if os.path.exists(JSON) else {}
    mel, eal = unreal.MaterialEditingLibrary, unreal.EditorAssetLibrary
    saved = 0
    for surface, values in sorted(data.items()):
        mi = unreal.load_asset(instance_path(surface))
        if mi is None:
            log("!! no material instance " + instance_path(surface))
            continue
        for name, value in sorted(values.items()):
            mel.set_material_instance_scalar_parameter_value(mi, name, float(value))
            log("%s %s = %s" % (surface, name, value))
        saved += 1 if eal.save_loaded_asset(mi, False) else 0   # False: save even if the set left the package clean
    log("lab overrides: %d surfaces saved from %s" % (saved, JSON))


if __name__ != "terrain_surface_overrides":   # run as a script (the bridge), not imported by upgrade_terrain_material.py
    apply(lambda msg: unreal.log("TERRAINOVR " + msg))
