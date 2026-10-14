"""Kitbash, Blender step (headless): small geometry edits on an exported item mesh, so one scan can serve a related item.

    blender -b --factory-startup --python Tools/kitbash_blender.py -- <in.fbx> <out.fbx> <op> [args...]
    (Tools/kitbash.py runs this between the editor export and re-import; see there for the whole loop)

Ops, all along the mesh's longest axis (the scans here are sticks, branches and antlers):
  taper <fraction> [end]   sharpen one end to a point: over the last <fraction> of the length the cross-section shrinks linearly
                           to nothing around the local centre line (per-slice centroid, so a bent stick stays bent).
                           end = max|min, the end to sharpen (default max).
  cut <fraction> [end]     keep only the last <fraction> of the length at that end (an antler tine from a whole antler) and
                           cap the cut with a fill. The cut is square to the global long axis, so a curved piece gets an oval
                           cap; the fill has no UVs of its own (one texel colour). Works best on straight-ish pieces.
  scale <x,y,z>            bake a non-uniform scale into the mesh (so it can serve as an attach head)
  attach <head.fbx> <head cm> <tip|axe|adze|stand|ring> <embed> <base scale x,y,z>
                           a second mesh (a flint point, a stone axe head) fitted to the shaft's + end, then joined: see
                           attach(). The base scale is baked first (the joined mesh keeps a uniform scale in the item row,
                           which a non-uniform row scale would squash the head with). Materials: base slots, then head slots.
UVs are untouched, so the source material instance still maps (the taper squeezes the tip's texels, which reads as carving).
Output lines start with KITBASH.
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

SLICES = 24
CROSS_LIFT = 0.3   # axe / adze heads: centre this fraction of the head length off the haft, toward the edge


def log(msg):
    print("KITBASH " + msg, flush=True)


def load(path, reset=True):
    """Import an FBX and return its meshes joined into one object (reset=False adds to the current scene)."""
    if reset:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    for other in [o for o in bpy.data.objects if o not in before and o.type != "MESH"]:
        bpy.data.objects.remove(other, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    return bpy.context.view_layer.objects.active


def long_axis(co):
    extent = co.max(0) - co.min(0)
    return int(np.argmax(extent))


def taper(obj, fraction, end):
    me = obj.data
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    axis = long_axis(co)
    others = [a for a in range(3) if a != axis]
    lo, hi = co[:, axis].min(), co[:, axis].max()
    length = hi - lo
    t = (co[:, axis] - lo) / length if end == "max" else (hi - co[:, axis]) / length   # 0 at the far end .. 1 at the tip end
    start = 1.0 - fraction
    # centre line: centroid of each slice along the axis, interpolated
    edges = np.linspace(0.0, 1.0, SLICES + 1)
    mids, centres = [], []
    for i in range(SLICES):
        sel = (t >= edges[i]) & (t <= edges[i + 1])
        if sel.any():
            mids.append((edges[i] + edges[i + 1]) / 2.0)
            centres.append(co[sel][:, others].mean(0))
    mids, centres = np.array(mids), np.array(centres)
    centre = np.column_stack([np.interp(t, mids, centres[:, k]) for k in range(2)])
    k = np.clip((t - start) / fraction, 0.0, 1.0)        # 0 before the taper .. 1 at the very tip
    shrink = 1.0 - k
    out = co.copy()
    for j, a in enumerate(others):
        out[:, a] = centre[:, j] + (co[:, a] - centre[:, j]) * shrink
    me.vertices.foreach_set("co", out.ravel())
    me.update()
    log("taper: axis %d, last %.0f%% at the %s end, %d of %d vertices moved" % (axis, fraction * 100, end, int((k > 0).sum()), len(co)))


def cut(obj, fraction, end):
    me = obj.data
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    axis = long_axis(co)
    lo, hi = co[:, axis].min(), co[:, axis].max()
    length = hi - lo
    plane = hi - fraction * length if end == "max" else lo + fraction * length
    # Square to the global axis on purpose: a plane square to the local direction (slab PCA) was tried on the deer antler and
    # went wrong wherever the slab caught a fork; an axis-aligned cut is predictable. Expect an oval cap on curved pieces.
    point = Vector([plane if a == axis else 0.0 for a in range(3)])
    normal = Vector((0.0, 0.0, 0.0))
    normal[axis] = 1.0 if end == "max" else -1.0
    bm = bmesh.new()
    bm.from_mesh(me)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    # clear_inner drops the side the normal points away from: the kept side is the one at <end>
    result = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=point, plane_no=normal, clear_inner=True)
    cut_edges = [e for e in result["geom_cut"] if isinstance(e, bmesh.types.BMEdge)]
    bmesh.ops.holes_fill(bm, edges=cut_edges, sides=0)
    bm.to_mesh(me)
    bm.free()
    me.update()
    log("cut: axis %d, kept the last %.0f%% at the %s end, %d vertices left" % (axis, fraction * 100, end, len(me.vertices)))


def vertices(obj):
    co = np.empty(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def principal_axes(co):
    """Unit axes (major, middle, minor) of a point cloud, as a right-handed 3x3 column basis."""
    evals, evecs = np.linalg.eigh(np.cov((co - co.mean(0)).T))
    basis = evecs[:, np.argsort(evals)[::-1]].copy()
    if np.linalg.det(basis) < 0:
        basis[:, 1] *= -1.0
    return basis


def end_spread(co, centre, axis_vec, minor_vec):
    """Thickness (spread along minor_vec) of the last 20% at each end along axis_vec: (at the + end, at the - end)."""
    t = (co - centre) @ axis_vec
    lo, hi = t.min(), t.max()
    thickness = np.abs((co - centre) @ minor_vec)
    plus = thickness[t > hi - 0.2 * (hi - lo)].mean()
    minus = thickness[t < lo + 0.2 * (hi - lo)].mean()
    return plus, minus


def attach(base, head, head_len, mode, embed):
    """Fit <head> onto the + end of <base>'s long axis and join them (base materials first, then the head's).
    mode tip:  the head continues the shaft, its thinner end outward, its flat facing up (spear point, arrowhead);
         axe:  the head crosses the shaft end: long axis up, width along the shaft, thinner end (the edge) up;
         adze: as axe but the width across the shaft (edge square to the haft);
         stand: the head upright on the base's top surface, <embed> of the base length in from its + end, its foot sunk a
                little into the base (a hand-drill spindle in its fireboard notch);
         ring: the head's thinnest axis along the shaft, centred on it <embed> of the length in (a spindle whorl)."""
    co = vertices(base)
    axis = long_axis(co)
    hi = co[:, axis].max()
    length = hi - co[:, axis].min()
    tip_slab = co[co[:, axis] > hi - 0.03 * length]
    anchor = tip_slab.mean(0)
    anchor[axis] = hi
    shaft = np.zeros(3)
    shaft[axis] = 1.0
    up = np.array([0.0, 0.0, 1.0]) if axis != 2 else np.array([0.0, 1.0, 0.0])
    across = np.cross(up, shaft)

    hco = vertices(head)
    centre = hco.mean(0)
    src = principal_axes(hco)                         # columns: major, middle, minor
    major, minor = src[:, 0], src[:, 2]
    plus, minus = end_spread(hco, centre, major, minor)
    if plus > minus:                                  # make the thinner end the + end of major
        src[:, 0] *= -1.0
        src[:, 1] *= -1.0                             # keep it right-handed
    extent = (hco - centre) @ src[:, 0]
    scale = head_len / (extent.max() - extent.min())
    if mode == "tip":
        target = np.column_stack([shaft, across, up])
    elif mode == "axe":
        target = np.column_stack([up, shaft, np.cross(up, shaft)])
    elif mode == "adze":
        target = np.column_stack([up, across, np.cross(up, across)])
    elif mode == "stand":
        target = np.column_stack([up, shaft, np.cross(up, shaft)])
    elif mode == "ring":
        target = np.column_stack([across, up, shaft])     # thinnest axis along the shaft: a disc threaded on it
    else:
        raise SystemExit("attach mode must be tip, axe, adze, stand or ring")
    rot = target @ src.T
    posed = ((hco - centre) @ rot.T) * scale
    if mode == "tip":
        along = posed @ shaft
        offset = anchor + shaft * (-along.min() - embed * head_len)   # the back embed fraction of the head sits in the shaft
    elif mode == "ring":
        offset = anchor - shaft * (embed * length)        # centred on the shaft, <embed> of its length in from the + end
    elif mode == "stand":
        # standing on the base's top surface at <embed> of its length in from the + end (a drill spindle in a fireboard notch)
        v = int(np.argmax(np.abs(up)))                 # the vertical axis
        top = co[:, v].max()
        sink = 0.3 * (top - anchor[v])                 # foot sunk 30% of the base's half-thickness (into the notch)
        offset = anchor - shaft * (embed * length)
        offset[v] = top - sink - posed[:, v].min()
    else:
        # embed of the shaft length in from its end; the head lifted toward its edge so the blade stands out on one side of the
        # haft and only a short butt shows on the other (centred, it read as a disc skewered on a stick)
        offset = anchor - shaft * (embed * length) + up * (CROSS_LIFT * head_len)
    head.data.vertices.foreach_set("co", (posed + offset).ravel())
    head.data.update()
    bpy.ops.object.select_all(action="DESELECT")
    head.select_set(True)
    base.select_set(True)
    bpy.context.view_layer.objects.active = base
    bpy.ops.object.join()
    log("attach %s: head %.1f cm (x%.3f), embed %.2f, shaft axis %d" % (mode, head_len * 100, scale, embed, axis))
    return bpy.context.view_layer.objects.active


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    src, dst, op = args[0], args[1], args[2]
    obj = load(src)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    if op == "attach":
        # attach <head.fbx> <head length cm> <tip|axe|adze> <embed> <base scale x,y,z>
        head_path, head_len, mode, embed = args[3], float(args[4]) / 100.0, args[5], float(args[6])
        base_scale = [float(v) for v in args[7].split(",")]
        obj.scale = base_scale                         # bake the shaft's proportions; the item row keeps a uniform scale
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        head = load(head_path, reset=False)
        bpy.ops.object.select_all(action="DESELECT")
        head.select_set(True)
        bpy.context.view_layer.objects.active = head
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        obj = attach(obj, head, head_len, mode, embed)
    elif op == "scale":
        # scale <x,y,z>: bake a non-uniform scale, so the result can be an attach head (a cup flattened into a whorl)
        obj.scale = [float(v) for v in args[3].split(",")]
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        log("scale: baked %s" % args[3])
    else:
        fraction = float(args[3])
        end = args[4] if len(args) > 4 else "max"
        {"taper": taper, "cut": cut}[op](obj, fraction, end)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.export_scene.fbx(filepath=dst, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE",
                             apply_scale_options="FBX_SCALE_NONE", bake_space_transform=False, add_leaf_bones=False)
    dims = obj.dimensions
    log("OK %s -> %s (%.3f x %.3f x %.3f blender units)" % (op, dst, dims[0], dims[1], dims[2]))


if __name__ == "__main__":
    main()
