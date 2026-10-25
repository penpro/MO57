"""The TRELLIS cliff formations draw both sides of their faces (BR25-15; Wes, Oct 25 2026: "they have holes in them where you can see light
through them").

The image-to-3D shells are not watertight: a gap in one leaves the far wall's BACK faces facing the camera, culled, so the sky shows through the
rock. Two-sided (a per-instance base-property override, no new master) draws that inner wall instead: a dark rock interior, no light. Every
material instance under /Game/ImageTo3D/Cliffs (the formations' own and their Granite / Sandstone / Limestone / Basalt variants).

    python Tools/ue.py py --file Content/Python/trellis_cliffs_two_sided.py        (editor open, NOT in PIE: it saves)

Idempotent. TRELLIS2S lines.
"""
import unreal

ROOT = "/Game/ImageTo3D/Cliffs"
reg = unreal.AssetRegistryHelpers.get_asset_registry()
mel = unreal.MaterialEditingLibrary
flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "MaterialInstanceConstant")], package_paths=[ROOT], recursive_paths=True)
changed, checked = 0, 0
for ad in reg.get_assets(flt):
    mi = ad.get_asset()
    checked += 1
    o = mi.get_editor_property("base_property_overrides")
    if o.get_editor_property("override_two_sided") and o.get_editor_property("two_sided"):
        continue
    o.set_editor_property("override_two_sided", True)
    o.set_editor_property("two_sided", True)
    mi.set_editor_property("base_property_overrides", o)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, only_if_is_dirty=False)
    back = mi.get_editor_property("base_property_overrides")
    changed += 1
    out("TRELLIS2S %s: two-sided %s" % (ad.asset_name, back.get_editor_property("two_sided") and back.get_editor_property("override_two_sided")))
out("TRELLIS2S done: %d instance(s) under %s, %d made two-sided" % (checked, ROOT, changed))
