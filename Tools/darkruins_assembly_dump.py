"""Dump the Dark Ruins Megascans sample's packed-level assemblies (Content/PLBPs/*.umap): every mesh in each, how many, and the assembly's size.

Run INSIDE the sample (MO57 has only a migrated slice of its assets; the sample has no Python plugin, so enable it for the run):

    UnrealEditor-Cmd.exe D:\\UEProjects\\DarkRuinsMegascansSample\\DarkRuinsMegascansSample.uproject -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities
        -ExecutePythonScript=D:/UEProjects/MO57/Tools/darkruins_assembly_dump.py -unattended -nullrhi -nosplash -stdout

Writes D:/UEProjects/MO57/Saved/DarkRuinsDump/assemblies.json ({level: {"bounds": [min, max], "meshes": {path: count}}}) and logs DRDUMP lines.
"""
import json
import os

import unreal

OUT = "D:/UEProjects/MO57/Saved/DarkRuinsDump/"
os.makedirs(OUT, exist_ok=True)
LEVELS = ["Mountain_01", "BoulderAssembly_01", "Buildup_Rocky_01", "ShelfAssembly_01", "GroundAssembly_01", "GroundAssembly_02", "GroundAssembly_03",
          "GroundAssembly_05", "GroundAssembly_06", "GroundAssembly_09", "Buildup_Sand_01", "Buildup_Sand_02", "TallPath_01", "Tower_01"]

result = {}
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for name in LEVELS:
    path = "/Game/PLBPs/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_warning("DRDUMP %s missing" % name)
        continue
    les.load_level(path)
    meshes, lo, hi, pieces = {}, [1e12] * 3, [-1e12] * 3, []
    for a in eas.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.get_editor_property("static_mesh")
            if not sm:
                continue
            ism = isinstance(c, unreal.InstancedStaticMeshComponent)
            n = c.get_instance_count() if ism else 1
            if n == 0:
                continue
            meshes[sm.get_path_name()] = meshes.get(sm.get_path_name(), 0) + n
            for i in range(n):
                wt = c.get_instance_transform(i, True) if ism else c.get_world_transform()
                loc, rot, scl = wt.translation, wt.rotation.rotator(), wt.scale3d
                pieces.append({"mesh": sm.get_path_name(), "loc": [loc.x, loc.y, loc.z], "rot": [rot.roll, rot.pitch, rot.yaw],
                               "scale": [scl.x, scl.y, scl.z]})
                for j, k in enumerate(("x", "y", "z")):
                    lo[j] = min(lo[j], getattr(loc, k))
                    hi[j] = max(hi[j], getattr(loc, k))
    result[name] = {"pivot_bounds": [lo, hi], "meshes": meshes, "pieces": pieces}
    top = sorted(meshes.items(), key=lambda kv: -kv[1])[:8]
    unreal.log_warning("DRDUMP %s: %d meshes, %d pieces, pivots span %.0f x %.0f x %.0f m; top %s" % (
        name, len(meshes), sum(meshes.values()), (hi[0] - lo[0]) / 100, (hi[1] - lo[1]) / 100, (hi[2] - lo[2]) / 100,
        ", ".join("%s x%d" % (p.split(".")[-1], c) for p, c in top)))
json.dump(result, open(OUT + "assemblies.json", "w"), indent=1)
unreal.log_warning("DRDUMP done: %d levels" % len(result))
