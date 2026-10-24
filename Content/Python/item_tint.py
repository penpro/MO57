"""Recoloured item materials: a child MI of a scan's material with the FabOverrides masters' colour controls set, so one mesh
can serve related items (raw vs smoked fish, the soils from one sand pile, leathers from one hide) through the item row's
WorldVisual.MaterialOverride (AMOWorldItem and the held mesh apply it to slot 0; the icon studio honours it too).

    python Tools/ue.py py -c "import item_tint as t; out(t.make('/Game/Kitbash/Piles/MI_Dirt', '<parent MI>', tint=(0.62, 0.45, 0.31), Brightness=0.75))"

Parameters the masters have (M_MS_Base / _Fuzz; check others with params(parent)): BaseColor Tint (vector), Brightness,
Contrast, Saturation, Normal Intensity, Min Roughness, Max Roughness. Defaults are identity (white, 1, 1, 1). A parameter the
master lacks is refused rather than silently ignored. Saves the MI. Idempotent.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def _master(material):
    while isinstance(material, unreal.MaterialInstance):
        material = material.get_editor_property("parent")
    return material


def params(material_path):
    master = _master(unreal.load_asset(material_path))
    return {"scalar": [str(n) for n in mel.get_scalar_parameter_names(master)],
            "vector": [str(n) for n in mel.get_vector_parameter_names(master)]}


def make(path, parent_path, tint=None, **scalars):
    """Create or update the MI at <path> as a child of <parent_path>. Scalar names may use _ for spaces (Max_Roughness)."""
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        # EditorAssetLibrary refuses every call in PIE ("The Editor is currently in a play mode"): does_asset_exist said no, create_asset then
        # opened a modal "Overwrite Existing Object" dialog per call and the editor stopped ticking (Oct 24). Refuse instead.
        raise RuntimeError("item_tint.make: end PIE first (ue.py pie end); the asset library refuses in PIE")
    parent = unreal.load_asset(parent_path)
    if parent is None:
        raise ValueError("no material at " + parent_path)
    have = params(parent_path)
    names = {k.replace("_", " "): v for k, v in scalars.items()}
    missing = [n for n in names if n not in have["scalar"]] + (["BaseColor Tint"] if tint and "BaseColor Tint" not in have["vector"] else [])
    if missing:
        raise ValueError("%s's master has no %s" % (parent_path, ", ".join(missing)))
    folder, name = path.rsplit("/", 1)
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    if tint:
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    for n, v in names.items():
        mel.set_material_instance_scalar_parameter_value(mi, n, v)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return "%s.%s" % (path, name)
