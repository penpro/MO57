"""Where a held bow sits in the survivor's left hand (Nov 2 2026), from her full draw: Sparrow's RMB_Loop on the MetaHuman.

    python Tools/ue.py py -c "import bow_hand_fit as f; out(f.fit('/Game/BlenderItems/SimpleBow01/SK_SimpleBow01', '<draw clip>'))"

The bow (Tools/blender_items_blender.py) has its grip centre at the root, its draw along the clip's nocking-point travel, its upper limb up (+Z)
and its nocking point NOCK_Z above the grip. In her full draw:
  G  the grip: the centre of the curled fingers of the left hand (the knuckle and middle joints of the four fingers)
  F  the string: between the drawing hand's index and middle fingers (index_02_r / middle_02_r: where UMOBowMeshComponent holds it)
  U  the bow's up: the fist's axis from the little finger to the index finger, square to the draw
The bow's root goes at F - FULL * D - NOCK_Z * U with D = (F - G) / |F - G|, so at full draw its nocking point is exactly at F and its grip in
the fist. Returns the transform relative to hand_l (the socket the bow hangs from) and the numbers it came from.
"""
import json

import unreal

CLIP = "/MOFramework/Characters/Bow/RMB_Loop_MH"


def _pose(anim, t):
    return unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, unreal.AnimPoseEvaluationOptions())


def _bone(pose, name):
    return unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)


def fit(bow_mesh_path, draw_path, t=0.33):
    body = unreal.load_asset(CLIP)
    pose = _pose(body, t)
    joints = ["%s_%s_l" % (f, j) for f in ("index", "middle", "ring", "pinky") for j in ("01", "02")]
    G = unreal.Vector(0, 0, 0)
    for j in joints:
        G += _bone(pose, j).translation
    G /= float(len(joints))
    F = (_bone(pose, "index_02_r").translation + _bone(pose, "middle_02_r").translation) * 0.5
    D = (F - G).normal()
    up0 = _bone(pose, "index_01_l").translation - _bone(pose, "pinky_01_l").translation
    U = (up0 - D * up0.dot(D)).normal()

    # the bow's own frame, from its draw clip: nocking point at the first / last frame, brace in the reference pose
    draw = unreal.load_asset(draw_path)
    L = draw.get_editor_property("sequence_length")
    first = _bone(_pose(draw, 0.0), "string_nock").translation
    last = _bone(_pose(draw, L), "string_nock").translation
    Db = (last - first).normal()
    full = last.dot(Db)
    nock_up = last.z            # the bow's up is +Z; the nocking point stands this high over the grip
    R = F - D * full - U * nock_up

    # rotation taking the bow's axes (Db, +Z) onto the body's (D, U): build it from the body side, X = Y x Z
    Ub = unreal.Vector(0, 0, 1)
    Xb = Db.cross(Ub)          # the bow's third axis in its own space
    Xw = D.cross(U)
    # bow local (Xb, Db, Ub) -> world (Xw, D, U); Db is +Y here (the importer keeps Blender's -Y draw as +Y), so the bow's X maps to Xw
    rot = unreal.MathLibrary.make_rot_from_yz(D, U) if abs(Db.y) > 0.99 else unreal.MathLibrary.make_rot_from_xz(Xw, U)
    if Db.y < 0:
        rot = unreal.MathLibrary.make_rot_from_yz(D * -1.0, U)
    bow_world = unreal.Transform(R, rot, unreal.Vector(1, 1, 1))
    hand = _bone(pose, "hand_l")
    rel = bow_world.make_relative(hand)

    # check: the bow's nocking point at full draw, put back through the hand, against F
    check = rel.multiply(hand).transform_location(last)
    out = {"grip_G": [round(G.x, 2), round(G.y, 2), round(G.z, 2)], "string_F": [round(F.x, 2), round(F.y, 2), round(F.z, 2)],
           "G_to_F_cm": round((F - G).length(), 2), "bow_full_cm": round(full, 2), "nock_up_cm": round(nock_up, 2),
           "bow_axis_draw": [round(Db.x, 3), round(Db.y, 3), round(Db.z, 3)], "xb": [round(Xb.x, 3), round(Xb.y, 3), round(Xb.z, 3)],
           "full_draw_nock_error_cm": round((check - F).length(), 3),
           "relative_to_hand_l": {"loc": [round(rel.translation.x, 3), round(rel.translation.y, 3), round(rel.translation.z, 3)],
                                  "rot": [round(rel.rotation.x, 6), round(rel.rotation.y, 6), round(rel.rotation.z, 6), round(rel.rotation.w, 6)],
                                  "rotator": [round(rel.rotation.rotator().roll, 2), round(rel.rotation.rotator().pitch, 2),
                                              round(rel.rotation.rotator().yaw, 2)]}}
    return json.dumps(out)
