"""Blender (headless): MO57's primitive weapons and gear built procedurally from measurements -- no scan, no image (Nov 2 2026; Wes: "take a few
items that need done and see if you can create some things, especially things like the crude bow that also needs animation").

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Tools/blender_items_blender.py -- <ItemId> <out_dir> [--res N] [--no-bake]

Driven by Tools/blender_items.py (which then imports into the editor). Writes into <out_dir>:
  <ItemId>.fbx            a bow: the skeletal mesh (armature "Armature", root bone "root") and its one action, Draw; anything else: the static mesh
  <ItemId>_static.fbx     (bows) the braced bow as a static mesh: the world item and the icon
  T_<ItemId>_BaseColor.png, T_<ItemId>_Normal.png (DirectX: green down), T_<ItemId>_ORM.png (R occlusion, G roughness, B metallic)
  <ItemId>.json           what was built: size, triangles, materials, and for a bow its draw (below)
  <ItemId>_preview.png    Eevee renders with the baked textures (a bow: braced from three sides and the draw from the side)

Units: the scene is in centimetres (unit scale 0.01, 1 Blender unit = 1 cm), so the FBX carries centimetres and the bones carry no scale.

A bow is solved, not keyframed by hand. The handle is rigid; each limb bends with curvature kappa(s) = rest(s) + lam * m(s) along its length
(m: where the limb works -- zero in the handle, ramping through the fade, easing off in the tips; a composite bow's siyahs are rigid levers set
forward). The string is a fixed length: at brace the limbs are bent until the string stands BRACE cm from the grip centre; for any other
draw D the solver finds lam so that the string, from both nocks to the nocking point (0, -D, NOCK_Z), keeps that length. The bones follow the
solved limb (8 per limb, plus a siyah), the string's three bones sit at the two nocks and the nocking point. Frames: the Draw action runs
LINEARLY from the string's forward limit (DRAW_MIN, the release overshoot) at frame 0 to full draw (her measured draw) at the last frame:
the game picks the frame from where the drawing hand is, and the release swings it about brace (MOBowMeshComponent).

Measured on the survivor (the retargeted Sparrow clips, Nov 2, after the bow arm's retarget was corrected -- its elbow had been bent 33 deg
and the wrist cocked 40): from the bow's seat in her fist (Content/Python/bow_hand_fit.py) the drawing fingers stand 11.5 cm off the grip at
the start of RMB_Drawback, where the string is taken (the brace), and 64.1 cm at full draw (RMB_Loop). Sparrow's own bow was braced as low:
its string 12.2 cm off her grip at the same frame, 10.5 at the survivor's size (the first build's 16 / 64.6 came from the bent arm).
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ITEM = ARGS[0] if ARGS else "SimpleBow01"
OUT = os.path.abspath(ARGS[1] if len(ARGS) > 1 else "blender_items_out")
RES = int(ARGS[ARGS.index("--res") + 1]) if "--res" in ARGS else 0
BAKE = "--no-bake" not in ARGS
os.makedirs(OUT, exist_ok=True)

BRACE = 11.5        # grip centre -> string at brace (cm): where her drawing fingers start the draw
FULL_DRAW = 64.1    # grip centre -> string at full draw (cm): her draw
DRAW_MIN = -0.15    # the Draw action's first frame, as a fraction of the brace -> full draw travel: the string's forward overshoot
NOCK_Z = 2.0        # the nocking point stands this far above the grip centre (the arrow passes over the fist)
FPS = 60
DRAW_FRAMES = 120   # frame 0 = DRAW_MIN ... frame 120 = full draw


def log(msg):
    print("BLENDERITEMS %s: %s" % (ITEM, msg), flush=True)


def srgb(hexstr, a=1.0):
    h = hexstr.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple((x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4) for x in c) + (a,)


def smoothstep(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


# ------------------------------------------------------------------------------------------------------------------------------------------
# Geometry: parts with a natural ("proc") UV in cm per island, packed into one atlas
# ------------------------------------------------------------------------------------------------------------------------------------------

class Part:
    """Vertices (cm), quads/tris, per-face proc UVs (cm) and island keys, per-vertex bone weights and attributes."""

    def __init__(self, name, material, smooth=True):
        self.name, self.material, self.smooth = name, material, smooth
        self.verts, self.faces, self.uvs, self.islands = [], [], [], []
        self.weights, self.attrs = [], []

    def add_vert(self, co, weights=None, **attrs):
        self.verts.append(Vector(co))
        self.weights.append(weights or {})
        self.attrs.append(attrs)
        return len(self.verts) - 1

    def add_face(self, idx, uvs, island):
        self.faces.append(tuple(idx))
        self.uvs.append(list(uvs))
        self.islands.append("%s:%s" % (self.name, island))


def loft(part, rings, ring_uv_u, along_v, closed=True, island_len=24.0, cap_start=False, cap_end=False):
    """rings: list of lists of vertex indices (same count); ring_uv_u: u (cm) per ring vertex (+ one for the seam), one list for every ring or
    a list per ring; along_v: v (cm) per ring. Islands split every island_len cm along v so the atlas packs well."""
    k = len(rings[0])
    per_ring = isinstance(ring_uv_u[0], (list, tuple))
    for r in range(len(rings) - 1):
        v0, v1 = along_v[r], along_v[r + 1]
        isl = int(((v0 + v1) * 0.5) // island_len)
        ua = ring_uv_u[r] if per_ring else ring_uv_u
        ub = ring_uv_u[r + 1] if per_ring else ring_uv_u
        for i in range(k if closed else k - 1):
            j = (i + 1) % k
            a, b, c, d = rings[r][i], rings[r][j], rings[r + 1][j], rings[r + 1][i]
            part.add_face((a, b, c, d), [(ua[i], v0), (ua[i + 1], v0), (ub[i + 1], v1), (ub[i], v1)], isl)
    for which, ring in (("cs", rings[0]) if cap_start else (None, None), ("ce", rings[-1]) if cap_end else (None, None)):
        if which is None:
            continue
        centre = sum((part.verts[i] for i in ring), Vector()) / len(ring)
        # a flat fan, projected on its own plane for the UV
        n = (part.verts[ring[1]] - part.verts[ring[0]]).cross(part.verts[ring[2]] - part.verts[ring[0]]).normalized()
        ax = (part.verts[ring[0]] - centre).normalized()
        ay = n.cross(ax)
        c_idx = part.add_vert(centre, dict(part.weights[ring[0]]), **part.attrs[ring[0]])
        for i in range(k):
            j = (i + 1) % k
            pi, pj = part.verts[ring[i]] - centre, part.verts[ring[j]] - centre
            tri = (ring[i], ring[j], c_idx) if which == "ce" else (ring[j], ring[i], c_idx)
            uv = {ring[i]: (pi.dot(ax), pi.dot(ay)), ring[j]: (pj.dot(ax), pj.dot(ay)), c_idx: (0.0, 0.0)}
            part.add_face(tri, [uv[t] for t in tri], which)


def pack_islands(parts, res):
    """Shelf-pack every island (its proc-UV bounding box) into one square atlas at one texel density. Returns per part, per face, atlas UVs."""
    boxes = {}
    for p in parts:
        for f, isl in enumerate(p.islands):
            us = [uv[0] for uv in p.uvs[f]]
            vs = [uv[1] for uv in p.uvs[f]]
            b = boxes.setdefault(isl, [1e9, 1e9, -1e9, -1e9])
            b[0], b[1], b[2], b[3] = min(b[0], min(us)), min(b[1], min(vs)), max(b[2], max(us)), max(b[3], max(vs))
    margin = 6.0 / res
    items = []
    for isl, (u0, v0, u1, v1) in boxes.items():
        w, h = max(u1 - u0, 1e-3), max(v1 - v0, 1e-3)
        rot = h > w          # long side horizontal
        items.append((isl, (h, w) if rot else (w, h), rot))
    items.sort(key=lambda it: -it[1][1])

    def place(scale):
        x = y = shelf_h = 0.0
        out = {}
        for isl, (w, h), rot in items:
            W, H = w * scale + margin, h * scale + margin
            if W > 1.0:
                return None
            if x + W > 1.0:
                x, y, shelf_h = 0.0, y + shelf_h, 0.0
            if y + H > 1.0:
                return None
            out[isl] = (x + margin * 0.5, y + margin * 0.5, rot)
            x += W
            shelf_h = max(shelf_h, H)
        return out

    lo, hi = 1e-5, 1.0
    for _ in range(40):
        mid = (lo + hi) * 0.5
        if place(mid) is None:
            hi = mid
        else:
            lo = mid
    scale, spots = lo, place(lo)
    result = []
    for p in parts:
        faces = []
        for f, isl in enumerate(p.islands):
            u0, v0, u1, v1 = boxes[isl]
            ox, oy, rot = spots[isl]
            uvs = []
            for (u, v) in p.uvs[f]:
                if rot:
                    uvs.append((ox + (v - v0) * scale, oy + (u1 - u) * scale))
                else:
                    uvs.append((ox + (u - u0) * scale, oy + (v - v0) * scale))
            faces.append(uvs)
        result.append(faces)
    log("atlas: %d islands, %.1f texels/cm at %d px" % (len(items), scale * res, res))
    return result


def build_object(name, parts, res):
    """One mesh object from the parts: material slot per part material, UV maps 'atlas' (render) and 'proc' (cm, for the procedural
    shaders), float attributes, vertex groups for every bone named in the weights."""
    atlas = pack_islands(parts, res)
    verts, faces, uv_atlas, uv_proc, mat_idx, smooth = [], [], [], [], [], []
    mats = []
    attr_names = sorted({k for p in parts for a in p.attrs for k in a})
    attr_vals = {k: [] for k in attr_names}
    groups = {}
    for pi, p in enumerate(parts):
        base = len(verts)
        verts += [tuple(v) for v in p.verts]
        for a in p.attrs:
            for k in attr_names:
                attr_vals[k].append(float(a.get(k, 0.0)))
        for vi, w in enumerate(p.weights):
            for bone, val in w.items():
                if val > 1e-4:
                    groups.setdefault(bone, []).append((base + vi, val))
        if p.material not in mats:
            mats.append(p.material)
        m = mats.index(p.material)
        for f, face in enumerate(p.faces):
            faces.append(tuple(base + i for i in face))
            uv_atlas += atlas[pi][f]
            uv_proc += p.uvs[f]
            mat_idx.append(m)
            smooth.append(p.smooth)
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    for uvname, data in (("atlas", uv_atlas), ("proc", uv_proc)):
        layer = me.uv_layers.new(name=uvname)
        layer.data.foreach_set("uv", [c for uv in data for c in uv])
    me.uv_layers["atlas"].active = True
    me.uv_layers["atlas"].active_render = True
    me.polygons.foreach_set("material_index", mat_idx)
    me.polygons.foreach_set("use_smooth", smooth)
    for k in attr_names:
        at = me.attributes.new(k, "FLOAT", "POINT")
        at.data.foreach_set("value", attr_vals[k])
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])   # every part is a closed shell: normals out
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    ob["mo_attrs"] = attr_names      # build-only data, stripped before export (strip_build_data)
    bpy.context.scene.collection.objects.link(ob)
    for m in mats:
        me.materials.append(m)
    for bone, entries in groups.items():
        vg = ob.vertex_groups.new(name=bone)
        for vi, w in entries:
            vg.add([vi], w, "REPLACE")
    return ob


# ------------------------------------------------------------------------------------------------------------------------------------------
# Materials (procedural, baked to one texture set)
# ------------------------------------------------------------------------------------------------------------------------------------------

class NB:
    def __init__(self, name):
        self.mat = bpy.data.materials.new(name)
        self.mat.use_nodes = True
        self.nt = self.mat.node_tree
        self.nodes, self.links = self.nt.nodes, self.nt.links
        self.bsdf = self.nodes["Principled BSDF"]
        self.out = self.nodes["Material Output"]

    def new(self, kind, **props):
        n = self.nodes.new(kind)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def feed(self, sock, val):
        if hasattr(val, "is_output") or hasattr(val, "links"):
            self.links.new(val, sock)
        elif isinstance(val, (tuple, list)):
            sock.default_value = val
        else:
            sock.default_value = val

    def attr(self, name):
        n = self.new("ShaderNodeAttribute")
        n.attribute_type = "GEOMETRY"
        n.attribute_name = name
        return n.outputs["Fac"]

    def uv(self, name="proc"):
        n = self.new("ShaderNodeUVMap")
        n.uv_map = name
        return n.outputs["UV"]

    def xyz(self, x, y, z):
        n = self.new("ShaderNodeCombineXYZ")
        for i, v in enumerate((x, y, z)):
            self.feed(n.inputs[i], v)
        return n.outputs[0]

    def sep(self, v):
        n = self.new("ShaderNodeSeparateXYZ")
        self.feed(n.inputs[0], v)
        return n.outputs[0], n.outputs[1], n.outputs[2]

    def m(self, op, a, b=0.0, c=0.0, clamp=False):
        n = self.new("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.feed(n.inputs[0], a)
        self.feed(n.inputs[1], b)
        if len(n.inputs) > 2:
            self.feed(n.inputs[2], c)
        return n.outputs[0]

    def vscale(self, v, sx, sy, sz):
        x, y, z = self.sep(v)
        return self.xyz(self.m("MULTIPLY", x, sx), self.m("MULTIPLY", y, sy), self.m("MULTIPLY", z, sz))

    def noise(self, vec, scale, detail=4.0, rough=0.5, distortion=0.0, dims="3D"):
        n = self.new("ShaderNodeTexNoise")
        n.noise_dimensions = dims
        self.feed(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distortion
        return n.outputs["Fac"]

    def wave(self, vec, scale, distortion=0.0, detail=2.0, direction="Y", profile="SIN"):
        n = self.new("ShaderNodeTexWave", wave_type="BANDS", bands_direction=direction, wave_profile=profile)
        self.feed(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Distortion"].default_value = distortion
        n.inputs["Detail"].default_value = detail
        return n.outputs["Fac"]

    def voronoi(self, vec, scale, feature="F1", out="Distance", randomness=1.0):
        n = self.new("ShaderNodeTexVoronoi", feature=feature)
        self.feed(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Randomness"].default_value = randomness
        return n.outputs[out]

    def ramp(self, fac, stops):
        n = self.new("ShaderNodeValToRGB")
        self.feed(n.inputs["Fac"], fac)
        els = n.color_ramp.elements
        while len(els) < len(stops):
            els.new(0.5)
        for e, (pos, col) in zip(els, stops):
            e.position = pos
            e.color = col if len(col) == 4 else tuple(col) + (1.0,)
        return n.outputs["Color"]

    def mix(self, fac, a, b, blend="MIX"):
        n = self.new("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        ins = [s for s in n.inputs if s.enabled]
        self.feed(ins[0], fac)
        self.feed([s for s in ins if s.name == "A"][0], a)
        self.feed([s for s in ins if s.name == "B"][0], b)
        return [s for s in n.outputs if s.enabled][0]

    def mixf(self, fac, a, b):
        n = self.new("ShaderNodeMix", data_type="FLOAT")
        ins = [s for s in n.inputs if s.enabled]
        self.feed(ins[0], fac)
        self.feed([s for s in ins if s.name == "A"][0], a)
        self.feed([s for s in ins if s.name == "B"][0], b)
        return [s for s in n.outputs if s.enabled][0]

    def step(self, x, edge, soft=0.05):
        n = self.new("ShaderNodeMapRange", clamp=True, interpolation_type="SMOOTHSTEP")
        self.feed(n.inputs["Value"], x)
        n.inputs["From Min"].default_value = edge - soft
        n.inputs["From Max"].default_value = edge + soft
        return n.outputs["Result"]

    def bump(self, height, strength, distance=0.1, normal=None):
        n = self.new("ShaderNodeBump")
        self.feed(n.inputs["Height"], height)
        n.inputs["Strength"].default_value = strength
        n.inputs["Distance"].default_value = distance
        if normal is not None:
            self.feed(n.inputs["Normal"], normal)
        return n.outputs["Normal"]

    def finish(self, color, roughness, normal=None, metallic=0.0):
        self.feed(self.bsdf.inputs["Base Color"], color)
        self.feed(self.bsdf.inputs["Roughness"], roughness)
        self.feed(self.bsdf.inputs["Metallic"], metallic)
        if normal is not None:
            self.feed(self.bsdf.inputs["Normal"], normal)
        self.metallic = metallic
        return self.mat


def stave_coords(nb, along_scale=1.0):
    """(side, depth, along) in cm: a coordinate frame that runs with the stave, so grain follows the bent limb."""
    return nb.xyz(nb.attr("side"), nb.attr("depth"), nb.m("MULTIPLY", nb.attr("along"), along_scale))


def mat_wood(name, light, dark, bark=0.0, grime=1.0, tips_dark=True):
    """Split and scraped bow wood: growth rings in depth, long fibre streaks, scraper facets, the odd pin knot; optional bark left on the back."""
    nb = NB(name)
    p = stave_coords(nb)
    rings = nb.wave(nb.vscale(p, 1.0, 1.0, 0.035), 2.2, distortion=5.0, detail=3.0, direction="Y")
    streak = nb.noise(nb.vscale(p, 5.0, 5.0, 0.12), 1.0, detail=8.0, rough=0.65)
    fine = nb.noise(nb.vscale(p, 30.0, 30.0, 0.6), 1.0, detail=4.0, rough=0.6)
    knot = nb.step(nb.voronoi(nb.vscale(p, 0.5, 0.5, 0.08), 1.0), 0.07, 0.03)   # 1 away from pin knots
    facets = nb.voronoi(nb.vscale(p, 0.8, 0.8, 0.12), 1.4, feature="DISTANCE_TO_EDGE")
    base = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", rings, 0.35), nb.m("MULTIPLY", streak, 0.75)),
                   [(0.25, srgb(light)), (0.62, srgb(dark)), (0.95, srgb(dark))])
    base = nb.mix(nb.m("MULTIPLY", fine, 0.25), base, srgb("#2b1d12"), "MULTIPLY")
    base = nb.mix(nb.m("SUBTRACT", 1.0, knot), base, srgb("#3a2414"))
    along_abs = nb.m("ABSOLUTE", nb.attr("along"))
    hand = nb.m("SUBTRACT", 1.0, nb.step(along_abs, 9.0, 3.0))          # the handle: handled, darker
    tip = nb.step(nb.attr("tipdist"), -3.5, 1.5) if tips_dark else 0.0   # tipdist > -3.5: the last 3.5 cm (nocks: greased, sooted)
    base = nb.mix(nb.m("MULTIPLY", hand, 0.35 * grime), base, srgb("#3b2a1c"))
    base = nb.mix(nb.m("MULTIPLY", tip, 0.6), base, srgb("#2a1c12"))
    rough = nb.m("ADD", 0.52, nb.m("MULTIPLY", streak, 0.18))
    height = nb.m("ADD", nb.m("MULTIPLY", fine, 0.25), nb.m("MULTIPLY", facets, 0.6))
    height = nb.m("ADD", height, nb.m("MULTIPLY", nb.m("SUBTRACT", 1.0, knot), -0.6))
    if bark > 0:
        back = nb.step(nb.m("MULTIPLY", nb.attr("depthn"), -1.0), 0.55, 0.12)      # the back (depthn -1) and its shoulders
        patch = nb.step(nb.noise(nb.vscale(p, 0.35, 0.35, 0.05), 1.0, detail=3.0), 0.6 - 0.15 * bark, 0.03)
        barkmask = nb.m("MULTIPLY", back, patch)
        bark_tex = nb.noise(nb.vscale(p, 4.0, 4.0, 1.2), 1.0, detail=6.0, rough=0.7)
        bark_col = nb.ramp(bark_tex, [(0.3, srgb("#2e261e")), (0.7, srgb("#5a4a3a"))])
        base = nb.mix(barkmask, base, bark_col)
        rough = nb.mixf(barkmask, rough, 0.85)
        height = nb.mixf(barkmask, height, nb.m("ADD", 0.6, nb.m("MULTIPLY", bark_tex, 1.2)))
    normal = nb.bump(height, 0.35, 0.08)
    return nb.finish(base, rough, normal)


def mat_layered(name, layers, back_cover=None):
    """A laminated stave seen by depth: layers = [(depthn_from, hexcolour, roughness)], sorted belly (+1) to back (-1); the back face may wear a
    cover (birch bark). Each layer keeps a little fibre streak."""
    nb = NB(name)
    p = stave_coords(nb)
    dn = nb.attr("depthn")
    streak = nb.noise(nb.vscale(p, 5.0, 5.0, 0.1), 1.0, detail=8.0, rough=0.6)
    col, rough = srgb(layers[0][1]), layers[0][2]
    for edge, hexc, r in layers[1:]:
        f = nb.m("SUBTRACT", 1.0, nb.step(dn, edge, 0.04))   # 1 below the edge (toward the back)
        col = nb.mix(f, col, srgb(hexc))
        rough = nb.mixf(f, rough, r)
    col = nb.mix(nb.m("MULTIPLY", streak, 0.3), col, srgb("#1a120c"), "MULTIPLY")
    height = nb.m("MULTIPLY", streak, 0.4)
    if back_cover == "birch":
        cover = nb.m("SUBTRACT", 1.0, nb.step(dn, -0.82, 0.05))
        lent = nb.step(nb.noise(nb.vscale(p, 0.25, 6.0, 1.6), 1.0, detail=2.0), 0.66, 0.02)   # lenticels: dashes across the length
        bark = nb.mix(lent, srgb("#e8e2d2"), srgb("#3a302a"))
        bark = nb.mix(nb.step(nb.noise(nb.vscale(p, 0.5, 0.5, 0.15), 1.0), 0.62, 0.03), bark, srgb("#b98a5c"))   # peeled to the tan
        col = nb.mix(cover, col, bark)
        rough = nb.mixf(cover, rough, 0.6)
        height = nb.mixf(cover, height, nb.m("MULTIPLY", lent, -0.5))
    return nb.finish(col, rough, nb.bump(height, 0.3, 0.06))


def mat_cord(name, light, dark, plies=2, pitch=0.42, gloss=0.55, along_scale=1.0):
    """Twisted cord: plies spiral along the proc UV (u around in cm, v along in cm)."""
    nb = NB(name)
    u, v, _ = nb.sep(nb.uv("proc"))
    twist = nb.m("ADD", nb.m("MULTIPLY", u, plies / 1.0), nb.m("MULTIPLY", v, 1.0 / pitch * along_scale))
    bands = nb.m("SINE", nb.m("MULTIPLY", twist, math.pi * 2.0 / plies))
    bands = nb.m("MULTIPLY", nb.m("ADD", bands, 1.0), 0.5)
    fib = nb.noise(nb.xyz(nb.m("MULTIPLY", u, 3.0), nb.m("MULTIPLY", v, 0.8), 0.0), 4.0, detail=6.0)
    col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", bands, 0.6), nb.m("MULTIPLY", fib, 0.4)), [(0.2, srgb(dark)), (0.8, srgb(light))])
    height = nb.m("ADD", nb.m("POWER", bands, 0.5), nb.m("MULTIPLY", fib, 0.3))
    return nb.finish(col, nb.m("ADD", gloss, nb.m("MULTIPLY", fib, 0.15)), nb.bump(height, 0.6, 0.03))


def mat_hide(name, light, dark, rough=0.55, pores=0.3):
    """Rawhide / leather strip: mottled, edges darker, fine pores."""
    nb = NB(name)
    u, v, _ = nb.sep(nb.uv("proc"))
    q = nb.xyz(u, v, 0.0)
    mott = nb.noise(q, 0.8, detail=5.0, rough=0.6)
    pore = nb.noise(q, 25.0, detail=2.0)
    col = nb.ramp(mott, [(0.3, srgb(dark)), (0.7, srgb(light))])
    edge = nb.attr("edge")
    col = nb.mix(nb.m("MULTIPLY", edge, 0.5), col, srgb("#2a1a10"))
    return nb.finish(col, nb.m("ADD", rough, nb.m("MULTIPLY", mott, 0.12)), nb.bump(nb.m("MULTIPLY", pore, pores), 0.4, 0.02))


# ------------------------------------------------------------------------------------------------------------------------------------------
# The bow
# ------------------------------------------------------------------------------------------------------------------------------------------

BOWS = {
    # Simple Bow: a crude self bow -- one stave of ash or hazel, scraped, bark left in patches on the back, nettle string, a bark-tanned
    # leather strip round the grip. D section (rounded belly, flatter back), stiff tips.
    "SimpleBow01": dict(length=158.0, handle=11.0, fade=7.0, grip=(3.1, 3.7), limb=(3.4, 2.1), tip=(1.5, 1.15), section=(3.4, 2.0),
                        siyah=0.0, siyah_angle=0.0, reflex=0.0, brace_tip_deg=None, crude=1.0, nock_from_tip=2.4,
                        wood=("#b08a5e", "#7d5a38"), bark=0.55, backing=0.0, layers=None, cover=None,
                        string=("nettle", "#a39577", "#5f5644", 0.16), wrap=("leather", "#7a5636", "#4a3220", 1.15, 0.13, 1.0)),
    # Sinew-Backed Bow: shorter, wide flat limbs, a cream sinew layer glued on the back (it reflexes the tips a little), sinew string, rawhide grip.
    "SinewBackedBow01": dict(length=142.0, handle=10.0, fade=6.0, grip=(3.3, 3.4), limb=(4.2, 1.5), tip=(1.7, 0.95), section=(4.0, 3.0),
                             siyah=0.0, siyah_angle=0.0, reflex=10.0, crude=0.4, nock_from_tip=2.2,
                             wood=("#a8835a", "#6e4f33"), bark=0.0, backing=0.32, layers=[(1.0, "#9b7650", 0.55), (-0.35, "#d8c08e", 0.42)],
                             cover=None, string=("sinew", "#c9a466", "#7c5a2e", 0.15), wrap=("rawhide", "#d2b07a", "#8a6a3e", 1.3, 0.1, 1.05)),
    # Composite Bow: horn belly, wood core, sinew back covered in birch bark; rigid siyahs (ears) set forward -- a static recurve; cord grip.
    "CompositeBow01": dict(length=128.0, handle=10.0, fade=5.0, grip=(3.0, 3.6), limb=(3.6, 1.7), tip=(2.2, 1.2), section=(4.0, 3.0),
                           siyah=17.0, siyah_angle=34.0, reflex=0.0, crude=0.15, nock_from_tip=1.6,
                           wood=("#9a7650", "#6e4f33"), bark=0.0, backing=0.0,
                           layers=[(1.0, "#2a2420", 0.32), (0.25, "#8a6a46", 0.55), (-0.15, "#c9ac78", 0.45)], cover="birch",
                           string=("sinew", "#cfae74", "#80602f", 0.14), wrap=("cord", "#6a5136", "#3c2c1c", 0.3, 0.3, 0.0)),
}
LIMB_BONES = 8


def grip_half_size(spec):
    """The wrapped handle as the hand meets it (cm): half its width across, half its depth along the draw, half the handle's length -- the
    item row's HeldVisual.GripHalfSize, what her fingers close on (UMOBodyRetargetAnimInstance). The wrap stands its thickness plus ~0.07
    (its turns ride 0.03-0.07 off the stave) over the grip's section."""
    kind, _, _, _, thick, _ = spec["wrap"]
    out = (0.28 if kind == "cord" else thick) + 0.07
    return [round(spec["grip"][0] / 2.0 + out, 2), round(spec["grip"][1] / 2.0 + out, 2), round(spec["handle"] / 2.0, 2)]


class BowSolver:
    def __init__(self, spec, rng):
        self.s = spec
        self.Lh = spec["length"] / 2.0
        self.h = spec["handle"] / 2.0
        self.fd = spec["fade"]
        self.Lw = self.Lh - spec["siyah"]           # where the working limb ends (a siyah carries on rigid)
        self.n = 400
        self.ds = self.Lh / self.n
        self.s_nock = self.Lh - spec["nock_from_tip"]

    def m(self, s):
        """How much the limb bends here (0 rigid .. 1)."""
        if s <= self.h:
            return 0.0
        if s <= self.h + self.fd:
            return smoothstep(self.h, self.h + self.fd, s)
        if s > self.Lw:
            return 0.0
        u = (s - self.h - self.fd) / max(1e-6, self.Lw - self.h - self.fd)
        return 1.0 - 0.55 * smoothstep(0.7, 1.0, u)     # stiffer tips

    def rest(self, s):
        """Curvature the stave holds unloaded (rad/cm): reflex toward the tips (sinew backing), the siyah set forward over the last 3 cm."""
        k = 0.0
        if self.s["reflex"] > 0:
            k -= math.radians(self.s["reflex"]) * smoothstep(self.Lw * 0.55, self.Lw, s) / (self.Lw * 0.25)
        if self.s["siyah"] > 0 and self.Lw - 3.0 < s <= self.Lw:
            k -= math.radians(self.s["siyah_angle"]) / 3.0
        return k

    def shape(self, lam):
        """One limb (top; the bottom mirrors it) at load lam: arrays s, y, z, theta (y toward the target is +, the archer is at -y)."""
        ss, ys, zs, th = [0.0], [0.0], [0.0], [0.0]
        y = z = t = 0.0
        for i in range(self.n):
            s = (i + 0.5) * self.ds
            t_mid = t + 0.5 * (self.rest(s) + lam * self.m(s)) * self.ds
            y += -math.sin(t_mid) * self.ds
            z += math.cos(t_mid) * self.ds
            t += (self.rest(s) + lam * self.m(s)) * self.ds
            ss.append((i + 1) * self.ds)
            ys.append(y)
            zs.append(z)
            th.append(t)
        return np.array(ss), np.array(ys), np.array(zs), np.array(th)

    @staticmethod
    def at(shape, s):
        ss, ys, zs, th = shape
        return float(np.interp(s, ss, ys)), float(np.interp(s, ss, zs)), float(np.interp(s, ss, th))

    def nock_point(self, shape, top=True):
        """Where the string leaves the limb at the nock: the limb's belly surface over the notch."""
        y, z, t = self.at(shape, self.s_nock)
        _, half_t = self.dims(self.s_nock)
        ny, nz = -math.cos(t), -math.sin(t)
        p = Vector((0.0, y + ny * half_t * 0.6, z + nz * half_t * 0.6))
        return p if top else Vector((0.0, p.y, -p.z))

    def dims(self, s):
        sp = self.s
        (gw, gt), (lw, lt), (tw, tt) = sp["grip"], sp["limb"], sp["tip"]
        if s <= self.h:
            w, t = gw, gt
        elif s <= self.h + self.fd:
            f = smoothstep(self.h, self.h + self.fd, s)
            w, t = gw + (lw - gw) * f, gt + (lt - gt) * f
        elif s <= self.Lw or sp["siyah"] <= 0:
            u = (s - self.h - self.fd) / max(1e-6, self.Lw - self.h - self.fd)
            u = min(1.0, u)
            w, t = lw + (tw - lw) * u ** 1.15, lt + (tt - lt) * u
        else:
            u = (s - self.Lw) / sp["siyah"]
            w, t = tw * (1.0 - 0.15 * u), tt * (1.35 - 0.2 * u)
        return w, t

    def string_len(self, lam, D):
        sh = self.shape(lam)
        a, b = self.nock_point(sh, True), self.nock_point(sh, False)
        N = Vector((0.0, -D, NOCK_Z))
        return (a - N).length + (b - N).length

    def solve_brace(self):
        """lam that puts the straight string BRACE cm from the grip centre at the nocking height; the string length follows."""
        def brace_d(lam):
            sh = self.shape(lam)
            a, b = self.nock_point(sh, True), self.nock_point(sh, False)
            f = (NOCK_Z - b.z) / (a.z - b.z)
            return -(b.y + (a.y - b.y) * f)
        lo, hi = -0.02, 0.05
        for _ in range(60):
            mid = (lo + hi) * 0.5
            if brace_d(mid) < BRACE:
                lo = mid
            else:
                hi = mid
        self.lam_brace = (lo + hi) * 0.5
        sh = self.shape(self.lam_brace)
        self.S = (self.nock_point(sh, True) - self.nock_point(sh, False)).length
        return self.lam_brace

    def solve(self, D):
        lo, hi = -0.03, 0.2
        for _ in range(60):
            mid = (lo + hi) * 0.5
            if self.string_len(mid, D) > self.S:
                lo = mid
            else:
                hi = mid
        return (lo + hi) * 0.5


def build_bow(spec):
    rng = random.Random(ITEM)
    sol = BowSolver(spec, rng)
    lam_b = sol.solve_brace()
    brace = sol.shape(lam_b)
    lam_full = sol.solve(FULL_DRAW)
    full = sol.shape(lam_full)
    tip_b, tip_f = math.degrees(sol.at(brace, sol.Lh)[2]), math.degrees(sol.at(full, sol.Lh)[2])
    log("string %.1f cm; brace lam %.5f (tips at %.1f deg), full draw %.1f cm lam %.5f (tips at %.1f deg)" % (
        sol.S, lam_b, tip_b, FULL_DRAW, lam_full, tip_f))

    crude = spec["crude"]
    ph = [rng.uniform(0, 6.28) for _ in range(6)]

    def wobble(s):
        return crude * (0.22 * math.sin(0.061 * s + ph[0]) + 0.12 * math.sin(0.17 * s + ph[1]))

    def dims_at(s):
        w, t = sol.dims(abs(s))
        w *= 1.0 + crude * 0.04 * math.sin(0.11 * s + ph[2])
        t *= 1.0 + crude * 0.05 * math.sin(0.09 * s + ph[3])
        d = abs(abs(s) - sol.s_nock)
        w *= 1.0 - 0.42 * math.exp(-(d / 0.32) ** 2)          # the side notches
        end = abs(s) - (sol.Lh - 1.1)
        if end > 0:
            f = math.sqrt(max(0.0, 1.0 - (end / 1.1) ** 2))
            w, t = w * max(0.3, f), t * max(0.3, f)
        return w, t

    # bones: the working limb in LIMB_BONES pieces from the handle's end, then the siyah
    edges = [sol.h + (sol.Lw - sol.h) * i / LIMB_BONES for i in range(LIMB_BONES + 1)]
    centres = [(edges[i] + edges[i + 1]) * 0.5 for i in range(LIMB_BONES)]

    def weights(s):
        side = "t" if s >= 0 else "b"
        a = abs(s)
        if spec["siyah"] > 0 and a > sol.Lw - 1.0:
            f = smoothstep(sol.Lw - 1.0, sol.Lw + 1.0, a)
            return {"siyah_%s" % side: f, "limb_%s_%02d" % (side, LIMB_BONES): 1.0 - f}
        if a <= sol.h:
            return {"root": 1.0}
        if a <= centres[0]:
            f = (a - sol.h) / (centres[0] - sol.h)
            return {"root": 1.0 - f, "limb_%s_01" % side: f}
        for i in range(LIMB_BONES - 1):
            if a <= centres[i + 1]:
                f = (a - centres[i]) / (centres[i + 1] - centres[i])
                return {"limb_%s_%02d" % (side, i + 1): 1.0 - f, "limb_%s_%02d" % (side, i + 2): f}
        return {"limb_%s_%02d" % (side, LIMB_BONES): 1.0}

    def frame(s):
        y, z, t = sol.at(brace, abs(s))
        if s >= 0:
            c = Vector((wobble(s), y, z))
            d = Vector((0.0, -math.sin(t), math.cos(t)))
        else:
            c = Vector((wobble(s), y, -z))
            d = Vector((0.0, math.sin(t), math.cos(t)))
        n = Vector((0.0, -d.z, d.y))      # toward the belly (the archer)
        return c, d, n

    # -- the stave --
    parts = []
    wood = spec["layers"]
    if wood:
        mat = mat_layered("M_%s_Stave" % ITEM, wood, spec["cover"])
    else:
        mat = mat_wood("M_%s_Stave" % ITEM, spec["wood"][0], spec["wood"][1], bark=spec["bark"])
    stave = Part("stave", mat)
    stations = []
    s = -sol.Lh
    while s < sol.Lh - 1e-6:
        stations.append(s)
        a = abs(s)
        fine = abs(a - sol.s_nock) < 2.0 or a > sol.Lh - 2.0 or (spec["siyah"] > 0 and abs(a - sol.Lw) < 4.0)
        s += 0.25 if fine else (0.6 if a < sol.h + sol.fd + 2 else 1.2)
    stations.append(sol.Lh)
    K = 16
    e_belly, e_back = spec["section"]
    rings, along_v, ring_us = [], [], []
    for s in stations:
        c, d, n = frame(s)
        w, t = dims_at(s)
        back_extra = spec["backing"]
        ring = []
        us = [0.0]
        prev = None
        for i in range(K):
            phi = 2.0 * math.pi * i / K
            cx, sy = math.cos(phi), math.sin(phi)
            a_ = 0.5 * w * math.copysign(abs(cx) ** (2.0 / 3.2), cx)
            if sy >= 0:    # belly
                b_ = 0.5 * t * (abs(sy) ** (2.0 / e_belly))
            else:          # back (a backing layer sits on it)
                b_ = -0.5 * t * (abs(sy) ** (2.0 / e_back)) - back_extra * (abs(sy) ** 0.5)
            p = c + Vector((1, 0, 0)) * a_ + n * b_
            dn = b_ / (0.5 * t + (back_extra if b_ < 0 else 0.0))
            tipdist = abs(s) - sol.Lh
            ring.append(stave.add_vert(p, weights(s), along=s, depth=b_, side=a_, depthn=dn, tipdist=tipdist))
            if prev is not None:
                us.append(us[-1] + (p - prev).length)
            prev = p
        us.append(us[-1] + (stave.verts[ring[0]] - prev).length)
        ring_us.append(us)
        rings.append(ring)
        along_v.append(s + sol.Lh)
    loft(stave, rings, ring_us, along_v, closed=True, cap_start=True, cap_end=True)
    parts.append(stave)

    # -- the grip wrap --
    kind, light, dark, width, thick, pitch_extra = spec["wrap"]
    if kind == "cord":
        wmat = mat_cord("M_%s_Wrap" % ITEM, light, dark, plies=2, pitch=0.3, gloss=0.6)
    else:
        wmat = mat_hide("M_%s_Wrap" % ITEM, light, dark, rough=0.5 if kind == "rawhide" else 0.68)
    wrap = Part("wrap", wmat)
    span = sol.h * 2.0 - 0.6
    if kind == "cord":
        width, thick = 0.28, 0.28
    pitch = width * (0.92 if kind != "cord" else 1.0)
    turns = span / pitch
    samples = int(turns * 40)
    wrings, wv = [], []
    vacc = 0.0
    prev = None
    for i in range(samples + 1):
        f = i / samples
        s = -sol.h + 0.3 + span * f
        phi = 2.0 * math.pi * turns * f
        c, d, n = frame(s)
        w, t = dims_at(s)
        cx, sy = math.cos(phi), math.sin(phi)
        a_ = 0.5 * w * math.copysign(abs(cx) ** (2.0 / 3.2), cx)
        b_ = 0.5 * t * math.copysign(abs(sy) ** (2.0 / (e_belly if sy >= 0 else e_back)), sy)
        surf = c + Vector((1, 0, 0)) * a_ + n * b_
        radial = (Vector((1, 0, 0)) * a_ + n * b_).normalized()
        up = d
        p0 = surf + radial * (thick * 0.5 + 0.03 + 0.04 * (i % 2))
        ring = []
        corners = [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)] if kind != "cord" else \
            [(0.5 * math.cos(2 * math.pi * j / 6), 0.5 * math.sin(2 * math.pi * j / 6)) for j in range(6)]
        for (ca, cb) in corners:
            q = p0 + up * (ca * width) + radial * (cb * thick)
            edge = 1.0 if abs(ca) > 0.45 and kind != "cord" else 0.0
            ring.append(wrap.add_vert(q, {"root": 1.0}, edge=edge, along=s))
        if prev is not None:
            vacc += (p0 - prev).length
        prev = p0
        wrings.append(ring)
        wv.append(vacc)
    cu = [0.0, width, width + thick, 2 * width + thick, 2 * (width + thick)] if kind != "cord" else [0.3 * j / 6 * math.pi for j in range(7)]
    loft(wrap, wrings, cu, wv, closed=True, cap_start=True, cap_end=True)
    parts.append(wrap)

    # -- the string: a cord from nock to nock through the nocking point, a serving round it, loops round the limb at the notches --
    sname, slight, sdark, sdia = spec["string"]
    smat = mat_cord("M_%s_String" % ITEM, slight, sdark, plies=2, pitch=0.45, gloss=0.5 if sname == "nettle" else 0.4)
    string = Part("string", smat)
    A, B = sol.nock_point(brace, True), sol.nock_point(brace, False)
    Nb = Vector((0.0, -BRACE, NOCK_Z))
    srings, sv = [], []
    SK = 6
    # z stations along the string (top to bottom)
    zs = sorted({A.z, B.z, NOCK_Z, NOCK_Z + 5.0, NOCK_Z - 5.0} | {z for z in np.arange(B.z, A.z, 3.0)}, reverse=True)
    vacc, prev = 0.0, None
    for z in zs:
        if z >= NOCK_Z:
            f = (z - NOCK_Z) / (A.z - NOCK_Z)
            p = Nb + (A - Nb) * f
            wts = {"string_top": f, "string_nock": 1.0 - f}
        else:
            f = (NOCK_Z - z) / (NOCK_Z - B.z)
            p = Nb + (B - Nb) * f
            wts = {"string_bott": f, "string_nock": 1.0 - f}
        serving = abs(z - NOCK_Z) <= 5.0 + 1e-6
        r = sdia * (1.35 if serving else 1.0)
        axis = (A - B).normalized()
        x1 = Vector((1, 0, 0))
        y1 = axis.cross(x1).normalized()
        ring = [string.add_vert(p + x1 * (r * math.cos(2 * math.pi * j / SK)) + y1 * (r * math.sin(2 * math.pi * j / SK)), wts)
                for j in range(SK)]
        if prev is not None:
            vacc += (p - prev).length
        prev = p
        srings.append(ring)
        sv.append(vacc)
    loft(string, srings, [2 * math.pi * sdia * j / SK for j in range(SK + 1)], sv, closed=True, cap_start=True, cap_end=True)
    # the loops round the notches: an ellipse round the notched section, on the limb's last bone
    for top in (True, False):
        s_ = sol.s_nock if top else -sol.s_nock
        c, d, n = frame(s_)
        w, t = dims_at(s_)
        bone = ("siyah_%s" if spec["siyah"] > 0 else "limb_%s_%02d" % ("%s", LIMB_BONES)) % ("t" if top else "b")
        lr, lv = [], []
        M = 20
        for i in range(M + 1):
            phi = 2 * math.pi * i / M
            centre = c + Vector((1, 0, 0)) * ((0.5 * w + sdia) * math.cos(phi)) + n * ((0.5 * t + sdia) * math.sin(phi))
            radial = (Vector((1, 0, 0)) * math.cos(phi) + n * math.sin(phi)).normalized()
            ring = [string.add_vert(centre + radial * (sdia * math.cos(2 * math.pi * j / SK)) + d * (sdia * math.sin(2 * math.pi * j / SK)),
                                    {bone: 1.0}) for j in range(SK)]
            lr.append(ring)
            lv.append(i * (2 * math.pi * (0.5 * (w + t) * 0.5 + sdia)) / M)
        loft(string, lr, [2 * math.pi * sdia * j / SK for j in range(SK + 1)], lv, closed=True)
    parts.append(string)
    return sol, parts, brace, edges


def bow_bone_matrices(sol, lam, D, edges):
    """Armature-space matrices of every bone for the bow at load lam with the nocking point D behind the grip."""
    sh = sol.shape(lam)
    mats = {"root": Matrix.Identity(4)}

    def bone_mat(head, tail):
        yv = (tail - head).normalized()
        xv = Vector((1.0, 0.0, 0.0))
        zv = xv.cross(yv).normalized()
        m = Matrix((xv, yv, zv)).transposed().to_4x4()
        m.translation = head
        return m

    def pt(s, top):
        y, z, _ = sol.at(sh, s)
        return Vector((0.0, y, z if top else -z))

    for top, side in ((True, "t"), (False, "b")):
        for i in range(LIMB_BONES):
            mats["limb_%s_%02d" % (side, i + 1)] = bone_mat(pt(edges[i], top), pt(edges[i + 1], top))
        if sol.s["siyah"] > 0:
            mats["siyah_%s" % side] = bone_mat(pt(sol.Lw, top), pt(sol.Lh, top))
    a, b = sol.nock_point(sh, True), sol.nock_point(sh, False)
    for name, p in (("string_top", a), ("string_bott", b), ("string_nock", Vector((0.0, -D, NOCK_Z)))):
        m = Matrix.Identity(4)
        m.translation = p
        mats[name] = m
    return mats


def make_armature(sol, edges):
    arm = bpy.data.armatures.new("Armature")
    ob = bpy.data.objects.new("Armature", arm)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    rest = bow_bone_matrices(sol, sol.lam_brace, BRACE, edges)
    order = ["root"]
    for side in ("t", "b"):
        order += ["limb_%s_%02d" % (side, i + 1) for i in range(LIMB_BONES)]
        if sol.s["siyah"] > 0:
            order.append("siyah_%s" % side)
    order += ["string_top", "string_nock", "string_bott"]
    parent = {}
    for side in ("t", "b"):
        prev = "root"
        for i in range(LIMB_BONES):
            parent["limb_%s_%02d" % (side, i + 1)] = prev
            prev = "limb_%s_%02d" % (side, i + 1)
        if sol.s["siyah"] > 0:
            parent["siyah_%s" % side] = prev
    for nme in ("string_top", "string_nock", "string_bott"):
        parent[nme] = "root"
    for nme in order:
        eb = arm.edit_bones.new(nme)
        m = rest[nme]
        head = m.translation.copy()
        yv = m.col[1].to_3d()
        length = 4.0 if nme in ("root",) or nme.startswith("string") else None
        if length is None:
            length = (edges[1] - edges[0]) if nme.startswith("limb") else sol.s["siyah"]
        eb.head = head
        eb.tail = head + yv * length
        eb.align_roll(m.col[2].to_3d())
        eb.use_deform = True
        if nme in parent:
            eb.parent = arm.edit_bones[parent[nme]]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in ob.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return ob, order


def key_draw(arm_ob, sol, edges, order):
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.frame_start, scene.frame_end = 0, DRAW_FRAMES
    bpy.context.view_layer.objects.active = arm_ob
    bpy.ops.object.mode_set(mode="POSE")
    report = []
    for fr in range(DRAW_FRAMES + 1):
        f = DRAW_MIN + (1.0 - DRAW_MIN) * fr / DRAW_FRAMES
        D = BRACE + f * (FULL_DRAW - BRACE)
        lam = sol.solve(D)
        mats = bow_bone_matrices(sol, lam, D, edges)
        for nme in order:
            pb = arm_ob.pose.bones[nme]
            pb.matrix = mats[nme]
            bpy.context.view_layer.update()
            pb.keyframe_insert("location", frame=fr)
            pb.keyframe_insert("rotation_quaternion", frame=fr)
            pb.keyframe_insert("scale", frame=fr)
        if fr % 30 == 0 or fr == DRAW_FRAMES:
            report.append((fr, round(f, 3), round(D, 1), round(math.degrees(sol.at(sol.shape(lam), sol.Lh)[2]), 1)))
    bpy.ops.object.mode_set(mode="OBJECT")
    act = arm_ob.animation_data.action
    act.name = "Draw"
    try:
        for fc in act.fcurves:
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
    except AttributeError:   # layered actions (4.4+): channelbags hold the curves
        for layer in act.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for fc in bag.fcurves:
                        for kp in fc.keyframe_points:
                            kp.interpolation = "LINEAR"
    log("draw keyed: frame, fraction, draw cm, tip angle deg: %s" % report)
    return report


# ------------------------------------------------------------------------------------------------------------------------------------------
# Tubes, lenses and the arrows
# ------------------------------------------------------------------------------------------------------------------------------------------

def tube(part, pts, radius, sides=8, cap_start=True, cap_end=True, weights=None, attrs=None, up=Vector((0.0, 0.0, 1.0)), island_len=24.0):
    """A tube along the polyline pts (parallel-transport frames). radius: a number, or one per point (a number or (r_normal, r_binormal)).
    attrs(i) -> dict of vertex attributes for ring i."""
    n = len(pts)
    tang = [(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized() for i in range(n)]
    nrm = up - tang[0] * up.dot(tang[0])
    if nrm.length < 1e-4:
        nrm = Vector((0.0, 1.0, 0.0)) - tang[0] * tang[0].y
    nrm.normalize()
    rings, us, vs, v = [], [], [], 0.0
    for i in range(n):
        if i:
            nrm = (nrm - tang[i] * nrm.dot(tang[i])).normalized()
            v += (pts[i] - pts[i - 1]).length
        bin_ = tang[i].cross(nrm)
        r = radius[i] if isinstance(radius, list) else radius
        rx, ry = r if isinstance(r, tuple) else (r, r)
        a = attrs(i) if attrs else {}
        rings.append([part.add_vert(pts[i] + nrm * (rx * math.cos(2 * math.pi * j / sides)) + bin_ * (ry * math.sin(2 * math.pi * j / sides)),
                                    dict(weights) if weights else None, **a) for j in range(sides)])
        circ = math.pi * (rx + ry)
        us.append([circ * j / sides for j in range(sides + 1)])
        vs.append(v)
    loft(part, rings, us, vs, closed=True, island_len=island_len, cap_start=cap_start, cap_end=cap_end)
    return rings


def lens(part, length, half_width, half_thick, origin, axis_x, axis_y, nx=32, ny=8, attrs=None):
    """A flat blade / point: outline half_width(x) and thickness half_thick(x, yn) for x in [0, length] (yn = y / half_width in -1..1), top
    and bottom sheets meeting at the edge. axis_x along the blade, axis_y across; the thickness is along axis_x x axis_y."""
    axis_z = axis_x.cross(axis_y).normalized()
    rows_top, rows_bot = [], []
    for i in range(nx + 1):
        x = length * i / nx
        w = max(half_width(x), 1e-3)
        top, bot = [], []
        for j in range(-ny, ny + 1):
            yn = j / ny
            y = w * yn
            t = half_thick(x, yn) if abs(j) < ny else 0.0
            base = origin + axis_x * x + axis_y * y
            a = attrs(x, y) if attrs else {}
            if abs(j) == ny and bot is not None and len(rows_bot) >= 0:
                vi = part.add_vert(base, None, **a)
                top.append(vi)
                bot.append(vi)
            else:
                top.append(part.add_vert(base + axis_z * t, None, **a))
                bot.append(part.add_vert(base - axis_z * t, None, **a))
        rows_top.append(top)
        rows_bot.append(bot)
    for i in range(nx):
        x0, x1 = length * i / nx, length * (i + 1) / nx
        for j in range(2 * ny):
            y0, y1 = (j - ny) / ny, (j + 1 - ny) / ny
            uv = [(y0, x0), (y1, x0), (y1, x1), (y0, x1)]
            part.add_face((rows_top[i][j], rows_top[i][j + 1], rows_top[i + 1][j + 1], rows_top[i + 1][j]), uv, "top")
            part.add_face((rows_bot[i][j + 1], rows_bot[i][j], rows_bot[i + 1][j], rows_bot[i + 1][j + 1]),
                          [(y1 + 3.0, x0), (y0 + 3.0, x0), (y0 + 3.0, x1), (y1 + 3.0, x1)], "bot")
    # the base (x = 0) and the tip closed between the sheets
    for row, key in ((0, "base"), (nx, "tip")):
        for j in range(2 * ny):
            a, b, c, d = rows_top[row][j], rows_top[row][j + 1], rows_bot[row][j + 1], rows_bot[row][j]
            if len({a, b, c, d}) == 4:
                part.add_face((b, a, d, c), [(j, 0), (j + 1, 0), (j + 1, 0.5), (j, 0.5)], key)


def mat_shaft(name, light, dark, nodes=0.0, scorch=0.0, cresting=None):
    """An arrow shaft: a shoot scraped straight (long fibre streaks), node bumps darker, heat-straightening scorch; painted cresting bands
    [(x0, x1, hex)] along the shaft."""
    nb = NB(name)
    x = nb.attr("ax")
    u, v, _ = nb.sep(nb.uv("proc"))
    q = nb.xyz(nb.m("MULTIPLY", u, 4.0), nb.m("MULTIPLY", x, 0.15), 0.0)
    streak = nb.noise(q, 3.0, detail=6.0, rough=0.6)
    col = nb.ramp(streak, [(0.3, srgb(dark)), (0.7, srgb(light))])
    if scorch > 0:
        sc = nb.step(nb.noise(nb.xyz(nb.m("MULTIPLY", u, 0.4), nb.m("MULTIPLY", x, 0.25), 1.7), 1.0, detail=3.0), 0.68 - 0.04 * scorch, 0.05)
        col = nb.mix(nb.m("MULTIPLY", sc, 0.35), col, srgb("#3a2414"))
    if nodes > 0:
        nd = nb.step(nb.noise(nb.xyz(nb.m("MULTIPLY", x, 0.3), 0.0, 0.0), 1.0, detail=1.0), 0.7, 0.01)
        col = nb.mix(nb.m("MULTIPLY", nd, 0.6 * nodes), col, srgb("#5a3a20"))
    for x0, x1, hexc in cresting or []:
        band = nb.m("MULTIPLY", nb.step(x, x0, 0.02), nb.m("SUBTRACT", 1.0, nb.step(x, x1, 0.02)))
        col = nb.mix(band, col, srgb(hexc))
    return nb.finish(col, nb.m("ADD", 0.5, nb.m("MULTIPLY", streak, 0.15)), nb.bump(streak, 0.25, 0.02))


def mat_feather(name, light, dark, quill, barred=True):
    """A vane: barbs angled back from the quill, bars across (barred feathers), the quill pale. Attributes fx (along, cm) and fh (out from the
    shaft, cm)."""
    nb = NB(name)
    fx, fh = nb.attr("fx"), nb.attr("fh")
    barbs = nb.m("SINE", nb.m("MULTIPLY", nb.m("ADD", nb.m("MULTIPLY", fx, 0.55), nb.m("MULTIPLY", fh, 0.84)), 2.0 * math.pi / 0.05))
    barbs = nb.m("MULTIPLY", nb.m("ADD", barbs, 1.0), 0.5)
    col = srgb(light)
    if barred:
        bars = nb.step(nb.m("SINE", nb.m("MULTIPLY", nb.m("ADD", fx, nb.m("MULTIPLY", fh, 0.6)), 2.0 * math.pi / 1.5)), 0.1, 0.25)
        col = nb.mix(bars, srgb(dark), srgb(light))
    else:
        col = nb.ramp(nb.noise(nb.xyz(fx, fh, 0.0), 1.5, detail=3.0), [(0.3, srgb(dark)), (0.7, srgb(light))])
    col = nb.mix(nb.m("MULTIPLY", barbs, 0.25), col, srgb("#1a1410"), "MULTIPLY")
    quill_mask = nb.m("SUBTRACT", 1.0, nb.step(fh, 0.12, 0.04))
    col = nb.mix(quill_mask, col, srgb(quill))
    return nb.finish(col, 0.62, nb.bump(barbs, 0.35, 0.01))


def mat_flint(name):
    """Knapped flint: grey-brown with paler mottling, a waxy lustre, flake scars as shallow scoops with ripples."""
    nb = NB(name)
    tc = nb.new("ShaderNodeTexCoord")
    p = tc.outputs["Object"]
    mott = nb.noise(p, 1.6, detail=6.0, rough=0.6)
    col = nb.ramp(mott, [(0.25, srgb("#3c3632")), (0.55, srgb("#6e6258")), (0.8, srgb("#a69684"))])
    scars = nb.voronoi(p, 3.2, feature="F1")
    ripple = nb.m("SINE", nb.m("MULTIPLY", scars, 60.0))
    height = nb.m("ADD", nb.m("MULTIPLY", nb.m("POWER", scars, 2.0), 2.0), nb.m("MULTIPLY", ripple, 0.05))
    return nb.finish(col, nb.m("ADD", 0.3, nb.m("MULTIPLY", mott, 0.1)), nb.bump(height, 0.45, 0.04))


def mat_iron(name):
    """Forged iron: dark grey metal, rust in the hollows and patches (not metallic where rusted)."""
    nb = NB(name)
    tc = nb.new("ShaderNodeTexCoord")
    p = tc.outputs["Object"]
    rust = nb.step(nb.noise(p, 1.2, detail=6.0, rough=0.65), 0.56, 0.05)
    grain = nb.noise(p, 12.0, detail=4.0)
    metal_col = nb.ramp(grain, [(0.3, srgb("#2e2e30")), (0.7, srgb("#5a5a5e"))])
    rust_col = nb.ramp(grain, [(0.3, srgb("#5a2e16")), (0.7, srgb("#8a4a24"))])
    col = nb.mix(rust, metal_col, rust_col)
    rough = nb.mixf(rust, 0.42, 0.88)
    metal = nb.mixf(rust, 0.92, 0.05)
    return nb.finish(col, rough, nb.bump(nb.m("ADD", grain, nb.m("MULTIPLY", rust, 0.6)), 0.3, 0.02), metallic=metal)


def mat_plain(name, hexc, rough):
    nb = NB(name)
    n = nb.noise(nb.new("ShaderNodeTexCoord").outputs["Object"], 6.0, detail=4.0)
    return nb.finish(nb.mix(nb.m("MULTIPLY", n, 0.3), srgb(hexc), srgb("#000000")), rough, nb.bump(n, 0.2, 0.02))


ARROWS = {
    # Fletched Arrow: a shoot shaft (nodes, heat-straightening scorch), a self nock bound with sinew, three barred brown feathers set with a
    # slight helix and bound front and back, a side-notched flint point in a split, lashed with sinew and sealed with pitch.
    "FletchedArrow01": dict(length=76.0, dia=0.86, head="flint", feather=("#e2d6bc", "#6b4a2c", "#efe8da", True), fletch=(12.0, 1.45),
                            shaft=("#c9a77a", "#8f6c46"), nodes=1.0, scorch=1.0, cresting=None),
    # Iron Arrow: a planed shaft with red-ochre and black cresting, grey goose fletching, a socketed leaf-bladed iron head.
    "IronArrow01": dict(length=78.0, dia=0.9, head="iron", feather=("#cfccc4", "#7a7670", "#f0eee8", False), fletch=(13.0, 1.6),
                        shaft=("#d4b483", "#a5845a"), nodes=0.0, scorch=0.0, cresting=[(-26.0, -24.6, "#7a2e1a"), (-24.1, -23.7, "#141210"),
                                                                                         (-23.2, -21.8, "#7a2e1a")]),
}


def build_arrow(spec):
    rng = random.Random(ITEM)
    L, dia = spec["length"], spec["dia"]
    xn, xf = -L / 2.0, L / 2.0          # the nock end and the shaft's front end (the head goes beyond)
    r0 = dia / 2.0
    parts = []
    shaft = Part("shaft", mat_shaft("M_%s_Shaft" % ITEM, spec["shaft"][0], spec["shaft"][1], spec["nodes"], spec["scorch"], spec["cresting"]))
    knots = [rng.uniform(xn + 20, xf - 10) for _ in range(2)] if spec["nodes"] else []
    pts, rads = [], []
    x = xn
    while x <= xf + 1e-6:
        u = (x - xn) / L
        r = r0 * (0.9 + 0.1 * math.sin(math.pi * u))          # barrelled: thickest mid-shaft
        r *= 1.0 + sum(0.05 * math.exp(-((x - k) / 0.6) ** 2) for k in knots)
        if spec["head"] == "flint" and x > xf - 1.5:           # the split end tapers into the lashing
            r *= 1.0 - 0.25 * smoothstep(xf - 1.5, xf, x)
        if spec["head"] == "iron" and x > xf - 2.6:            # whittled to go up inside the socket (which narrows to 2.2 mm)
            r = min(r, r0 - (r0 - 0.16) * smoothstep(xf - 2.6, xf - 0.1, x))
        pts.append(Vector((x, 0.0, 0.0)))
        rads.append(r)
        x += 0.5 if (x < xn + 3 or x > xf - 3) else 1.0
    tube(shaft, pts, rads, sides=10, attrs=lambda i: {"ax": pts[i].x})
    parts.append(shaft)

    wrap_mat = mat_cord("M_%s_Sinew" % ITEM, "#c8a46c", "#7a5a30", plies=1, pitch=0.07, gloss=0.4)
    wraps = Part("wraps", wrap_mat)

    def lashing(x0, x1, r_start, r_end):
        n = max(4, int((x1 - x0) / 0.2))
        wp = [Vector((x0 + (x1 - x0) * i / n, 0.0, 0.0)) for i in range(n + 1)]
        tube(wraps, wp, [r_start + (r_end - r_start) * i / n + 0.04 * (1 if 0 < i < n else 0) for i in range(n + 1)], sides=12)

    lashing(xn + 1.0, xn + 1.8, r0 * 0.92 + 0.05, r0 * 0.92 + 0.05)              # behind the nock (keeps the slot from splitting)
    fl, fh = spec["fletch"]
    fx0 = xn + 2.8
    lashing(fx0 - 0.6, fx0 + 0.3, r0 * 0.95 + 0.05, r0 * 0.95 + 0.05)             # the feathers' back ends
    lashing(fx0 + fl - 0.3, fx0 + fl + 0.6, r0 * 0.97 + 0.05, r0 * 0.97 + 0.05)   # and their fronts
    parts.append(wraps)

    # the feathers: three vanes, a slight helix, the cock feather up
    fmat = mat_feather("M_%s_Feather" % ITEM, spec["feather"][0], spec["feather"][1], spec["feather"][2], spec["feather"][3])
    vanes = Part("vanes", fmat)
    for k in range(3):
        phi0 = math.radians(120.0 * k)        # the cock feather out along +Y: square to the nock slot
        splits = [rng.uniform(0.25, 0.9) for _ in range(3)]
        stations = 44
        rings, us, vs = [], [], []
        for i in range(stations + 1):
            s = i / stations
            x = fx0 + fl * s
            if s < 0.12:
                h = fh * (0.3 + 0.7 * smoothstep(0.0, 0.12, s))
            elif s < 0.5:
                h = fh
            else:
                h = fh * (1.0 - 0.9 * ((s - 0.5) / 0.5) ** 1.5)
            h *= 1.0 - sum(0.28 * math.exp(-((s - sp) / 0.015) ** 2) for sp in splits)
            phi = phi0 + math.radians(5.0) * (s - 0.5)          # the helix
            dr = Vector((0.0, math.cos(phi), math.sin(phi)))
            dt = Vector((0.0, -math.sin(phi), math.cos(phi)))
            rb = r0 * 0.95 - 0.02
            sec = [(rb, -0.06), (rb + 0.35 * h, -0.025), (rb + h, -0.012), (rb + h, 0.012), (rb + 0.35 * h, 0.025), (rb, 0.06)]
            ring = [vanes.add_vert(Vector((x, 0.0, 0.0)) + dr * rr + dt * tt, None, fx=x - fx0, fh=rr - rb) for rr, tt in sec]
            rings.append(ring)
            u = [0.0]
            for a_, b_ in zip(sec, sec[1:] + sec[:1]):
                u.append(u[-1] + math.hypot(b_[0] - a_[0], b_[1] - a_[1]))
            us.append(u)
            vs.append(x - fx0)
        loft(vanes, rings, us, vs, closed=True, cap_start=True, cap_end=True, island_len=6.0)
    parts.append(vanes)

    # the head
    if spec["head"] == "flint":
        hp = Part("point", mat_flint("M_%s_Flint" % ITEM))
        Lp = 4.4

        def hw(x):
            if x < 0.95:
                return 0.40 + 0.06 * x / 0.95
            if x < 1.25:
                return 0.46 - 0.14 * math.sin(math.pi * (x - 0.95) / 0.3)      # the side notches
            if x < 1.4:
                return 0.46 + 0.5 * smoothstep(1.25, 1.4, x)
            u = (x - 1.4) / (Lp - 1.4)
            return 0.96 * (1.0 - u) ** 0.85 + 0.01

        rnd = [rng.uniform(0, 6.28) for _ in range(4)]

        def ht(x, yn):
            core = 0.23 * (1.0 - min(1.0, abs(yn)) ** 2) ** 0.6
            core *= min(1.0, (Lp - x) / 1.3) ** 0.6 + 0.05
            core *= 1.0 + 0.12 * math.sin(7.0 * x + 5.0 * yn + rnd[0]) * math.sin(3.0 * x - 4.0 * yn + rnd[1])
            return core

        lens(hp, Lp, hw, ht, Vector((xf - 1.0, 0.0, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0)), nx=40, ny=8)
        parts.append(hp)
        lashing(xf - 1.6, xf + 0.3, r0 * 0.78 + 0.05, 0.42)       # the lashing over the split and the notches
        pitch = Part("pitch", mat_plain("M_%s_Pitch" % ITEM, "#1e140c", 0.22))
        pp = [Vector((xf + 0.1 + 0.15 * i, 0.0, 0.0)) for i in range(4)]
        tube(pitch, pp, [0.43, 0.4, 0.3, 0.12], sides=10)
        parts.append(pitch)
        head_len = Lp - 1.0
    else:
        ip = Part("iron", mat_iron("M_%s_Iron" % ITEM))
        sock = [Vector((xf - 2.4 + 0.2 * i, 0.0, 0.0)) for i in range(15)]
        tube(ip, sock, [r0 + 0.07 - (r0 + 0.07 - 0.22) * smoothstep(0.0, 1.0, i / 14.0) for i in range(15)], sides=12)
        Lb = 5.2

        def bw(x):
            u = x / Lb
            return 0.82 * math.sin(math.pi * min(1.0, u ** 0.75)) * (1.0 - 0.15 * u) + 0.2 * (1.0 - u) ** 4 + 0.01   # a short neck off the socket

        def bt(x, yn):
            return (0.16 * (1.0 - abs(yn)) ** 0.8 + 0.02) * min(1.0, (Lb - x) / 0.8 + 0.08)

        lens(ip, Lb, bw, bt, Vector((xf + 0.15, 0.0, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0)), nx=40, ny=8)
        parts.append(ip)
        head_len = Lb + 0.4

    def nock_cut(ob):
        """The self nock: a slot sawn upright into the shaft's back end, square to the cock feather (the string stands in it)."""
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(xn + 0.175, 0.0, 0.0))
        cut = bpy.context.active_object
        cut.scale = (1.35, 0.34, 3.0)          # 0.85 cm deep, 3.4 mm wide, vertical: the string stands in it
        cut.data.materials.append(ob.data.materials[0])    # the slot's inside is shaft wood (an empty slot would not bake)
        mod = ob.modifiers.new("Nock", "BOOLEAN")
        mod.object = cut
        mod.operation = "DIFFERENCE"
        mod.solver = "EXACT"
        mod.material_mode = "TRANSFER"
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.modifier_apply(modifier="Nock")
        bpy.data.objects.remove(cut, do_unlink=True)

    log("arrow %.0f cm shaft + %.1f cm head, %.1f cm feathers" % (L, head_len, fl))
    return parts, nock_cut


# ------------------------------------------------------------------------------------------------------------------------------------------
# Kits: the bow drill and the snare
# ------------------------------------------------------------------------------------------------------------------------------------------

def mat_stick(name, bark="#4e4236", wood="#c2a27a", peeled=0.25):
    """A cut branch: bark with lenticels and cracks, peeled to the pale wood in places and at the cut ends (attribute 'cut': 1 at an end)."""
    nb = NB(name)
    u, v, _ = nb.sep(nb.uv("proc"))
    q = nb.xyz(u, nb.m("MULTIPLY", v, 0.25), 0.0)
    cracks = nb.noise(nb.xyz(nb.m("MULTIPLY", u, 3.0), nb.m("MULTIPLY", v, 0.5), 0.0), 2.0, detail=6.0, rough=0.7)
    bark_col = nb.ramp(cracks, [(0.3, srgb("#2a221c")), (0.6, srgb(bark)), (0.8, srgb("#6e6254"))])
    lent = nb.step(nb.noise(nb.xyz(nb.m("MULTIPLY", u, 8.0), nb.m("MULTIPLY", v, 1.0), 3.0), 2.0, detail=2.0), 0.7, 0.02)
    bark_col = nb.mix(nb.m("MULTIPLY", lent, 0.5), bark_col, srgb("#9a8a70"))
    peel = nb.step(nb.noise(q, 0.8, detail=3.0), 1.0 - peeled, 0.03)
    peel = nb.m("MAXIMUM", peel, nb.attr("cut"))
    wood_col = nb.ramp(nb.noise(nb.xyz(nb.m("MULTIPLY", u, 6.0), nb.m("MULTIPLY", v, 0.3), 0.0), 2.0, detail=6.0),
                       [(0.3, srgb("#8e7050")), (0.7, srgb(wood))])
    col = nb.mix(peel, bark_col, wood_col)
    col = nb.mix(nb.m("MULTIPLY", nb.attr("char"), 0.95), col, srgb("#141010"))
    height = nb.mixf(peel, nb.m("MULTIPLY", cracks, 1.2), 0.2)
    return nb.finish(col, nb.mixf(peel, 0.82, 0.6), nb.bump(height, 0.5, 0.05))


def mat_board(name, sockets, notch_edge_y, light="#cdb48e", dark="#8e7452"):
    """A split fireboard of soft dry wood, grain along X; burnt sockets at sockets [(x, y, r)] on its top, each with a notch cut to the edge at
    notch_edge_y and a spill of black dust out of it."""
    nb = NB(name)
    tc = nb.new("ShaderNodeTexCoord")
    p = tc.outputs["Object"]
    x, y, z = nb.sep(p)
    grain = nb.wave(nb.vscale(p, 0.05, 1.0, 1.0), 3.0, distortion=4.0, detail=3.0, direction="Y")
    col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", grain, 0.6), nb.m("MULTIPLY", nb.noise(nb.vscale(p, 0.2, 3.0, 3.0), 1.0, detail=6.0), 0.4)),
                  [(0.3, srgb(dark)), (0.7, srgb(light))])
    burn, depth = 0.0, 0.0
    for sx, sy, sr in sockets:
        d = nb.new("ShaderNodeVectorMath", operation="DISTANCE")
        nb.feed(d.inputs[0], nb.xyz(x, y, 0.0))
        d.inputs[1].default_value = (sx, sy, 0.0)
        dist = d.outputs["Value"]
        inside = nb.m("SUBTRACT", 1.0, nb.step(dist, sr, 0.15))
        rim = nb.m("SUBTRACT", 1.0, nb.step(dist, sr + 0.5, 0.3))
        dx = nb.m("ABSOLUTE", nb.m("SUBTRACT", x, sx))
        toward = nb.m("MULTIPLY", nb.step(nb.m("MULTIPLY", nb.m("SUBTRACT", y, sy), -1.0 if notch_edge_y < sy else 1.0), 0.0, 0.05),
                      nb.m("SUBTRACT", 1.0, nb.step(dx, 0.32, 0.05)))
        burn = nb.m("MAXIMUM", burn, nb.m("MAXIMUM", inside, nb.m("MULTIPLY", rim, 0.55)))
        burn = nb.m("MAXIMUM", burn, toward)
        depth = nb.m("MAXIMUM", depth, nb.m("MAXIMUM", nb.m("MULTIPLY", inside, nb.m("SUBTRACT", 1.0, nb.m("DIVIDE", dist, sr))), toward))
    col = nb.mix(nb.m("MULTIPLY", burn, 0.95), col, srgb("#16100c"))
    height = nb.m("SUBTRACT", nb.m("MULTIPLY", grain, 0.15), nb.m("MULTIPLY", depth, 1.0))
    return nb.finish(col, nb.mixf(burn, 0.7, 0.9), nb.bump(height, 0.6, 0.15))


def rounded_box(part, size, centre, sides=20, attrs=None):
    """A block (sx along X, sy, sz) with rounded edges: a superellipse section swept along X, capped."""
    sx, sy, sz = size
    n = max(2, int(sx / 1.0))
    pts = [Vector((centre[0] - sx / 2 + sx * i / n, centre[1], centre[2])) for i in range(n + 1)]
    rings, us, vs = [], [], []
    for i, p in enumerate(pts):
        ring = []
        for j in range(sides):
            a = 2 * math.pi * j / sides
            c, s = math.cos(a), math.sin(a)
            yy = 0.5 * sy * math.copysign(abs(c) ** 0.35, c)
            zz = 0.5 * sz * math.copysign(abs(s) ** 0.35, s)
            ring.append(part.add_vert(p + Vector((0.0, yy, zz)), None, **(attrs or {})))
        rings.append(ring)
        us.append([(sy + sz) * 2.0 * j / sides for j in range(sides + 1)])
        vs.append(p.x - pts[0].x)
    loft(part, rings, us, vs, closed=True, cap_start=True, cap_end=True)


def build_bowdrill():
    """The kit as it lies: the fireboard with three burnt sockets, the spindle laid across the bow's cord with one turn round it, the bow (a
    curved branch, bark on), and the bearing block."""
    parts = []
    # the bow: a branch bent in an arc, tips at y = 12, its belly toward -y
    stick = Part("bow", mat_stick("M_%s_Bow" % ITEM, peeled=0.15))
    n, half = 40, 31.0
    pts = [Vector((-half + 2 * half * i / n, 12.0 + 6.5 * math.sin(math.pi * i / n), 1.1)) for i in range(n + 1)]
    tube(stick, pts, [1.15 - 0.35 * abs(2.0 * i / n - 1.0) for i in range(n + 1)], sides=10,
         attrs=lambda i: {"cut": 1.0 if i in (0, n) else 0.0, "char": 0.0})
    parts.append(stick)
    # the spindle across the cord at x = 0, its axis along y
    spin = Part("spindle", mat_stick("M_%s_Spindle" % ITEM, bark="#b09878", wood="#d6bc94", peeled=1.0))
    sl = 21.0
    sp = [Vector((0.0, 12.0 - sl / 2 + sl * i / 30, 1.0)) for i in range(31)]
    tube(spin, sp, [0.95 * (0.35 + 0.65 * smoothstep(0, 0.12, i / 30.0)) * (0.55 + 0.45 * smoothstep(0, 0.1, 1 - i / 30.0)) for i in range(31)],
         sides=12, attrs=lambda i: {"cut": 1.0, "char": 1.0 if i <= 2 else 0.0})
    parts.append(spin)
    # the cord: tip to the spindle, one turn round it, on to the other tip
    cord = Part("cord", mat_cord("M_%s_Cord" % ITEM, "#a39577", "#5f5644", plies=2, pitch=0.35))
    A, B = pts[0] + Vector((0.8, 0.0, 0.0)), pts[-1] - Vector((0.8, 0.0, 0.0))
    rc, ys = 0.95 + 0.14, 12.0 - 0.6
    path = [A]
    for i in range(1, 9):
        path.append(A.lerp(Vector((-rc, ys, 1.0)), i / 9.0))
    for i in range(0, 41):
        th = math.pi + 2.0 * math.pi * i / 40.0      # from the near side, over the top and round
        path.append(Vector((rc * math.cos(th), ys + 1.2 * i / 40.0, 1.0 - rc * math.sin(th))))
    end = path[-1]
    for i in range(1, 10):
        path.append(end.lerp(B, i / 9.0))
    tube(cord, path, 0.14, sides=6)
    parts.append(cord)
    # the fireboard and the bearing block
    sockets = [(-9.0, -4.5, 1.1), (-3.0, -4.5, 1.1), (3.0, -4.5, 1.1)]
    board = Part("board", mat_board("M_%s_Board" % ITEM, sockets, -6.5))
    rounded_box(board, (30.0, 5.0, 1.6), (0.0, -4.0, 0.8))
    parts.append(board)
    block = Part("block", mat_stick("M_%s_Block" % ITEM, bark="#5a4a3a", wood="#9a7a56", peeled=0.85))
    rounded_box(block, (7.5, 5.5, 3.2), (22.0, -4.0, 1.6), attrs={"cut": 1.0, "char": 0.0})
    parts.append(block)
    return parts, None


def build_snare():
    """A noose snare as it is carried: the anchor stake (sharpened, notched, bark on) and the cord tied round its head, run out to a noose
    of ~11 cm with its slip knot."""
    parts = []
    stake = Part("stake", mat_stick("M_%s_Stake" % ITEM, peeled=0.1))
    n = 30
    sl = 28.0
    pts = [Vector((-sl / 2 + sl * i / n, 0.0, 1.2)) for i in range(n + 1)]
    tube(stake, pts, [1.2 * min(1.0, 0.15 + 0.85 * (i / n) / 0.22) for i in range(n + 1)], sides=10,
         attrs=lambda i: {"cut": 1.0 if i < 7 or i == n else 0.0, "char": 0.0})
    parts.append(stake)
    cord = Part("cord", mat_cord("M_%s_Cord" % ITEM, "#a8946c", "#5a4a32", plies=2, pitch=0.3))
    # the tie round the stake head: three turns
    xh = sl / 2 - 3.0
    turns = [Vector((xh + 0.35 * t / 12.0, 1.33 * math.cos(2 * math.pi * t / 12.0), 1.2 + 1.33 * math.sin(2 * math.pi * t / 12.0)))
             for t in range(37)]
    run = [turns[-1].lerp(Vector((xh + 5.0, 6.0, 0.2)), i / 8.0) for i in range(1, 9)]
    cx, cy, R = xh + 5.0 + 0.0, 6.0 + 5.5, 5.5        # the noose, lying on the ground beyond the stake
    loop = [Vector((cx + R * math.sin(2 * math.pi * i / 40.0), cy - R * math.cos(2 * math.pi * i / 40.0), 0.2 + 0.05 * math.sin(6 * i / 40.0)))
            for i in range(41)]
    tube(cord, turns + run + loop, 0.12, sides=6)
    knot = Part("knot", cord.material)
    kp = [Vector((cx - 0.5 + 0.1 * i, cy - R, 0.25)) for i in range(11)]
    tube(knot, kp, [0.12 + 0.2 * math.sin(math.pi * i / 10.0) for i in range(11)], sides=8)
    parts.append(cord)
    parts.append(knot)
    return parts, None


# The wooden spears (Nov 2, N-260: the kitbash SM_WoodenSpear they shared was a trench stick 7-8 cm thick as held -- no hand closes on that).
# A peeled sapling, sharpened: a real one (Schoningen's are 2.3 m, 3-5 cm at their thickest) is thickest about a third of the way back from
# the point, so it flies and balances point-forward, and tapers to the butt; the point is carved over the last ~22 cm. A fire-hardened
# spear's point was held in the embers: charred black at the tip, browning off over ~30 cm.
SPEARS = {
    "WoodenSpear01": {"length": 200.0, "butt_r": 1.45, "max_r": 1.65, "max_at": 0.62, "point": 22.0, "char": 0.0, "seed": 31},
    "FireHardenedSpear01": {"length": 200.0, "butt_r": 1.45, "max_r": 1.65, "max_at": 0.62, "point": 24.0, "char": 34.0, "seed": 57},
}


def build_spear(spec):
    """The spear along +X, the point at +X, its middle at the origin: a slightly wandering sapling, peeled, its point carved (and charred)."""
    rnd = random.Random(spec["seed"])
    L, point, char_len = spec["length"], spec["point"], spec["char"]
    ph = [rnd.uniform(0.0, 2.0 * math.pi) for _ in range(4)]
    n = int(L / 1.0)
    pts, radii, attrs = [], [], []
    for i in range(n + 1):
        t = i / n                       # 0 butt .. 1 tip
        x = -L / 2 + L * t
        from_tip = L * (1.0 - t)
        # the sapling's wander, a few millimetres
        y = 0.45 * math.sin(2 * math.pi * 1.2 * t + ph[0]) + 0.2 * math.sin(2 * math.pi * 3.3 * t + ph[1])
        z = 0.35 * math.sin(2 * math.pi * 0.9 * t + ph[2]) + 0.15 * math.sin(2 * math.pi * 4.1 * t + ph[3])
        # thickest a third back from the point, tapering to the butt
        if t <= spec["max_at"]:
            r = spec["butt_r"] + (spec["max_r"] - spec["butt_r"]) * math.sin(0.5 * math.pi * t / spec["max_at"])
        else:
            r = spec["max_r"] - 0.1 * (t - spec["max_at"]) / (1.0 - spec["max_at"])
        cut = 0.0
        if from_tip < point:            # the carved point: a hollow taper to a near-sharp tip
            u = 1.0 - from_tip / point
            r = max(0.06, r * (1.0 - u) ** 1.25)
            cut = 1.0
        if i == 0:
            cut = 1.0                   # the cut butt
        r *= 1.0 + 0.03 * math.sin(7.0 * t * math.pi * 2 + ph[1])
        ch = 0.0
        if char_len > 0.0:
            ch = 1.0 - smoothstep(14.0, char_len, from_tip)   # black over the last 14 cm, browning off to char_len
        pts.append(Vector((x, y, z)))
        radii.append(r)
        attrs.append({"cut": cut, "char": ch})
    shaft = Part("shaft", mat_stick("M_%s_Shaft" % ITEM, bark="#5a4836", wood="#c4a47a", peeled=0.4))   # peeled, bark left in patches
    tube(shaft, pts, radii, sides=12, attrs=lambda i: attrs[i], island_len=40.0)
    return [shaft], None


def run_spear():
    spec = SPEARS[ITEM]
    tip = spec["length"] / 2
    run_static(lambda: build_spear(spec), [
        ("whole", (0, 0, 0), (0.3, -1.0, 0.6), 210.0), ("side", (0, 0, 0), (0.0, -1.0, 0.0), 210.0), ("top", (0, 0, 0), (0.0, -0.05, 1.0), 210.0),
        ("point", (tip - 14, 0, 0), (0.4, -1.0, 0.6), 34.0), ("grip", (-tip + 60, 0, 0), (0.3, -1.0, 0.7), 16.0),
        ("butt", (-tip + 2, 0, 0), (-0.6, -1.0, 0.5), 8.0)])


def run_bowdrill():
    run_static(build_bowdrill, [("whole", (0, 4, 0), (0.6, -1.0, 1.1), 70.0), ("top", (0, 4, 0), (0.0, -0.08, 1.0), 70.0),
                                ("side", (0, 4, 0), (0.0, -1.0, 0.25), 70.0), ("wrap", (0, 12, 1), (0.8, -1.0, 0.9), 9.0),
                                ("sockets", (-3, -4.5, 1.6), (0.3, -0.7, 1.0), 16.0), ("block", (22, -4, 1.6), (0.7, -1.0, 0.9), 12.0)])


def run_snare():
    run_static(build_snare, [("whole", (4, 6, 0), (0.6, -1.0, 1.1), 44.0), ("top", (4, 6, 0), (0.0, -0.08, 1.0), 44.0),
                             ("side", (4, 6, 0), (0.0, -1.0, 0.3), 44.0), ("tie", (11, 0, 1.2), (0.8, -1.0, 0.9), 7.0),
                             ("knot", (16, 6, 0.2), (0.5, -1.0, 1.0), 6.0), ("point", (-13, 0, 1.2), (0.8, -1.0, 0.9), 8.0)])


KITS = {"BowDrill01": run_bowdrill, "Snare01": run_snare, "WoodenSpear01": run_spear, "FireHardenedSpear01": run_spear}


# ------------------------------------------------------------------------------------------------------------------------------------------
# Bake, preview, export
# ------------------------------------------------------------------------------------------------------------------------------------------

def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.01
    sc.unit_settings.length_unit = "CENTIMETERS"
    return sc


def new_image(name, res, color=(0, 0, 0, 1), non_color=False):
    img = bpy.data.images.new(name, res, res, alpha=False)
    img.generated_color = color
    if non_color:
        img.colorspace_settings.name = "Non-Color"
    return img


def bake(ob, res, hide_for_ao=()):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 24
    sc.render.bake.margin = 8
    sc.render.bake.use_clear = True
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    mats = [s.material for s in ob.material_slots]
    imgs = {"color": new_image("bake_color", res), "normal": new_image("bake_normal", res, (0.5, 0.5, 1.0, 1.0), True),
            "rough": new_image("bake_rough", res, (0.6, 0.6, 0.6, 1), True), "ao": new_image("bake_ao", res, (1, 1, 1, 1), True),
            "metal": new_image("bake_metal", res, (0, 0, 0, 1), True)}
    img_nodes = {}
    for m in mats:
        n = m.node_tree.nodes.new("ShaderNodeTexImage")
        img_nodes[m.name] = n

    def target(key):
        for m in mats:
            n = img_nodes[m.name]
            n.image = imgs[key]
            m.node_tree.nodes.active = n

    target("color")
    bpy.ops.object.bake(type="DIFFUSE", pass_filter={"COLOR"}, margin=8, use_clear=True)
    target("normal")
    sc.render.bake.normal_space = "TANGENT"
    sc.render.bake.normal_r, sc.render.bake.normal_g, sc.render.bake.normal_b = "POS_X", "NEG_Y", "POS_Z"   # DirectX for Unreal
    bpy.ops.object.bake(type="NORMAL", margin=8, use_clear=True)
    target("rough")
    bpy.ops.object.bake(type="ROUGHNESS", margin=8, use_clear=True)
    target("ao")
    sc.cycles.samples = 64
    bpy.ops.object.bake(type="AO", margin=8, use_clear=True)
    # metallic through emission: whatever feeds each material's Metallic (a socket or its value) lights the emission for one bake
    def metal_in(m):
        return m.node_tree.nodes["Principled BSDF"].inputs["Metallic"]
    if any(metal_in(m).is_linked or metal_in(m).default_value > 0.0 for m in mats):
        target("metal")
        saved = []
        for m in mats:
            nt = m.node_tree
            em = nt.nodes.new("ShaderNodeEmission")
            mi = metal_in(m)
            if mi.is_linked:
                nt.links.new(mi.links[0].from_socket, em.inputs["Color"])
            else:
                mv = mi.default_value
                em.inputs["Color"].default_value = (mv, mv, mv, 1.0)
            out = nt.nodes["Material Output"]
            old = out.inputs["Surface"].links[0].from_socket if out.inputs["Surface"].links else None
            nt.links.new(em.outputs[0], out.inputs["Surface"])
            saved.append((nt, out, old, em))
        bpy.ops.object.bake(type="EMIT", margin=8, use_clear=True)
        for nt, out, old, em in saved:
            if old:
                nt.links.new(old, out.inputs["Surface"])
            nt.nodes.remove(em)
    files = {}
    for key, fname in (("color", "BaseColor"), ("normal", "Normal")):
        path = os.path.join(OUT, "T_%s_%s.png" % (ITEM, fname))
        imgs[key].filepath_raw = path
        imgs[key].file_format = "PNG"
        imgs[key].save()
        files[key] = path
    # ORM: R occlusion, G roughness, B metallic
    n = res * res * 4
    ao, ro, me = np.empty(n, np.float32), np.empty(n, np.float32), np.empty(n, np.float32)
    imgs["ao"].pixels.foreach_get(ao)
    imgs["rough"].pixels.foreach_get(ro)
    imgs["metal"].pixels.foreach_get(me)
    orm = np.empty(n, np.float32)
    orm[0::4], orm[1::4], orm[2::4], orm[3::4] = ao[0::4], ro[0::4], me[0::4], 1.0
    oimg = new_image("bake_orm", res, non_color=True)
    oimg.pixels.foreach_set(orm)
    path = os.path.join(OUT, "T_%s_ORM.png" % ITEM)
    oimg.filepath_raw = path
    oimg.file_format = "PNG"
    oimg.save()
    files["orm"] = path
    for m in mats:
        m.node_tree.nodes.remove(img_nodes[m.name])
    log("baked %s" % ", ".join(os.path.basename(p) for p in files.values()))
    return files


def final_material(ob, files):
    """Replace the procedural slots with ONE material on the baked textures (what Unreal gets: one slot, M_<ItemId>)."""
    nb = NB("M_%s" % ITEM)
    col = nb.new("ShaderNodeTexImage")
    col.image = bpy.data.images.load(files["color"]) if files else None
    if files:
        nrm = nb.new("ShaderNodeTexImage")
        nrm.image = bpy.data.images.load(files["normal"])
        nrm.image.colorspace_settings.name = "Non-Color"
        orm = nb.new("ShaderNodeTexImage")
        orm.image = bpy.data.images.load(files["orm"])
        orm.image.colorspace_settings.name = "Non-Color"
        sepc = nb.new("ShaderNodeSeparateColor")
        nb.links.new(nrm.outputs["Color"], sepc.inputs[0])
        comb = nb.new("ShaderNodeCombineColor")
        nb.links.new(sepc.outputs[0], comb.inputs[0])
        nb.links.new(nb.m("SUBTRACT", 1.0, sepc.outputs[1]), comb.inputs[1])   # DirectX -> OpenGL for Blender's own preview
        nb.links.new(sepc.outputs[2], comb.inputs[2])
        nm = nb.new("ShaderNodeNormalMap")
        nb.links.new(comb.outputs[0], nm.inputs["Color"])
        so = nb.new("ShaderNodeSeparateColor")
        nb.links.new(orm.outputs["Color"], so.inputs[0])
        nb.links.new(col.outputs["Color"], nb.bsdf.inputs["Base Color"])
        nb.links.new(so.outputs[1], nb.bsdf.inputs["Roughness"])
        nb.links.new(so.outputs[2], nb.bsdf.inputs["Metallic"])
        nb.links.new(nm.outputs["Normal"], nb.bsdf.inputs["Normal"])
    ob.data.materials.clear()
    ob.data.materials.append(nb.mat)
    ob.data.polygons.foreach_set("material_index", [0] * len(ob.data.polygons))
    return nb.mat


def preview_rig():
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    try:
        sc.eevee.taa_render_samples = 48
    except AttributeError:
        pass
    world = bpy.data.worlds.new("Preview")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.42, 0.44, 0.47, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.9
    sc.world = world
    for name, energy, rot in (("Key", 4.0, (50, 0, 35)), ("Fill", 1.2, (60, 0, -140))):
        lt = bpy.data.objects.new(name, bpy.data.lights.new(name, "SUN"))
        lt.data.energy = energy
        lt.rotation_euler = tuple(math.radians(a) for a in rot)
        sc.collection.objects.link(lt)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    sc.view_settings.view_transform = "Standard"
    return cam


def shoot(cam, target, direction, size_cm, path, res=(640, 960), lens=None, ortho=True):
    sc = bpy.context.scene
    sc.render.resolution_x, sc.render.resolution_y = res
    d = Vector(direction).normalized()
    cam.data.type = "ORTHO" if ortho else "PERSP"
    cam.data.ortho_scale = size_cm
    cam.location = Vector(target) + d * 500.0
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_start, cam.data.clip_end = 1.0, 2000.0
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return path


def sheet(paths, out_path, cols):
    imgs = [bpy.data.images.load(p) for p in paths]
    w, h = imgs[0].size
    rows = (len(imgs) + cols - 1) // cols
    canvas = np.zeros((rows * h, cols * w, 4), np.float32)
    for i, img in enumerate(imgs):
        px = np.empty(w * h * 4, np.float32)
        img.pixels.foreach_get(px)
        r, c = i // cols, i % cols
        canvas[(rows - 1 - r) * h:(rows - r) * h, c * w:(c + 1) * w] = px.reshape(h, w, 4)
    out = bpy.data.images.new("sheet", cols * w, rows * h, alpha=False)
    out.pixels.foreach_set(canvas.ravel())
    out.filepath_raw = out_path
    out.file_format = "PNG"
    out.save()
    for p in paths:
        try:
            os.remove(p)
        except OSError:
            pass
    return out_path


def export_fbx(path, objects, anim):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", use_tspace=False, add_leaf_bones=False,
                             use_armature_deform_only=False, armature_nodetype="NULL", bake_anim=anim, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, bake_anim_step=1.0,
                             bake_anim_simplify_factor=0.0, use_mesh_modifiers=False)
    log("wrote %s" % os.path.basename(path))


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def strip_build_data(ob):
    """The procedural UV and the shader attributes only served the bake."""
    ob.data.uv_layers.remove(ob.data.uv_layers["proc"])
    for k in list(ob.get("mo_attrs", [])):
        if k in ob.data.attributes:
            ob.data.attributes.remove(ob.data.attributes[k])


def run_static(build, views):
    """A static item: build -> one object -> bake -> export -> previews. views: [(label, target, direction, size_cm)]; the last two are
    close-ups."""
    res = RES or 1024
    setup_scene()
    parts, post = build()
    ob = build_object(ITEM, parts, res)
    if post:
        post(ob)
    files = bake(ob, res) if BAKE else {}
    final_material(ob, files)
    strip_build_data(ob)
    export_fbx(os.path.join(OUT, "%s.fbx" % ITEM), [ob], False)
    cam = preview_rig()
    shots = [shoot(cam, t, d, s, os.path.join(OUT, "_v%d.png" % i), res=(960, 540)) for i, (label, t, d, s) in enumerate(views)]
    preview = sheet(shots, os.path.join(OUT, "%s_preview.png" % ITEM), 3)
    bbox = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    size = [max(c[i] for c in bbox) - min(c[i] for c in bbox) for i in range(3)]
    info = {"item": ITEM, "kind": "static", "size_cm": [round(v, 2) for v in size], "triangles": tri_count(ob), "textures": files,
            "preview": preview}
    with open(os.path.join(OUT, "%s.json" % ITEM), "w") as f:
        json.dump(info, f, indent=1)
    log("done: %s, %d triangles, size %s cm" % (preview, info["triangles"], info["size_cm"]))


def run_arrow():
    spec = ARROWS[ITEM]
    L = spec["length"]
    back, front = -L / 2.0, L / 2.0
    run_static(lambda: build_arrow(spec), [
        ("whole", (0, 0, 0), (0.25, -1.0, 0.6), L * 1.12), ("side", (0, 0, 0), (0.0, -1.0, 0.0), L * 1.12),
        ("top", (0, 0, 0), (0.0, -0.05, 1.0), L * 1.12), ("head", (front + 1.5, 0, 0), (0.5, -1.0, 0.7), 9.0),
        ("fletching", (back + 8.0, 0, 0), (0.3, -1.0, 0.8), 18.0), ("nock", (back + 1.0, 0, 0), (-1.0, -0.6, 0.5), 5.0)])


def run_bow():
    spec = BOWS[ITEM]
    res = RES or 2048
    setup_scene()
    sol, parts, brace, edges = build_bow(spec)
    ob = build_object(ITEM, parts, res)
    files = bake(ob, res) if BAKE else {}
    final_material(ob, files)
    strip_build_data(ob)
    # the static (braced) copy: the world item and the icon
    st = ob.copy()
    st.data = ob.data.copy()
    st.name = ITEM + "_static"
    st.vertex_groups.clear()
    bpy.context.scene.collection.objects.link(st)
    export_fbx(os.path.join(OUT, "%s_static.fbx" % ITEM), [st], False)
    # the rig
    arm_ob, order = make_armature(sol, edges)
    ob.parent = arm_ob
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_ob
    report = key_draw(arm_ob, sol, edges, order)
    bpy.context.scene.frame_set(0)
    export_fbx(os.path.join(OUT, "%s.fbx" % ITEM), [arm_ob, ob], True)

    # previews: braced from three sides (the static copy), then the draw from the side (the rigged one)
    cam = preview_rig()
    st.hide_render = False
    ob.hide_render = True
    L = spec["length"]
    shots = [shoot(cam, (0, -8, 0), (1.0, -0.6, 0.25), L * 1.08, os.path.join(OUT, "_p0.png")),
             shoot(cam, (0, -8, 0), (1.0, 0.0, 0.0), L * 1.08, os.path.join(OUT, "_p1.png")),
             shoot(cam, (0, -8, 0), (0.0, -1.0, 0.0), L * 1.08, os.path.join(OUT, "_p2.png"))]
    st.hide_render = True
    ob.hide_render = False
    for fr in (0, int(round(-DRAW_MIN / (1.0 - DRAW_MIN) * DRAW_FRAMES)), DRAW_FRAMES // 2, DRAW_FRAMES):
        bpy.context.scene.frame_set(fr)
        shots.append(shoot(cam, (0, -30, 0), (1.0, 0.0, 0.0), L * 1.08, os.path.join(OUT, "_d%03d.png" % fr)))
    # close-ups: the grip and the top nock
    bpy.context.scene.frame_set(int(round(-DRAW_MIN / (1.0 - DRAW_MIN) * DRAW_FRAMES)))
    y, z, _ = sol.at(brace, sol.s_nock)
    shots.append(shoot(cam, (0, -2, 0), (0.8, -0.8, 0.35), 24.0, os.path.join(OUT, "_c0.png")))
    shots.append(shoot(cam, (0, y, z), (0.8, -0.8, 0.35), 12.0, os.path.join(OUT, "_c1.png")))
    preview = sheet(shots, os.path.join(OUT, "%s_preview.png" % ITEM), 9)
    bbox = [ob.matrix_world @ Vector(c) for c in st.bound_box]
    size = [max(c[i] for c in bbox) - min(c[i] for c in bbox) for i in range(3)]
    info = {"item": ITEM, "kind": "bow", "size_cm": [round(v, 1) for v in size], "triangles": tri_count(st),
            "length_cm": spec["length"], "string_cm": round(sol.S, 2), "brace_cm": BRACE, "full_draw_cm": FULL_DRAW,
            "draw_min_fraction": DRAW_MIN, "nock_z_cm": NOCK_Z, "fps": FPS, "draw_frames": DRAW_FRAMES,
            "grip_cm": grip_half_size(spec), "bones": order, "draw_report": report, "textures": files, "preview": preview}
    with open(os.path.join(OUT, "%s.json" % ITEM), "w") as f:
        json.dump(info, f, indent=1)
    log("done: %s, %d triangles, size %s cm" % (preview, info["triangles"], info["size_cm"]))


if __name__ == "__main__":
    if ITEM in BOWS:
        run_bow()
    elif ITEM in KITS:
        KITS[ITEM]()
    elif ITEM in ARROWS:
        run_arrow()
    else:
        log("unknown item")
        sys.exit(2)
