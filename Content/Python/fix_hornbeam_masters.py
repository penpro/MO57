"""Make the migrated Electric Dreams hornbeam materials cookable (2026-10-12): split M_Foliage into a DefaultLit and a TwoSidedFoliage master.

Why (see fix_substrate_shading_model_pin.py for the engine mechanism): the foliage master's shading model comes from MaterialAttributes (a BreakMaterialAttributes feeds the Substrate legacy
conversion node's shading-model pin), chosen PER INSTANCE by the static switches 'DefaultLit Trunk' / 'TwoSided Leaves' -- so the generic repair (a constant node -> ShadingModelOverride)
cannot be applied. Instead the shading model becomes a property of the master and the instances are sorted by it:

    M_Foliage             override DefaultLit          <- MI_EuropeanHornbeam_01 -> Trunk_01 / Trunk_02 / Trunk_03 / Tileable      (DefaultLit Trunk = True)
    M_Foliage_TwoSided    override TwoSidedFoliage     <- MI_EuropeanHornbeam_01_TwoSided -> TwoSided / Decoration                (DefaultLit Trunk = False)

The attribute graph is untouched (it still computes a shading-model attribute; with the pin disconnected the node uses its override). Run in the editor, idempotent:

    python Tools/ue.py py --file Content/Python/fix_hornbeam_masters.py

After it: HORNBEAM_COOKS = True in biome_palettes.py, regenerate + apply the palettes, package. If a hornbeam part ever looks wrong (a translucent trunk, an opaque leaf), the instance is in the wrong
chain: move it with `mi.set_editor_property('parent', <the other base MI>)`.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MD = "/Game/MSPresets/MS_Foliage_Material_LATEST/MasterMaterials/"
HB = "/Game/Megascans/3D_Plants/EuropeanHornbeam/Materials/SimpleWind/"
PIN = "Single Shading Model"


def front_node(material):
    return MEL.get_material_property_input_node(material, unreal.MaterialProperty.MP_FRONT_MATERIAL)


# 1. M_Foliage becomes the DefaultLit master
lit = unreal.load_asset(MD + "M_Foliage")
node = front_node(lit)
node.set_editor_property("shading_model_override", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
if MEL.disconnect_material_expressions(node, PIN):
    unreal.log("HORNBEAM M_Foliage: pin '%s' disconnected, single shading model = DefaultLit" % PIN)
else:
    unreal.log("HORNBEAM M_Foliage: pin already disconnected")
EAL.save_loaded_asset(lit, False)

# 2. its TwoSidedFoliage twin (duplicated AFTER step 1: a duplicate of the unrepaired master compiles on creation and asserts)
twin_path = MD + "M_Foliage_TwoSided"
if not EAL.does_asset_exist(twin_path):
    EAL.duplicate_asset(MD + "M_Foliage", twin_path)
twin = unreal.load_asset(twin_path)
front_node(twin).set_editor_property("shading_model_override", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
EAL.save_loaded_asset(twin, False)

# 3. the base instance of the two-sided chain, and the instances that belong to it
base2_path = HB + "MI_EuropeanHornbeam_01_TwoSided"
if not EAL.does_asset_exist(base2_path):
    EAL.duplicate_asset(HB + "MI_EuropeanHornbeam_01", base2_path)
base2 = unreal.load_asset(base2_path)
base2.set_editor_property("parent", twin)
EAL.save_loaded_asset(base2, False)
for name in ("MI_EuropeanHornbeam_TwoSided", "MI_EuropeanHornbeam_Decoration"):
    mi = unreal.load_asset(HB + name)
    mi.set_editor_property("parent", base2)
    EAL.save_loaded_asset(mi, False)
    unreal.log("HORNBEAM %s -> parent %s" % (name, base2.get_name()))
unreal.log("HORNBEAM done: DefaultLit chain = MI_EuropeanHornbeam_01 (+ Trunk_01/02/03, Tileable), TwoSided chain = MI_EuropeanHornbeam_01_TwoSided (+ TwoSided, Decoration)")
