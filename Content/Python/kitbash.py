"""Kitbash, editor steps: export a static mesh to FBX, and bring an edited FBX back as a new mesh on the source's materials.

    Driven by Tools/kitbash.py. Helpers:
      export(mesh_path, fbx_path)                      LOD0, no collision
      import_edit(fbx_path, dest_mesh_path, src_mesh)  -> JSON {mesh, size, slots}; the new mesh gets the source materials,
                                                          matched by slot name (src_mesh may be a list: shaft + head)

Why the importer is switched for the import: the Interchange FBX pipeline creates materials and textures from the file, and
assets cannot be deleted safely from editor Python (an ensure in the delete path brings up the crash reporter). The legacy FBX
factory honours FbxImportUI (no materials, no textures, no collision), so the import makes exactly one mesh. Both cvars are
restored afterwards. Interchange.FeatureFlags.Import.SyncToBrowser is off for the same import so no Content Browser pops up.
Collision and Nanite come from item_mesh_prepare afterwards (the driver runs it).
"""
import json

import unreal

eal = unreal.EditorAssetLibrary
CVARS = {"Interchange.FeatureFlags.Import.FBX": "0", "Interchange.FeatureFlags.Import.SyncToBrowser": "0"}


def export(mesh_path, fbx_path):
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh, unreal.StaticMesh):
        # A null object reaches UExporter's check(Object) and takes the editor down (Oct 24): refuse here instead.
        raise ValueError("kitbash.export: no static mesh at %s (got %r)" % (mesh_path, mesh))
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = fbx_path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.StaticMeshExporterFBX()
    task.options = options
    return "OK" if unreal.Exporter.run_asset_export_task(task) else "FAIL"


def import_edit(fbx_path, dest_mesh_path, src_mesh_path):
    folder, name = dest_mesh_path.rsplit("/", 1)
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.auto_generate_collision = False
    ui.static_mesh_import_data.convert_scene_unit = True   # Blender writes metres + a unit factor; without this 131 cm -> 1.3 cm
    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = ui
    previous = {k: unreal.SystemLibrary.get_console_variable_string_value(k) for k in CVARS}
    for k, v in CVARS.items():
        unreal.SystemLibrary.execute_console_command(None, "%s %s" % (k, v))
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        for k, v in previous.items():
            unreal.SystemLibrary.execute_console_command(None, "%s %s" % (k, v or "1"))
    mesh = unreal.load_asset(dest_mesh_path)
    if not isinstance(mesh, unreal.StaticMesh):
        return json.dumps({"error": "import produced no static mesh at " + dest_mesh_path})
    # the FBX carries the source materials' names as slot names (Blender keeps them), so map by name; fall back to the order
    # (base slots, then an attached head's slots)
    by_name, ordered = {}, []
    for path in src_mesh_path if isinstance(src_mesh_path, (list, tuple)) else [src_mesh_path]:
        for slot in unreal.load_asset(path).static_materials:
            material = slot.material_interface
            ordered.append(material)
            by_name[material.get_name()] = material
            by_name[str(slot.material_slot_name)] = material
    mapped = []
    for i, slot in enumerate(mesh.static_materials):
        name = str(slot.material_slot_name).split(".")[0]   # Blender may suffix duplicates (.001)
        material = by_name.get(name) or (ordered[i] if i < len(ordered) else None)
        if material:
            mesh.set_material(i, material)
            mapped.append("%s=%s" % (name, material.get_name()))
    eal.save_loaded_asset(mesh)
    box = mesh.get_bounding_box()
    size = box.max - box.min
    return json.dumps({"mesh": dest_mesh_path, "size": [round(size.x, 1), round(size.y, 1), round(size.z, 1)],
                       "slots": mapped})
