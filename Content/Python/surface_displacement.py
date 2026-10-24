"""The ground's Nanite displacement per surface (Wes, Oct 24 2026, BR24-17: "the nanite world position offset (or whatever the other displacement
version is) of the material is really high, which is great for the nordic strand rocks, but looks odd in some other ones").

M_QuixelBase displaces by its height map x HeightMultiplier (12 cm magnitude about 0.5); every voxel surface (VST_MI_<Name>) is an instance of it
with its own HeightMultiplier. Rock keeps its relief (the Nordic strand's DarkPebbles, cliff rock, the rocky grounds); soil, leaf litter, grass and
sand were lumpy at full height and get a third to two fifths of it. Idempotent: run in the editor with `python Tools/ue.py py --file
Content/Python/surface_displacement.py`; prints each surface's value before and after.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ROOT = "/Game/VoxelExamples/NaniteMaterials/Materials/%s/MI_%s.MI_%s"

HEIGHT = {
    # rock: the relief is the point
    "DarkPebbles": 1.0, "Cliff": 1.0, "IcelandicQuarryRock": 0.7, "MossyRockyGround": 0.7,
    # soil, litter, grass, sand: a gentle surface
    "MuddyLeaves": 0.35, "ForestLitter": 0.35, "JungleFloor": 0.35, "AlpineHeath": 0.4, "Gravel": 0.5,
    "MossyGrass": 0.3, "NordicMoss": 0.35, "DriedGrassOnDampSoil": 0.35, "Sand": 0.4,
}

for name, value in HEIGHT.items():
    mi = unreal.load_asset(ROOT % (name, name, name))
    if not mi:
        print("SURFDISP missing", name)
        continue
    before = MEL.get_material_instance_scalar_parameter_value(mi, "HeightMultiplier")
    if abs(before - value) > 1e-4:
        MEL.set_material_instance_scalar_parameter_value(mi, "HeightMultiplier", value)
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
    print("SURFDISP %-22s %.2f -> %.2f" % (name, before, MEL.get_material_instance_scalar_parameter_value(mi, "HeightMultiplier")))
