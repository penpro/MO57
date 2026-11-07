"""The editor's side of Tools/close_rocks.py (Wes, Nov 6: close the open rock scans in Blender so they sit in the ground and cannot be seen through).

export(mesh, fbx): the mesh at full detail (a Nanite mesh's render LOD0 is its coarse fallback, so a transient copy built from the source description: icon_studio_export._full_detail).
import_closed(fbx, source, dest): the Blender result as a NEW mesh beside nothing of the source's: the vendor scan packs are gitignored and per machine, a closed copy under /Game/ClosedRocks is in git. Same materials by slot name,
the source's Nanite settings, and the source's kind of collision (none, simple shapes rebuilt as a convex hull set, or complex as simple): the shape changed, so its old hulls would not fit.
"""
import json

import unreal

import icon_studio_export
import kitbash


def export(mesh_path, fbx_path):
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise ValueError("close_rocks.export: no static mesh at %s (got %r)" % (mesh_path, mesh))
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    task = unreal.AssetExportTask()
    task.object = icon_studio_export._full_detail(mesh)
    task.filename = fbx_path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.StaticMeshExporterFBX()
    task.options = options
    ok = unreal.Exporter.run_asset_export_task(task)
    return json.dumps({"ok": bool(ok), "triangles": task.object.get_num_triangles(0), "nanite": bool(mesh.get_editor_property("nanite_settings").enabled)})


def set_seat(mesh, bottom_z):
    """Where the open scan ended: FMOSpeciesPlacement::SeatBounds seats the mesh there, so its back and walls stay under the ground (Wes, Nov 6: "the newly closed geometry assets need to still be pushed into the terrain")."""
    socket = mesh.find_socket("MOSeat")
    if not socket:
        socket = unreal.new_object(unreal.StaticMeshSocket, mesh)
        socket.set_editor_property("socket_name", "MOSeat")
        mesh.add_socket(socket)
    socket.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, bottom_z))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return bottom_z


def seat_existing(closed_path, source_path):
    """A copy made before the seat existed: its source's bottom, from the source."""
    source = unreal.load_asset(source_path)
    bounds = source.get_bounds()
    z = set_seat(unreal.load_asset(closed_path), bounds.origin.z - bounds.box_extent.z)
    return json.dumps({"closed": closed_path, "seat": z})


def source_triangles(mesh):
    try:
        return mesh.get_static_mesh_description(0).get_triangle_count()
    except Exception:
        return -1


def give_fbx_source(mesh, fbx_path):
    """An import over an existing mesh is a reimport, and a mesh whose first source file isn't an .fbx or .obj (a pack's mesh with none recorded) fails the FBX reimporter's CanReimport, so FReimportManager opens an
    open-file prompt the automated flag doesn't stop (Editor.cpp GetNewReimportPath), which held a batch for 7.4 hours (Nov 6). With an FBX source recorded first, the reimporter takes it and never asks."""
    data = mesh.get_editor_property("asset_import_data")
    names = data.extract_filenames() if data else []
    if not (names and names[0].lower().endswith((".fbx", ".obj"))):
        if not isinstance(data, unreal.FbxStaticMeshImportData):
            data = unreal.new_object(unreal.FbxStaticMeshImportData, mesh)
            mesh.set_editor_property("asset_import_data", data)
        data.scripted_add_filename(fbx_path, 0, "")
    names = mesh.get_editor_property("asset_import_data").extract_filenames()
    return bool(names and names[0].lower().endswith((".fbx", ".obj")))


def import_closed(fbx_path, source_path, dest_path):
    # Everything of the source's is read BEFORE the import: closing in place overwrites it.
    source = unreal.load_asset(source_path)
    in_place = source_path == dest_path
    triangles_before = source_triangles(source)
    if in_place and not give_fbx_source(source, fbx_path):
        return json.dumps({"error": "could not record an FBX source on %s, so the import would prompt for a file: nothing imported" % source_path})
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    source_bounds = source.get_bounds()
    source_bottom = source_bounds.origin.z - source_bounds.box_extent.z
    existing_seat = source.find_socket("MOSeat")
    if existing_seat:
        source_bottom = existing_seat.get_editor_property("relative_location").z
    slots = [(str(s.material_slot_name), s.material_interface) for s in source.static_materials]
    nanite = source.get_editor_property("nanite_settings")
    body = source.get_editor_property("body_setup")
    flag = body.get_editor_property("collision_trace_flag") if body else None
    hulls = max(sm.get_convex_collision_count(source), 0)
    simple = max(sm.get_simple_collision_count(source), 0)
    result = json.loads(kitbash.import_edit(fbx_path, dest_path, source_path))
    if "error" in result:
        return json.dumps(result)
    mesh = unreal.load_asset(dest_path)
    if in_place and source_triangles(mesh) == triangles_before:   # a closed mesh always has more; a cancelled or refused reimport leaves the old one, and nothing after this may save it as if it were closed
        return json.dumps({"error": "the import left %s as it was (%d triangles): not closed, not saved" % (source_path, triangles_before)})
    by_name = dict(slots)
    for i, slot in enumerate(mesh.static_materials):
        material = by_name.get(str(slot.material_slot_name).split(".")[0]) or (slots[i][1] if i < len(slots) else None)
        if material:
            mesh.set_material(i, material)
    set_seat(mesh, source_bottom)
    # The source's Nanite settings.
    sm.set_nanite_settings(mesh, nanite, apply_changes=True)
    # The source's kind of collision.
    new_body = mesh.get_editor_property("body_setup")
    notes = []
    if new_body and flag is not None:
        new_body.set_editor_property("collision_trace_flag", flag)
        notes.append(str(flag).split(".")[-1].split(":")[0])
    if flag != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE and hulls + simple > 0:
        sm.set_convex_decomposition_collisions(mesh, max(hulls, 1), 16, 100000)
        notes.append("%d hull(s), source %d hull(s) + %d simple" % (max(sm.get_convex_collision_count(mesh), 0), hulls, simple))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    result["collision"] = notes
    result["nanite"] = bool(mesh.get_editor_property("nanite_settings").enabled)
    result["triangles"] = mesh.get_num_triangles(0)
    result["seat"] = source_bottom
    return json.dumps(result)
