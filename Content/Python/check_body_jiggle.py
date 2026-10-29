"""Does a base body jiggle? (Oct 28 2026; ensure_body_jiggle.py puts ABP_MO_Body_PP on the bases.) A mechanical check, not a look: in a new
blank map (never saved), two actors carry BODY (default Jalena's body mesh): one as it is (its post-process: the Creator's correctives, then the
springs) and a CONTROL with its post-process blueprint off. Both rise 2 cm a frame for 10 frames and stop; every frame the chest and lower-glute
helper joints (the springs') and a collarbone (no spring) are read in component space against their rest offset. A spring lags the move and
rings after it; a bone without one, or the control's, does not move against the body.

    python Tools/ue.py py --file Content/Python/check_body_jiggle.py           (starts it on the editor's tick and returns)
    python Tools/ue.py py --file Content/Python/check_body_jiggle_result.py    (a few seconds later: the "JIGGLE CHECK" lines, the level back)

Measured Oct 28 (Jalena): the four springs 5.16 cm at most (all four alike: the springs' displacement limit), the collarbone 0.00, the
control 0.00.
"""
import json
import os

import unreal

BODY = globals().get("BODY", "/Game/MetaHumans/MH_Jalena_Base/Body/SKM_MH_Jalena_Base_BodyMesh")
SPRUNG = ["clavicle_pec_l", "clavicle_pec_r", "thigh_bck_lwr_l", "thigh_bck_lwr_r"]
STILL = ["clavicle_l"]
RESULT = os.path.join(unreal.Paths.project_saved_dir(), "body_jiggle_check.json")
SETTLE, RISE, AFTER = 40, 10, 50

back_to = unreal.EditorLevelLibrary.get_editor_world().get_path_name().split(".")[0]
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mesh = unreal.load_asset(BODY)
cases = {}
for i, name in enumerate(["jiggle", "control"]):
    actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0.0, 200.0 * i, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    comp = actor.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(mesh)
    comp.set_update_animation_in_editor(True)
    if name == "control":
        comp.set_editor_property("disable_post_process_blueprint", True)
    cases[name] = (actor, comp)
state = {"frame": 0, "rest": {}, "dev": {n: {b: 0.0 for b in SPRUNG + STILL} for n in cases}, "post": {}}


def offsets(comp):
    return {b: comp.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT).translation for b in SPRUNG + STILL}


def tick(_dt):
    f = state["frame"]
    state["frame"] += 1
    if f == SETTLE:
        for n, (actor, comp) in cases.items():
            state["rest"][n] = offsets(comp)
            state["post"][n] = str(comp.get_post_process_instance().get_class().get_name()) if comp.get_post_process_instance() else "none"
    elif SETTLE < f <= SETTLE + RISE + AFTER:
        for n, (actor, comp) in cases.items():
            if f <= SETTLE + RISE:
                actor.set_actor_location(actor.get_actor_location() + unreal.Vector(0.0, 0.0, 2.0), False, False)
            now = offsets(comp)
            for b in SPRUNG + STILL:
                d = (now[b] - state["rest"][n][b]).length()
                state["dev"][n][b] = max(state["dev"][n][b], d)
    elif f > SETTLE + RISE + AFTER:
        unreal.unregister_slate_post_tick_callback(state["handle"])
        json.dump({"body": BODY, "post_process": state["post"], "max_cm": state["dev"], "frames": f, "back_to": back_to}, open(RESULT, "w"), indent=1)


state["handle"] = unreal.register_slate_post_tick_callback(tick)
out("JIGGLE CHECK started on %s (result in %s)" % (mesh.get_name() if mesh else "NO MESH", RESULT))
