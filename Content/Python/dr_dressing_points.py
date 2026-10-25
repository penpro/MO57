"""Dressing points for the Dark Ruins cliff compositions, from their real geometry (Wes, Oct 25 2026: "we need stuff spawned on top of them
... add other stuff to the cliffs for detail, more rocks, etc to help mask the repetition").

For each assembly (its pieces and transforms from Saved/DarkRuinsDump/assemblies.json, written by Tools/darkruins_assembly_dump.py; the
same frame as its PCG data asset):
  TOP   the up-facing triangles (normal z >= UP_NZ) that are the highest UP-FACING surface at their spot (a 1 m grid of the heights of
        up-facing samples only: a wall rising beside a ledge does not hide it, an overhang's top above it does), sampled 1 per TOP_M2 square
        metres: where grass, moss, shrubs and loose rocks grow
  FOOT  the base outline at the ground the assembly was made on (its root's plane, z 0: the sample's assemblies stand on it, and TallPath's
        pieces reach 55 m below it into the sample's terrain): every vertex within BASE_CM of z 0, its convex hull pushed out FOOT_OUT_CM, a
        point every FOOT_STEP_CM: where the rock clusters (talus) sit
Writes Content/Python/data/dr_dressing_points.json ({assembly: {"top": [[x, y, z]], "foot": [[x, y, z, yaw]]}}) for
make_dr_compositions.py. Run in the editor (it reads the meshes' triangles); PIE not needed.

    python Tools/ue.py py --file Content/Python/dr_dressing_points.py --timeout 300
"""
import json
import math
import os
import random

import unreal

ASSEMBLIES = ["Mountain_01", "TallPath_01"]
DUMP = "D:/UEProjects/MO57/Saved/DarkRuinsDump/assemblies.json"
OUT = os.path.join(unreal.Paths.project_content_dir(), "Python", "data", "dr_dressing_points.json")
CELL = 100.0
TOP_M2 = 1.5
UP_NZ = 0.6
BASE_CM = 400.0
FOOT_OUT_CM = 250.0
FOOT_STEP_CM = 700.0


def triangles(piece):
    sm = unreal.load_asset(piece["mesh"].split(".")[0])
    if not sm:
        return []
    r = piece["rot"]
    xf = unreal.Transform(location=unreal.Vector(*piece["loc"]), rotation=unreal.Rotator(roll=r[0], pitch=r[1], yaw=r[2]),
                          scale=unreal.Vector(*piece["scale"]))
    out_tris = []
    for s in range(sm.get_num_sections(0)):
        verts, tris, _n, _uv, _t = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
        world = [unreal.MathLibrary.transform_location(xf, v) for v in verts]
        mirrored = piece["scale"][0] * piece["scale"][1] * piece["scale"][2] < 0
        for t in range(0, len(tris), 3):
            a, b, c = world[tris[t]], world[tris[t + 1]], world[tris[t + 2]]
            if mirrored:
                b, c = c, b
            out_tris.append(((a.x, a.y, a.z), (b.x, b.y, b.z), (c.x, c.y, c.z)))
    return out_tris


def normal(a, b, c):
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    l = math.sqrt(nx * nx + ny * ny + nz * nz)
    return (nx / l, ny / l, nz / l, l * 0.5) if l > 0 else (0, 0, 0, 0)


def hull(pts):
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts
    cross = lambda o, a, b: (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def dressing(name, pieces, rng):
    tris = []
    for p in pieces:
        tris += triangles(p)
    samples, top = [], {}
    for a, b, c in tris:
        nx, ny, nz, area = normal(a, b, c)
        if nz < UP_NZ or area <= 0:
            continue
        n = area / (TOP_M2 * 10000.0)
        count = int(n) + (1 if rng.random() < n - int(n) else 0)
        for v in (a, b, c):   # the corners mark the up-facing height too (small triangles get no sample of their own)
            k = (int(math.floor(v[0] / CELL)), int(math.floor(v[1] / CELL)))
            top[k] = max(top.get(k, -1e12), v[2])
        for _ in range(count):
            u, v = rng.random(), rng.random()
            if u + v > 1:
                u, v = 1 - u, 1 - v
            p = tuple(a[i] + (b[i] - a[i]) * u + (c[i] - a[i]) * v for i in range(3))
            samples.append(p)
            k = (int(math.floor(p[0] / CELL)), int(math.floor(p[1] / CELL)))
            top[k] = max(top.get(k, -1e12), p[2])
    top_pts = [[round(p[0], 1), round(p[1], 1), round(p[2], 1)] for p in samples
               if p[2] >= top[(int(math.floor(p[0] / CELL)), int(math.floor(p[1] / CELL)))] - 40.0 and p[2] > BASE_CM]
    low = 0.0
    base = [(round(v[0]), round(v[1])) for t in tris for v in t if abs(v[2]) <= BASE_CM]
    ring = hull(base)
    cx = sum(p[0] for p in ring) / len(ring)
    cy = sum(p[1] for p in ring) / len(ring)
    foot = []
    for i in range(len(ring)):
        (x0, y0), (x1, y1) = ring[i], ring[(i + 1) % len(ring)]
        seg = math.hypot(x1 - x0, y1 - y0)
        steps = max(1, int(seg // FOOT_STEP_CM))
        for s in range(steps):
            f = (s + rng.random() * 0.6) / steps
            x, y = x0 + (x1 - x0) * f, y0 + (y1 - y0) * f
            d = math.hypot(x - cx, y - cy) or 1.0
            x, y = x + (x - cx) / d * FOOT_OUT_CM, y + (y - cy) / d * FOOT_OUT_CM
            foot.append([round(x, 1), round(y, 1), round(low, 1), round(math.degrees(math.atan2(y - cy, x - cx)), 1)])
    out("DRDRESS %s: %d triangles, %d top points, base ring %d corners -> %d foot points, base z %.0f, top z %.0f" % (
        name, len(tris), len(top_pts), len(ring), len(foot), low, max(top.values()) if top else 0))
    return {"top": top_pts, "foot": foot}


dump = json.load(open(DUMP, encoding="utf-8"))
rng = random.Random(25)
result = {name: dressing(name, dump[name]["pieces"], rng) for name in ASSEMBLIES}
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(result, open(OUT, "w", encoding="utf-8"))
out("DRDRESS wrote %s" % OUT)
