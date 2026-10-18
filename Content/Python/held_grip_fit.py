"""Fit a held tool to the right hand from the hand bones, instead of nudging RightHandTransform numbers by eye.

    (PIE running, the tool equipped in the right hand)
    python Tools/ue.py py -c "import importlib, held_grip_fit as h; importlib.reload(h); h.fit(world, out, -19.0, 0.0, 5.6)"

grip_x/y/z: where the hand closes on the handle, in the MESH's own cm (measure the handle centreline near its end, e.g. in Blender).
The handle is laid across the palm (pinky -> index side) with the head toward the index side and the mesh's up back up the arm,
then the result is made relative to the hand_r socket (what AMOCharacter::UpdateHeldItemMesh applies), so it follows every
animation. Applies it live to RightHandMesh to LOOK at, and prints the heldVisual patch for `ue.py rows set` (end PIE first:
saves are refused in PIE). Built 2026-10-17 for StoneMaul01; the torch's transform held the maul up like a torch.
"""
import unreal


def fit(world, out, grip_x, grip_y, grip_z, socket="hand_r"):
    p = unreal.GameplayStatics.get_player_pawn(world, 0)
    sk = p.get_editor_property("mesh")
    loc_of = sk.get_socket_location
    hand, mid, idx, pinky = loc_of(socket), loc_of("middle_01_r"), loc_of("index_01_r"), loc_of("pinky_01_r")
    fingers = (mid - hand).normal()
    across = idx - pinky
    across = (across - fingers * across.dot(fingers)).normal()
    palm = hand + (mid - hand) * 0.55
    rot = unreal.MathLibrary.make_rot_from_xz(across, fingers * -1.0)
    loc = palm - rot.quaternion().rotate_vector(unreal.Vector(grip_x, grip_y, grip_z))
    desired = unreal.Transform(loc, rot, unreal.Vector(1, 1, 1))
    rel = unreal.MathLibrary.make_relative_transform(desired, sk.get_socket_transform(socket, unreal.RelativeTransformSpace.RTS_WORLD))
    applied = False
    for c in p.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() == "RightHandMesh":
            c.set_relative_transform(rel, False, False)
            applied = True
    q, t = rel.rotation, rel.translation
    out('HELDGRIP applied=%s {"heldVisual": {"RightHandTransform": {"Rotation": {"X": %.6f, "Y": %.6f, "Z": %.6f, "W": %.6f}, '
        '"Translation": {"X": %.2f, "Y": %.2f, "Z": %.2f}, "Scale3D": {"X": 1, "Y": 1, "Z": 1}}}}' % (applied, q.x, q.y, q.z, q.w, t.x, t.y, t.z))
    return rel
