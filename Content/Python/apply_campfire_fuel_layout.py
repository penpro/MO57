"""Re-lay BP_Campfire_StoneRing's 20 fuel sticks in place from campfire_fuel_layout.py (no delete / rebuild of the Blueprint).

    python Tools/ue.py py --file Content/Python/apply_campfire_fuel_layout.py      (editor open, PIE NOT running)

Each FuelStick_XX component template gets the layout's location (relative to the Mesh component, which sits MESH_Z below the actor), rotation and
scale; the Blueprint is compiled and saved. Idempotent. Prints FUELLAYOUT lines.
"""
import importlib

import unreal
import campfire_fuel_layout

importlib.reload(campfire_fuel_layout)

BP = "/MOFramework/BP_Campfire_StoneRing"
MESH_Z = -6.0   # as make_stone_ring_campfire.py: the Mesh component sits 6 cm down; parts are placed relative to it

SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
bp = unreal.load_asset(BP)
templates = {}
for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
    o = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(SDS.k2_find_subobject_data_from_handle(h))
    if isinstance(o, unreal.StaticMeshComponent):
        templates[o.get_name().replace("_GEN_VARIABLE", "")] = o
done, missing = 0, []
for name, loc, rot, scale in campfire_fuel_layout.layout():
    comp = templates.get(name)
    if not comp:
        missing.append(name)
        continue
    comp.set_editor_property("relative_location", unreal.Vector(loc[0], loc[1], loc[2] - MESH_Z))
    comp.set_editor_property("relative_rotation", unreal.Rotator(roll=rot[2], pitch=rot[1], yaw=rot[0]))
    comp.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
    done += 1
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(BP, only_if_is_dirty=False)
out("FUELLAYOUT %d of 20 sticks re-laid%s" % (done, (", missing " + ", ".join(missing)) if missing else ""))
