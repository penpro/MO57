"""Close an open rock scan so it sits in the ground and cannot be seen through (Wes, Nov 6: "put them all into blender and curve the edges down so they sit better and close them off so worst case they're not see through from one side").

    blender -b --factory-startup --python Tools/close_rocks_blender.py -- <in.fbx> <out.fbx> [--up 1|0] [--drop-share 0.12] [--rim-share 0.18] [--back-share 0.2] [--back-relief 0.5]
    (Tools/close_rocks.py runs this between the editor's export and re-import.)

A scan is a one-sided skin: a sheet with a rim. What this does to it, keeping every vertex's UVs and material, so it still looks like the same rock from its open side:
1. Weld (the export splits vertices at UV seams; welded, the only boundary edges left are the real holes). Holes much smaller than the mesh are filled where they are.
2. Which way it is open: the area-weighted normal of its faces points out of its visible side, so the open side is against that (straight down when the sheet mostly faces up: --up 1 forces it).
3. Curve the rim: everything within --rim-share of the mesh's narrower width of a boundary (walked along the surface) sinks along the open side, by up to --drop-share of that width, quadratically from nothing to all of it at the
   rim itself. So the middle stays where it was, and the rim goes into the ground instead of standing on it.
4. Close: a back surface, a copy of the skin with the same UVs and faces turned round, joined to the skin along every boundary edge. It lies past the lowest point of the skin by --back-relief of each vertex's height above that point,
   at most --back-share of the narrower width (and --back-max-cm; the rim's sink and width have caps too): a shallow plate becomes a lens, a tall boulder gets a low flat-ish base, a backless face gets a rock-textured back rather than a hole. Only pieces that have a boundary get one; closed pieces in the same mesh are left alone.
5. The back is thinned to --back-tris (nobody sees it; the skin and walls keep every triangle).
6. Check: no boundary edges left. Print CLOSE lines (the last says WATERTIGHT or what is wrong) and write the FBX.
"""
import heapq
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src, dst = argv[0], argv[1]
opt = {"--up": "auto", "--drop-share": "0.12", "--rim-share": "0.18", "--back-share": "0.2", "--back-relief": "0.5", "--hole-share": "0.08", "--drop-max-cm": "60", "--rim-max-cm": "150", "--back-max-cm": "80", "--back-tris": "30000"}
for i, a in enumerate(argv[2:]):
    if a in opt and 2 + i + 1 < len(argv):
        opt[a] = argv[2 + i + 1]


def say(msg):
    print("CLOSE " + msg, flush=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src, use_custom_normals=False)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if not meshes:
    raise SystemExit("CLOSE no mesh in " + src)
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
units_per_cm = abs(obj.scale.x)   # the FBX's centimetres come in under an object scale (0.01): what a centimetre is once that scale is applied
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)   # as Tools/kitbash_blender.py: the mesh's data is now in the scene's axes
me = obj.data

bm = bmesh.new()
bm.from_mesh(me)
bm.verts.ensure_lookup_table()
co = np.array([v.co for v in bm.verts], dtype=np.float64)
size = co.max(0) - co.min(0)
tris_before = sum(len(f.verts) - 2 for f in bm.faces)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=float(size.max()) * 1e-5)
boundary_before = sum(1 for e in bm.edges if e.is_boundary)
say("imported %s: %.1f x %.1f x %.1f units, %d triangles, %d boundary edges after welding" % (src, size[0], size[1], size[2], tris_before, boundary_before))
if boundary_before == 0:
    say("already closed: nothing to do")


# The open side.
s = Vector((0.0, 0.0, 0.0))
area = 0.0
for f in bm.faces:
    a = f.calc_area()
    s += f.normal * a
    area += a
openness = s.length / max(area, 1e-9)
facing = s.normalized() if s.length > 0.0 else Vector((0.0, 0.0, 1.0))
force_up = opt["--up"] == "1" or (opt["--up"] == "auto" and facing.z >= 0.7)
open_dir = Vector((0.0, 0.0, -1.0)) if force_up else -facing
say("openness %.2f, visible side (%.2f, %.2f, %.2f), closing toward (%.2f, %.2f, %.2f)" % (openness, facing.x, facing.y, facing.z, open_dir.x, open_dir.y, open_dir.z))

# The mesh's narrower width across the open direction.
u = open_dir.orthogonal().normalized()
v = open_dir.cross(u).normalized()
co = np.array([p.co for p in bm.verts], dtype=np.float64)
along_u = co @ np.array(u)
along_v = co @ np.array(v)
width = float(min(along_u.max() - along_u.min(), along_v.max() - along_v.min()))
if width <= 0.0:
    width = float(size.max())


# Boundary loops; small holes are filled where they are.
def boundary_loops(mesh):
    seen, loops = set(), []
    for e in mesh.edges:
        if not e.is_boundary or e.index in seen:
            continue
        loop, stack = [], [e]
        seen.add(e.index)
        while stack:
            g = stack.pop()
            loop.append(g)
            for w in g.verts:
                for h in w.link_edges:
                    if h.is_boundary and h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        loops.append(loop)
    return loops


bm.edges.ensure_lookup_table()
bm.edges.index_update()
hole_limit = float(opt["--hole-share"]) * width * 3.14159
loops = boundary_loops(bm)
lengths = [sum(e.calc_length() for e in loop) for loop in loops]
longest = max(lengths) if lengths else 0.0
filled = 0
for loop, length in zip(loops, lengths):
    if length < longest and length < hole_limit:
        result = bmesh.ops.holes_fill(bm, edges=loop, sides=0)
        filled += 1
bm.edges.ensure_lookup_table()
bm.edges.index_update()
bm.faces.ensure_lookup_table()
bm.faces.index_update()
bm.verts.ensure_lookup_table()
bm.verts.index_update()
say("%d boundary loop(s), the longest %.1f units; %d small hole(s) filled" % (len(loops), longest, filled))

# The rim: distance along the surface from a boundary, as far as the rim's width. Then the drop.
# A share of its width, capped: a 200 m cliff's rim is already out of sight, and needs no 12 m sink.
rim = min(float(opt["--rim-share"]) * width, float(opt["--rim-max-cm"]) * units_per_cm)
drop = min(float(opt["--drop-share"]) * width, float(opt["--drop-max-cm"]) * units_per_cm)
dist = {}
heap = []
for e in bm.edges:
    if e.is_boundary:
        for w in e.verts:
            if w.index not in dist:
                dist[w.index] = 0.0
                heapq.heappush(heap, (0.0, w.index))
while heap:
    d, i = heapq.heappop(heap)
    if d > dist.get(i, 1e30):
        continue
    w = bm.verts[i]
    for e in w.link_edges:
        o = e.other_vert(w)
        nd = d + e.calc_length()
        if nd < rim and nd < dist.get(o.index, 1e30):
            dist[o.index] = nd
            heapq.heappush(heap, (nd, o.index))
for i, d in dist.items():
    t = 1.0 - d / rim
    bm.verts[i].co += open_dir * (drop * t * t)
say("rim: %d vertices within %.0f cm of a boundary sink up to %.0f cm (a unit is %.2f cm; the narrower width %.0f cm)" % (len(dist), rim / units_per_cm, drop / units_per_cm, 1.0 / units_per_cm, width / units_per_cm))

# Which pieces are open.
piece = {}
open_pieces = set()
for f in bm.faces:
    if f.index in piece:
        continue
    pid = f.index
    stack = [f]
    piece[f.index] = pid
    while stack:
        g = stack.pop()
        for e in g.edges:
            if e.is_boundary:
                open_pieces.add(pid)
            for h in e.link_faces:
                if h.index not in piece:
                    piece[h.index] = pid
                    stack.append(h)
faces = [f for f in bm.faces if piece[f.index] in open_pieces]
say("%d piece(s), %d open, %d of %d faces get a back" % (len(set(piece.values())), len(open_pieces), len(faces), len(bm.faces)))

# The back: every open piece's vertices copied past the lowest point along the open direction.
uv_layers = list(bm.loops.layers.uv.values())
depths = np.array([w.co.dot(open_dir) for w in bm.verts], dtype=np.float64)
floor = float(depths.max())   # the point furthest along the open direction
back_limit = min(float(opt["--back-share"]) * width, float(opt["--back-max-cm"]) * units_per_cm)
relief = float(opt["--back-relief"])
copies = {}


def back_of(w):
    c = copies.get(w.index)
    if c is None:
        height = floor - w.co.dot(open_dir)   # >= 0: how far the vertex stands above that point
        c = bm.verts.new(w.co + open_dir * (height + min(relief * height, back_limit)))
        copies[w.index] = c
    return c


boundary_edges = [e for e in bm.edges if e.is_boundary and piece[e.link_faces[0].index] in open_pieces]
# Only the back is selected, for the thinning below: an import leaves everything selected, and a decimate of that took the skin too.
for f in bm.faces:
    f.select = False
for e in bm.edges:
    e.select = False
for w in bm.verts:
    w.select = False
new_faces = 0
skipped = 0
source_faces = list(faces)
for f in source_faces:
    corners = [(loop.vert, [loop[layer].uv.copy() for layer in uv_layers]) for loop in f.loops]
    verts = [back_of(w) for w, _ in corners][::-1]
    try:
        nf = bm.faces.new(verts)
    except ValueError:
        skipped += 1
        continue
    nf.material_index = f.material_index
    nf.smooth = True
    nf.select_set(True)
    for loop, (_, uvs) in zip(nf.loops, corners[::-1]):
        for layer, uv in zip(uv_layers, uvs):
            loop[layer].uv = uv
    new_faces += 1

# The wall between the skin and its back, along every boundary edge, on that edge's own UVs.
walls = 0
for e in boundary_edges:
    f = e.link_faces[0]
    a_uv, b_uv = None, None
    # The edge's direction as the face runs it, so the wall's winding faces out.
    for loop in f.loops:
        if loop.edge == e:
            a, b = loop.vert, loop.link_loop_next.vert
            a_uv = [loop[layer].uv.copy() for layer in uv_layers]
            b_uv = [loop.link_loop_next[layer].uv.copy() for layer in uv_layers]
            break
    try:
        nf = bm.faces.new((b, a, back_of(a), back_of(b)))
    except ValueError:
        skipped += 1
        continue
    nf.material_index = f.material_index
    nf.smooth = True
    for loop, uvs in zip(nf.loops, (b_uv, a_uv, a_uv, b_uv)):
        for layer, uv in zip(uv_layers, uvs):
            loop[layer].uv = uv
    walls += 1

for f in bm.faces:
    f.smooth = True
bm.normal_update()
bm.to_mesh(me)
bm.free()
me.update()

# Nobody sees the back: thin it, the skin and the walls untouched (a decimate of a selection keeps its border). A 1 M triangle gravel plate would otherwise come back with 2 M.
back_tris_wanted = int(opt["--back-tris"])
back_tris = sum(len(p.vertices) - 2 for p in me.polygons if p.select)
if back_tris > back_tris_wanted:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.context.tool_settings.mesh_select_mode = (False, False, True)
    bpy.ops.mesh.decimate(ratio=back_tris_wanted / float(back_tris))
    bpy.ops.object.mode_set(mode="OBJECT")
    say("back thinned from %d triangles toward %d" % (back_tris, back_tris_wanted))
bm = bmesh.new()
bm.from_mesh(me)
boundary_after = sum(1 for e in bm.edges if e.is_boundary)
nonmanifold_after = sum(1 for e in bm.edges if not e.is_manifold and not e.is_boundary)
tris_after = sum(len(f.verts) - 2 for f in bm.faces)
bm.free()
if tris_after < tris_before:
    say("final: the skin lost triangles (%d of %d left): NOT WATERTIGHT" % (tris_after, tris_before))
    raise SystemExit(1)
dims = np.array([list(p.co) for p in me.vertices])
d = dims.max(0) - dims.min(0)
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(filepath=dst, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE", apply_scale_options="FBX_SCALE_NONE", bake_space_transform=False, add_leaf_bones=False, bake_anim=False)
say("final: %.1f x %.1f x %.1f units, %d back faces, %d wall faces, %d skipped, boundary %d, nonmanifold %d, triangles %d (were %d) -- %s" % (
    d[0], d[1], d[2], new_faces, walls, skipped, boundary_after, nonmanifold_after, tris_after, tris_before, "WATERTIGHT" if boundary_after == 0 else "NOT WATERTIGHT"))
say("wrote " + dst)
