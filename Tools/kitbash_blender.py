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
UVs are untouched, so the source material instance still maps (the taper squeezes the tip's texels, which reads as carving).
Output lines start with KITBASH.
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

SLICES = 24


def log(msg):
    print("KITBASH " + msg, flush=True)


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for other in [o for o in bpy.data.objects if o.type != "MESH"]:
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


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    src, dst, op = args[0], args[1], args[2]
    obj = load(src)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
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
