"""Build BP_Campfire_StoneRing (Wes, Oct 24 2026): a campfire from loose existing assets, so each part can change on its own.

    python Tools/ue.py py --file Content/Python/make_stone_ring_campfire.py      (editor open, PIE NOT running)

"make a different fireplace asset that has a ring of stones from our existing assets that are not linked and some fresh looking sticks and
some burnt sticks and the fire effect from the other camp fire asset ... while it's building it will switch each stone to it's proper texture
as the building timer fills up ... when you add wood it shows various levels of "full" by making the firewood assets visible (so like barely
full is one stick visible, compeltely full is 20 sticks visible) and after you burnt it it turns to charred looking sticks".

- A copy of BP_Campfire (its fire NFire_03, light, sounds, station settings), its merged Megascans campfire mesh cleared.
- Stone_01..10: the RiverCobble01 mesh (the recipe deposits 10 river cobbles) in a ring, numbered round it: AMOBuildableActor::StagedBuildParts,
  each in the planned (ghost) look until the build reaches it (UnbuiltStagedPartStyle Ghost).
- FuelStick_01..20: the Firewood01 mesh, a log-cabin base then a teepee (campfire_fuel_layout.py): AMOCraftingStationActor::FuelStickParts (shown by fuel, charred once
  burnt: CharredFuelMaterial = MI_Firewood_Charred).
- No other sticks (Wes, Oct 24, after seeing it: "fireplace sticks visible before firewood being added, some sticks are also on outside of
  fireplace"): the fresh and burnt sticks he asked for ARE the fuel sticks -- fresh when laid, charred once burnt. The ring is empty until
  wood goes in.
- The stones lie every which way (Wes: "add some random rotation to the stone so they don't look so uniform").
Every part hangs under the native Mesh component, so the building's collision profile (and the interaction trace) reaches it.
Refuses if BP_Campfire_StoneRing exists (delete it on disk with the editor closed to rebuild). Prints STONERING lines.
"""
import math
import random

import unreal
import campfire_fuel_layout
import item_tint

EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SRC = "/MOFramework/BP_Campfire"
DST = "/MOFramework/BP_Campfire_StoneRing"
MAT_DIR = "/MOFramework/Buildings/Campfire"
COBBLE = "/MOFramework/Items/RiverCobble01/Medium/wd5mbhd_tier_2/StaticMeshes/wd5mbhd_tier_2"
FIREWOOD = "/Game/Fab/Megascans/3D/Firewood_tiukbj3qx/Medium/tiukbj3qx_tier_2/StaticMeshes/tiukbj3qx_tier_2"
MESH_Z = -6.0          # BP_Campfire's Mesh component sits 6 cm down; parts are placed relative to it
RING_RADIUS = 38.0     # cobble centres; ten 24 cm cobbles close the ring

lines = []
rng = random.Random(57)


def log(msg):
    lines.append(msg)


# --- charred materials (near-black albedo, dull) ---------------------------------------------------------------------------------------
charred_firewood = item_tint.make(MAT_DIR + "/MI_Firewood_Charred", "/Game/Fab/Megascans/3D/Firewood_tiukbj3qx/Medium/tiukbj3qx_tier_2/Materials/MI_tiukbj3qx",
                                  tint=(0.07, 0.06, 0.055), Brightness=0.35, Saturation=0.1, Min_Roughness=0.6)
log("material %s" % charred_firewood)

# --- the blueprint ----------------------------------------------------------------------------------------------------------------------
if EAL.does_asset_exist(DST):
    # Deleting assets from editor Python hangs the editor (CLAUDE.md): remove BP_Campfire_StoneRing.uasset on disk with the editor closed, then rerun.
    raise RuntimeError("%s exists: delete it on disk with the editor closed, then run this again" % DST)
bp = EAL.duplicate_asset(SRC, DST)
handles = SDS.k2_gather_subobject_data_for_blueprint(bp)


def obj_of(h):
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(SDS.k2_find_subobject_data_from_handle(h))


mesh_handle = next(h for h in handles if obj_of(h) and obj_of(h).get_name() == "Mesh")
obj_of(mesh_handle).set_editor_property("static_mesh", None)


def add_part(name, mesh_path, loc, rot, scale, material=None):
    params = unreal.AddNewSubobjectParams(parent_handle=mesh_handle, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle):
        raise RuntimeError("could not add %s: %s" % (name, fail))
    SDS.rename_subobject(handle, unreal.Text(name))
    comp = obj_of(handle)
    comp.set_editor_property("static_mesh", unreal.load_asset(mesh_path))
    comp.set_editor_property("relative_location", unreal.Vector(loc[0], loc[1], loc[2] - MESH_Z))
    comp.set_editor_property("relative_rotation", unreal.Rotator(roll=rot[2], pitch=rot[1], yaw=rot[0]))
    comp.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
    if material:
        comp.set_editor_property("override_materials", [unreal.load_asset(material.split(".")[0])])
    return comp


# Stones: round the ring, numbered in build order.
stones = []
for k in range(10):
    a = 2 * math.pi * k / 10 + rng.uniform(-0.1, 0.1)
    r = RING_RADIUS + rng.uniform(-3.5, 3.5)
    s = rng.uniform(0.85, 1.25)
    name = "Stone_%02d" % (k + 1)
    # Any yaw, a real tilt either way, sunk by a random few cm: gathered cobbles dropped round a fire, not a laid kerb.
    add_part(name, COBBLE, (r * math.cos(a), r * math.sin(a), rng.uniform(-4.5, -1.0)), (rng.uniform(0, 360), rng.uniform(-18, 18), rng.uniform(-18, 18)), s)
    stones.append(name)

# Fuel: a log-cabin base of four, then a teepee of sixteen (campfire_fuel_layout.py: lower and less perfect since Oct 24 2026, BR25-8).
fuel = []
for name, loc, rot, scale in campfire_fuel_layout.layout():
    add_part(name, FIREWOOD, loc, rot, scale)
    fuel.append(name)


# --- class defaults ---------------------------------------------------------------------------------------------------------------------
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("staged_build_parts", [unreal.Name(n) for n in stones])
cdo.set_editor_property("unbuilt_staged_part_style", unreal.MOStagedPartStyle.GHOST)
cdo.set_editor_property("fuel_stick_parts", [unreal.Name(n) for n in fuel])
cdo.set_editor_property("charred_fuel_material", unreal.load_asset(charred_firewood.split(".")[0]))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
EAL.save_asset(DST, only_if_is_dirty=False)

check = unreal.get_default_object(unreal.load_asset(DST).generated_class())
log("saved %s: staged %d, fuel %d, charred %s" % (DST, len(check.get_editor_property("staged_build_parts")), len(check.get_editor_property("fuel_stick_parts")),
                                                  check.get_editor_property("charred_fuel_material")))
out("STONERING " + "\nSTONERING ".join(lines))
