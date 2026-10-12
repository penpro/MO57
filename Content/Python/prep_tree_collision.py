"""Collision for the migrated European Hornbeam trees (and any other tree whose mesh carries none): a trunk capsule, like the sample's own variants.

    python Tools/ue.py py --file Content/Python/prep_tree_collision.py        (editor open, not in PIE)

The test MOFramework.Data.SolidMeshCollision (run by `ue.py auto`) lists every Solid species mesh without collision geometry; a tree you can walk through is a bug. The Electric Dreams variants come with
a capsule on most meshes; the missing ones get one sized like their siblings (radius ~ 3.2 % of the tree's height, from the ground up to 60 % of its height, centred on the pivot's trunk).
Output lines start with TREECOL. Idempotent (skips meshes that already have any simple collision).
"""
import unreal

BASE = "/Game/Megascans/3D_Plants/EuropeanHornbeam/Geometry/SimpleWind/SM_EuropeanHornbeam_"
NAMES = ["Forest_%02d" % i for i in range(1, 11)] + ["Field_%02d" % i for i in range(1, 5)] + ["Sapling_%02d" % i for i in range(1, 9)] + ["Seedling_%02d" % i for i in range(1, 6)]


def shapes(m):
    bs = m.get_editor_property("body_setup")
    if not bs:
        return None
    ag = bs.get_editor_property("agg_geom")
    return {k: len(ag.get_editor_property(k)) for k in ("convex_elems", "sphyl_elems", "box_elems", "sphere_elems")}


for n in NAMES:
    m = unreal.load_asset(BASE + n)
    if not m:
        unreal.log("TREECOL %s missing" % n)
        continue
    sh = shapes(m)
    total = sum(sh.values()) if sh else 0
    ext = m.get_bounds().box_extent
    ctr = m.get_bounds().origin
    unreal.log("TREECOL %-12s shapes=%s ext=(%.0f,%.0f,%.0f) origin=(%.0f,%.0f,%.0f)" % (n, sh, ext.x, ext.y, ext.z, ctr.x, ctr.y, ctr.z))
    if total > 0:
        continue
    height = ext.z * 2.0
    radius = max(12.0, 0.027 * height)   # the siblings: r = 40 cm on a 15 m tree
    length = max(0.0, 0.6 * height - 2.0 * radius)
    unreal.EditorStaticMeshLibrary.remove_collisions(m)
    ok = unreal.EditorStaticMeshLibrary.add_simple_collisions(m, unreal.ScriptingCollisionShapeType.CAPSULE)
    bs = m.get_editor_property("body_setup")
    ag = bs.get_editor_property("agg_geom")
    sp = list(ag.get_editor_property("sphyl_elems"))
    if sp:
        e = sp[0]
        e.set_editor_property("center", unreal.Vector(0.0, 0.0, radius + length * 0.5))
        e.set_editor_property("radius", radius)
        e.set_editor_property("length", length)
        e.set_editor_property("rotation", unreal.Rotator(0, 0, 0))
        sp[0] = e
        ag.set_editor_property("sphyl_elems", sp)
        bs.set_editor_property("agg_geom", ag)
        m.set_editor_property("body_setup", bs)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log("TREECOL %-12s added capsule ok=%s r=%.0f len=%.0f -> %s" % (n, ok, radius, length, shapes(m)))
