"""Every rock-like mesh the world places, and how open each is (Wes, Nov 6: "there's still a bunch of open geometry assets being used, like the circular gravel assets around the caves").

    python Tools/ue.py py --file Content/Python/open_mesh_census.py      (writes Saved/open_mesh_census.json; prints OPENMESH lines, most open first)

Where they come from: DT_CaveScatter (the cave and bowl dressing), every DT_Biomes species tagged a rock, cover or debris, the static meshes DT_WorldFeatures' PCG graphs depend on (asset registry, two levels), and what the
compositions really spawn (Content/Python/data/worldfeature_meshes.json).
The measures, on LOD0's render sections (a Nanite mesh's fallback: the shape, not the detail):
  openness  |sum of area-weighted normals| / area, as Content/Python/open_geometry_probe.py: closed ~0, a one-sided sheet 0.3-1.0; `up` says which way its visible side faces.
  boundary  edges used by one triangle only, positions welded (a UV seam is not a hole), and their length against the perimeter of the mesh's box. Unlike openness it sees a closed-looking rock with a hole.
A foliage material (two-sided or a Foliage master) is left out: a leaf card is open by nature, and is not what was asked.
"""
import json
import math
import os

import unreal

OPEN_AT = 0.2


def weld(v):
    return (round(v.x, 1), round(v.y, 1), round(v.z, 1))


def measure(sm):
    sx = sy = sz = total = 0.0
    tris_n = 0
    edges = {}
    for s in range(sm.get_num_sections(0)):
        verts, tris, normals, _uvs, _tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
        keys = [weld(v) for v in verts]
        for t in range(0, len(tris), 3):
            ia, ib, ic = tris[t], tris[t + 1], tris[t + 2]
            a, b, c = verts[ia], verts[ib], verts[ic]
            e1x, e1y, e1z = b.x - a.x, b.y - a.y, b.z - a.z
            e2x, e2y, e2z = c.x - a.x, c.y - a.y, c.z - a.z
            nx, ny, nz = e1y * e2z - e1z * e2y, e1z * e2x - e1x * e2z, e1x * e2y - e1y * e2x
            l = math.sqrt(nx * nx + ny * ny + nz * nz)
            if l <= 0.0:
                continue
            vn = normals[ia]
            if nx * vn.x + ny * vn.y + nz * vn.z < 0.0:
                nx, ny, nz = -nx, -ny, -nz
            area = 0.5 * l
            sx += nx / l * area
            sy += ny / l * area
            sz += nz / l * area
            total += area
            tris_n += 1
            for p, q in ((keys[ia], keys[ib]), (keys[ib], keys[ic]), (keys[ic], keys[ia])):
                if p != q:
                    k = (p, q) if p < q else (q, p)
                    edges[k] = edges.get(k, 0) + 1
    strength = math.sqrt(sx * sx + sy * sy + sz * sz) / max(total, 1.0)
    up = sz / max(math.sqrt(sx * sx + sy * sy + sz * sz), 1e-9)
    boundary = [k for k, n in edges.items() if n == 1]
    boundary_cm = sum(math.dist(p, q) for p, q in boundary)
    return strength, up, tris_n, len(boundary), boundary_cm


def is_foliage(sm):
    for slot in sm.static_materials:
        m = slot.material_interface
        if not m:
            continue
        name = m.get_path_name().lower()
        base = m.get_base_material().get_path_name().lower() if m.get_base_material() else ""
        if "foliage" in name or "foliage" in base or "leaf" in name or "grass" in name or "fern" in name or "vine" in name or "bark" in name:
            return True
    return False


uses = {}


def use(path, where):
    if path and path != "None":
        uses.setdefault(path.split(".")[0], set()).add(where)


# The cave and bowl dressing.
for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset("/MOFramework/Data/DT_CaveScatter"))):
    use(row.get("Mesh"), "CaveScatter")

# The biomes' rocks, covers and debris.
TAGS = ("MOResource_Rock", "MOBiomeCover_Rock", "MOBiomeCover_Debris")
for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset("/MOFramework/Data/DT_Biomes"))):
    for field in ("Species", "WaterEdgeSpecies"):
        for e in row.get(field) or []:
            if e.get("HISMTag") in TAGS:
                use(e.get("Mesh"), "Biome")

# The world features' graphs: what their packages depend on, two levels down (a graph, then a subgraph or data asset).
registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True, include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
features = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset("/MOFramework/Data/DT_WorldFeatures")))
for row in features:
    text = json.dumps(row)
    seeds = set(tok.split(".")[0].strip("\"'\\") for tok in text.replace("\\\"", "\"").split("\"") if tok.startswith("/Game/") or tok.startswith("/MOFramework/"))
    seen = set()
    level = seeds
    for _ in range(3):
        nxt = set()
        for pkg in level:
            if pkg in seen:
                continue
            seen.add(pkg)
            for dep in registry.get_dependencies(pkg, options) or []:
                dep = str(dep)
                if dep.startswith("/Game/") or dep.startswith("/MOFramework/"):
                    nxt.add(dep)
        level = nxt
    for pkg in seen:
        for data in registry.get_assets_by_package_name(pkg) or []:
            if str(data.asset_class_path.asset_name) == "StaticMesh":
                use(pkg, "Feature")

# What the compositions really spawn: their meshes are point attributes inside PCG data the registry cannot see, so a PIE run wrote them down (Content/Python/worldfeature_meshes.py).
manifest = os.path.join(unreal.Paths.project_content_dir(), "Python", "data", "worldfeature_meshes.json")
for path in json.load(open(manifest, encoding="utf-8")):
    if path.startswith(("/Game/", "/MOFramework/")):
        use(path, "Composition")

results, skipped = [], 0
for path, where in sorted(uses.items()):
    sm = unreal.load_asset(path)
    if not isinstance(sm, unreal.StaticMesh):
        continue
    if is_foliage(sm):
        skipped += 1
        continue
    strength, up, tris_n, boundary, boundary_cm = measure(sm)
    ext = sm.get_bounds().box_extent
    box_perimeter = 4.0 * (ext.x + ext.y)
    results.append({"mesh": path, "openness": round(strength, 3), "up": round(up, 2), "boundaryEdges": boundary, "boundaryCm": round(boundary_cm),
                    "boundaryShare": round(boundary_cm / max(box_perimeter, 1.0), 2), "triangles_lod0": tris_n,
                    "nanite": bool(sm.get_editor_property("nanite_settings").enabled), "size_cm": [round(ext.x * 2), round(ext.y * 2), round(ext.z * 2)], "where": sorted(where)})
results.sort(key=lambda r: -r["openness"])
json.dump(results, open(os.path.join(unreal.Paths.project_saved_dir(), "open_mesh_census.json"), "w"), indent=1)
unreal.log_warning("OPENMESH %d meshes measured, %d foliage left out; %d with boundary edges, %d open by the normals" % (len(results), skipped, sum(1 for r in results if r["boundaryEdges"] > 0), sum(1 for r in results if r["openness"] >= OPEN_AT)))
