"""Collision status of every mesh the DataTables can place in the world (TBLAUDIT lines):
  DT_Biomes species meshes (+ their DT_ResourceNodes type) and DT_ResourceNodes mesh variations.
    python Tools/ue.py py --file Content/Python/audit_table_mesh_collision.py
A mesh BLOCKS the pawn only if it has simple collision geometry (convex / box / sphere / capsule) or its trace flag is USE_COMPLEX_AS_SIMPLE.
"""
import re
import unreal

ESL = unreal.EditorStaticMeshLibrary


def status(mesh):
    simple = ESL.get_simple_collision_count(mesh)
    convex = ESL.get_convex_collision_count(mesh)
    flag = "?"
    try:
        flag = mesh.get_editor_property("body_setup").get_editor_property("collision_trace_flag").name
    except Exception:
        pass
    blocks = (max(simple, 0) + max(convex, 0)) > 0 or flag == "CTF_USE_COMPLEX_AS_SIMPLE"
    return blocks, simple, convex, flag


def paths_in(text):
    return re.findall(r'"(/[^"]+?\.[^"]+?)"', text or "")


seen = {}
# --- resource nodes
rn = unreal.load_asset("/MOFramework/Data/DT_Resources")
rtype = {}
if rn:
    csv = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(rn)
    import csv as csvmod, io, sys
    csvmod.field_size_limit(2**31 - 1)
    rows = list(csvmod.reader(io.StringIO(csv)))
    hdr = rows[0]
    ti = hdr.index("ResourceType") if "ResourceType" in hdr else None
    mi = hdr.index("MeshVariations") if "MeshVariations" in hdr else None
    for r in rows[1:]:
        t = r[ti] if ti is not None else "?"
        rtype[r[0]] = t
        for p in paths_in(r[mi]) if mi is not None else []:
            seen.setdefault(p, set()).add("node:%s(%s)" % (r[0], t))
# --- biome species
bio = unreal.load_asset("/MOFramework/Data/DT_Biomes")
if bio:
    csv = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(bio)
    import csv as csvmod, io, sys
    csvmod.field_size_limit(2**31 - 1)
    rows = list(csvmod.reader(io.StringIO(csv)))
    si = rows[0].index("Species")
    for r in rows[1:]:
        for m in re.finditer(r'Mesh="([^"]+)".*?ResourceNodeId="?([^,")]*)"?', r[si]):
            seen.setdefault(m.group(1), set()).add("biome:%s res=%s(%s)" % (r[0], m.group(2), rtype.get(m.group(2), "decorative")))

bad = 0
for path in sorted(seen):
    mesh = unreal.load_asset(path.split(".")[0]) if path else None
    if mesh is None:
        print("TBLAUDIT MISSING %s used by %s" % (path, ",".join(sorted(seen[path]))))
        continue
    blocks, simple, convex, flag = status(mesh)
    kinds = ",".join(sorted(seen[path]))
    wants_block = any(k.endswith("(Rock)") or k.endswith("(Ore)") or k.endswith("(Tree)") for k in seen[path])
    verdict = "BLOCKS " if blocks else ("NOBLOCK-BUG" if wants_block else "noblock")
    bad += 1 if (wants_block and not blocks) else 0
    print("TBLAUDIT %-11s %s simple=%d convex=%d trace=%s used by %s" % (verdict, path.rsplit("/", 1)[-1].split(".")[0], simple, convex, flag, kinds))
print("TBLAUDIT summary: %d meshes, %d solid-type meshes WITHOUT collision geometry" % (len(seen), bad))
