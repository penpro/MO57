"""The survivor Blueprint's body and movement (Wes, Oct 28 2026). Re-runnable; prints SETUP_BODY lines; compiles and saves.

- The capsule fits the bases (Wes: "fit the capsule to somewhere in that range" -- Harvey 194 cm, Trey 185, Zuri 180, Jalena 168): 185 cm
  tall (half height 92.5), the mesh lowered with it so the feet keep their place on the capsule's floor. The capsule and the body at scale
  1: the Blueprint carried a root scale of 0.9425 and a mesh scale of 1.05 from its early setup (a 174 cm capsule, the body at 99 %), which
  the look's height hid until Oct 28 by overwriting the root's scale in game; a base's body is its real height now.
- The movement matches the animation set (Wes: "match the animation project settings"; GASP's SandboxCharacter_CMC): walk / jog / sprint
  200 / 500 / 700, crouched 225, jump 500, air control 0.25, no air braking, ground friction 5 (the movement component does the gait's
  acceleration, braking, friction and turn rate: Docs/Animation_GASP.md).
- Her body's sex (Wes: "add it"): Female for the Blueprint's own body (MH_F_1); a base names its own (Config/DefaultGame.ini).

    python Tools/ue.py py --file Content/Python/setup_survivor_body.py
"""
import unreal

SURVIVOR = "/MOFramework/Characters/BP_MOMetaHuman_Female"
HALF_HEIGHT = 92.5   # 185 cm


def say(ok, msg):
    try:
        out("SETUP_BODY " + ("ok " if ok else "FAIL ") + msg)
    except NameError:
        unreal.log_warning("SETUP_BODY " + msg)


def main():
    bp = unreal.load_asset(SURVIVOR)
    cdo = unreal.get_default_object(bp.generated_class())
    capsule = cdo.get_editor_property("capsule_component")
    mesh = cdo.get_editor_property("mesh")
    old_half = capsule.get_editor_property("capsule_half_height")
    rel = mesh.get_editor_property("relative_location")
    old_x, old_y, old_z = float(rel.x), float(rel.y), float(rel.z)   # copies: rel is the component's own struct
    # The feet stay where they were on the capsule's floor: the mesh moves down by what the half height grows.
    new_z = old_z - (HALF_HEIGHT - old_half)
    capsule.set_editor_property("capsule_half_height", HALF_HEIGHT)
    mesh.set_editor_property("relative_location", unreal.Vector(old_x, old_y, new_z))
    say(True, "capsule half height %.2f -> %.2f; mesh z %.2f -> %.2f" % (old_half, HALF_HEIGHT, old_z, new_z))
    # Read the numbers out first: get_editor_property hands back the component's own struct, which the set below changes.
    old_scales = (float(capsule.get_editor_property("relative_scale3d").z), float(mesh.get_editor_property("relative_scale3d").z))
    capsule.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    mesh.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    say(True, "capsule scale %.4f -> 1, mesh scale %.4f -> 1" % old_scales)

    for prop, value in (("walk_speed", 200.0), ("jog_speed", 500.0), ("sprint_speed", 700.0), ("crouched_walk_speed", 225.0)):
        cdo.set_editor_property(prop, value)
    cm = cdo.get_editor_property("character_movement")
    for prop, value in (("max_walk_speed", 200.0), ("max_walk_speed_crouched", 225.0), ("jump_z_velocity", 500.0), ("air_control", 0.25),
                        ("braking_deceleration_falling", 0.0), ("ground_friction", 5.0)):
        cm.set_editor_property(prop, value)
    cdo.set_editor_property("default_body_sex", unreal.MOBodySex.FEMALE)
    say(True, "speeds %s / %s / %s, crouched %s; jump %s, air control %s; sex %s" % (
        cdo.get_editor_property("walk_speed"), cdo.get_editor_property("jog_speed"), cdo.get_editor_property("sprint_speed"),
        cdo.get_editor_property("crouched_walk_speed"), cm.get_editor_property("jump_z_velocity"), cm.get_editor_property("air_control"),
        cdo.get_editor_property("default_body_sex")))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    say(unreal.EditorAssetLibrary.save_loaded_asset(bp, False), "saved " + SURVIVOR)


main()
