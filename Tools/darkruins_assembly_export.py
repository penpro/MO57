"""Turn the Dark Ruins sample's packed-level assemblies into PCG data assets (points carrying their meshes and transforms), the form the
Electric Dreams compositions use (`/Game/PCG/Assets/PCGAssemblies/...`, loaded by a PCGAsset element in the composition graph).

Run INSIDE the sample, which has no Python plugin and no PCG plugin enabled; the run enables both:

    UnrealEditor-Cmd.exe D:\\UEProjects\\DarkRuinsMegascansSample\\DarkRuinsMegascansSample.uproject
        -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities,PCG -ExecutePythonScript=D:/UEProjects/MO57/Tools/darkruins_assembly_export.py
        -unattended -nullrhi -nosplash -stdout

DREXPORT_ONLY=<level> exports just that one (each skin-stripped cluster needs a run of its own: see the loop).
Writes /Game/MO57Export/PCGAssemblies/PL_<level>_PCG in the SAMPLE (a folder of its own; nothing of the sample's is changed), for
`Tools/ede_migrate.py copy --source darkruins` to bring over with the meshes they name. Logs DREXPORT lines. Idempotent (re-exports).
"""
import unreal

ASSEMBLIES = ["Mountain_01", "TallPath_01", "BoulderAssembly_01", "Buildup_Rocky_01", "ShelfAssembly_01"]
# The rock clusters' shallow ground skins (rocky-ground plates, sand, mud) do not sit on uneven terrain (Wes, Oct 24 2026: "the shallow depth
# ones are hard to get to sit properly in the terrain so avoid those ones"): removed from the loaded level before the export (in memory; the
# sample's levels are never saved), so the clusters bring only their boulders, rocks and slabs.
SKINS = ("Rocky_Ground", "Sand_01", "MudGround", "Mud_01")
CLUSTERS = ("BoulderAssembly_01", "Buildup_Rocky_01", "ShelfAssembly_01")
DEST = "/Game/MO57Export/PCGAssemblies"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def drop_skins():
    dropped = 0
    for a in eas.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.get_editor_property("static_mesh")
            if not sm or not any(k in sm.get_name() for k in SKINS):
                continue
            if isinstance(c, unreal.InstancedStaticMeshComponent):
                dropped += c.get_instance_count()
                c.clear_instances()
            else:
                c.set_editor_property("static_mesh", None)
                dropped += 1
    return dropped


# One assembly per run when DREXPORT_ONLY names it: a cluster level stripped in memory is never followed by another load_level (which could
# prompt to save it); run each cluster on its own.
import os
for name in [os.environ["DREXPORT_ONLY"]] if os.environ.get("DREXPORT_ONLY") else ASSEMBLIES:
    if not les.load_level("/Game/PLBPs/" + name):
        unreal.log_warning("DREXPORT %s: level did not load" % name)
        continue
    if name in CLUSTERS:
        unreal.log_warning("DREXPORT %s: %d ground-skin piece(s) left out" % (name, drop_skins()))
    exporter = unreal.new_object(unreal.PCGLevelToAsset)
    exporter.set_world(ues.get_editor_world())
    params = unreal.PCGAssetExporterParameters()
    params.set_editor_property("open_save_dialog", False)
    params.set_editor_property("asset_name", "PL_%s_PCG" % name)
    params.set_editor_property("asset_path", DEST)
    params.set_editor_property("save_on_export_ended", True)
    package = unreal.PCGAssetExporterUtils.create_asset(exporter, params)
    unreal.log_warning("DREXPORT %s -> %s" % (name, package.get_name() if package else "FAILED"))
unreal.log_warning("DREXPORT done")
