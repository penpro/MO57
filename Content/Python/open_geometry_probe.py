"""Which scattered rocks and cliffs are OPEN geometry (one-sided scans with no back or no underside), measured -- BR25-2 (Wes, Oct 24 2026: "remove all
open faced geometry assets, log them for rebuilding in trellis to be closed geometry").

    python Tools/ue.py py --file Content/Python/open_geometry_probe.py      (prints OPENGEO lines, most open first; writes Saved/open_geometry.json)

The measure: the area-weighted mean of the triangle normals, |sum(n * area)| / sum(area), on LOD0's render sections (front-facing by the vertex normals,
as cliff_face_probe.py). A closed rock's faces point every way and cancel: ~0 (MassiveNordicCoastalCliff 0.00, the TRELLIS formations); a one-sided
sheet points one way: a rocky-ground scan up (it has no underside), a cliff slab out of its face (no back): 0.4-1.0. Its direction tells which.
Candidates: every DT_Biomes species (main and water-edge lists) tagged as a rock, a rock cover or debris (foliage cards are open by nature, and are
not what was asked).
"""
import json
import math
import os

import unreal

TAGS = ("MOResource_Rock", "MOBiomeCover_Rock", "MOBiomeCover_Debris")
OPEN_AT = 0.2

table = unreal.load_asset("/MOFramework/Data/DT_Biomes")
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
uses = {}
for row in rows:
    for field in ("Species", "WaterEdgeSpecies"):
        for e in row.get(field) or []:
            if e.get("HISMTag") in TAGS:
                uses.setdefault(e["Mesh"], set()).add(row["Name"])

results = []
for path, biomes in sorted(uses.items()):
    sm = unreal.load_asset(path.split(".")[0])
    if not sm:
        continue
    sx = sy = sz = total = 0.0
    tris_n = 0
    for s in range(sm.get_num_sections(0)):
        verts, tris, normals, _uvs, _tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
        for t in range(0, len(tris), 3):
            a, b, c = verts[tris[t]], verts[tris[t + 1]], verts[tris[t + 2]]
            e1x, e1y, e1z = b.x - a.x, b.y - a.y, b.z - a.z
            e2x, e2y, e2z = c.x - a.x, c.y - a.y, c.z - a.z
            nx, ny, nz = e1y * e2z - e1z * e2y, e1z * e2x - e1x * e2z, e1x * e2y - e1y * e2x
            l = math.sqrt(nx * nx + ny * ny + nz * nz)
            if l <= 0.0:
                continue
            vn = normals[tris[t]]
            if nx * vn.x + ny * vn.y + nz * vn.z < 0.0:
                nx, ny, nz = -nx, -ny, -nz
            area = 0.5 * l
            sx += nx / l * area
            sy += ny / l * area
            sz += nz / l * area
            total += area
            tris_n += 1
    strength = math.sqrt(sx * sx + sy * sy + sz * sz) / max(total, 1.0)
    up = sz / max(math.sqrt(sx * sx + sy * sy + sz * sz), 1e-9)
    ext = sm.get_bounds().box_extent
    kind = "open, faces up (no underside)" if strength >= OPEN_AT and up > 0.7 else ("open, faces sideways (no back)" if strength >= OPEN_AT else "closed")
    results.append({"mesh": path, "openness": round(strength, 3), "up": round(up, 2), "kind": kind, "triangles_lod0": tris_n,
                    "size_cm": [round(ext.x * 2), round(ext.y * 2), round(ext.z * 2)], "biomes": sorted(biomes)})
results.sort(key=lambda r: -r["openness"])
json.dump(results, open(os.path.join(unreal.Paths.project_saved_dir(), "open_geometry.json"), "w"), indent=1)
out("OPENGEO " + "\nOPENGEO ".join("%.2f %-30s %s  %s  in %s" % (r["openness"], r["kind"], r["mesh"].split(".")[-1], "x".join(map(str, r["size_cm"])), ",".join(r["biomes"]))
                                 for r in results))
