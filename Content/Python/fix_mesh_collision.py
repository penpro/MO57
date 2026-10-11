"""Give every solid-kind mesh the data tables can place real collision geometry, and fix the PCG nodes that place rocks/trees with no-collision profiles.

    python Tools/ue.py py --file Content/Python/fix_mesh_collision.py        (editor open, PIE NOT running: saving is blocked in PIE)

Idempotent: a mesh that already blocks (simple shapes, or "use complex as simple") is left alone.
  * ROCK / ORE meshes  -> convex decomposition (a few hulls), so a boulder is as wide as it looks.
  * TREE meshes        -> one trunk capsule at the pivot (radius ~2.2% of the tree's height, 20..55 cm; length ~45% of the height, at most 6 m). A tree's collision must NOT be its
                          canopy: a bounding capsule of the whole tree would be an invisible wall 4 m from the trunk.
  * PCG graph nodes    -> native Static Mesh Spawner entries of rocks/trees that carry NoCollision / IgnoreOnlyPawn become BlockAll (the FreshWaterSpring boulders, the locust woodland).
The audit that finds the meshes: Content/Python/audit_table_mesh_collision.py; the regression test: MOFramework.Data.SolidMeshCollision.
Output lines start with FIXCOLL.
"""
import csv
import io
import re

import unreal

ESL = unreal.EditorStaticMeshLibrary
SOLID = ("Tree", "Rock", "Ore")


def blocks(mesh):
    """The pawn can hit it: simple shapes, or the render triangles ARE the collision. (The aggregate-geometry arrays are not read through Python: an empty struct array comes back as
    a one-element placeholder, which made every mesh look fine. The editor library's counts are right: -1 means "none".)"""
    body = mesh.get_editor_property("body_setup")
    if body is None:
        return False
    if body.get_editor_property("collision_trace_flag").name == "CTF_USE_COMPLEX_AS_SIMPLE":
        return True
    return max(ESL.get_simple_collision_count(mesh), 0) + max(ESL.get_convex_collision_count(mesh), 0) > 0


def table_rows(path):
    dt = unreal.load_asset(path)
    csv.field_size_limit(2 ** 31 - 1)
    return list(csv.reader(io.StringIO(unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(dt))))


def solid_meshes():
    """{mesh object path: (kind, who)} for every mesh placed as a Tree / Rock / Ore."""
    found = {}
    rtype = {}
    rows = table_rows("/MOFramework/Data/DT_Resources")
    hdr = rows[0]
    ti, mi = hdr.index("ResourceType"), hdr.index("MeshVariations")
    for r in rows[1:]:
        rtype[r[0]] = r[ti]
        if r[ti] in SOLID:
            for p in re.findall(r'"(/[^"]+?\.[^"]+?)"', r[mi]):
                found[p] = (r[ti], "node:" + r[0])
    rows = table_rows("/MOFramework/Data/DT_Biomes")
    si = rows[0].index("Species")
    for r in rows[1:]:
        for m in re.finditer(r'Mesh="([^"]+)".*?ResourceNodeId="?([^,")]*)"?', r[si]):
            kind = rtype.get(m.group(2))
            if kind in SOLID:
                found.setdefault(m.group(1), (kind, "biome:" + r[0]))
        # Decorative species marked Solid (a fallen trunk with no resource node) block the pawn too: they need hulls like a rock.
        for m in re.finditer(r'Mesh="([^"]+)"[^()]*?Collision=Solid', r[si]):
            found.setdefault(m.group(1), ("Rock", "biome:" + r[0] + " (Collision=Solid)"))
    return found


def fix_rock(mesh):
    hull = ESL.set_convex_decomposition_collisions(mesh, 4, 32, 100000)
    return "convex decomposition (%s)" % ("ok" if hull is None or hull else "FAILED")


def fix_tree(mesh):
    b = mesh.get_bounds()
    height = 2.0 * b.box_extent.z
    base = b.origin.z - b.box_extent.z
    radius = max(20.0, min(55.0, 0.022 * height))
    length = max(60.0, min(600.0, 0.45 * height))
    body = mesh.get_editor_property("body_setup")
    ag = body.get_editor_property("agg_geom")
    capsule = unreal.KSphylElem()
    capsule.set_editor_property("center", unreal.Vector(0.0, 0.0, base + radius + length * 0.5))
    capsule.set_editor_property("radius", radius)
    capsule.set_editor_property("length", length)
    ag.set_editor_property("sphyl_elems", [capsule])
    body.set_editor_property("agg_geom", ag)
    return "trunk capsule r=%.0f len=%.0f (tree height %.0f)" % (radius, length, height)


def fix_meshes():
    fixed = 0
    for path, (kind, who) in sorted(solid_meshes().items()):
        mesh = unreal.load_asset(path.split(".")[0])
        if mesh is None:
            print("FIXCOLL MISSING %s" % path)
            continue
        if blocks(mesh):
            print("FIXCOLL ok      %s (%s, %s)" % (path.rsplit("/", 1)[-1].split(".")[0], kind, who))
            continue
        mesh.modify()
        note = fix_tree(mesh) if kind == "Tree" else fix_rock(mesh)
        body = mesh.get_editor_property("body_setup")
        body.modify()
        try:
            body.invalidate_physics_data()
        except Exception:
            pass
        saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        now = blocks(mesh)
        fixed += 1 if now else 0
        print("FIXCOLL %s %s (%s, %s): %s, saved=%s" % ("FIXED  " if now else "STILL-BROKEN", path.rsplit("/", 1)[-1].split(".")[0], kind, who, note, saved))
    return fixed


# --------------------------------------------------------------------------- PCG node profiles
GRAPH_FIXES = [
    # (graph asset, node name, new profile): rocks of the spring POI, and the locust woodland trees of the main scatter graph
    ("/MOFramework/PCG_POIS/PCG_FreshWaterSpring", "StaticMeshSpawner_2", "BlockAll"),
    ("/MOFramework/PCG_POIS/PCG_FreshWaterSpring", "StaticMeshSpawner_3", "BlockAll"),
    # Node 3 places the Black Locust A / B / C: ground-level bushes (0.5 - 1.5 m), walk-through. (It was BlockAll from the first collision pass, which made knee-high bushes stop the pawn.)
    ("/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1", "StaticMeshSpawner_3", "OverlapAllDynamic"),
]


def fix_graph_nodes():
    changed = 0
    for graph_path, node_name, profile in GRAPH_FIXES:
        g = unreal.load_asset(graph_path)
        if g is None:
            print("FIXCOLL graph missing %s" % graph_path)
            continue
        for node in g.get_editor_property("nodes"):
            if node.get_name() != node_name:
                continue
            settings = node.get_settings()
            selector = settings.get_editor_property("mesh_selector_parameters")
            entries = selector.get_editor_property("mesh_entries")
            new_entries = []
            for e in entries:
                d = e.get_editor_property("descriptor")
                bi = d.get_editor_property("body_instance")
                before = str(bi.get_editor_property("collision_profile_name"))
                if before != profile:
                    bi.set_editor_property("collision_profile_name", profile)
                    d.set_editor_property("body_instance", bi)
                    e.set_editor_property("descriptor", d)
                    changed += 1
                    print("FIXCOLL graph %s %s: %s -> %s" % (graph_path.rsplit("/", 1)[-1], node_name, before, profile))
                new_entries.append(e)
            selector.set_editor_property("mesh_entries", new_entries)
        unreal.EditorAssetLibrary.save_loaded_asset(g)
    return changed


if __name__ == "__main__" or True:
    n = fix_meshes()
    m = fix_graph_nodes()
    print("FIXCOLL summary: %d meshes given collision, %d graph entries changed" % (n, m))
