"""Icon studio, step 1 of 3 (editor): export each item's world mesh + the textures its materials use, for the Blender render.

    Driven by Tools/icon_studio.py (python Tools/icon_studio.py --missing). By hand:
    python Tools/ue.py py -c "import icon_studio_export as e; out(e.run({'Mushroom01': '/Game/...'}, 'D:/UEProjects/MO57/Saved/IconStudio'))"

Per item, into <out>/<ItemId>/: mesh.fbx (LOD0, no collision) + slot<N>_<base|normal|orm|mask>.png (.exr for an HDR source) + meta.json:
  slots[i] = {slot, name, kind: opaque|foliage, base, normal, orm, mask, normal_opengl}
  scale    = the item row's WorldVisual Scale3D (UE axes), so the icon shows the item as the world does: a flatbread is a rye
             loaf at Z 0.4. Uniform scales change nothing in the framing.
  extent   = the mesh's bounding-box size (UE axes, cm); Blender matches it against the imported mesh to map scale onto its axes
  slot 0 uses the row's WorldVisual.MaterialOverride when it has one (AMOWorldItem and the held mesh do the same), and a
  slot's non-default colour controls (BaseColor Tint, Brightness, Contrast, Saturation of the FabOverrides masters) go into
  slots[i].adjust, so a recoloured scan (IronBloom01 = a porous rock darkened) renders as the world shows it.
Texture parameter names are the ones every project master shares (FabOverrides M_MS_Base / _Fuzz / _Foliage and the glTF
masters): BaseColorTexture, NormalTexture, MetallicRoughnessTexture (Megascans ORM: R=AO, G=roughness, B=metal), Mask (foliage
ORT: R=opacity, G=roughness). normal_opengl records the texture's flip-green flag: glTF sources are OpenGL (the import flips
green at runtime), Megascans are DirectX, so Blender flips green only when it is False.
"""
import json
import os
import re

import unreal

mel = unreal.MaterialEditingLibrary
PARAMS = (("BaseColorTexture", "base"), ("NormalTexture", "normal"), ("MetallicRoughnessTexture", "orm"), ("Mask", "mask"))
ITEMS_TABLE = "/MOFramework/Data/DT_Items.DT_Items"
SCALE_RE = re.compile(r"Scale3D=\(X=([-\d.e]+),Y=([-\d.e]+),Z=([-\d.e]+)\)")


OVERRIDE_RE = re.compile(r'MaterialOverride="([^"]+)"')
ADJUST = {"Brightness": 1.0, "Contrast": 1.0, "Saturation": 1.0}   # the masters' defaults (identity)


def _row_visuals():
    """{ItemId: (scale [x, y, z] or None, material override path or None)} from every item row's WorldVisual."""
    table = unreal.load_asset(ITEMS_TABLE)
    lib = unreal.DataTableFunctionLibrary
    visuals = {}
    for name, visual in zip(lib.get_data_table_row_names(table), lib.get_data_table_column_as_string(table, "WorldVisual")):
        scale = SCALE_RE.search(visual)
        override = OVERRIDE_RE.search(visual)
        visuals[str(name)] = ([float(v) for v in scale.groups()] if scale else None, override.group(1) if override else None)
    return visuals


def _adjust(material):
    """The material's non-default colour controls, or {} when it is untinted."""
    master = _master(material)
    scalars = {str(n) for n in mel.get_scalar_parameter_names(master)}   # a missing parameter reads 0, not "absent"
    vectors = {str(n) for n in mel.get_vector_parameter_names(master)}
    adjust = {}
    for name, default in ADJUST.items():
        if name in scalars:
            value = mel.get_material_instance_scalar_parameter_value(material, name)
            if abs(value - default) > 1e-4:
                adjust[name.lower()] = value
    if "BaseColor Tint" in vectors:
        tint = mel.get_material_instance_vector_parameter_value(material, "BaseColor Tint")
        if any(abs(c - 1.0) > 1e-4 for c in (tint.r, tint.g, tint.b)):
            adjust["tint"] = [tint.r, tint.g, tint.b]
    return adjust


def _export(obj, filename, exporter, options=None):
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = filename
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    if exporter is not None:
        task.exporter = exporter
    if options is not None:
        task.options = options
    return unreal.Exporter.run_asset_export_task(task)


def _export_texture(tex, stem):
    """Exports a texture's source as <stem>.png, or <stem>.exr when the source is HDR (float: the Nordic rock scans are RGBA16F). Returns the file written, or None.

    No explicit exporter: UE then picks one by extension through SupportsObject, which checks the source format. Handing it TextureExporterPNG directly skips
    that check, and an HDR source then asserts (check SupportsTexture, EditorFactories.cpp) and takes the editor down -- Oct 17 2026, twice."""
    for ext in ("png", "exr"):
        filename = "%s.%s" % (stem, ext)
        if _export(tex, filename, None):
            return filename
    return None


def _master(material):
    while isinstance(material, unreal.MaterialInstance):
        material = material.get_editor_property("parent")
    return material


def _two_sided(material):
    """Whether the engine draws both faces of this material: an instance's override wins, else its master's setting."""
    mi = material
    while isinstance(mi, unreal.MaterialInstance):
        overrides = mi.get_editor_property("base_property_overrides")
        if overrides.get_editor_property("override_two_sided"):
            return bool(overrides.get_editor_property("two_sided"))
        mi = mi.get_editor_property("parent")
    master = _master(material)
    return bool(master and master.get_editor_property("two_sided"))


def _full_detail(mesh):
    """The mesh the icon is rendered from. A Nanite mesh's render LOD0 (what the FBX exporter writes) is its coarse fallback --
    BronzeHelmet01: 2,257 of 24,999 triangles, and its thin shell broke up in the icon (Oct 24) -- so export a transient non-Nanite copy
    built from the source mesh description instead. The asset itself is not touched."""
    if not mesh.get_editor_property("nanite_settings").enabled:
        return mesh
    try:
        copy = unreal.new_object(unreal.StaticMesh)
        copy.set_editor_property("static_materials", mesh.static_materials)
        copy.build_from_static_mesh_descriptions([mesh.get_static_mesh_description(0)], False)
        return copy
    except Exception as e:  # no source description (cooked content): the fallback is still better than no icon
        unreal.log_warning("icon studio: %s exports its Nanite fallback (%s)" % (mesh.get_name(), e))
        return mesh


def export_item(item_id, mesh_path, out_root, scale=None, override=None):
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh, unreal.StaticMesh):
        return "SKIP %s: %s is not a static mesh" % (item_id, mesh_path)
    folder = os.path.join(out_root, item_id).replace("\\", "/")
    os.makedirs(folder, exist_ok=True)
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    if not _export(_full_detail(mesh), folder + "/mesh.fbx", unreal.StaticMeshExporterFBX(), options):
        return "FAIL %s: mesh export" % item_id
    slots = []
    for index, slot in enumerate(mesh.static_materials):
        material = slot.material_interface
        if index == 0 and override:
            material = unreal.load_asset(override) or material
        master = _master(material)
        info = {"slot": index, "name": str(slot.material_slot_name),
                "kind": "foliage" if master and "Foliage" in master.get_name() else "opaque", "two_sided": _two_sided(material)}
        if isinstance(material, unreal.MaterialInstance):
            for param, key in PARAMS:
                tex = mel.get_material_instance_texture_parameter_value(material, param)
                if not tex or tex.get_path_name().startswith(("/Engine/", "/InterchangeAssets/")) or "/Textures/Standard/T_Default" in tex.get_path_name():
                    continue
                filename = _export_texture(tex, "%s/slot%d_%s" % (folder, index, key))
                if filename:
                    info[key] = os.path.basename(filename)
                    if key == "normal":
                        info["normal_opengl"] = bool(tex.get_editor_property("flip_green_channel"))
            adjust = _adjust(material)
            if adjust:
                info["adjust"] = adjust
        slots.append(info)
    with open(folder + "/meta.json", "w") as f:
        box = mesh.get_bounding_box()
        size = box.max - box.min
        json.dump({"item": item_id, "mesh": mesh_path, "slots": slots, "scale": scale or [1.0, 1.0, 1.0],
                   "extent": [size.x, size.y, size.z]}, f, indent=1)
    return "OK %s: %d slot(s), %s" % (item_id, len(slots), ", ".join(sorted({k for s in slots for k in s if k in ("base", "normal", "orm", "mask")})) or "no textures")


def run(items, out_root):
    """items: {ItemId: static mesh path}. Returns one report line per item."""
    visuals = _row_visuals()
    return "\n".join(export_item(item_id, path, out_root, *visuals.get(item_id, (None, None))) for item_id, path in items.items())
