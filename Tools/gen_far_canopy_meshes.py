"""Generate the far-canopy proxy meshes as OBJ files (system Python, no editor):   python Tools/gen_far_canopy_meshes.py [outdir=Saved/GeneratedMeshes/farcanopy]

Four low-poly tree-CLUMP stand-ins (an egg-shaped lumpy crown on a short tapered trunk, ~130 triangles each) for the `MOFar_Canopy` tier. Units: cm, Z up, pivot at the foot of the trunk.
UV: crown V in 0..1, TRUNK V = -1 (the material reads that as "trunk"). Crown normals are smooth (outward, tilted up above the middle), trunk normals radial.
Import + material + palettes: Content/Python/make_far_canopy.py (the editor does the import).
"""
import math
import os
import random
import sys

# name: (crown width cm, total height cm, trunk fraction of the height, trunk radius cm). These are CLUMPS, not single trees: at 3-9 km one instance stands for a handful of trees, so it is wide and low
# (a uniformly scaled single tree would stand 50 m tall); the tier plants about 30 per hectare of forest instead of 200 trees.
SHAPES = {
    "Tall":  (1700.0, 2300.0, 0.20, 70.0),
    "Mid":   (1650.0, 1500.0, 0.20, 60.0),
    "Round": (2400.0, 1900.0, 0.22, 90.0),
    "Small": (1050.0, 900.0, 0.18, 40.0),
}
SEGMENTS = 8
RINGS = 5
LOBES = 4


def norm(x, y, z):
    l = math.sqrt(x * x + y * y + z * z) or 1.0
    return (x / l, y / l, z / l)


def build(width, height, trunk_frac, trunk_r, seed):
    """A clump = a main egg-shaped lobe plus LOBES-1 smaller lumps around it (a cauliflower silhouette instead of one smooth dome), on a short trunk."""
    rng = random.Random(seed)
    verts, uvs, normals, faces = [], [], [], []

    def add(pos, nrm, uv):
        verts.append(pos)
        normals.append(nrm)
        uvs.append(uv)
        return len(verts)   # OBJ is 1-based

    crown_z0 = height * trunk_frac * 0.75
    crown_h = height - crown_z0
    rx = width * 0.5

    def lobe(cx, cy, cz, lrx, lh):
        def radius_at(t):
            return lrx * math.pow(math.sin(math.pi * math.pow(t, 0.78)), 0.85)
        ring_ids = []
        for r in range(RINGS):
            t = (r + 0.5) / RINGS
            rr = radius_at(t)
            z = cz + t * lh
            ids = []
            for sidx in range(SEGMENTS):
                a = 2.0 * math.pi * sidx / SEGMENTS + 0.5 * r
                lump = 0.80 + 0.40 * rng.random()
                x, y = cx + math.cos(a) * rr * lump, cy + math.sin(a) * rr * lump
                tilt = (t - 0.45) * 1.4
                ids.append(add((x, y, z), norm(x - cx, y - cy, tilt * lrx * 0.9), (sidx / SEGMENTS, t)))
            ring_ids.append(ids)
        bottom = add((cx, cy, cz), (0.0, 0.0, -1.0), (0.5, 0.0))
        top = add((cx, cy, cz + lh), (0.0, 0.0, 1.0), (0.5, 1.0))
        for sidx in range(SEGMENTS):
            s2 = (sidx + 1) % SEGMENTS
            faces.append((bottom, ring_ids[0][s2], ring_ids[0][sidx]))
            faces.append((top, ring_ids[RINGS - 1][sidx], ring_ids[RINGS - 1][s2]))
            for r in range(RINGS - 1):
                a, b, c, d = ring_ids[r][sidx], ring_ids[r][s2], ring_ids[r + 1][s2], ring_ids[r + 1][sidx]
                faces.append((a, b, c))
                faces.append((a, c, d))

    lobe(0.0, 0.0, crown_z0, rx * 0.78, crown_h)                      # the main lobe
    for k in range(LOBES - 1):
        a = 2.0 * math.pi * (k / (LOBES - 1)) + rng.random() * 0.8
        d = rx * (0.45 + 0.25 * rng.random())
        lh = crown_h * (0.50 + 0.25 * rng.random())
        lobe(math.cos(a) * d, math.sin(a) * d, crown_z0 + crown_h * (0.05 + 0.25 * rng.random()), rx * (0.38 + 0.18 * rng.random()), lh)

    TS = 6
    trunk_h = crown_z0 + crown_h * 0.15
    base, tip = [], []
    for sidx in range(TS):
        a = 2.0 * math.pi * sidx / TS
        nx, ny = math.cos(a), math.sin(a)
        base.append(add((nx * trunk_r, ny * trunk_r, 0.0), (nx, ny, 0.0), (sidx / TS, -1.0)))
        tip.append(add((nx * trunk_r * 0.45, ny * trunk_r * 0.45, trunk_h), (nx, ny, 0.0), (sidx / TS, -1.0)))
    for sidx in range(TS):
        s2 = (sidx + 1) % TS
        faces.append((base[sidx], base[s2], tip[s2]))
        faces.append((base[sidx], tip[s2], tip[sidx]))
    return verts, uvs, normals, faces


def write_obj(path, name, verts, uvs, normals, faces):
    with open(path, "w", newline="\n") as f:
        f.write("o %s\n" % name)
        for v in verts:
            f.write("v %.3f %.3f %.3f\n" % v)
        for u in uvs:
            f.write("vt %.4f %.4f\n" % (u[0], u[1]))
        for n in normals:
            f.write("vn %.4f %.4f %.4f\n" % n)
        for a, b, c in faces:
            f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Saved", "GeneratedMeshes", "farcanopy")
    os.makedirs(out, exist_ok=True)
    seed = 11
    for name, (w, h, tf, tr) in SHAPES.items():
        verts, uvs, normals, faces = build(w, h, tf, tr, seed)
        seed += 7
        p = os.path.join(out, "SM_FarCanopy_%s.obj" % name)
        write_obj(p, "SM_FarCanopy_" + name, verts, uvs, normals, faces)
        print("wrote", p, len(verts), "verts", len(faces), "tris")


if __name__ == "__main__":
    main()
