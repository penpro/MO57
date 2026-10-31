"""Give every solid-kind mesh the data tables can place real collision geometry, and fix the PCG nodes that place rocks/trees with no-collision profiles.

    python Tools/ue.py py --file Content/Python/fix_mesh_collision.py        (editor open, PIE NOT running: saving is blocked in PIE)

Idempotent: a mesh that already blocks (simple shapes, or "use complex as simple") is left alone.
  * WORLD-FEATURE compositions (DT_WorldFeatures, manifest data/worldfeature_meshes.json from worldfeature_meshes.py) -> rocks / cliffs / logs: complex as simple
                          (climbable hero terrain); standing trees: trunk capsule; plants stay walk-through. Electric Dreams meshes are gitignored and migrated
                          per machine (Tools/ede_migrate.py): re-run this tool after a migration.
  * ROCK / ORE meshes  -> convex decomposition (a few hulls), so a boulder is as wide as it looks; a rock 3 m or more (BIG_ROCK_CM, its
                          largest side unscaled) is climbed, so its real surface is its collision (complex as simple; Wes, Oct 31).
  * TREE meshes        -> one convex hull round the MEASURED trunk: the convex outline of the bark's LOD0 vertices 30-110 cm above the base (a stray branch stub
                          left out), upright from the base to the capsule's top (BR31-8, Oct 30-31 2026: a pivot capsule left wood walk-through, one capsule round the
                          measured trunk over-covered a lopsided trunk by up to 71 cm). A mesh with no bark there gets a capsule, 2.2 % of the tree's height (20..55 cm)
                          at the pivot. Height ~45% of the tree's, at most 6 m. A tree's collision must NOT be its canopy: a bounding capsule of the whole tree would
                          be an invisible wall 4 m from the trunk.
  * PLANTS THAT BLOCK (trees, saplings, bushes) are never a floor: their body's walkable slope is Unwalkable, so a bush stops you but never holds you up
                          (BR31-5, Oct 30 2026: the player stood on top of one). Rocks stay walkable (you climb boulders).
  * PCG graph nodes    -> native Static Mesh Spawner entries of rocks/trees that carry NoCollision / IgnoreOnlyPawn become BlockAll (the FreshWaterSpring boulders, the locust woodland).
The audit that finds the meshes: Content/Python/audit_table_mesh_collision.py; the regression test: MOFramework.Data.SolidMeshCollision.
Output lines start with FIXCOLL.
"""
import csv
import io
import math
import re

import unreal

ESL = unreal.EditorStaticMeshLibrary
SMES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
SOLID = ("Tree", "Rock", "Ore", "Bush")   # bushes block at their core since 2026-10-14 (playtest #21)
NOT_BARK = ("TwoSided", "Leaf", "Leaves", "Foliage", "Needle", "Frond", "Billboard", "Card", "Decoration", "Moss", "Ivy")   # material names of a tree's non-wood sections


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
BIG_ROCK_CM = 300.0       # cm: a rock this big (its largest side, unscaled) is climbed, not hopped onto: its real surface is its collision


def big_rock(mesh):
    ext = mesh.get_bounds().box_extent
    return 2.0 * max(ext.x, ext.y, ext.z) >= BIG_ROCK_CM


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


def trunk_only(mesh, hull=False):
    """A tree / sapling is done when its collision is ONE trunk shape and nothing else -- a capsule, or (hull: a tree whose trunk was measured) one convex
    hull round the wood: no canopy hulls (an invisible wall metres from the trunk; Wes, 2026-10-14 #11: "make all the little saplings have a narrow trunk
    collision, their branches block movement") and not its render triangles. (The simple count is boxes + spheres + capsules; hulls count apart.)"""
    body = mesh.get_editor_property("body_setup")
    if body is None or body.get_editor_property("collision_trace_flag").name == "CTF_USE_COMPLEX_AS_SIMPLE":
        return False
    if hull:
        return ESL.get_convex_collision_count(mesh) == 1 and ESL.get_simple_collision_count(mesh) == 0
    return ESL.get_convex_collision_count(mesh) <= 0 and ESL.get_simple_collision_count(mesh) == 1


def is_unwalkable(mesh):
    body = mesh.get_editor_property("body_setup")
    return body is not None and body.get_editor_property("walkable_slope_override").get_editor_property("walkable_slope_behavior") == \
        unreal.WalkableSlopeBehavior.WALKABLE_SLOPE_UNWALKABLE


def set_unwalkable(body):
    body.set_editor_property("walkable_slope_override", unreal.WalkableSlopeOverride(
        walkable_slope_behavior=unreal.WalkableSlopeBehavior.WALKABLE_SLOPE_UNWALKABLE, walkable_slope_angle=0.0))


def hull_2d(pts):
    """Convex hull of (x, y) points, counter-clockwise (Andrew's monotone chain)."""
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

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


# Where the trunk is measured: cross-sections of the bark at these heights above the base (cm). The top stays below a field tree's first branches
# (BlackAlder_Field_02's leave the trunk ~136 cm up: a band to 150 drew one branch's base -- 138 cm out -- down to hip height as a prism, a wall
# 65 cm short of the bark; Oct 31 2026).
TRUNK_SLICES = (30.0, 50.0, 70.0, 90.0, 110.0)


def bark_slices(mesh, base, heights):
    """(x, y) where the bark's LOD0 triangles cross each horizontal plane base + h: the trunk's real cross-sections. Vertices alone miss a straight
    trunk -- a low-poly cylinder has a ring at the root and the next one metres up (EuropeanHornbeam_Forest_01: none between 23 and 110 cm)."""
    pts = []
    for s in range(mesh.get_num_sections(0)):
        mat = mesh.get_material(SMES.get_lod_material_slot(mesh, 0, s))
        if mat is not None and any(w in mat.get_name() for w in NOT_BARK):
            continue
        section = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, s)
        verts, tris = section[0], section[1]
        for h in heights:
            z = base + h
            for i in range(0, len(tris) - 2, 3):
                tri = (verts[tris[i]], verts[tris[i + 1]], verts[tris[i + 2]])
                if min(v.z for v in tri) > z or max(v.z for v in tri) < z:
                    continue
                for p, q in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
                    if (p.z - z) * (q.z - z) < 0.0:
                        t = (z - p.z) / (q.z - p.z)
                        pts.append((p.x + t * (q.x - p.x), p.y + t * (q.y - p.y)))
    return pts


def measure_trunk(mesh, base):
    """(centre x, centre y, radius, outline) of the wood a walker bumps into: the bark's cross-sections 30-110 cm above the base (TRUNK_SLICES), the radius
    taking in 90 % of their points, the outline their convex hull (a stray branch stub -- a point past 1.5x that radius -- left out). None when there is
    no bark there. (BR31-8, Oct 30 2026: the 2.2 %-of-height capsule at the pivot was r 38 on BlackAlder_Field_02, whose trunk reaches 68 cm round a
    centre 31 cm off the pivot: the player walked into the wood. Then one capsule round the measured trunk over-covered a lopsided one by up to 71 cm:
    the outline is the shape.)"""
    pts = bark_slices(mesh, base, TRUNK_SLICES)
    if len(pts) < 5:
        return None
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    d = sorted(math.hypot(x - cx, y - cy) for x, y in pts)
    r90 = d[int(0.9 * (len(d) - 1))]
    outline = hull_2d([(round(x, 1), round(y, 1)) for x, y in pts if math.hypot(x - cx, y - cy) <= 1.5 * r90])
    return cx, cy, r90, outline


def trunk_shape(mesh, sapling=False):
    """(centre x, centre y, centre z, radius, length, outline) of the trunk collision a tree should have: a capsule, or -- outline set, a tree whose trunk was
    measured -- that outline as an upright prism over the capsule's height (base to base + 2 radius + length)."""
    b = mesh.get_bounds()
    height = 2.0 * b.box_extent.z
    base = b.origin.z - b.box_extent.z
    cx, cy, outline = 0.0, 0.0, None
    if sapling:
        radius = max(6.0, min(12.0, 0.03 * height))     # a sapling's stem: you brush past the twigs, you bump into the stem
        length = max(40.0, min(250.0, 0.6 * height))
    else:
        # Only a real tree has a trunk at hip height: on a knee-high "Tree" (the Black Locust wdvhberja variants, 0.5-1.5 m) that band is the whole plant.
        trunk = measure_trunk(mesh, base) if height >= 400.0 else None
        if trunk:
            cx, cy, radius = trunk[0], trunk[1], max(20.0, min(120.0, trunk[2]))
            outline = trunk[3] if len(trunk[3]) >= 3 else None
        else:
            radius = max(20.0, min(55.0, 0.022 * height))
        length = max(60.0, min(600.0, 0.45 * height))
    return cx, cy, base + radius + length * 0.5, radius, length, outline


def trunk_prism(outline, bottom, top):
    """One convex hull: the trunk's outline from bottom to top. Python cannot write a hull's vertices (not editor properties), but the struct's text import
    can; the cook builds the hull from these points."""
    xs, ys = [p[0] for p in outline], [p[1] for p in outline]
    verts = ",".join("(X=%.2f,Y=%.2f,Z=%.2f)" % (x, y, z) for z in (bottom, top) for x, y in outline)
    elem = unreal.KConvexElem()
    ok = elem.import_text("(VertexData=(%s),ElemBox=(Min=(X=%.2f,Y=%.2f,Z=%.2f),Max=(X=%.2f,Y=%.2f,Z=%.2f),IsValid=True))"
                          % (verts, min(xs), min(ys), bottom, max(xs), max(ys), top))
    return elem if ok else None


def hull_extent(elem):
    """(min x, min y, min z, max x, max y, max z) of a convex element's points, read back through its text export."""
    pts = [tuple(float(v) for v in m) for m in re.findall(r"\(X=([-\d.]+),Y=([-\d.]+),Z=([-\d.]+)\)", elem.export_text().split("IndexData=")[0])]
    if not pts:
        return None
    return (min(p[0] for p in pts), min(p[1] for p in pts), min(p[2] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts), max(p[2] for p in pts))


def plant_done(mesh, kind):
    """A tree / sapling / bush: one trunk shape, never walkable, and a tree's shape where its trunk is: the measured outline's prism (its extent within 5 cm),
    or the capsule within 15 % of the radius / 10 cm of the centre."""
    want = trunk_shape(mesh) if kind == "Tree" else None
    hull = bool(want and want[5])
    if not (trunk_only(mesh, hull) and is_unwalkable(mesh)):
        return False
    if kind != "Tree":
        return True
    ag = mesh.get_editor_property("body_setup").get_editor_property("agg_geom")
    if hull:
        have = hull_extent(ag.get_editor_property("convex_elems")[0])
        xs, ys = [p[0] for p in want[5]], [p[1] for p in want[5]]
        top = want[2] + want[3] + want[4] * 0.5
        return have is not None and max(abs(a - b) for a, b in zip(have, (min(xs), min(ys), have[2], max(xs), max(ys), top))) <= 5.0
    have = ag.get_editor_property("sphyl_elems")[0]
    c = have.get_editor_property("center")
    return abs(have.get_editor_property("radius") - want[3]) <= 0.15 * want[3] and math.hypot(c.x - want[0], c.y - want[1]) <= 10.0


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
    set_unwalkable(body)
    return "core capsule r=%.0f len=%.0f (bush %.0f cm wide, %.0f tall), unwalkable" % (radius, length, 2.0 * max(b.box_extent.x, b.box_extent.y), height)


def fix_tree(mesh, sapling=False):
    cx, cy, cz, radius, length, outline = trunk_shape(mesh, sapling)
    ESL.remove_collisions(mesh)   # the canopy hulls go: the trunk is the only thing that stops you
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    ag = body.get_editor_property("agg_geom")
    prism = trunk_prism(outline, cz - radius - length * 0.5, cz + radius + length * 0.5) if outline else None
    if prism is not None:
        ag.set_editor_property("convex_elems", [prism])
        body.set_editor_property("agg_geom", ag)
        set_unwalkable(body)
        xs, ys = [p[0] for p in outline], [p[1] for p in outline]
        return "trunk hull of %d points, %.0f x %.0f cm, %.0f cm tall (tree height %.0f), unwalkable" % (
            len(outline), max(xs) - min(xs), max(ys) - min(ys), 2.0 * radius + length, 2.0 * mesh.get_bounds().box_extent.z)
    capsule = unreal.KSphylElem()
    capsule.set_editor_property("center", unreal.Vector(cx, cy, cz))
    capsule.set_editor_property("radius", radius)
    capsule.set_editor_property("length", length)
    ag.set_editor_property("sphyl_elems", [capsule])
    body.set_editor_property("agg_geom", ag)
    set_unwalkable(body)
    return "trunk capsule r=%.0f len=%.0f at (%.0f, %.0f) (tree height %.0f), unwalkable" % (radius, length, cx, cy, 2.0 * mesh.get_bounds().box_extent.z)


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
        if kind == "Rock" and big_rock(mesh):
            # A rock you climb collides with its REAL surface too (Wes, Oct 31: "make the collision for the cliffs and ED cliff rocks complex"): a
            # climber's hands and feet land on what is drawn, and a few hulls bridge the cracks and ledges a climb goes by (NordicBoulder_02, up
            # to 7 m in the beach palette, had 6 hulls).
            kind = "Composition"
        if kind == "Composition" and "/Items/" in path:
            # An ITEM mesh (Bark01 = shore driftwood debris AND the bark pickup) is also a dropped world item that simulates physics, and a complex-as-simple
            # body cannot simulate: it keeps convex hulls (Oct 14 2026: Bark01 had been switched to its real surface by the Solid-species rule).
            kind = "Rock"
        done =(complex_as_simple(mesh) if kind == "Composition" else plant_done(mesh, kind) if kind in ("Tree", "Sapling", "Bush")
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
        now = (complex_as_simple(mesh) if kind == "Composition" else plant_done(mesh, kind) if kind in ("Tree", "Sapling", "Bush")
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
