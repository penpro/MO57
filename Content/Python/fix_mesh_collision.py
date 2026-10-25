"""Give every solid-kind mesh the data tables can place real collision geometry, and fix the PCG nodes that place rocks/trees with no-collision profiles.

    python Tools/ue.py py --file Content/Python/fix_mesh_collision.py        (editor open, PIE NOT running: saving is blocked in PIE)

Idempotent: a mesh that already blocks (simple shapes, or "use complex as simple") is left alone.
  * WORLD-FEATURE compositions (DT_WorldFeatures, manifest data/worldfeature_meshes.json from worldfeature_meshes.py) -> rocks / cliffs / logs: complex as simple
                          (climbable hero terrain); standing trees: trunk capsule; plants stay walk-through. Electric Dreams meshes are gitignored and migrated
                          per machine (Tools/ede_migrate.py): re-run this tool after a migration.
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
SOLID = ("Tree", "Rock", "Ore", "Bush")   # bushes block at their core since 2026-10-14 (playtest #21)


def blocks(mesh):
    """The pawn can hit it: simple shapes, or the render triangles ARE the collision. (The aggregate-geometry arrays are not read through Python: an empty struct array comes back as
    a one-element placeholder, which made every mesh look fine. The editor library's counts are right: -1 means "none".)"""
    body = mesh.get_editor_property("body_setup")
    if body is None:
        return False
    if body.get_editor_property("collision_trace_flag").name == "CTF_USE_COMPLEX_AS_SIMPLE":
        return True
    return max(ESL.get_simple_collision_count(mesh), 0) + max(ESL.get_convex_collision_count(mesh), 0) > 0


def complex_as_simple(mesh):
    body = mesh.get_editor_property("body_setup")
    return body is not None and body.get_editor_property("collision_trace_flag").name == "CTF_USE_COMPLEX_AS_SIMPLE"


def table_rows(path):
    dt = unreal.load_asset(path)
    csv.field_size_limit(2 ** 31 - 1)
    return list(csv.reader(io.StringIO(unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(dt))))


def sapling_or(kind, mesh_path):
    """A Tree resource whose mesh is a sapling / seedling gets the narrow stem, not a trunk."""
    return "Sapling" if kind == "Tree" and ("Sapling" in mesh_path or "Seedling" in mesh_path) else kind


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
                found[p] = (sapling_or(r[ti], p), "node:" + r[0])
    rows = table_rows("/MOFramework/Data/DT_Biomes")
    si = rows[0].index("Species")
    for r in rows[1:]:
        for m in re.finditer(r'Mesh="([^"]+)".*?ResourceNodeId="?([^,")]*)"?', r[si]):
            kind = rtype.get(m.group(2))
            if kind in SOLID:
                found.setdefault(m.group(1), (sapling_or(kind, m.group(1)), "biome:" + r[0]))
        # Decorative species marked Solid (rock plates, cliff slabs, a fallen trunk with no resource node) are scanned terrain the player stands and climbs on: their
        # REAL surface is the collision (a plate's few hulls float the feet over its dips; the species gallery's contact test measures it).
        for m in re.finditer(r'Mesh="([^"]+)"[^()]*?Collision=Solid', r[si]):
            name = m.group(1).rsplit(".", 1)[-1]
            kind = "Sapling" if "Sapling" in name or "Seedling" in name else worldfeature_kind(name)
            if kind == "?":
                kind = worldfeature_kind(megascans_folder(m.group(1)))
            if kind in ("Composition", "Tree", "Sapling", "Bush"):
                found.setdefault(m.group(1), (kind, "biome:" + r[0] + " (Collision=Solid)"))
    # The cave scatter (DT_CaveScatter, the MO Cave Spawner): fallen blocks marked Solid and ore lumps (a solid-kind resource) block the pawn -- convex hulls like any rock.
    if unreal.EditorAssetLibrary.does_asset_exist("/MOFramework/Data/DT_CaveScatter"):
        rows = table_rows("/MOFramework/Data/DT_CaveScatter")
        hdr = rows[0]
        mi, ci, ri = hdr.index("Mesh"), hdr.index("Collision"), hdr.index("ResourceNodeId")
        for r in rows[1:]:
            kind = "Rock" if r[ci] == "Solid" else (rtype.get(r[ri]) if r[ci] == "Auto" else None)
            if kind in SOLID and r[mi] not in ("", "None"):
                found.setdefault(r[mi], (kind, "cave:" + r[0]))
    return found


# --------------------------------------------------------------------------- world-feature compositions (DT_WorldFeatures)
# The manifest of what the compositions spawn (written by worldfeature_meshes.py in PIE: their meshes are PCG point attributes the asset registry cannot see).
# A composition's rocks are hero terrain the player climbs (Wes, Oct 14 2026: "i fall through them if i try to jump up them"), so they collide with their
# real surface (complex as simple), not a few hulls. Standing trees get the trunk capsule. Plants, vines, saplings stay walk-through.
WF_MANIFEST = "data/worldfeature_meshes.json"
WF_PLANT = ("Vine", "Ivy", "Fern", "Plant", "Sapling", "Palm", "Elderberry", "Grass", "Clover", "Leaves", "Leaf", "Flower", "Bush", "Shrub", "Branch", "Twig",
            "Weed", "Nettle", "Seedling", "GroundCover", "Dracaena", "DragonTree", "Ginger", "Amaryllis", "Kikuyu", "Arrowhead", "Castor", "MistCard", "WaterPlane", "Icon",
            "Circle_", "Periwinkle", "Archangel", "Pebbles", "MoneyPlant")
WF_BUSH = ("Elderberry", "CommonHazel_Forest")   # the berry bushes of the Electric Dreams assemblies (playtest #21): a core capsule when at least BUSH_MIN_HEIGHT tall
WF_ROCK = ("Cliff", "Rock", "Boulder", "Embankment", "Formation", "Stone", "Ground", "RiverBed", "Terrain", "Path", "Roots", "Stump", "Fallen", "Log",
           "Shelf", "Mud", "Bank", "Ledge", "Scree", "Slab", "Arch")
WF_TREE = ("Hornbeam", "DeadTree", "Tree", "Spruce", "Birch", "Beech", "Oak", "Pine")


def worldfeature_kind(name):
    """'Composition' (complex as simple), 'Tree' (trunk capsule), 'Bush' (core capsule), 'Plant' (no collision at all) or '?' (not classified: reported, left alone)."""
    if any(k in name for k in ("Fallen", "Stump", "Log_", "NordicBeach_", "Embankment")):
        return "Composition"   # (before the plant words: a GrassyEmbankment is a bank of earth you walk on)
    if "Sapling" in name:
        return "Sapling"       # every sapling: a narrow stem (Wes, playtest #11), in a composition as in the biome scatter
    if any(k in name for k in WF_BUSH):
        return "Bush"
    if any(k in name for k in WF_PLANT):
        return "Plant"
    if any(k in name for k in WF_TREE):
        return "Tree"
    if any(k in name for k in WF_ROCK):
        return "Composition"
    return "?"


def megascans_folder(mesh_path):
    """The asset's folder name: a Megascans 'Smart' scan is named by its id alone (SM_venrdcgga_RM_N1), its folder says what it is
    (/Game/MS/3D/Huge_Nordic_Coastal_Cliff_venrdcgga/Smart/...)."""
    parts = mesh_path.split(".")[0].split("/")
    return parts[-3] if len(parts) >= 3 and parts[-2] == "Smart" else parts[-2]


def worldfeature_meshes():
    """{mesh object path: (kind, who)} for the compositions' solid meshes; prints the unclassified ones."""
    import json
    import os
    path = os.path.join(unreal.Paths.project_content_dir(), "Python", WF_MANIFEST)   # the bridge exec()s this file: no __file__
    if not os.path.exists(path):
        print("FIXCOLL no world-feature manifest (%s): run worldfeature_meshes.py in PIE first" % WF_MANIFEST)
        return {}
    found = {}
    for mesh_path, rows in json.load(open(path, encoding="utf-8")).items():
        if mesh_path.startswith("/Engine/"):
            continue   # engine shapes (a gizmo sphere) are never edited
        name = mesh_path.rsplit("/", 1)[-1].split(".")[0]
        kind = worldfeature_kind(name)
        if kind == "?":
            kind = worldfeature_kind(megascans_folder(mesh_path))   # a Megascans 'Smart' scan's name is its id alone (SM_vdssailfa_RM_N1)
        if kind == "?":
            print("FIXCOLL unclassified world-feature mesh %s (%s): left alone" % (name, ",".join(rows)))
        else:
            found[mesh_path] = (kind, "feature:" + ",".join(rows))
    return found


BUSH_MIN_HEIGHT = 100.0   # cm: a lower "bush" is a step you would climb onto -- it stays walk-through (no collision)


def no_collision(mesh):
    """A composition PLANT is done when it has nothing to collide with: the compositions' spawners place everything BlockAll and the mesh decides."""
    body = mesh.get_editor_property("body_setup")
    if body is not None and body.get_editor_property("collision_trace_flag").name == "CTF_USE_COMPLEX_AS_SIMPLE":
        return False
    return max(ESL.get_simple_collision_count(mesh), 0) == 0 and max(ESL.get_convex_collision_count(mesh), 0) == 0


def fix_plant(mesh):
    ESL.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    return "no collision (walk-through plant)"


def fix_composition(mesh):
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    return "complex as simple (climbable surface)"


def fix_rock(mesh):
    hull = ESL.set_convex_decomposition_collisions(mesh, 4, 32, 100000)
    return "convex decomposition (%s)" % ("ok" if hull is None or hull else "FAILED")


def trunk_only(mesh):
    """A tree / sapling is done when its collision is ONE capsule (the trunk) and nothing else: no canopy hulls (an invisible wall metres from the trunk; Wes,
    2026-10-14 #11: "make all the little saplings have a narrow trunk collision, their branches block movement") and not its render triangles."""
    body = mesh.get_editor_property("body_setup")
    if body is None or body.get_editor_property("collision_trace_flag").name == "CTF_USE_COMPLEX_AS_SIMPLE":
        return False
    return ESL.get_convex_collision_count(mesh) <= 0 and ESL.get_simple_collision_count(mesh) == 1


def fix_bush(mesh):
    """A bush stops you at its stems, not at its outer leaves: one upright capsule about a third of its width, through most of its height."""
    b = mesh.get_bounds()
    height = 2.0 * b.box_extent.z
    base = b.origin.z - b.box_extent.z
    radius = max(15.0, min(60.0, 0.3 * max(b.box_extent.x, b.box_extent.y)))
    length = max(20.0, 0.75 * height - 2.0 * radius)
    ESL.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    ag = body.get_editor_property("agg_geom")
    capsule = unreal.KSphylElem()
    # At the PIVOT, where the stems come out of the ground (PivotPainter / scanned shrubs are rooted there): a lopsided crown puts the bounds centre a metre off the
    # stems, and a core capsule there let the character walk straight through the stems (gallery, SM_CommonHazel_Forest_03).
    capsule.set_editor_property("center", unreal.Vector(0.0, 0.0, base + radius + length * 0.5))
    capsule.set_editor_property("radius", radius)
    capsule.set_editor_property("length", length)
    ag.set_editor_property("sphyl_elems", [capsule])
    body.set_editor_property("agg_geom", ag)
    return "core capsule r=%.0f len=%.0f (bush %.0f cm wide, %.0f tall)" % (radius, length, 2.0 * max(b.box_extent.x, b.box_extent.y), height)


def fix_tree(mesh, sapling=False):
    b = mesh.get_bounds()
    height = 2.0 * b.box_extent.z
    base = b.origin.z - b.box_extent.z
    if sapling:
        radius = max(6.0, min(12.0, 0.03 * height))     # a sapling's stem: you brush past the twigs, you bump into the stem
        length = max(40.0, min(250.0, 0.6 * height))
    else:
        radius = max(20.0, min(55.0, 0.022 * height))
        length = max(60.0, min(600.0, 0.45 * height))
    ESL.remove_collisions(mesh)   # the canopy hulls go: the trunk is the only thing that stops you
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    ag = body.get_editor_property("agg_geom")
    capsule = unreal.KSphylElem()
    capsule.set_editor_property("center", unreal.Vector(0.0, 0.0, base + radius + length * 0.5))
    capsule.set_editor_property("radius", radius)
    capsule.set_editor_property("length", length)
    ag.set_editor_property("sphyl_elems", [capsule])
    body.set_editor_property("agg_geom", ag)
    return "trunk capsule r=%.0f len=%.0f (tree height %.0f)" % (radius, length, height)


def fix_meshes(dry=False, force=()):
    """force: kinds rebuilt even when they look done -- a shape list edited without rebuilding the mesh's cooked physics data keeps the OLD collision in the game
    (2026-10-14: BlackAlder trees had a correct trunk capsule in the asset and the character walked through them; only a rebuild fixed it)."""
    fixed = 0
    targets = solid_meshes()
    for mesh_path, entry in worldfeature_meshes().items():
        targets.setdefault(mesh_path, entry)   # a mesh the biome palette also places keeps the palette's verdict
    for path, (kind, who) in sorted(targets.items()):
        mesh = unreal.load_asset(path.split(".")[0])
        if mesh is None:
            print("FIXCOLL MISSING %s" % path)
            continue
        # A composition rock must collide with its REAL surface: hulls are fine for a boulder, but a scaled-up cliff's few hulls left 15-25 m
        # drops where the rock is visibly there (grid test, Oct 14 2026: SM_Cliff_06 at 4x). So "has hulls" is not enough for these.
        if kind == "Bush" and who.startswith("feature:") and 2.0 * mesh.get_bounds().box_extent.z < BUSH_MIN_HEIGHT:
            kind = "Plant"   # a knee-high composition shrub: nothing to bump into (the palette's harvestable bushes keep their core capsule as an interaction target)
        if kind == "Composition" and "/Items/" in path:
            # An ITEM mesh (Bark01 = shore driftwood debris AND the bark pickup) is also a dropped world item that simulates physics, and a complex-as-simple
            # body cannot simulate: it keeps convex hulls (Oct 14 2026: Bark01 had been switched to its real surface by the Solid-species rule).
            kind = "Rock"
        done =(complex_as_simple(mesh) if kind == "Composition" else trunk_only(mesh) if kind in ("Tree", "Sapling", "Bush")
                else no_collision(mesh) if kind == "Plant" else blocks(mesh)) and kind not in force
        if done:
            print("FIXCOLL ok      %s (%s, %s)" % (path.rsplit("/", 1)[-1].split(".")[0], kind, who))
            continue
        if dry:
            print("FIXCOLL would fix %s (%s, %s)" % (path.rsplit("/", 1)[-1].split(".")[0], kind, who))
            continue
        mesh.modify()
        note = (fix_tree(mesh, sapling=(kind == "Sapling")) if kind in ("Tree", "Sapling") else fix_bush(mesh) if kind == "Bush"
                else fix_plant(mesh) if kind == "Plant" else fix_composition(mesh) if kind == "Composition" else fix_rock(mesh))
        body = mesh.get_editor_property("body_setup")
        body.modify()
        try:
            body.invalidate_physics_data()
        except Exception:
            pass
        saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        now = (complex_as_simple(mesh) if kind == "Composition" else trunk_only(mesh) if kind in ("Tree", "Sapling", "Bush")
               else no_collision(mesh) if kind == "Plant" else blocks(mesh))
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


# The world-feature compositions' graphs: every Static Mesh Spawner places BlockAll and the MESH decides (rock: its surface, tree: trunk, bush: core, plant: nothing).
# Electric Dreams' LargeAssembly graph had five of its seven spawners at NoCollision -- rocks and embankments among them were walk-through (in-world audit, 2026-10-14).
WORLDFEATURE_GRAPHS = ["/Game/PCG/Graphs/LargeAssembly/LargeAssemblyGraph", "/Game/PCG/Graphs/SmallAssembly/SmallAssemblyGraph",
                       "/Game/WorldFeatures/Cliffs/PCG_PL_LargeCliff_01b", "/Game/WorldFeatures/Cliffs/PCG_PL_LargeCliff_01c"]


def fix_worldfeature_graphs(profile="BlockAll"):
    changed = 0
    for graph_path in WORLDFEATURE_GRAPHS:
        g = unreal.load_asset(graph_path)
        if g is None:
            print("FIXCOLL graph missing %s" % graph_path)
            continue
        dirty = False
        for node in g.get_editor_property("nodes"):
            st = node.get_settings()
            if st is None or st.get_class().get_name() != "PCGStaticMeshSpawnerSettings":
                continue
            sel = st.get_editor_property("mesh_selector_parameters")
            try:
                d = sel.get_editor_property("template_descriptor")
            except Exception:
                d = None
            if d is not None:
                bi = d.get_editor_property("body_instance")
                before = str(bi.get_editor_property("collision_profile_name"))
                if before != profile:
                    bi.set_editor_property("collision_profile_name", profile)
                    d.set_editor_property("body_instance", bi)
                    sel.set_editor_property("template_descriptor", d)
                    dirty = True
                    changed += 1
                    print("FIXCOLL graph %s %s template: %s -> %s" % (graph_path.rsplit("/", 1)[-1], node.get_name(), before, profile))
            try:
                entries = sel.get_editor_property("mesh_entries")
            except Exception:
                entries = None
            if entries:
                new_entries = []
                for e in entries:
                    ed = e.get_editor_property("descriptor")
                    bi = ed.get_editor_property("body_instance")
                    before = str(bi.get_editor_property("collision_profile_name"))
                    if before != profile:
                        bi.set_editor_property("collision_profile_name", profile)
                        ed.set_editor_property("body_instance", bi)
                        e.set_editor_property("descriptor", ed)
                        dirty = True
                        changed += 1
                        print("FIXCOLL graph %s %s entry: %s -> %s" % (graph_path.rsplit("/", 1)[-1], node.get_name(), before, profile))
                    new_entries.append(e)
                sel.set_editor_property("mesh_entries", new_entries)
        if dirty:
            unreal.EditorAssetLibrary.save_loaded_asset(g)
    return changed


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
    DRY = globals().get("FIXCOLL_DRY", False)   # python Tools/ue.py py -c "FIXCOLL_DRY=True; exec(open('Content/Python/fix_mesh_collision.py').read())"
    n = fix_meshes(DRY, force=globals().get("FIXCOLL_FORCE", ()))   # e.g. FIXCOLL_FORCE=("Tree", "Sapling", "Bush")
    m = 0 if DRY else fix_graph_nodes() + fix_worldfeature_graphs()
    print("FIXCOLL summary: %d meshes given collision, %d graph entries changed" % (n, m))
