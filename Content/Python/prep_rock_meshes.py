"""Give the migrated Megascans / Electric Dreams rocks the collision they were migrated without (HasCollision=None): a boulder you can walk through is not a boulder.

    python Tools/ue.py py --file Content/Python/prep_rock_meshes.py        (editor open, not in PIE)

Convex decomposition for solid rocks (hull count by size); nothing for the flat ground plates (ErodedRockyGround, SandstoneRockyGround, SmoothRockShelf, MossyRocks, DryRiverBed, embankments):
they lie flush in the terrain, the terrain is what you walk on. Idempotent (skips meshes that already have convex elements).
"""
import unreal

BASE = "/Game/Megascans/3D_Assets/"
# (asset, hulls, max verts per hull)
SOLID = [
    ("MossyForestBoulder/SM_MossyForestBoulder_02", 3, 24),
    ("MossyForestBoulder/SM_MossyForestBoulder_04", 8, 32),
    ("MossyForestBoulder/SM_MossyForestBoulder_05", 4, 24),
    ("LichenedForestBoulder/SM_LichenedForestBoulder_01", 3, 24),
    ("MossyForestRock/SM_MossyForestRock_02", 4, 24),
    ("NordicBeachRocks/SM_NordicBeachRocks_01", 10, 32),
    ("NordicBeachRock/SM_NordicBeachRock_01", 2, 24),
    ("HugeSandstoneCliff/SM_HugeSandstoneCliff_04", 14, 32),
    ("HugeSandstoneCliff/SM_HugeSandstoneCliff_07", 14, 32),
    # the Electric Forest set (2026-10-12)
    ("MossyForestBoulder/SM_MossyForestBoulder_01", 4, 24),
    ("MossyForestBoulder/SM_MossyForestBoulder_03", 4, 24),
    ("MossyForestBoulder/SM_MossyForestBoulder_06", 3, 24),
    ("MossyForestBoulder/SM_MossyForestBoulder_07", 4, 24),
    ("MossyForestRock/SM_MossyForestRock_01", 4, 24),
    ("MossyForestRock/SM_MossyForestRock_03", 4, 24),
    ("DeadTree/SM_DeadTree_01", 4, 24),
    ("DeadTree/SM_DeadTree_02", 4, 24),
    # the Nordic set (2026-10-12): NordicBoulder_01 (20 hulls) and the coastal cliff (2 hulls) arrive with the sample's own collision; these two did not
    ("NordicBoulder/SM_NordicBoulder_02", 6, 24),
    ("TundraMossyBoulder/SM_TundraMossyBoulder_01", 4, 24),
]

lib = unreal.EditorStaticMeshLibrary


def convex_count(m):
    bs = m.get_editor_property("body_setup")
    return len(bs.get_editor_property("agg_geom").get_editor_property("convex_elems")) if bs else 0


for rel, hulls, verts in SOLID:
    m = unreal.load_asset(BASE + rel)
    if not m:
        unreal.log("ROCKPREP missing %s" % rel)
        continue
    if convex_count(m) > 0:
        unreal.log("ROCKPREP %s already has %d hull(s)" % (rel, convex_count(m)))
        continue
    ok = lib.set_convex_decomposition_collisions(m, hulls, verts, 100000)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log("ROCKPREP %s hulls=%d ok=%s -> %d" % (rel, hulls, ok, convex_count(m)))
