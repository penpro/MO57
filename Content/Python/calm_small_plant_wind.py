"""Small plants get calm wind (playtest 2026-10-14 #19, Wes: "wind effect on these tiny sapling looking things is very spazzy/jittery, too fast").

Cause: the European hornbeam is the one SimpleWind species. Its 1 m seedlings, 2 m saplings and 16 m forest trees share ONE pair of material instances, tuned for
the tree: leaf noise 1.0, gust 0.65, gust noise 2.0. Those are world-space amplitudes, the same few centimetres of flutter on a 16 m crown and on a hand-sized
leaf of a knee-high seedling, so the seedling shook like a tree in a storm. (The PivotPainter species, black alder and hazel, sway hierarchically by branch size
and do not have the problem.)

Fix: child instances of the tree's two instances with the wind scaled down, assigned to the small meshes only. Seedlings get SEEDLING_SCALE of the tree's
wind, saplings SAPLING_SCALE. The tree instances are untouched. Re-runnable: the child instances are created once and updated in place.

Run: python Tools/ue.py py --file Content/Python/calm_small_plant_wind.py
"""
import unreal

MEL = unreal.MaterialEditingLibrary
FOLDER = "/Game/Megascans/3D_Plants/EuropeanHornbeam/Materials/SimpleWind"
GEO = "/Game/Megascans/3D_Plants/EuropeanHornbeam/Geometry/SimpleWind"
PARENTS = {"TwoSided": FOLDER + "/MI_EuropeanHornbeam_TwoSided", "Tileable": FOLDER + "/MI_EuropeanHornbeam_Tileable"}
WIND = ("Wind Noise Strength", "Wind Gust Strength", "Wind Gust Noise Strength")
SEEDLING_SCALE = 0.3
SAPLING_SCALE = 0.6
GROUPS = {
    "Seedling": (SEEDLING_SCALE, ["SM_EuropeanHornbeam_Seedling_%02d" % i for i in range(1, 5)]),
    "Sapling": (SAPLING_SCALE, ["SM_EuropeanHornbeam_Sapling_%02d" % i for i in range(1, 9)]),
}


def child_instance(slot, group, scale):
    parent = unreal.load_asset(PARENTS[slot])
    name = "MI_EuropeanHornbeam_%s_%s" % (slot, group)
    path = FOLDER + "/" + name
    mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mi is None:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for p in WIND:
        base = MEL.get_material_instance_scalar_parameter_value(parent, p)
        MEL.set_material_instance_scalar_parameter_value(mi, p, base * scale)
    MEL.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    print("WIND %s: %s" % (name, ", ".join("%s %.2f" % (p, MEL.get_material_instance_scalar_parameter_value(mi, p)) for p in WIND)))
    return mi


def main():
    for group, (scale, meshes) in GROUPS.items():
        mis = {slot: child_instance(slot, group, scale) for slot in PARENTS}
        for m in meshes:
            path = GEO + "/" + m
            if not unreal.EditorAssetLibrary.does_asset_exist(path):
                continue
            sm = unreal.load_asset(path)
            mats = sm.get_editor_property("static_materials")
            changed = 0
            for i, sl in enumerate(mats):
                mi = sl.material_interface
                for slot, parent_path in PARENTS.items():
                    if mi and mi.get_path_name().split(".")[0] in (parent_path, mis[slot].get_path_name().split(".")[0]):
                        if mi != mis[slot]:
                            sm.set_material(i, mis[slot])
                            changed += 1
            if changed:
                unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
            print("WIND %s -> %s instances (%d slots changed)" % (m, group, changed))


main()
