"""Blender (headless): MO57's building pieces built procedurally, part by part in build order (Nov 2 2026; Wes: "let's flesh out a lot of the
building system and remember to have the assets 'pop in' as building goes on so it looks like it's actually finishing").

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Tools/blender_buildings_blender.py -- <Piece|surfaces> <out_dir> [--res N]

Driven by Tools/blender_buildings.py. Two jobs:

  surfaces   bakes every tiling surface in Tools/building_pieces.py SURFACES into <out_dir>/T_Surf_<Name>_BaseColor / _Normal / _ORM.png
             (2048 px, seamless: every pattern is 4D noise sampled on a torus, so u and v wrap) and Surfaces_preview.png.
  <Piece>    builds the piece's parts into <out_dir>/<Piece>/: <Piece>_<NN>_<Part>.fbx (one static mesh per part, in build order, every part's
             pivot at the piece's origin), <Piece>.json (the parts and their surfaces, triangles, the size) and <Piece>_preview.png (the
             finished piece and three stages of its build, on the baked surfaces).

Coordinates: every builder works in Unreal's frame (X forward, Y right, Z up, cm); the mesh is mirrored into Blender's (Y negated, winding
reversed) as it is made, and the FBX export carries it back, so a vertex at Unreal (x, y, z) imports at (x, y, z). The frames the pieces share
(a wall's panel at X -50, a roof's eave at X 0 Z -50 ...) are AMOBuildPieceActor's (MOBuildPieceActor.h).

UVs are world-scaled: one texture repeat covers the surface's tile_cm, so a wall and a tent read at the same texel density. A material slot is
named Surf_<Name>; the editor gives it MI_Surf_<Name>.

Natural variation (Nov 3; Wes: "give all the assets you made in blender some natural variation so they're not so uniform"): every primitive
(a stick, a timber, a lump, a sheet) draws its own look from the piece's one seeded source (Vary): where the tiling texture starts on it, how
light and how weathered it is (a second UV channel, "Tint", that M_Surf_Base reads), its seed for the noise that shapes it. A straight-ish
stick also bows a little, swells and narrows along its length and is cut on a slant; a timber bows, twists and is not quite square. The
builders lay things out unevenly on top of that (stakes spaced by eye, courses packed by the rods' own thickness, thatch in bundles).

Stones are not built here: a cobble piece's mortar and its flat top are; the editor lays the scanned stones (import_blender_buildings.py).
"""
import json
import math
import os
import random
import sys
import zlib

import bpy
import numpy as np
from mathutils import Matrix, Vector, noise

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import building_pieces as BP   # noqa: E402
import blender_items_blender as BI   # noqa: E402  (NB, srgb, smoothstep, setup_scene, new_image, preview_rig, shoot, sheet)

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
JOB = ARGS[0] if ARGS else "WattleWall"
OUT = os.path.abspath(ARGS[1] if len(ARGS) > 1 else "blender_buildings_out")
RES = int(ARGS[ARGS.index("--res") + 1]) if "--res" in ARGS else 2048
SURF_DIR = OUT if JOB == "surfaces" else os.path.join(os.path.dirname(OUT), "Surfaces")
TAU = math.pi * 2.0


def log(msg):
    print("BLENDERBUILD %s: %s" % (JOB, msg), flush=True)


def tile(surf):
    return BP.SURFACES[surf]["tile_cm"]


# ------------------------------------------------------------------------------------------------------------------------------------------
# Tiling surfaces: 4D noise on a torus (u and v both wrap)
# ------------------------------------------------------------------------------------------------------------------------------------------

def torus(nb, nu, nv, du=0.0, dv=0.0):
    """(vector, w) on a torus whose circles carry nu features across u and nv along v: a texture that repeats exactly over UV 0..1."""
    u, v, _ = nb.sep(nb.uv("UVMap"))
    a = nb.m("MULTIPLY", nb.m("ADD", u, du), TAU)
    b = nb.m("MULTIPLY", nb.m("ADD", v, dv), TAU)
    ru, rv = nu / TAU, nv / TAU
    vec = nb.xyz(nb.m("MULTIPLY", nb.m("COSINE", a), ru), nb.m("MULTIPLY", nb.m("SINE", a), ru), nb.m("MULTIPLY", nb.m("COSINE", b), rv))
    return vec, nb.m("MULTIPLY", nb.m("SINE", b), rv)


def noise4(nb, nu, nv, detail=4.0, rough=0.5, distortion=0.0, seed=0.0):
    vec, w = torus(nb, nu, nv, seed * 0.37, seed * 0.61)
    n = nb.new("ShaderNodeTexNoise")
    n.noise_dimensions = "4D"
    nb.feed(n.inputs["Vector"], vec)
    nb.feed(n.inputs["W"], w)
    n.inputs["Scale"].default_value = 1.0
    n.inputs["Detail"].default_value = detail
    n.inputs["Roughness"].default_value = rough
    n.inputs["Distortion"].default_value = distortion
    return n.outputs["Fac"]


def voronoi4(nb, nu, nv, feature="F1", out="Distance", randomness=1.0, seed=0.0):
    vec, w = torus(nb, nu, nv, seed * 0.29, seed * 0.53)
    n = nb.new("ShaderNodeTexVoronoi", feature=feature)
    n.voronoi_dimensions = "4D"
    nb.feed(n.inputs["Vector"], vec)
    nb.feed(n.inputs["W"], w)
    n.inputs["Scale"].default_value = 1.0
    n.inputs["Randomness"].default_value = randomness
    return n.outputs[out]


def stripes(nb, ku, kv):
    """sin(2 pi (ku u + kv v)) mapped to 0..1: periodic over the tile for whole ku, kv (a cord's plies)."""
    u, v, _ = nb.sep(nb.uv("UVMap"))
    ph = nb.m("ADD", nb.m("MULTIPLY", u, ku * TAU), nb.m("MULTIPLY", v, kv * TAU))
    return nb.m("MULTIPLY", nb.m("ADD", nb.m("SINE", ph), 1.0), 0.5)


def surface_material(name):
    """The tiling surface's node tree: (material, height socket). Colours are sRGB hex."""
    nb = BI.NB("Surf_" + name)
    c = BI.srgb
    if name == "Bark":
        fis = voronoi4(nb, 26, 3, "DISTANCE_TO_EDGE", seed=1)
        mott = noise4(nb, 6, 6, 6.0, 0.6, seed=2)
        fine = noise4(nb, 80, 30, 4.0, 0.6, seed=3)
        lent = nb.step(voronoi4(nb, 26, 110, seed=4), 0.12, 0.03)
        lent = nb.m("MULTIPLY", nb.m("SUBTRACT", 1.0, lent), nb.step(noise4(nb, 5, 9, 2.0, seed=5), 0.5, 0.08))
        lichen = nb.step(noise4(nb, 3, 3, 5.0, 0.6, seed=6), 0.66, 0.04)
        crack = nb.m("MULTIPLY", nb.m("SUBTRACT", 1.0, nb.step(fis, 0.025, 0.015)), nb.step(noise4(nb, 4, 10, 3.0, seed=7), 0.45, 0.1))
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", mott, 0.7), nb.m("MULTIPLY", fine, 0.3)), [(0.3, c("#4a4038")), (0.7, c("#73685b"))])
        col = nb.mix(nb.m("MULTIPLY", crack, 0.5), col, c("#2a221b"))
        col = nb.mix(nb.m("MULTIPLY", lent, 0.7), col, c("#a39478"))
        col = nb.mix(nb.m("MULTIPLY", lichen, 0.55), col, c("#8d9479"))
        h = nb.m("ADD", nb.m("MULTIPLY", crack, -0.6), nb.m("MULTIPLY", fine, 0.5))
        h = nb.m("ADD", h, nb.m("MULTIPLY", lent, 0.15))
        rough = nb.m("ADD", 0.72, nb.m("MULTIPLY", crack, 0.2))
    elif name == "Rod":
        streak = noise4(nb, 60, 3, 4.0, 0.5, seed=1)
        mott = noise4(nb, 4, 8, 4.0, 0.5, seed=2)
        dots = nb.m("SUBTRACT", 1.0, nb.step(voronoi4(nb, 18, 40, seed=3), 0.1, 0.03))
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", mott, 0.7), nb.m("MULTIPLY", streak, 0.3)), [(0.3, c("#3e2418")), (0.75, c("#7a4a30"))])
        col = nb.mix(nb.m("MULTIPLY", dots, 0.8), col, c("#c4ab84"))
        col = nb.mix(nb.m("MULTIPLY", nb.step(noise4(nb, 3, 5, 3.0, seed=4), 0.62, 0.05), 0.5), col, c("#6f6a5c"))   # grey bloom
        h = nb.m("ADD", nb.m("MULTIPLY", streak, 0.25), nb.m("MULTIPLY", dots, 0.2))
        rough = nb.m("ADD", 0.48, nb.m("MULTIPLY", mott, 0.15))
    elif name in ("Hewn", "Plank"):
        grain = noise4(nb, 90, 3, 6.0, 0.55, distortion=0.6, seed=1)
        rays = nb.step(noise4(nb, 140, 12, 2.0, seed=2), 0.68, 0.03)
        facet = voronoi4(nb, 3, 9, "F1", seed=3) if name == "Hewn" else voronoi4(nb, 1, 2, "F1", seed=3)
        weather = noise4(nb, 4, 4, 5.0, 0.6, seed=4)
        light, dark = (("#9b7a55", "#5a4129") if name == "Hewn" else ("#a88a64", "#6a5038"))
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", grain, 0.75), nb.m("MULTIPLY", weather, 0.25)), [(0.3, c(dark)), (0.7, c(light))])
        col = nb.mix(nb.m("MULTIPLY", rays, 0.4), col, c("#c9ae86"))
        col = nb.mix(nb.m("MULTIPLY", nb.step(weather, 0.6, 0.08), 0.45), col, c("#8a8478"))
        h = nb.m("ADD", nb.m("MULTIPLY", facet, 1.0 if name == "Hewn" else 0.3), nb.m("MULTIPLY", grain, 0.35))
        rough = nb.m("ADD", 0.62, nb.m("MULTIPLY", weather, 0.2))
    elif name == "Moss":
        clump = voronoi4(nb, 11, 11, "F1", seed=1)
        fine = noise4(nb, 70, 70, 5.0, 0.7, seed=2)
        hue = noise4(nb, 5, 5, 4.0, 0.5, seed=3)
        dead = nb.step(noise4(nb, 9, 9, 4.0, 0.6, seed=4), 0.62, 0.05)
        dome = nb.m("SUBTRACT", 1.0, nb.m("POWER", clump, 1.6))
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", hue, 0.6), nb.m("MULTIPLY", fine, 0.4)),
                      [(0.25, c("#2b3f16")), (0.55, c("#4d6a22")), (0.8, c("#83953a"))])
        col = nb.mix(nb.m("MULTIPLY", dead, 0.7), col, c("#6b5530"))
        col = nb.mix(nb.m("MULTIPLY", nb.m("SUBTRACT", 1.0, dome), 0.5), col, c("#1d2a10"), "MULTIPLY")
        h = nb.m("ADD", nb.m("MULTIPLY", dome, 1.0), nb.m("MULTIPLY", fine, 0.5))
        rough = nb.m("ADD", 0.82, nb.m("MULTIPLY", fine, 0.1))
    elif name == "Daub":
        body = noise4(nb, 5, 5, 6.0, 0.55, seed=1)
        smooth = noise4(nb, 2, 3, 3.0, 0.5, distortion=1.5, seed=2)
        straw_u = nb.m("SUBTRACT", 1.0, nb.step(voronoi4(nb, 36, 150, seed=3), 0.06, 0.02))
        straw_v = nb.m("SUBTRACT", 1.0, nb.step(voronoi4(nb, 150, 36, seed=4), 0.06, 0.02))
        straw = nb.m("MAXIMUM", nb.m("MULTIPLY", straw_u, nb.step(noise4(nb, 8, 8, 2.0, seed=5), 0.52, 0.05)),
                     nb.m("MULTIPLY", straw_v, nb.step(noise4(nb, 8, 8, 2.0, seed=6), 0.52, 0.05)))
        crack = nb.m("MULTIPLY", nb.m("SUBTRACT", 1.0, nb.step(voronoi4(nb, 9, 9, "DISTANCE_TO_EDGE", seed=7), 0.005, 0.003)), nb.step(noise4(nb, 3, 3, 3.0, seed=9), 0.5, 0.1))
        grit = nb.step(noise4(nb, 220, 220, 2.0, seed=8), 0.66, 0.04)
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", body, 0.6), nb.m("MULTIPLY", smooth, 0.4)),
                      [(0.3, c("#5f4c38")), (0.6, c("#87705a")), (0.85, c("#a48e72"))])
        col = nb.mix(nb.m("MULTIPLY", straw, 0.85), col, c("#b99d5f"))
        col = nb.mix(nb.m("MULTIPLY", crack, 0.45), col, c("#3a2c20"))
        col = nb.mix(nb.m("MULTIPLY", grit, 0.25), col, c("#4a3c2e"))
        h = nb.m("ADD", nb.m("MULTIPLY", smooth, 0.8), nb.m("MULTIPLY", body, 0.4))
        h = nb.m("ADD", h, nb.m("SUBTRACT", nb.m("MULTIPLY", straw, 0.25), nb.m("MULTIPLY", crack, 0.5)))
        rough = nb.m("ADD", 0.86, nb.m("MULTIPLY", straw, -0.15))
    elif name == "Thatch":
        strand = noise4(nb, 170, 2, 3.0, 0.5, seed=1)
        strand2 = noise4(nb, 340, 3, 2.0, 0.5, seed=2)
        bundle = noise4(nb, 7, 1, 2.0, 0.5, seed=3)
        weather = noise4(nb, 4, 6, 5.0, 0.6, seed=4)
        gap = nb.step(nb.m("ADD", nb.m("MULTIPLY", strand, 0.7), nb.m("MULTIPLY", strand2, 0.3)), 0.42, 0.06)
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", strand, 0.5), nb.m("MULTIPLY", bundle, 0.5)),
                      [(0.25, c("#5e4f33")), (0.55, c("#8f7c50")), (0.8, c("#b8a46c"))])
        col = nb.mix(nb.m("MULTIPLY", nb.step(weather, 0.55, 0.1), 0.6), col, c("#7a756a"))
        col = nb.mix(nb.m("MULTIPLY", nb.m("SUBTRACT", 1.0, gap), 0.75), col, c("#231b10"))
        h = nb.m("ADD", nb.m("MULTIPLY", gap, 0.9), nb.m("MULTIPLY", strand2, 0.3))
        rough = nb.m("ADD", 0.74, nb.m("MULTIPLY", weather, 0.15))
    elif name == "Hide":
        mott = noise4(nb, 3, 3, 6.0, 0.6, seed=1)
        smoke = noise4(nb, 2, 2, 4.0, 0.55, seed=2)
        wrinkle = noise4(nb, 9, 14, 4.0, 0.6, distortion=2.5, seed=3)
        scrape = noise4(nb, 40, 3, 2.0, 0.5, seed=4)
        pores = noise4(nb, 260, 260, 2.0, seed=5)
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", mott, 0.7), nb.m("MULTIPLY", scrape, 0.3)), [(0.3, c("#6e5236")), (0.75, c("#b08e62"))])
        col = nb.mix(nb.m("MULTIPLY", nb.step(smoke, 0.55, 0.15), 0.65), col, c("#3e2b1c"))
        h = nb.m("ADD", nb.m("MULTIPLY", wrinkle, 0.8), nb.m("MULTIPLY", pores, 0.15))
        rough = nb.m("ADD", 0.58, nb.m("MULTIPLY", smoke, 0.15))
    elif name == "Mortar":
        body = noise4(nb, 6, 6, 6.0, 0.6, seed=1)
        grit = voronoi4(nb, 120, 120, seed=2)
        peb = nb.m("SUBTRACT", 1.0, nb.step(voronoi4(nb, 26, 26, seed=3), 0.16, 0.03))
        col = nb.ramp(body, [(0.3, c("#56493b")), (0.75, c("#857463"))])
        col = nb.mix(nb.m("MULTIPLY", nb.step(grit, 0.2, 0.05), 0.35), col, c("#3b3128"))
        col = nb.mix(nb.m("MULTIPLY", peb, nb.step(noise4(nb, 10, 10, 2.0, seed=4), 0.55, 0.05)), col, c("#9a9184"))
        h = nb.m("ADD", nb.m("MULTIPLY", body, 0.6), nb.m("MULTIPLY", peb, 0.6))
        rough = 0.9
    elif name == "Cord":
        ply = stripes(nb, 3, 8)
        fib = noise4(nb, 60, 160, 3.0, 0.5, seed=1)
        col = nb.ramp(nb.m("ADD", nb.m("MULTIPLY", ply, 0.6), nb.m("MULTIPLY", fib, 0.4)), [(0.2, c("#4e3d28")), (0.8, c("#9b8158"))])
        h = nb.m("ADD", nb.m("POWER", ply, 0.6), nb.m("MULTIPLY", fib, 0.3))
        rough = 0.7
    else:
        raise ValueError(name)
    nb.finish(col, rough, nb.bump(h, 0.6, 0.25))
    return nb.mat, h


def bake_surface(name, res):
    """One surface baked on a 1 x 1 plane: BaseColor, Normal (DirectX), ORM (R from the height: hollows darker, G roughness, B 0)."""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 4
    bpy.ops.mesh.primitive_plane_add(size=100.0)
    ob = bpy.context.active_object
    ob.name = "Bake_" + name
    mat, height = surface_material(name)
    ob.data.materials.append(mat)
    nt = mat.node_tree
    imgs = {"color": BI.new_image("c_" + name, res), "normal": BI.new_image("n_" + name, res, (0.5, 0.5, 1, 1), True),
            "rough": BI.new_image("r_" + name, res, (0.6, 0.6, 0.6, 1), True), "height": BI.new_image("h_" + name, res, (0.5, 0.5, 0.5, 1), True)}
    node = nt.nodes.new("ShaderNodeTexImage")
    nt.nodes.active = node

    def go(kind, key, **kw):
        node.image = imgs[key]
        bpy.ops.object.bake(type=kind, margin=0, use_clear=True, **kw)

    go("DIFFUSE", "color", pass_filter={"COLOR"})
    sc.render.bake.normal_space = "TANGENT"
    sc.render.bake.normal_r, sc.render.bake.normal_g, sc.render.bake.normal_b = "POS_X", "NEG_Y", "POS_Z"
    go("NORMAL", "normal")
    go("ROUGHNESS", "rough")
    out_node = nt.nodes["Material Output"]
    keep = out_node.inputs["Surface"].links[0].from_socket
    em = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(height, em.inputs["Color"])
    nt.links.new(em.outputs[0], out_node.inputs["Surface"])
    go("EMIT", "height")
    nt.links.new(keep, out_node.inputs["Surface"])
    files = {}
    for key, fname in (("color", "BaseColor"), ("normal", "Normal")):
        p = os.path.join(OUT, "T_Surf_%s_%s.png" % (name, fname))
        imgs[key].filepath_raw = p
        imgs[key].file_format = "PNG"
        imgs[key].save()
        files[fname] = p
    n = res * res * 4
    h, r = np.empty(n, np.float32), np.empty(n, np.float32)
    imgs["height"].pixels.foreach_get(h)
    imgs["rough"].pixels.foreach_get(r)
    hh = h[0::4]
    lo, hi = np.percentile(hh, 2), np.percentile(hh, 98)
    hn = np.clip((hh - lo) / max(hi - lo, 1e-4), 0.0, 1.0)
    orm = np.empty(n, np.float32)
    orm[0::4], orm[1::4], orm[2::4], orm[3::4] = 0.45 + 0.55 * hn, r[0::4], 0.0, 1.0
    oimg = BI.new_image("orm_" + name, res, non_color=True)
    oimg.pixels.foreach_set(orm)
    p = os.path.join(OUT, "T_Surf_%s_ORM.png" % name)
    oimg.filepath_raw = p
    oimg.file_format = "PNG"
    oimg.save()
    files["ORM"] = p
    bpy.data.objects.remove(ob)
    log("surface %s baked" % name)
    return files


def run_surfaces():
    BI.setup_scene()
    os.makedirs(OUT, exist_ok=True)
    made = {name: bake_surface(name, RES) for name in BP.SURFACES}
    # a contact sheet: each surface 2 x 2 tiles (the seams show if a pattern does not wrap)
    sw = 256
    names = list(made)
    sheet = np.zeros((sw * 2, sw * 2 * len(names), 4), np.float32)
    for i, name in enumerate(names):
        img = bpy.data.images.load(made[name]["BaseColor"])
        img.scale(sw, sw)
        px = np.empty(sw * sw * 4, np.float32)
        img.pixels.foreach_get(px)
        px = px.reshape(sw, sw, 4)
        sheet[:, i * sw * 2:(i + 1) * sw * 2] = np.tile(px, (2, 2, 1))
    simg = bpy.data.images.new("surf_sheet", sw * 2 * len(names), sw * 2, alpha=False)
    simg.pixels.foreach_set(sheet.ravel())
    simg.filepath_raw = os.path.join(OUT, "Surfaces_preview.png")
    simg.file_format = "PNG"
    simg.save()
    with open(os.path.join(OUT, "Surfaces.json"), "w") as f:
        json.dump(made, f, indent=1)
    log("done: %d surfaces" % len(made))


# ------------------------------------------------------------------------------------------------------------------------------------------
# Geometry in Unreal's frame
# ------------------------------------------------------------------------------------------------------------------------------------------

class Vary:
    """The piece's one source of natural variation, seeded by its name: each primitive draws its look in build order, so a rebuild is the same
    piece. look() -> (light, weather, du, dv): light 0..1 (0.5 is the surface as baked), weather 0..1 (greyed, sun-bleached), and where the
    tiling texture starts on it (in tiles)."""

    def __init__(self, key):
        self.rng = random.Random(zlib.crc32(key.encode()) & 0xFFFFFF)
        self.field_seed = self.rng.uniform(0.0, 100.0)

    def look(self, spread=1.0):
        r = self.rng
        light = min(1.0, max(0.0, 0.5 + r.gauss(0.0, 0.21 * spread)))
        weather = min(1.0, (r.random() ** 2.2) * (0.9 * spread + 0.1))
        return light, weather, r.random(), r.random()

    def seed(self):
        return self.rng.uniform(0.0, 50.0)

    def uniform(self, a, b):
        return self.rng.uniform(a, b)

    def field(self, p):
        """A slow change across the whole piece (one end of a wall a little darker, a patch more weathered) on top of each primitive's own."""
        s = self.field_seed
        return (0.10 * noise.noise(p * 0.011 + Vector((s, 0.0, 0.0))), 0.16 * noise.noise(p * 0.017 + Vector((0.0, s, 3.0))))


VARY = Vary("default")   # run_piece / run_item / run_supports set the job's own


class Mesh:
    """Vertices in Unreal's frame (cm); faces with per-corner UVs and a surface. Each vertex carries its tint (light, weather): the primitive
    being made sets it with begin() and the piece-wide field adds to it."""

    def __init__(self):
        self.v, self.f, self.uv, self.surf, self.smooth, self.t = [], [], [], [], [], []
        self.look = (0.5, 0.0)
        self.uvoff = (0.0, 0.0)

    def begin(self, spread=1.0):
        """A new primitive: its own tint and its own start on the tiling texture (spread 0 keeps the surface as baked, a sheet wants little)."""
        light, weather, du, dv = VARY.look(spread)
        self.look = (light, weather)
        self.uvoff = (du, dv)

    def vert(self, p):
        p = Vector(p)
        fa, fb = VARY.field(p)
        self.v.append(p)
        self.t.append((min(1.0, max(0.0, self.look[0] + fa)), min(1.0, max(0.0, self.look[1] + fb))))
        return len(self.v) - 1

    def face(self, idx, uvs, surf, smooth=True):
        du, dv = self.uvoff
        self.f.append(tuple(idx))
        self.uv.append([(u[0] + du, u[1] + dv) for u in uvs])
        self.surf.append(surf)
        self.smooth.append(smooth)

    def extend(self, other):
        base = len(self.v)
        self.v += other.v
        self.t += other.t
        for f, uv, s, sm in zip(other.f, other.uv, other.surf, other.smooth):
            self.f.append(tuple(base + i for i in f))
            self.uv.append(list(uv))
            self.surf.append(s)
            self.smooth.append(sm)

    def tris(self):
        return sum(len(f) - 2 for f in self.f)


def frame_of(d):
    """Two unit vectors square to direction d (a stable up)."""
    d = d.normalized()
    up = Vector((0, 0, 1)) if abs(d.z) < 0.95 else Vector((1, 0, 0))
    a = d.cross(up).normalized()
    return a, d.cross(a).normalized()


def natural_stick(pts, lengths, seed, bow):
    """A straight-ish run of points bowed a little sideways (a cut stick is never true): bow cm at its middle, None = 0.7% of its length.
    Only when the run is near straight -- a hoop, a helix of cord or a bent fork keeps the shape it was given."""
    L = lengths[-1]
    if len(pts) < 2 or L < 12.0 or (pts[-1] - pts[0]).length < 0.9 * L:
        return pts
    bow = 0.007 * L * (0.4 + 0.8 * abs(noise.noise(Vector((seed, 1.7, 0.3))))) if bow is None else bow
    if bow <= 0.0:
        return pts
    d = (pts[-1] - pts[0]).normalized()
    a, b = frame_of(d)
    ang = TAU * (0.5 + 0.5 * noise.noise(Vector((seed, 5.1, 2.2))))
    side = a * math.cos(ang) + b * math.sin(ang)
    # mostly one curve, a little S in some
    return [p + side * bow * (math.sin(math.pi * l / L) + 0.3 * math.sin(TAU * l / L + seed)) for p, l in zip(pts, lengths)]


def tube(m, pts, radius, surf, sides=8, cap_surf="Hewn", wobble=0.0, seed=0, caps=(True, True), twist=0.0, bow=None, swell=0.07, cut=None):
    """A round stick along pts (Unreal cm): radius(t) or a number; bark UVs: u around (cm / tile), v along. Cut ends in cap_surf.
    Natural by default: bowed (natural_stick), swelling and narrowing along its length (swell, of the radius), each cut end on a slant (cut
    degrees; None = 5..30 on a bark or rod stick, square on anything else), its own seed when none is given."""
    m.begin()
    if not seed:
        seed = VARY.seed()
    rfn = radius if callable(radius) else (lambda t, r=radius: r)
    T = tile(surf)
    lengths = [0.0]
    for i in range(1, len(pts)):
        lengths.append(lengths[-1] + (pts[i] - pts[i - 1]).length)
    L = max(lengths[-1], 1e-3)
    pts = natural_stick(pts, lengths, seed, bow)
    base_rfn = rfn
    rfn = lambda t: base_rfn(t) * (1.0 + swell * noise.noise(Vector((t * L * 0.045, seed, 2.0))))   # noqa: E731
    if cut is None:
        cut = VARY.uniform(5.0, 30.0) if surf in ("Bark", "Rod") else 0.0
    rings = []
    prev_a = None
    for i, p in enumerate(pts):
        d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        if prev_a is None:
            a, b = frame_of(d)
        else:   # parallel transport: the seam does not spin round the stick
            a = (prev_a - d * prev_a.dot(d)).normalized()
            b = d.cross(a).normalized()
        prev_a = a
        t = lengths[i] / L
        r = rfn(t)
        ring = []
        for k in range(sides):
            ang = TAU * k / sides + twist * t
            wob = 1.0 + wobble * noise.noise(Vector((math.cos(ang) * 0.7 + seed, math.sin(ang) * 0.7, lengths[i] * 0.06)))
            ring.append(m.vert(p + (a * math.cos(ang) + b * math.sin(ang)) * r * wob))
        rings.append((ring, r))
    if cut > 0.0:
        # the end rings slanted across the stick (each end its own way); the faces next to them keep their length
        tc = math.tan(math.radians(cut))
        for which, i in ((0, 0), (1, len(rings) - 1)):
            if not caps[which] or len(rings) < 3:
                continue
            ring, r = rings[i]
            d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            c = sum((m.v[j] for j in ring), Vector()) / len(ring)
            a, b = frame_of(d)
            phi = seed * 2.3 + which * 1.9
            tilt = a * math.cos(phi) + b * math.sin(phi)
            reach = min(r * tc, 0.45 * (pts[1] - pts[0]).length if which == 0 else 0.45 * (pts[-1] - pts[-2]).length)
            for j in ring:
                m.v[j] = m.v[j] + d * ((m.v[j] - c).dot(tilt) / max(r, 1e-3)) * reach
    for i in range(len(rings) - 1):
        (ra, rr0), (rb, rr1) = rings[i], rings[i + 1]
        circ = TAU * (rr0 + rr1) * 0.5
        v0, v1 = lengths[i] / T, lengths[i + 1] / T
        for k in range(sides):
            k1 = (k + 1) % sides
            u0, u1 = circ * k / sides / T, circ * (k + 1) / sides / T
            m.face((ra[k], ra[k1], rb[k1], rb[k]), [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], surf)
    for which, (ring, r) in (("s", rings[0]), ("e", rings[-1])):
        if not caps[0 if which == "s" else 1]:
            continue
        c = sum((m.v[i] for i in ring), Vector()) / len(ring)
        ci = m.vert(c)
        tc = tile(cap_surf)
        for k in range(sides):
            k1 = (k + 1) % sides
            tri = (ring[k1], ring[k], ci) if which == "s" else (ring[k], ring[k1], ci)
            m.face(tri, [((m.v[j] - c).x / tc + 0.5, (m.v[j] - c).y / tc + 0.5) for j in tri], cap_surf, smooth=False)
    return m


def path(p0, p1, n, bend=None, seed=0.0, wander=0.0):
    """n+1 points from p0 to p1, bent by bend(t) -> offset vector, plus a little noise wander (a hand-cut stick is never straight)."""
    pts = []
    for i in range(n + 1):
        t = i / n
        p = p0.lerp(p1, t)
        if bend:
            p = p + bend(t)
        if wander:
            p = p + Vector((noise.noise(Vector((t * 2.3, seed, 0.1))), noise.noise(Vector((t * 2.3, seed, 4.7))),
                            noise.noise(Vector((t * 2.3, seed, 9.3))))) * wander
        pts.append(p)
    return pts


def beam(m, p0, p1, w, h, surf="Hewn", chamfer=1.2, seg=6, seed=0.0, wobble=0.4, up=None, bow=None, twist=None):
    """A hewn timber from p0 to p1: a w x h section with chamfered arrises, its faces a little uneven (the axe). Natural by default: the
    section 3% off its nominal size, bowed (bow cm at the middle, None = 0.25% of its length) and twisted (twist degrees end to end, None =
    up to 2.5) as a timber dries, each in its own direction."""
    m.begin(0.8)
    if not seed:
        seed = VARY.seed()
    w *= 1.0 + 0.03 * noise.noise(Vector((seed, 0.4, 7.0)))
    h *= 1.0 + 0.03 * noise.noise(Vector((seed, 2.4, 7.0)))
    d = (p1 - p0)
    L = d.length
    d.normalize()
    if up is None:
        a, b = frame_of(d)
    else:
        b = (up - d * up.dot(d)).normalized()
        a = b.cross(d).normalized()
    bow = 0.0025 * L * noise.noise(Vector((seed, 3.3, 1.1))) * 2.0 if bow is None else bow
    twist = math.radians(2.5 * noise.noise(Vector((seed, 6.1, 4.4)))) if twist is None else math.radians(twist)
    bow_dir = a * math.cos(seed) + b * math.sin(seed)
    hw, hh, c = w * 0.5, h * 0.5, chamfer
    sec = [(hw, hh - c), (hw - c, hh), (-hw + c, hh), (-hw, hh - c), (-hw, -hh + c), (-hw + c, -hh), (hw - c, -hh), (hw, -hh + c)]
    perim = [0.0]
    for i in range(len(sec)):
        x0, y0 = sec[i]
        x1, y1 = sec[(i + 1) % len(sec)]
        perim.append(perim[-1] + math.hypot(x1 - x0, y1 - y0))
    T = tile(surf)
    rings = []
    for i in range(seg + 1):
        t = i / seg
        ring = []
        ca, sa = math.cos(twist * (t - 0.5)), math.sin(twist * (t - 0.5))
        at, bt = a * ca + b * sa, b * ca - a * sa
        mid = p0 + d * (L * t) + bow_dir * (bow * math.sin(math.pi * t))
        for k, (x, y) in enumerate(sec):
            n = noise.noise(Vector((x * 0.15 + seed, y * 0.15, t * L * 0.04))) * wobble
            ring.append(m.vert(mid + at * (x + n * (1 if x > 0 else -1)) + bt * (y + n * (1 if y > 0 else -1))))
        rings.append(ring)
    k_n = len(sec)
    for i in range(seg):
        v0, v1 = L * i / seg / T, L * (i + 1) / seg / T
        for k in range(k_n):
            k1 = (k + 1) % k_n
            u0, u1 = perim[k] / T, perim[k + 1] / T
            m.face((rings[i][k], rings[i][k1], rings[i + 1][k1], rings[i + 1][k]), [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], surf, smooth=False)
    for ring, rev in ((rings[0], True), (rings[-1], False)):
        c = sum((m.v[i] for i in ring), Vector()) / len(ring)
        ci = m.vert(c)
        for k in range(k_n):
            k1 = (k + 1) % k_n
            tri = (ring[k1], ring[k], ci) if rev else (ring[k], ring[k1], ci)
            m.face(tri, [((m.v[j] - c).dot(a) / T + 0.5, (m.v[j] - c).dot(b) / T + 0.5) for j in tri], surf, smooth=False)
    return m


def blob(m, centre, radii, surf, rot=None, seed=0.0, rough=0.25, rings=6, segs=10, freq=1.7, spread=0.8):
    """A lump (moss, a clod, a stone): a squashed sphere pushed about by noise (freq: how many bumps round it -- a stone a few, a moss
    cushion many). spread: how far its tint strays from its neighbours' (Vary.look)."""
    m.begin(spread)
    rot = rot or Matrix.Identity(3)
    T = tile(surf)
    idx = []
    for i in range(rings + 1):
        lat = math.pi * (i / rings - 0.5)
        row = []
        for j in range(segs):
            lon = TAU * j / segs
            dvec = Vector((math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat)))
            k = 1.0 + rough * noise.noise(dvec * freq + Vector((seed, seed * 0.7, seed * 1.3)))
            p = Vector((dvec.x * radii[0], dvec.y * radii[1], dvec.z * radii[2])) * k
            row.append(m.vert(centre + rot @ p))
        idx.append(row)
    rmean = (radii[0] + radii[1] + radii[2]) / 3.0
    for i in range(rings):
        for j in range(segs):
            j1 = (j + 1) % segs
            u0, u1 = rmean * TAU * j / segs / T, rmean * TAU * (j + 1) / segs / T
            v0, v1 = rmean * math.pi * i / rings / T, rmean * math.pi * (i + 1) / rings / T
            m.face((idx[i][j], idx[i][j1], idx[i + 1][j1], idx[i + 1][j]), [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], surf)
    return m


def sheet(m, fn, s0, s1, t0, t1, ns, nt, thick, surf, back=True, rim=True):
    """A slab over a parametric surface: fn(s, t) -> (point, outward normal); the outer face, an inner one thick cm in, and its rim. UVs in cm
    along s and t. A sheet varies little in itself (the piece-wide field does the rest): the courses of a roof do not stripe."""
    m.begin(0.35)
    T = tile(surf)
    first = len(m.f)
    outer, inner = [], []
    for i in range(ns + 1):
        s = s0 + (s1 - s0) * i / ns
        ro, ri = [], []
        for j in range(nt + 1):
            t = t0 + (t1 - t0) * j / nt
            p, n = fn(s, t)
            th = thick(s, t) if callable(thick) else thick
            ro.append(m.vert(p))
            ri.append(m.vert(p - n * th))
        outer.append(ro)
        inner.append(ri)
    for i in range(ns):
        for j in range(nt):
            sa, sb = (s0 + (s1 - s0) * i / ns) / T, (s0 + (s1 - s0) * (i + 1) / ns) / T
            ta, tb = (t0 + (t1 - t0) * j / nt) / T, (t0 + (t1 - t0) * (j + 1) / nt) / T
            m.face((outer[i][j], outer[i + 1][j], outer[i + 1][j + 1], outer[i][j + 1]), [(ta, sa), (ta, sb), (tb, sb), (tb, sa)], surf)
            if back:
                m.face((inner[i][j + 1], inner[i + 1][j + 1], inner[i + 1][j], inner[i][j]), [(tb, sa), (tb, sb), (ta, sb), (ta, sa)], surf)
    if rim:
        def strip(a, b, uvs_along):
            for k in range(len(a) - 1):
                u0, u1 = uvs_along[k], uvs_along[k + 1]
                m.face((a[k], a[k + 1], b[k + 1], b[k]), [(u0, 0.0), (u1, 0.0), (u1, 0.05), (u0, 0.05)], surf)
        along_t = [(t0 + (t1 - t0) * j / nt) / T for j in range(nt + 1)]
        along_s = [(s0 + (s1 - s0) * i / ns) / T for i in range(ns + 1)]
        strip(inner[0], outer[0], along_t)
        strip(outer[ns], inner[ns], along_t)
        strip([outer[i][0] for i in range(ns + 1)], [inner[i][0] for i in range(ns + 1)], along_s)
        strip([inner[i][nt] for i in range(ns + 1)], [outer[i][nt] for i in range(ns + 1)], along_s)
    # the outer face points along fn's normal whichever way s and t run: flip every face made here if it came out inward
    a, b, c = m.v[outer[0][0]], m.v[outer[1][0]], m.v[outer[0][1]]
    _, n_mid = fn((s0 + s1) * 0.5, (t0 + t1) * 0.5)
    if (b - a).cross(c - a).dot(n_mid) < 0.0:
        for k in range(first, len(m.f)):
            m.f[k] = tuple(reversed(m.f[k]))
            m.uv[k] = list(reversed(m.uv[k]))
    return m


def lashing(m, centre, axis, radius, turns=5, width=4.0, cord=0.35, seed=0.0):
    """Cord wound round a joint: a short helix of turns about axis at the given radius."""
    axis = axis.normalized()
    a, b = frame_of(axis)
    pts = []
    n = turns * 12
    for i in range(n + 1):
        t = i / n
        ang = TAU * turns * t + seed
        pts.append(centre + axis * (width * (t - 0.5)) + (a * math.cos(ang) + b * math.sin(ang)) * (radius + cord))
    return tube(m, pts, cord, "Cord", sides=5, cap_surf="Cord")


# ------------------------------------------------------------------------------------------------------------------------------------------
# The pieces: each returns [(part name, Mesh)] in build order
# ------------------------------------------------------------------------------------------------------------------------------------------

WX = -50.0   # a wall panel's centre plane (the stick wall's frame)


def by_eye(ys, rng, jitter):
    """Positions set out by eye, not measured: each moved up to jitter cm, the ends kept inside where they were."""
    out = [y + rng.uniform(-jitter, jitter) for y in ys]
    out[0], out[-1] = max(out[0], ys[0] - jitter * 0.3), min(out[-1], ys[-1] + jitter * 0.3)
    return sorted(out)


def stakes_row(ys, z0, z1, r, rng, seed=0, x=WX):
    """Stakes driven in a row: each its own thickness, leaning a little, cut off at its own height."""
    m = Mesh()
    for i, y in enumerate(ys):
        rr = r * rng.uniform(0.8, 1.2)
        top = Vector((x + rng.uniform(-1.2, 1.2), y + rng.uniform(-1.8, 1.8), z1 + rng.uniform(-4.0, 1.5)))
        tube(m, path(Vector((x, y, z0)), top, 10, seed=seed + i * 3.1, wander=0.8),
             lambda t, rr=rr: rr * (1.0 - 0.12 * t), "Bark", sides=9, wobble=0.06, seed=seed + i)
    return m


def packed_courses(z0, z1, rod_r, rng, spread=(0.65, 1.35)):
    """Rods beaten down on each other: each course rests on the one below by both rods' thickness, so a thick rod lifts the rest -> [(z, r)]."""
    out = []
    r = rod_r * rng.uniform(*spread)
    z = z0 + r
    while z + r < z1:
        out.append((z, r))
        r2 = rod_r * rng.uniform(*spread)
        z += r + r2 + rng.uniform(0.0, 0.35)
        r = r2
    return out


def weave_courses(ys, courses, stake_r, rng, seed=0, x=WX, y_end=51.0):
    """One rod per course, passing in front of one stake and behind the next; alternate courses in opposite phase. ys may be uneven (the weave
    turns at each stake); courses [(z, r)]; each rod tapers from its butt (either end), runs a little uphill or down and ends where it ends."""
    rods = []
    ext = [ys[0] - (ys[1] - ys[0])] + list(ys) + [ys[-1] + (ys[-1] - ys[-2])]
    for k, (z, r) in enumerate(courses):
        m = Mesh()
        amp = stake_r + r * 0.95
        y0 = -y_end - rng.uniform(0.0, 5.0)
        y1 = y_end + rng.uniform(0.0, 5.0)
        slope = rng.uniform(-0.008, 0.008)
        butt_left = rng.random() < 0.5
        pts = []
        n = 48
        for i in range(n + 1):
            y = y0 + (y1 - y0) * i / n
            j = max(0, min(len(ext) - 2, next((q for q in range(len(ext) - 1) if y < ext[q + 1]), len(ext) - 2)))
            t = (y - ext[j]) / (ext[j + 1] - ext[j])
            sign = 1.0 if (j + k) % 2 == 0 else -1.0
            off = amp * sign * math.cos(math.pi * t)
            pts.append(Vector((x + off, y, z + slope * y + noise.noise(Vector((y * 0.05, k * 1.7, seed))) * 0.35)))
        taper = (lambda t, r=r: r * (1.0 - 0.3 * t)) if butt_left else (lambda t, r=r: r * (0.7 + 0.3 * t))
        tube(m, pts, taper, "Rod", sides=7, wobble=0.05, seed=seed + k * 1.9)
        rods.append(m)
    return rods


def group(meshes, n):
    """Merge a list of meshes into n parts (build stages) in order."""
    out = []
    per = max(1, int(math.ceil(len(meshes) / float(n))))
    for i in range(0, len(meshes), per):
        g = Mesh()
        for mm in meshes[i:i + per]:
            g.extend(mm)
        out.append(g)
    return out


def geo_wattle_wall(rng):
    ys = by_eye([-40.0, -20.0, 0.0, 20.0, 40.0], rng, 3.0)
    parts = [("Stakes", stakes_row(ys, -58.0, 57.0, 1.9, rng, seed=1))]
    rods = weave_courses(ys, packed_courses(-49.7, 49.5, 1.1, rng), 1.9, rng, seed=3)
    for i, g in enumerate(group(rods, 8)):
        parts.append(("Weave%d" % (i + 1), g))
    return parts


def geo_moss_daub(rng, bands=8):
    """Moss stuffed into the weave by the handful from the bottom up: clumps of every size packed edge to edge over both faces (many small,
    a few big mats), each turned its own way, standing proud by its own amount, a few wisps hanging; staged in bands from the bottom."""
    out = [Mesh() for _ in range(bands)]
    for side in (1, -1):
        placed = []
        for _ in range(5000):
            r = 2.4 + 6.0 * rng.random() ** 2.0
            y, z = rng.uniform(-50.0 + r * 0.4, 50.0 - r * 0.4), rng.uniform(-50.0 + r * 0.4, 50.0 - r * 0.4)
            if any((y - py) ** 2 + (z - pz) ** 2 < ((r + pr) * 0.48) ** 2 for py, pz, pr in placed):
                continue
            placed.append((y, z, r))
        for i, (y, z, r) in enumerate(placed):
            m = out[min(bands - 1, int((z + 50.0) / (100.0 / bands)))]
            # a soft cushion, its surface all small bumps (the moss's own tufts), overlapping its neighbours into one stuffed mass that stands
            # proud of the weave (the rods reach ~4.5 cm off the panel's plane: none may show through); clumps of one gathering, so their
            # tints stay close
            rad = (r * rng.uniform(0.3, 0.45), r * rng.uniform(0.95, 1.3), r * rng.uniform(0.8, 1.15))
            c = Vector((WX + side * (3.6 + rad[0] * 0.5 + rng.uniform(-0.3, 0.6)), y, z))
            rot = Matrix.Rotation(rng.uniform(0.0, math.pi), 3, "X") @ Matrix.Rotation(rng.uniform(-0.2, 0.2), 3, "Z")
            blob(m, c, rad, "Moss", rot=rot, seed=side * 7.0 + i * 1.37, rough=0.2, rings=7, segs=14, freq=4.5, spread=0.4)
            if r > 4.5 and rng.random() < 0.3:   # a wisp hanging from a big clump
                blob(m, c + Vector((side * 0.6, rng.uniform(-1.5, 1.5), -r * 0.8)), (0.7, 1.0, rng.uniform(2.5, 4.5)), "Moss",
                     rot=Matrix.Rotation(rng.uniform(-0.3, 0.3), 3, "X"), seed=i * 2.9, rough=0.3, rings=4, segs=6)
    return [("Moss%d" % (i + 1), m) for i, m in enumerate(out)]


def daub_slab(m, z0, z1, offset, surf, y0=-50.0, y1=50.0, ny=24, nz=4, close_bottom=False, close_top=False, x=WX):
    """Daub over both faces of a panel between heights z0 and z1: offset(side, y, z) is how far it stands off the panel's centre plane; the
    two faces meet round the panel's ends (and its foot / head when asked), so it reads as one plastered thickness from any side."""
    m.begin(0.25)
    T = tile(surf)
    grids = {}
    for side in (1, -1):
        g = []
        for i in range(nz + 1):
            z = z0 + (z1 - z0) * i / nz
            g.append([m.vert(Vector((x + side * offset(side, y0 + (y1 - y0) * j / ny, z), y0 + (y1 - y0) * j / ny, z))) for j in range(ny + 1)])
        grids[side] = g
        for i in range(nz):
            for j in range(ny):
                za, zb = (z0 + (z1 - z0) * i / nz) / T, (z0 + (z1 - z0) * (i + 1) / nz) / T
                ya, yb = (y0 + (y1 - y0) * j / ny) / T, (y0 + (y1 - y0) * (j + 1) / ny) / T
                q = (g[i][j], g[i][j + 1], g[i + 1][j + 1], g[i + 1][j])
                uv = [(ya, za), (yb, za), (yb, zb), (ya, zb)]
                # +X face: (+y, +z) winds to -X by the right-hand rule, so the +X face reverses
                if side > 0:
                    q, uv = tuple(reversed(q)), list(reversed(uv))
                m.face(q, uv, surf)
    f, b = grids[1], grids[-1]
    for i in range(nz):   # the panel's two ends
        za, zb = (z0 + (z1 - z0) * i / nz) / T, (z0 + (z1 - z0) * (i + 1) / nz) / T
        m.face((f[i][0], f[i + 1][0], b[i + 1][0], b[i][0]), [(0, za), (0, zb), (0.1, zb), (0.1, za)], surf)
        m.face((b[i][ny], b[i + 1][ny], f[i + 1][ny], f[i][ny]), [(0, za), (0, zb), (0.1, zb), (0.1, za)], surf)
    for close, i, rev in ((close_bottom, 0, False), (close_top, nz, True)):
        if not close:
            continue
        for j in range(ny):
            ya, yb = (y0 + (y1 - y0) * j / ny) / T, (y0 + (y1 - y0) * (j + 1) / ny) / T
            q = (f[i][j], b[i][j], b[i][j + 1], f[i][j + 1])
            uv = [(ya, 0), (ya, 0.1), (yb, 0.1), (yb, 0)]
            if rev:
                q, uv = tuple(reversed(q)), list(reversed(uv))
            m.face(q, uv, surf)
    return m


def geo_clay_daub(rng):
    """Daub thrown on by the handful and pressed flat: a lumpy plaster over both faces, in bands from the bottom up."""
    def offset(side, y, z):
        lump = 1.6 * noise.noise(Vector((y * 0.06, z * 0.06, side * 3.0))) + 0.7 * noise.noise(Vector((y * 0.22, z * 0.22, side * 1.3)))
        hand = 0.35 * noise.noise(Vector((y * 0.8, z * 0.8, side * 7.0)))
        edge = min(1.0, (50.5 - abs(y)) / 5.0, (50.5 - abs(z)) / 5.0)
        return (5.0 + lump + hand) * (0.6 + 0.4 * max(edge, 0.0))
    parts = []
    nb = 8
    for bi in range(nb):
        m = Mesh()
        z0 = -50.0 + 100.0 * bi / nb - (0.0 if bi == 0 else 0.8)
        z1 = -50.0 + 100.0 * (bi + 1) / nb
        daub_slab(m, z0, z1, offset, "Daub", nz=4, ny=26, close_bottom=(bi == 0), close_top=(bi == nb - 1))
        parts.append(("Daub%d" % (bi + 1), m))
    return parts


def geo_pole_post(rng):
    m = Mesh()
    tube(m, path(Vector((0, 0, -50.5)), Vector((0, 0, 50.5)), 12, seed=1.3, wander=0.5), lambda t: 3.5 * (1.0 - 0.08 * t), "Bark", sides=10, wobble=0.05)
    lash = Mesh()
    lashing(lash, Vector((0, 0, 44.0)), Vector((0, 0, 1)), 3.4, turns=6, width=5.0)
    lashing(lash, Vector((0, 0, -44.0)), Vector((0, 0, 1)), 3.6, turns=6, width=5.0, seed=1.0)
    return [("Pole", m), ("Lashing", lash)]


def geo_pole_rail(rng):
    m = Mesh()
    tube(m, path(Vector((0, -50.5, 0)), Vector((0, 50.5, 0)), 12, seed=2.1, wander=0.5), lambda t: 3.0 * (1.0 - 0.08 * t), "Bark", sides=10, wobble=0.05)
    lash = Mesh()
    lashing(lash, Vector((0, -44.0, 0)), Vector((0, 1, 0)), 3.0, turns=6, width=5.0)
    lashing(lash, Vector((0, 44.0, 0)), Vector((0, 1, 0)), 2.9, turns=6, width=5.0, seed=1.0)
    return [("Pole", m), ("Lashing", lash)]


def pegs(m, centres, axis, r=1.1, length=4.0):
    """Treenails: each whittled to its own thickness, driven to its own depth."""
    for i, c in enumerate(centres):
        rr, out = r * VARY.uniform(0.85, 1.15), VARY.uniform(-0.15, 0.25) * length
        tube(m, [c - axis * length * 0.5, c + axis * (length * 0.5 + out)], rr, "Hewn", sides=7, caps=(True, True))


def geo_timber_post(rng):
    m = Mesh()
    beam(m, Vector((0, 0, -50.0)), Vector((0, 0, 50.0)), 14.0, 14.0, seed=1.0, up=Vector((1, 0, 0)))
    p = Mesh()
    pegs(p, [Vector((7.2, 0, 42.0)), Vector((7.2, 0, -42.0)), Vector((-7.2, 0, 42.0)), Vector((-7.2, 0, -42.0))], Vector((1, 0, 0)), length=1.2)
    return [("Post", m), ("Pegs", p)]


def geo_timber_beam(rng):
    m = Mesh()
    beam(m, Vector((0, -50.0, 0)), Vector((0, 50.0, 0)), 14.0, 14.0, seed=2.0, up=Vector((0, 0, 1)))
    p = Mesh()
    pegs(p, [Vector((0, 42.0, 7.2)), Vector((0, -42.0, 7.2))], Vector((0, 0, 1)), length=1.2)
    return [("Beam", m), ("Pegs", p)]


def geo_timber_brace(rng):
    m = Mesh()
    beam(m, Vector((WX, -44.0, -44.0)), Vector((WX, 44.0, 44.0)), 9.0, 9.0, seed=3.0, up=Vector((1, 0, 0)))
    p = Mesh()
    pegs(p, [Vector((WX + 4.6, -38.0, -38.0)), Vector((WX + 4.6, 38.0, 38.0))], Vector((1, 0, 0)), length=1.2)
    return [("Brace", m), ("Pegs", p)]


def mortar_core(x0, x1, y0, y1, z0, z1, inset=3.0, seed=0.0):
    """The earth-mortar heart of a stone piece, a little inside its faces (the stones stand proud of it)."""
    m = Mesh()
    cx, cy, cz = (x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5
    hx, hy, hz = (x1 - x0) * 0.5 - inset, (y1 - y0) * 0.5, (z1 - z0) * 0.5

    def face(axis, sign):
        def fn(s, t):
            p = Vector((cx, cy, cz))
            if axis == 0:
                p += Vector((sign * hx, t, s))
                n = Vector((sign, 0, 0))
            elif axis == 1:
                p += Vector((t, sign * hy, s))
                n = Vector((0, sign, 0))
            else:
                p += Vector((t, s, sign * hz))
                n = Vector((0, 0, sign))
            p += n * 0.8 * noise.noise(p * 0.08 + Vector((seed, 0, 0)))
            return p, n
        return fn
    for axis, sign, (s0, s1), (t0, t1) in ((0, 1, (-hz, hz), (-hy, hy)), (0, -1, (-hz, hz), (-hy, hy)), (1, 1, (-hz, hz), (-hx, hx)),
                                           (1, -1, (-hz, hz), (-hx, hx)), (2, 1, (-hy, hy), (-hx, hx))):
        sm = Mesh()
        sheet(sm, face(axis, sign), s0, s1, t0, t1, 6, 6, 1.0, "Mortar", back=False, rim=False)
        m.extend(sm)
    return m


def geo_cobble_foundation(rng):
    core = mortar_core(-50.0, 50.0, -50.0, 50.0, -35.0, 3.0, inset=7.0, seed=1.0)
    top = Mesh()
    # the levelled top: rammed earth with a few flat stones set in it
    def fn(s, t):
        return Vector((t, s, 4.2 + 0.5 * noise.noise(Vector((t * 0.05, s * 0.05, 3.0))))), Vector((0, 0, 1))
    sheet(top, fn, -49.0, 49.0, -49.0, 49.0, 10, 10, 2.0, "Mortar", back=False, rim=True)
    for i in range(9):
        c = Vector((rng.uniform(-40, 40), rng.uniform(-40, 40), 4.4))
        blob(top, c, (rng.uniform(9, 14), rng.uniform(7, 12), 1.6), "Mortar", seed=i * 2.3, rough=0.12, rings=4, segs=10)
    return [("Core", core), ("Top", top)]


def geo_cobble_wall(rng):
    return [("Core", mortar_core(WX - 20.0, WX + 20.0, -50.0, 50.0, -50.0, 44.0, inset=8.0, seed=2.0))]   # the top course caps it


def geo_cobble_infill(rng):
    parts = [("Core", mortar_core(WX - 7.0, WX + 7.0, -50.0, 50.0, -50.0, 50.0, inset=2.5, seed=3.0))]
    # the daub smoothed over the faces, leaving the stones' crowns showing: bottom half, then top half
    def offset(side, y, z):
        return 6.2 + 0.8 * noise.noise(Vector((y * 0.08, z * 0.08, side * 2.0))) + 0.25 * noise.noise(Vector((y * 0.7, z * 0.7, side)))
    for hi, (z0, z1) in enumerate(((-50.0, 0.5), (-0.5, 50.0))):
        m = Mesh()
        daub_slab(m, z0, z1, offset, "Daub", nz=6, ny=14, close_bottom=(hi == 0), close_top=(hi == 1))
        parts.append(("Daub%d" % (hi + 1), m))
    return parts


# The stick roof's slab: eave at X 0 Z -50, ridge at X -100 Z +50; s = distance up the slope from the eave, y across.
ROOF_N = Vector((1.0, 0.0, 1.0)).normalized()     # outward
ROOF_UP = Vector((-1.0, 0.0, 1.0)).normalized()   # up the slope
ROOF_EAVE = Vector((0.0, 0.0, -50.0))
ROOF_LEN = 141.42


def roof_point(s, y, off):
    return ROOF_EAVE + ROOF_UP * s + Vector((0, y, 0)) + ROOF_N * off


class Bundles:
    """A course of thatch as it is laid: bundles (yealms) side by side across y, each its own width, set a little up or down the slope and a
    little fuller or thinner than its neighbours; their butts ragged where the straws end."""

    def __init__(self, rng, y0, y1, width=(9.0, 16.0), stagger=3.5, full=(0.82, 1.15)):
        self.edges = [y0 - rng.uniform(0.0, width[0])]
        while self.edges[-1] < y1:
            self.edges.append(self.edges[-1] + rng.uniform(*width))
        n = len(self.edges) - 1
        self.shift = [rng.uniform(-stagger, stagger) for _ in range(n)]
        self.full = [rng.uniform(*full) for _ in range(n)]
        self.seed = rng.uniform(0.0, 40.0)

    def at(self, y):
        """(butt shift down the slope cm, fullness x) at y: each bundle rounded across its width, its neighbours blended at the seam."""
        e = self.edges
        j = max(0, min(len(e) - 2, next((q for q in range(len(e) - 1) if y < e[q + 1]), len(e) - 2)))
        t = (y - e[j]) / (e[j + 1] - e[j])
        k = j + 1 if t > 0.5 else j - 1
        k = max(0, min(len(e) - 2, k))
        w = 0.5 - 0.5 * math.cos(math.pi * min(1.0, abs(t - 0.5) * 2.0))   # 0 mid-bundle .. 1 at its edge
        blend = 0.5 * w
        shift = self.shift[j] * (1.0 - blend) + self.shift[k] * blend
        full = (self.full[j] * (1.0 - blend) + self.full[k] * blend) * (0.9 + 0.1 * math.sin(math.pi * t))
        return shift, full

    def ragged(self, y):
        """The straws' ends: a short ragged line along the butt."""
        return 1.4 * noise.noise(Vector((y * 0.45, self.seed, 0.0))) + 0.6 * noise.noise(Vector((y * 1.7, self.seed, 3.0)))


def geo_thatch(rng):
    parts = []
    gauge = 22.0
    s_start = -26.0           # the eaves overhang the frame
    base_off = 9.0            # the battens / sticks stand this far above the slab's mid-plane
    courses = []
    s = s_start
    while s < ROOF_LEN - 10.0:
        courses.append(s)
        s += gauge + rng.uniform(-3.0, 3.0)
    for k, sb in enumerate(courses):
        m = Mesh()
        top = min(sb + 64.0, ROOF_LEN + 8.0)
        bun = Bundles(rng, -53.0, 53.0)

        def fn(sv, y, k=k, sb=sb, top=top, bun=bun):
            # a wedge: thick at the butts (sv = sb), thin where the next course covers it; the butts staggered bundle by bundle
            frac = (sv - sb) / max(top - sb, 1.0)
            shift, full = bun.at(y)
            butt = (shift + (bun.ragged(y) if frac < 0.08 else 0.0)) * (1.0 - frac)
            thick = 19.0 * (1.0 - 0.65 * frac) * (1.0 + (full - 1.0) * (1.0 - frac))
            off = base_off + k * 0.8 + thick + 1.2 * noise.noise(Vector((y * 0.06, sv * 0.06, k)))
            yy = y * (1.0 + 0.05 * (1.0 - frac))
            return roof_point(sv - butt, yy, off), ROOF_N
        sheet(m, fn, sb, top, -53.0, 53.0, 8, 56, lambda sv, y, sb=sb, top=top: 19.0 * (1.0 - 0.65 * (sv - sb) / max(top - sb, 1.0)),
              "Thatch", back=True, rim=True)
        courses_name = "Course%d" % (k + 1)
        parts.append((courses_name, m))
    # the ridge roll: a bundle laid along the top edge over the last course
    ridge = Mesh()
    r_pts = [roof_point(ROOF_LEN - 2.0, y, base_off + 16.0) for y in [-55.0 + 110.0 * i / 20 for i in range(21)]]
    tube(ridge, r_pts, 9.0, "Thatch", sides=12, cap_surf="Thatch", wobble=0.08)
    parts.append(("Ridge", ridge))
    # sways: hazel rods pinning the top course
    sways = Mesh()
    for k, s_rod in enumerate((ROOF_LEN - 30.0, ROOF_LEN - 58.0)):
        pts = [roof_point(s_rod, y, base_off + 13.5 + 0.6 * k) for y in [-52.0 + 104.0 * i / 16 for i in range(17)]]
        tube(sways, pts, 1.0, "Rod", sides=6)
    parts.append(("Sways", sways))
    return parts


def board_widths(rng, total, lo, hi):
    """Riven boards as they came off the log: each its own width, together filling the span."""
    ws = []
    while sum(ws) < total - lo:
        ws.append(rng.uniform(lo, hi))
    k = total / sum(ws)
    return [w * k for w in ws]


def geo_plank_floor(rng):
    joists = Mesh()
    jx = [x + rng.uniform(-1.5, 1.5) for x in (-32.0, 32.0)]
    for x in jx:
        beam(joists, Vector((x, -50.0, -6.5)), Vector((x, 50.0, -6.5)), 9.0, 9.0, seed=x, up=Vector((0, 0, 1)))
    parts = [("Joists", joists)]
    y = -50.0
    for i, w in enumerate(board_widths(rng, 100.0, 15.0, 25.0)):
        m = Mesh()
        y0, y1 = y + rng.uniform(0.15, 0.5), y + w - rng.uniform(0.15, 0.5)
        th = 3.2 * rng.uniform(0.9, 1.08)
        z = 5.0 - th * 0.5 - rng.uniform(0.0, 0.25)   # the top at Z +5 (the floor's frame), a board a hair proud or low
        beam(m, Vector((-50.0 - rng.uniform(0.0, 0.8), (y0 + y1) * 0.5, z)), Vector((50.0 + rng.uniform(0.0, 0.8), (y0 + y1) * 0.5, z)), (y1 - y0), th,
             surf="Plank", chamfer=rng.uniform(0.3, 0.7), seed=i * 3.7, wobble=0.25, up=Vector((0, 0, 1)))
        pegs(m, [Vector((x + rng.uniform(-0.8, 0.8), (y0 + y1) * 0.5 + rng.uniform(-2.0, 2.0), z + th * 0.5 + 0.1)) for x in jx], Vector((0, 0, 1)),
             r=0.9, length=0.6)
        parts.append(("Plank%d" % (i + 1), m))
        y += w
    return parts


def geo_wattle_door(rng):
    frame = Mesh()
    for y in (-46.0, 46.0):
        tube(frame, path(Vector((WX, y, -56.0)), Vector((WX, y, 150.0)), 16, seed=y, wander=0.6), lambda t: 4.2 * (1.0 - 0.1 * t), "Bark", sides=10)
    lintel = Mesh()
    tube(lintel, path(Vector((WX, -54.0, 144.0)), Vector((WX, 54.0, 144.0)), 12, seed=5.0, wander=0.5), 4.0, "Bark", sides=10)
    lashing(lintel, Vector((WX, -46.0, 144.0)), Vector((0, 1, 0)), 4.0, turns=5)
    lashing(lintel, Vector((WX, 46.0, 144.0)), Vector((0, 1, 0)), 4.0, turns=5, seed=0.7)
    sill = Mesh()
    tube(sill, path(Vector((WX, -50.0, -46.0)), Vector((WX, 50.0, -46.0)), 10, seed=6.0, wander=0.3), 3.6, "Bark", sides=10)
    # the hurdle: four stakes and rods woven through, hung from the left post on two rawhide loops
    ys = by_eye([-34.0, -12.0, 12.0, 34.0], rng, 2.5)
    leaf_stakes = stakes_row(ys, -45.0, 134.0, 1.6, rng, seed=8)
    rods = weave_courses(ys, packed_courses(-43.0, 131.0, 1.0, rng), 1.6, rng, seed=9, y_end=37.0)
    hinges = Mesh()
    for z in (110.0, -20.0):
        pts = [Vector((WX + 4.0 * math.cos(a), -40.0 + 4.0 * math.sin(a), z)) for a in [TAU * i / 16 for i in range(17)]]
        tube(hinges, pts, 0.8, "Hide", sides=6)
    parts = [("Posts", frame), ("Sill", sill), ("Lintel", lintel), ("LeafStakes", leaf_stakes)]
    for i, g in enumerate(group(rods, 4)):
        parts.append(("LeafWeave%d" % (i + 1), g))
    parts.append(("Hinges", hinges))
    return parts


def geo_plank_door(rng):
    posts = Mesh()
    for y in (-44.0, 44.0):
        beam(posts, Vector((WX, y, -50.0)), Vector((WX, y, 150.0)), 14.0, 12.0, seed=y, up=Vector((1, 0, 0)))
    lintel = Mesh()
    beam(lintel, Vector((WX, -56.0, 142.0)), Vector((WX, 56.0, 142.0)), 14.0, 16.0, seed=4.0, up=Vector((0, 0, 1)))
    sill = Mesh()
    beam(sill, Vector((WX, -50.0, -46.0)), Vector((WX, 50.0, -46.0)), 14.0, 8.0, seed=5.0, up=Vector((0, 0, 1)))
    leaf = Mesh()
    y = -37.0
    for i, w in enumerate(board_widths(rng, 75.0, 14.0, 23.0)):
        beam(leaf, Vector((WX, y + w * 0.5, -45.0 + rng.uniform(0.0, 1.0))), Vector((WX, y + w * 0.5, 133.0 - rng.uniform(0.0, 1.5))), 3.0,
             w - rng.uniform(0.3, 0.7), surf="Plank", chamfer=0.4, seed=i * 2.1, wobble=0.2, up=Vector((0, 1, 0)))
        y += w
    battens = Mesh()
    for z in (-25.0, 112.0):
        beam(battens, Vector((WX + 3.0, -36.0, z)), Vector((WX + 3.0, 36.0, z)), 3.0, 11.0, surf="Plank", chamfer=0.5, seed=z, up=Vector((0, 0, 1)))
    beam(battens, Vector((WX + 3.0, -30.0, -18.0)), Vector((WX + 3.0, 30.0, 104.0)), 3.0, 10.0, surf="Plank", chamfer=0.5, seed=9.0, up=Vector((1, 0, 0)))
    pg = Mesh()
    pegs(pg, [Vector((WX + 4.6, yy, z)) for z in (-25.0, 112.0) for yy in (-28.0, -9.0, 10.0, 28.0)], Vector((1, 0, 0)), r=0.8, length=1.0)
    return [("Posts", posts), ("Sill", sill), ("Lintel", lintel), ("Leaf", leaf), ("Battens", battens), ("Pegs", pg)]


def leanto_frame(rng, half_w):
    """Two forked uprights at the front (X 0), a ridge pole on them at 150 cm, rafters slanting back to the ground at X -180."""
    up = Mesh()
    for y in (-half_w + 5.0, half_w - 5.0):
        tube(up, path(Vector((0.0, y, -15.0)), Vector((0.0, y, 152.0)), 14, seed=y * 0.1, wander=1.0), lambda t: 4.6 * (1.0 - 0.15 * t), "Bark", sides=10)
        # the fork
        tube(up, path(Vector((0.0, y, 138.0)), Vector((0.0, y + 9.0, 160.0)), 6, seed=y * 0.2), lambda t: 2.6 * (1.0 - 0.4 * t), "Bark", sides=8)
    ridge = Mesh()
    tube(ridge, path(Vector((0.0, -half_w - 8.0, 150.0)), Vector((0.0, half_w + 8.0, 150.0)), 16, seed=3.3, wander=1.2), 3.8, "Bark", sides=10)
    rafters = Mesh()
    n = 6
    for i in range(n):
        y = -half_w + 2.0 * half_w * i / (n - 1) + (rng.uniform(-6.0, 6.0) if 0 < i < n - 1 else 0.0)
        r = 2.8 * rng.uniform(0.8, 1.2)
        tube(rafters, path(Vector((12.0 + rng.uniform(-4, 4), y, 160.0 + rng.uniform(-3, 4))), Vector((-190.0 + rng.uniform(-8, 8), y + rng.uniform(-3, 3), -8.0)),
                           14, seed=i * 1.3, wander=1.2), lambda t, r=r: r * (1.0 - 0.3 * t), "Bark", sides=8)
    return up, ridge, rafters


def slope_point(s, y, off):
    """The lean-to's roof plane: from the ridge (X 0, Z 150) down to the ground at X -180; s down the slope (cm), off out of it."""
    top = Vector((0.0, 0.0, 150.0))
    down = Vector((-180.0, 0.0, -150.0)).normalized()
    n = Vector((150.0, 0.0, -180.0)).normalized() * -1.0   # outward (up and back)
    return top + down * s + Vector((0, y, 0)) + n * off, n


def geo_thatch_leanto(rng):
    up, ridge, rafters = leanto_frame(rng, 110.0)
    lattice = Mesh()
    L = Vector((180.0, 0, 150.0)).length
    for k in range(7):
        s = 15.0 + k * (L - 25.0) / 6.0 + (rng.uniform(-4.0, 4.0) if 0 < k < 6 else 0.0)
        y0, y1 = -118.0 - rng.uniform(0.0, 6.0), 118.0 + rng.uniform(0.0, 6.0)
        pts = [slope_point(s, y, 4.0)[0] for y in [y0 + (y1 - y0) * i / 16 for i in range(17)]]
        tube(lattice, pts, 1.2 * rng.uniform(0.8, 1.2), "Rod", sides=6)
    parts = [("Uprights", up), ("Ridge", ridge), ("Rafters", rafters), ("Lattice", lattice)]
    gauge = 26.0
    courses = []
    s = L + 12.0
    while s > 10.0:
        courses.append(s)
        s -= gauge + rng.uniform(-3.0, 3.0)
    for k, sb in enumerate(courses):
        m = Mesh()
        top = max(sb - 70.0, -14.0)
        bun = Bundles(rng, -120.0, 120.0)

        def fn(sv, y, k=k, sb=sb, top=top, bun=bun):
            frac = (sb - sv) / max(sb - top, 1.0)
            shift, full = bun.at(y)
            butt = (shift + (bun.ragged(y) if frac < 0.08 else 0.0)) * (1.0 - frac)
            thick = 16.0 * (1.0 - 0.6 * frac) * (1.0 + (full - 1.0) * (1.0 - frac))
            p, n = slope_point(sv + butt, y * (1.0 + 0.04 * (1.0 - frac)), 6.0 + k * 0.6 + thick + 1.2 * noise.noise(Vector((y * 0.05, sv * 0.05, k))))
            return p, n
        sm = Mesh()
        sheet(sm, fn, top, sb, -120.0, 120.0, 8, 110, lambda sv, y, sb=sb, top=top: 16.0 * (1.0 - 0.6 * (sb - sv) / max(sb - top, 1.0)), "Thatch")
        m.extend(sm)
        parts.append(("Thatch%d" % (k + 1), m))
    return parts


def hide_outline(u, seed, amp):
    """How far a hide's edge stands in or out at u (0..1 along it): the animal's outline -- a slow wave, a leg or two, a ragged trim."""
    leg = sum(math.exp(-((u - c) / 0.06) ** 2) for c in (0.18 + 0.1 * math.sin(seed), 0.8 + 0.08 * math.cos(seed)))
    return amp * (noise.noise(Vector((u * 2.2, seed, 0.5))) + 0.6 * leg + 0.25 * noise.noise(Vector((u * 11.0, seed, 4.0))))


def hide_panel(m, s0, s1, y0, y1, rng, seed=0.0, lift=5.0, sag=2.5):
    """A hide stretched over the rafters, sagging between them by its own amount, wrinkled where it was stretched unevenly, its edge the
    animal's outline (hide_outline) rather than a cut rectangle."""
    sag *= rng.uniform(0.7, 1.4)
    amp = 0.6 + rng.uniform(0.3, 1.0)

    def fn(s, y):
        u, v = (s - s0) / (s1 - s0), (y - y0) / (y1 - y0)
        # the outline: the ends (s0 / s1) and the sides (y0 / y1) each pushed in or out along their length
        s = s + (1.0 - u) * hide_outline(v, seed, 3.5) - u * hide_outline(v, seed + 3.3, 3.5)   # the overlaps stay closed
        y = y + (1.0 - v) * hide_outline(u, seed + 6.1, 4.0) - v * hide_outline(u, seed + 8.7, 4.0)
        between = abs(math.sin(math.pi * (y + 110.0) / 44.0))
        wrinkle = amp * noise.noise(Vector((s * 0.05, y * 0.05, seed))) + 0.35 * noise.noise(Vector((s * 0.16, y * 0.04, seed + 2.0)))
        p, n = slope_point(s, y, lift - sag * between + wrinkle)
        return p, n
    sheet(m, fn, s0, s1, y0, y1, 16, 30, 0.6, "Hide")
    return m


def geo_hide_leanto(rng):
    up, ridge, rafters = leanto_frame(rng, 105.0)
    L = Vector((180.0, 0, 150.0)).length
    parts = [("Uprights", up), ("Ridge", ridge), ("Rafters", rafters)]
    spans = [(-8.0, L * 0.42, -110.0, 110.0), (L * 0.36, L * 0.74, -110.0, 110.0), (L * 0.68, L + 6.0, -110.0, 110.0)]
    for i, (s0, s1, y0, y1) in enumerate(spans):
        m = Mesh()
        hide_panel(m, s0, s1, y0, y1, rng, seed=i * 4.1, lift=4.5 + i * 0.8)
        parts.append(("Hide%d" % (i + 1), m))
    lash = Mesh()
    for i in range(6):
        y = -105.0 + 210.0 * i / 5
        p, n = slope_point(4.0, y, 3.0)
        lashing(lash, p, Vector((0, 1, 0)), 3.0, turns=4, width=3.0, seed=i)
    parts.append(("Lashings", lash))
    return parts


TENT_R = 160.0
TENT_H = 270.0
TENT_POLES = 8
TENT_SAG = [random.Random(31 + i).uniform(0.6, 1.45) for i in range(TENT_POLES)]   # each span between two poles sags its own amount


def tent_cone(theta, z, off=0.0):
    """The cover's surface at angle theta and height z (sagging inward between the poles, each span its own amount, lumpy where the hides
    were pulled unevenly); outward normal."""
    r = TENT_R * (1.0 - z / TENT_H)
    gap = int((theta % TAU) / (TAU / TENT_POLES)) % TENT_POLES
    sag = 1.0 - 0.07 * TENT_SAG[gap] * abs(math.sin(TENT_POLES * theta * 0.5))   # 0 at the poles (theta = i * tau / 8), deepest between
    lumps = 1.4 * noise.noise(Vector((math.cos(theta) * 1.6, math.sin(theta) * 1.6, z * 0.018))) * min(1.0, (TENT_H - z) / 60.0)
    rr = r * sag + off + lumps
    p = Vector((rr * math.cos(theta), rr * math.sin(theta), z))
    n = Vector((math.cos(theta), math.sin(theta), TENT_R / TENT_H)).normalized()
    return p, n


def tent_seam(key, x, amp):
    """How far a seam between two hides wanders (cm, or radians round the cone) at x: one function per seam, so the two hides it joins follow
    the same line and their overlap stays closed."""
    return amp * (noise.noise(Vector((x, key * 3.7, 1.3))) + 0.35 * noise.noise(Vector((x * 4.0, key * 3.7, 6.1))))


def geo_hide_tent(rng):
    poles_a, poles_b = Mesh(), Mesh()
    for i in range(TENT_POLES):
        th = TAU * i / TENT_POLES
        # under the cover: its line runs 5 cm inside the cover's radius, through the apex and out past it where they cross, each pole its own
        # length past the crossing and its own thickness
        rf = TENT_R * (1.0 + 12.0 / TENT_H) - 5.0 - rng.uniform(0.0, 5.0)
        foot = Vector((rf * math.cos(th), rf * math.sin(th), -12.0))
        e = rng.uniform(14.0, 48.0)
        tip = Vector((-TENT_R * e / TENT_H * math.cos(th), -TENT_R * e / TENT_H * math.sin(th), TENT_H + e))
        target = poles_a if i % 2 == 0 else poles_b
        r = 3.6 * rng.uniform(0.82, 1.18)
        tube(target, path(foot, tip, 18, seed=i * 2.7, wander=0.8), lambda t, r=r: r * (1.0 - 0.45 * t), "Bark", sides=9)
    tie = Mesh()
    lashing(tie, Vector((0.0, 0.0, TENT_H - 6.0)), Vector((0, 0, 1)), 9.0, turns=7, width=8.0)
    parts = [("Poles1", poles_a), ("Poles2", poles_b), ("Tie", tie)]
    door = (0.04, 0.75)   # the doorway's angle range (radians from +X): between the first two poles
    bands = [(8.0, 80.0), (74.0, 150.0), (144.0, 222.0)]
    for bi, (z0, z1) in enumerate(bands):
        m = Mesh()
        for seg in range(4):
            a0 = door[1] + (TAU - (door[1] - door[0])) * seg / 4.0 - (0.02 if seg else 0.0)
            a1 = door[1] + (TAU - (door[1] - door[0])) * (seg + 1) / 4.0 + 0.02
            if bi > 0:   # above the doorway the cover closes round
                a0 = TAU * seg / 4.0 - 0.02
                a1 = TAU * (seg + 1) / 4.0 + 0.02

            def fn(z, th, bi=bi, seg=seg, z0=z0, z1=z1, a0=a0, a1=a1):
                # the hide's edges: the seams above and below it and either side wander, shared with the hide on the other side of each
                fz, fa = (z - z0) / (z1 - z0), (th - a0) / (a1 - a0)
                z = z + (1.0 - fz) * tent_seam(bi, th * 1.3, 5.0 if bi else 2.0) + fz * tent_seam(bi + 1, th * 1.3, 5.0 if bi < 2 else 1.0)
                right = (seg + 1) % 4 if bi else seg + 1
                th = th + (1.0 - fa) * tent_seam(10 + bi * 5 + seg, z * 0.02, 0.035) + fa * tent_seam(10 + bi * 5 + right, z * 0.02, 0.035)
                p, n = tent_cone(th, z, 2.0 + bi * 0.5 + 0.5 * noise.noise(Vector((th * 3.0, z * 0.05, bi))))
                return p, n
            sm = Mesh()
            sheet(sm, fn, z0, z1, a0, a1, 6, 12, 0.6, "Hide")
            # UVs: the sheet's t is an angle; rescale it to cm along the circumference at the band's middle height
            rmid = TENT_R * (1.0 - (z0 + z1) * 0.5 / TENT_H)
            sm.uv = [[(u * rmid, v) for (u, v) in face] for face in sm.uv]
            m.extend(sm)
        parts.append(("Cover%d" % (bi + 1), m))
    flap = Mesh()

    def flap_fn(z, th):
        p, n = tent_cone(th, z, 3.2)
        return p + n * 6.0 * math.sin(math.pi * (z - 8.0) / 70.0), n
    sheet(flap, flap_fn, 8.0, 78.0, door[0] - 0.05, door[1] - 0.25, 5, 6, 0.6, "Hide")
    parts.append(("DoorFlap", flap))
    pegs_m = Mesh()
    for i in range(12):
        th = TAU * (i + 0.25) / 12
        if door[0] - 0.05 < th < door[1] + 0.05:
            continue
        p, n = tent_cone(th, 10.0, 6.0)
        lean = Vector((rng.uniform(-1.5, 1.5), rng.uniform(-1.5, 1.5), 0.0))
        tube(pegs_m, [p + Vector((0, 0, rng.uniform(5.0, 10.0))) + n * 2.0 + lean, p + Vector((0, 0, -12.0)) + n * 6.0], 1.2 * rng.uniform(0.8, 1.2),
             "Hewn", sides=6)
        blob(pegs_m, p + n * rng.uniform(8.0, 12.0) + Vector((0, 0, 3.0)), (rng.uniform(5.5, 10.0), rng.uniform(5.0, 8.5), rng.uniform(3.5, 6.0)),
             "Mortar", rot=Matrix.Rotation(rng.uniform(0, TAU), 3, "Z"), seed=i * 1.9, rough=0.25, rings=4, segs=8)
    parts.append(("Pegs", pegs_m))
    return parts


GEOMETRY = {
    "wattle_wall": geo_wattle_wall, "moss_daub": geo_moss_daub, "clay_daub": geo_clay_daub, "pole_post": geo_pole_post, "pole_rail": geo_pole_rail,
    "timber_post": geo_timber_post, "timber_beam": geo_timber_beam, "timber_brace": geo_timber_brace,
    "cobble_foundation": geo_cobble_foundation, "cobble_wall": geo_cobble_wall, "cobble_infill": geo_cobble_infill, "thatch": geo_thatch,
    "plank_floor": geo_plank_floor, "wattle_door": geo_wattle_door, "plank_door": geo_plank_door, "thatch_leanto": geo_thatch_leanto,
    "hide_leanto": geo_hide_leanto, "hide_tent": geo_hide_tent,
}

# Support posts (AMOBuildPieceActor::SupportMesh: 100 cm tall on its foot at the origin) for the floors that grow them.
SUPPORTS = {
    "PoleSupport": lambda: tube(Mesh(), path(Vector((0, 0, 0)), Vector((0, 0, 100.0)), 8, seed=7.0, wander=0.4), 3.6, "Bark", sides=10),
    "TimberSupport": lambda: beam(Mesh(), Vector((0, 0, 0)), Vector((0, 0, 100.0)), 12.0, 12.0, seed=8.0, up=Vector((1, 0, 0))),
}


# ------------------------------------------------------------------------------------------------------------------------------------------
# Blender objects, export, preview
# ------------------------------------------------------------------------------------------------------------------------------------------

_SURF_MATS = {}


def surf_mat(name):
    """The preview material for a surface: its baked textures when they exist (what the editor will show), else a flat stand-in."""
    if name in _SURF_MATS:
        return _SURF_MATS[name]
    mat = bpy.data.materials.new("Surf_" + name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    base = os.path.join(SURF_DIR, "T_Surf_%s_" % name)
    if os.path.isfile(base + "BaseColor.png"):
        col = nt.nodes.new("ShaderNodeTexImage")
        col.image = bpy.data.images.load(base + "BaseColor.png")
        nt.links.new(col.outputs["Color"], bsdf.inputs["Base Color"])
        orm = nt.nodes.new("ShaderNodeTexImage")
        orm.image = bpy.data.images.load(base + "ORM.png")
        orm.image.colorspace_settings.name = "Non-Color"
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(orm.outputs["Color"], sep.inputs[0])
        nt.links.new(sep.outputs[1], bsdf.inputs["Roughness"])
        nrm = nt.nodes.new("ShaderNodeTexImage")
        nrm.image = bpy.data.images.load(base + "Normal.png")
        nrm.image.colorspace_settings.name = "Non-Color"
        sepn = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(nrm.outputs["Color"], sepn.inputs[0])
        comb = nt.nodes.new("ShaderNodeCombineColor")
        nt.links.new(sepn.outputs[0], comb.inputs[0])
        inv = nt.nodes.new("ShaderNodeMath")
        inv.operation = "SUBTRACT"
        inv.inputs[0].default_value = 1.0
        nt.links.new(sepn.outputs[1], inv.inputs[1])
        nt.links.new(inv.outputs[0], comb.inputs[1])
        nt.links.new(sepn.outputs[2], comb.inputs[2])
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(comb.outputs[0], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    else:
        bsdf.inputs["Base Color"].default_value = BI.srgb({"Moss": "#4d6a22", "Daub": "#87705a", "Thatch": "#8f7c50", "Hide": "#9a7a52",
                                                           "Mortar": "#6e6052", "Cord": "#7a6448"}.get(name, "#6a5a48"))
        bsdf.inputs["Roughness"].default_value = 0.8
    tint_preview(nt, bsdf, name)
    _SURF_MATS[name] = mat
    return mat


def tint_preview(nt, bsdf, name):
    """The tint as M_Surf_Base applies it: base colour x lerp(light range, tint.x), then toward its weather colour x its brightness by
    tint.y x weather_amount."""
    spec = BP.SURFACES[name]
    src = bsdf.inputs["Base Color"].links[0].from_socket if bsdf.inputs["Base Color"].links else None
    if src is None:
        rgb = nt.nodes.new("ShaderNodeRGB")
        rgb.outputs[0].default_value = bsdf.inputs["Base Color"].default_value
        src = rgb.outputs[0]
    uv = nt.nodes.new("ShaderNodeUVMap")
    uv.uv_map = "Tint"
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uv.outputs["UV"], sep.inputs[0])
    lf = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(sep.outputs[0], lf.inputs["Value"])
    lf.inputs["To Min"].default_value, lf.inputs["To Max"].default_value = spec["light"]
    lit = nt.nodes.new("ShaderNodeVectorMath")
    lit.operation = "SCALE"
    nt.links.new(src, lit.inputs[0])
    nt.links.new(lf.outputs["Result"], lit.inputs["Scale"])
    bw = nt.nodes.new("ShaderNodeRGBToBW")
    nt.links.new(lit.outputs["Vector"], bw.inputs[0])
    aged = nt.nodes.new("ShaderNodeVectorMath")
    aged.operation = "SCALE"
    aged.inputs[0].default_value = spec["weather"]
    nt.links.new(bw.outputs[0], aged.inputs["Scale"])
    amt = nt.nodes.new("ShaderNodeMath")
    amt.operation = "MULTIPLY"
    nt.links.new(sep.outputs[1], amt.inputs[0])
    amt.inputs[1].default_value = spec["weather_amount"]
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(amt.outputs[0], mix.inputs["Factor"])
    nt.links.new(lit.outputs["Vector"], mix.inputs[6])
    nt.links.new(aged.outputs["Vector"], mix.inputs[7])
    nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])


def to_object(name, mesh):
    """The mesh into Blender's frame (Y negated, winding reversed so the faces still point out) as one object, a slot per surface."""
    verts = [(p.x, -p.y, p.z) for p in mesh.v]
    faces = [tuple(reversed(f)) for f in mesh.f]
    uvs = [list(reversed(u)) for u in mesh.uv]
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    layer = me.uv_layers.new(name="UVMap")
    layer.data.foreach_set("uv", [c for face in uvs for uv in face for c in uv])
    # the second channel: each corner's tint (light, weather), read by M_Surf_Base
    tint = me.uv_layers.new(name="Tint")
    tint.data.foreach_set("uv", [c for face in faces for vi in face for c in mesh.t[vi]])
    surfs = []
    for s in mesh.surf:
        if s not in surfs:
            surfs.append(s)
    for s in surfs:
        me.materials.append(surf_mat(s))
    me.polygons.foreach_set("material_index", [surfs.index(s) for s in mesh.surf])
    me.polygons.foreach_set("use_smooth", mesh.smooth)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, surfs


def export_part(ob, path_):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path_, use_selection=True, object_types={"MESH"}, apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
                             mesh_smooth_type="FACE", use_tspace=False, add_leaf_bones=False, bake_anim=False, use_mesh_modifiers=False)


def run_piece(piece):
    global VARY
    spec = BP.PIECES[piece]
    VARY = Vary(piece)
    BI.setup_scene()
    out_dir = os.path.join(OUT)
    os.makedirs(out_dir, exist_ok=True)
    rng = random.Random(zlib.crc32(piece.encode()) & 0xFFFF)
    parts = GEOMETRY[spec["geometry"]](rng)
    info = {"piece": piece, "parts": [], "supports": None}
    objects = []
    for i, (pname, mesh) in enumerate(parts):
        obname = "%s_%02d_%s" % (piece, i, pname)
        ob, surfs = to_object(obname, mesh)
        fbx = os.path.join(out_dir, obname + ".fbx")
        export_part(ob, fbx)
        objects.append(ob)
        info["parts"].append({"name": pname, "fbx": os.path.basename(fbx), "mesh": "SM_" + obname, "surfaces": surfs, "triangles": mesh.tris()})
    lo = Vector((1e9, 1e9, 1e9))
    hi = -lo
    for ob in objects:
        for v in ob.data.vertices:
            p = Vector((v.co.x, -v.co.y, v.co.z))
            lo = Vector(map(min, lo, p))
            hi = Vector(map(max, hi, p))
    info["bounds_ue"] = [list(lo), list(hi)]
    info["triangles"] = sum(p["triangles"] for p in info["parts"])
    # previews: the finished piece from three sides, then three stages of its build
    cam = BI.preview_rig()
    c = (lo + hi) * 0.5
    size = max(hi - lo) * 1.25
    blc = Vector((c.x, -c.y, c.z))
    shots = []
    for k, d in enumerate(((1.0, -0.8, 0.45), (-1.0, -0.6, 0.35), (0.6, 1.0, 0.5))):
        shots.append(BI.shoot(cam, blc, d, size, os.path.join(out_dir, "_v%d.png" % k), res=(720, 720)))
    n = len(objects)
    for k, frac in enumerate((0.25, 0.5, 0.75)):
        built = max(1, int(math.floor(frac * n)))
        for i, ob in enumerate(objects):
            ob.hide_render = i >= built
        shots.append(BI.shoot(cam, blc, (1.0, -0.8, 0.45), size, os.path.join(out_dir, "_s%d.png" % k), res=(720, 720)))
    for ob in objects:
        ob.hide_render = False
    info["preview"] = BI.sheet(shots, os.path.join(out_dir, "%s_preview.png" % piece), 3)
    with open(os.path.join(out_dir, "%s.json" % piece), "w") as f:
        json.dump(info, f, indent=1)
    log("done: %d parts, %d triangles, bounds %s .. %s" % (len(parts), info["triangles"], [round(x) for x in lo], [round(x) for x in hi]))


def item_moss():
    """Moss01: an armful of moss torn from a rock, about 30 x 25 x 12 cm (0.5 kg damp), its underside darker with soil."""
    m = Mesh()
    rng = random.Random(77)
    for i in range(14):   # tufted cushions torn off together, as the moss daub's (Nov 3)
        c = Vector((rng.uniform(-10, 10), rng.uniform(-8, 8), rng.uniform(3.0, 5.5)))
        # the blob's poles turned sideways (its local X up), so no pinch shows on top
        rot = Matrix.Rotation(rng.uniform(0.0, TAU), 3, "Z") @ Matrix.Rotation(rng.uniform(-0.3, 0.3), 3, "X") @ Matrix.Rotation(math.pi / 2, 3, "Y")
        blob(m, c, (rng.uniform(2.0, 3.8), rng.uniform(3.5, 7), rng.uniform(4, 8)), "Moss", rot=rot, seed=i * 3.3, rough=0.2, rings=7, segs=14,
             freq=4.5)
    return m


ITEMS = {"Moss01": item_moss}


def run_item(item):
    global VARY
    VARY = Vary(item)
    BI.setup_scene()
    os.makedirs(OUT, exist_ok=True)
    mesh = ITEMS[item]()
    ob, surfs = to_object(item, mesh)
    export_part(ob, os.path.join(OUT, item + ".fbx"))
    cam = BI.preview_rig()
    shot = BI.shoot(cam, (0, 0, 5), (1.0, -0.8, 0.7), 40.0, os.path.join(OUT, "_i.png"), res=(720, 720))
    BI.sheet([shot], os.path.join(OUT, item + "_preview.png"), 1)
    with open(os.path.join(OUT, item + ".json"), "w") as f:
        json.dump({"item": item, "fbx": item + ".fbx", "surfaces": surfs, "triangles": mesh.tris()}, f, indent=1)
    log("done: item %s, %d triangles" % (item, mesh.tris()))


def run_supports():
    """The support posts every floor shares (AMOBuildPieceActor::SupportMesh): <out_dir>/<Name>.fbx."""
    global VARY
    VARY = Vary("supports")
    BI.setup_scene()
    os.makedirs(OUT, exist_ok=True)
    for name, make in SUPPORTS.items():
        ob, surfs = to_object("SM_" + name, make())
        export_part(ob, os.path.join(OUT, name + ".fbx"))
    with open(os.path.join(OUT, "Supports.json"), "w") as f:
        json.dump({"supports": list(SUPPORTS)}, f)
    log("done: %d support posts" % len(SUPPORTS))


if __name__ == "__main__":
    if JOB == "supports":
        run_supports()
    elif JOB == "surfaces":
        run_surfaces()
    elif JOB in ITEMS:
        run_item(JOB)
    elif JOB in BP.PIECES:
        run_piece(JOB)
    else:
        log("unknown job")
