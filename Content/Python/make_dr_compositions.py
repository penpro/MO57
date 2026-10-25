"""World-feature composition graphs from the Dark Ruins sample's own cliff assemblies, dressed (Wes, Oct 25 2026: "we need to have more of
them and we need stuff spawned on top of them ... add other stuff to the cliffs for detail, more rocks, etc to help mask the repetition ...
meshes pcg spawned on meshes and all that").

The assemblies come in as PCG data assets (Tools/darkruins_assembly_export.py, migrated by ede_migrate.py --source darkruins:
/Game/MO57Export/PCGAssemblies/PL_<name>_PCG, pins Root + Points, every point carrying its Mesh). Each graph starts from the Electric Dreams
spire graph's core (/Game/WorldFeatures/Cliffs/PCG_PL_LargeCliff_01b: DataFromActor -> Copy Points -> MO Conform To Ground -> Static Mesh
Spawner by the Mesh attribute; its PCGAsset Blueprint element replaced by the native Load PCG Data Asset node) and adds:

  FOOT  rock clusters round the base: the sample's boulder / rocky-buildup / shelf assemblies, each copied onto a third of the foot points
        (Content/Python/dr_dressing_points.py: the base outline pushed out 2.5 m), the ring copied onto the site first (its scale and yaw,
        like the assembly). They join the cliff's points, so Conform To Ground seats them with it and the same spawner draws them.
  TOP   grass, wild grass, dead shrubs, alder seedlings, a flower and small stones on the cliff's own top surfaces (the up-facing triangles
        that are the highest surface at their spot, 1 per 1.5 m2), walk-through. Their points (density 0.5, every other point 1) are MERGED
        into Conform To Ground's input, so they move with the composition's median shift and stay on the rock; a density filter after it
        splits them off to their own spawner (random yaw, x0.7-1.3 absolute: the grass keeps its size whatever the site's scale).

    python Tools/ue.py py --file Content/Python/make_dr_compositions.py        (editor open, PIE NOT running)

Writes /Game/WorldFeatures/DarkRuins/PCG_DR_<name>. Idempotent: everything outside the template's core is removed and built again. DRCOMP lines.
"""
import json
import math
import os

import unreal

TEMPLATE = "/Game/WorldFeatures/Cliffs/PCG_PL_LargeCliff_01b"
DEST = "/Game/WorldFeatures/DarkRuins/"
ASSETS = "/Game/MO57Export/PCGAssemblies/"
POINTS = os.path.join(unreal.Paths.project_content_dir(), "Python", "data", "dr_dressing_points.json")
CORE = ("PCGDataFromActorSettings", "PCGCopyPointsSettings", "MOPCGConformToGroundSettings", "PCGStaticMeshSpawnerSettings")
FOOT_CLUSTERS = ["PL_BoulderAssembly_01_PCG", "PL_Buildup_Rocky_01_PCG", "PL_ShelfAssembly_01_PCG"]
# (the clusters' shallow ground skins are left out at the export: Tools/darkruins_assembly_export.py SKINS)
# graph -> (assembly asset, the assembly's dressing points key, MO Conform To Ground EmbedCm)
COMPOSITIONS = {
    "PCG_DR_NordicMountain": ("PL_Mountain_01_PCG", "Mountain_01", 200.0),
    "PCG_DR_NordicTallPath": ("PL_TallPath_01_PCG", "TallPath_01", 150.0),
}
FAB = "/Game/Fab/Megascans/Plants/"
TOP_MESHES = [   # (mesh, weight): the highland biomes' own ground cover (RockyHighland / AlpineHeath palettes)
    (FAB + "Grass_smdrailr/Medium/smdrailr_tier_2/StaticMeshes/SM_smdrailr_VarB.SM_smdrailr_VarB", 6),
    (FAB + "Grass_smdrailr/Medium/smdrailr_tier_2/StaticMeshes/SM_smdrailr_VarE.SM_smdrailr_VarE", 4),
    (FAB + "Wild_Grass_vlkhcbxia/Medium/vlkhcbxia_tier_2/StaticMeshes/SM_vlkhcbxia_VarB.SM_vlkhcbxia_VarB", 3),
    (FAB + "Dead_Shrubs_vdknafgha/Medium/vdknafgha_tier_2/StaticMeshes/SM_vdknafgha_VarA.SM_vdknafgha_VarA", 1),
    (FAB + "Dead_Shrubs_vdknafgha/Medium/vdknafgha_tier_2/StaticMeshes/SM_vdknafgha_VarE.SM_vdknafgha_VarE", 1),
    ("/Game/BlackAlder/Geometry/PivotPainter/SM_BlackAlder_Seedling_01_PP.SM_BlackAlder_Seedling_01_PP", 1),
    ("/Game/PN_MeadowFlowers/Meshes/flowerMesh/flower_09_02.flower_09_02", 1),
    ("/Game/Megascans/3D_Assets/SmallStonesPack/SM_SmallStonesPack_10.SM_SmallStonesPack_10", 1),
    ("/Game/Megascans/3D_Assets/SmallStonesPack/SM_SmallStonesPack_12.SM_SmallStonesPack_12", 1),
]
DRESSING_DENSITY = 0.5
# OFF since Oct 25 2026: the top points do not seat. Measured in PIE (seed 4242, two mountains at x0.33): of ~290 sampled plants a third hung
# 2-3 m (median) over the terrain beside the rock and the rest stood a median 0.6-1.4 m above the rock under them. Two causes: the points come
# from the meshes' LOD0 sections, which for these Nanite scans are the coarse fallback, not the surface you see; and MO Conform To Ground gives
# every point low in the composition the TERRAIN correction of a base piece, so the plants on the low ledges leave the rock. Rebuild (BACKLOG
# N-211): points traced against the visible surface, carried with the piece they sit on. Until then the mountains are bare rock with the foot talus.
TOP_DRESSING = False

eal = unreal.EditorAssetLibrary
log = lambda m: out("DRCOMP " + m)


def kind(node):
    return node.get_settings().get_class().get_name()


def points(rows, density):
    pts = []
    for i, r in enumerate(rows):
        p = unreal.PCGPoint()
        yaw = r[3] if len(r) > 3 else 0.0
        p.set_editor_property("transform", unreal.Transform(location=unreal.Vector(r[0], r[1], r[2]), rotation=unreal.Rotator(roll=0, pitch=0, yaw=yaw)))
        p.set_editor_property("density", density)
        p.set_editor_property("seed", 1000 + i)
        p.set_editor_property("bounds_min", unreal.Vector(-50, -50, -50))
        p.set_editor_property("bounds_max", unreal.Vector(50, 50, 50))
        pts.append(p)
    return pts


def create_points(g, rows, density):
    """Points in the ASSEMBLY's own space (as the data asset's are): onto_site places them."""
    n = g.add_node_of_type(unreal.PCGCreatePointsSettings)[0]
    s = n.get_settings()
    s.set_editor_property("points_to_create", points(rows, density))
    s.set_editor_property("coordinate_space", unreal.PCGCoordinateSpace.WORLD)
    return n


def onto_site(g, src, actor):
    """Assembly-space points onto the site -- the actor's point: its location, yaw and SCALE -- exactly as the template copies the assembly
    (CopyPoints_2). Until Oct 25 the foot ring and the top dressing were created in the component's space and never took the site's scale or
    yaw: at a site's x0.3-0.6 every top plant floated (census: 112 of 112, up to 77 m above the rock)."""
    cp = g.add_node_of_type(unreal.PCGCopyPointsSettings)[0]
    for prop in ("apply_target_rotation_to_positions", "apply_target_scale_to_positions"):
        cp.get_settings().set_editor_property(prop, True)
    src.add_edge_to("Out", cp, "Source")
    actor.add_edge_to("Out", cp, "Target")
    return cp


def load(g, asset_name):
    n = g.add_node_of_type(unreal.PCGLoadDataAssetSettings)[0]
    n.get_settings().set_editor_property("asset", unreal.load_asset(ASSETS + asset_name))
    return n


def weighted_spawner(g):
    n = g.add_node_of_type(unreal.PCGStaticMeshSpawnerSettings)[0]
    s = n.get_settings()
    s.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
    entries = []
    for mesh, weight in TOP_MESHES:
        e = unreal.PCGMeshSelectorWeightedEntry()
        d = e.get_editor_property("descriptor")
        d.set_editor_property("static_mesh", unreal.load_asset(mesh.split(".")[0]))
        bi = d.get_editor_property("body_instance")
        bi.set_editor_property("collision_profile_name", "NoCollision")
        d.set_editor_property("body_instance", bi)
        d.set_editor_property("cast_shadow", True)
        e.set_editor_property("descriptor", d)
        e.set_editor_property("weight", weight)
        entries.append(e)
    s.get_editor_property("mesh_selector_parameters").set_editor_property("mesh_entries", entries)
    return n


def density_filter(g, lo, hi):
    n = g.add_node_of_type(unreal.PCGDensityFilterSettings)[0]
    s = n.get_settings()
    s.set_editor_property("lower_bound", lo)
    s.set_editor_property("upper_bound", hi)
    return n


def build(name, asset_name, dressing_key, embed_cm, dressing):
    path = DEST + name
    if not eal.does_asset_exist(path):
        eal.duplicate_asset(TEMPLATE, path)
    g = unreal.load_asset(path)
    # back to the template's core: DataFromActor, its two Copy Points, Conform To Ground, the mesh spawner
    for n in list(g.get_editor_property("nodes")):
        if kind(n) not in CORE or (kind(n) == "PCGStaticMeshSpawnerSettings" and n.get_name() != "StaticMeshSpawner_3") \
                or (kind(n) == "PCGCopyPointsSettings" and n.get_name() not in ("CopyPoints_2", "CopyPoints_6")):
            g.remove_node(n)
    nodes = {n.get_name(): n for n in g.get_editor_property("nodes")}
    actor, copy_main, copy_root = nodes["DataFromActor_1"], nodes["CopyPoints_2"], nodes["CopyPoints_6"]
    conform, cliff_spawner = nodes["MOConformToGround_0"], nodes["StaticMeshSpawner_3"]
    copy_main.remove_edge_to("Out", conform, "In")
    conform.remove_edge_to("Out", cliff_spawner, "In")
    conform.get_settings().set_editor_property("embed_cm", embed_cm)
    # The artist's layout, not the spire's: the template pushes its base pieces 1.3x outward (Wes, Oct 14, for the ED spire); on a Dark Ruins
    # mountain that pulled the foot apart, and the low ledges' plants and the foot rocks with it (measured Oct 25: base pieces 1.3x out, 4-6 m down)
    conform.get_settings().set_editor_property("base_spread", 1.0)

    main = load(g, asset_name)
    main.add_edge_to("Points", copy_main, "Source")
    main.add_edge_to("Root", copy_root, "Source")
    merge = g.add_node_of_type(unreal.PCGMergeSettings)[0]
    copy_main.add_edge_to("Out", merge, "In")

    foot = dressing.get("foot", [])
    for i, cluster in enumerate(FOOT_CLUSTERS):
        rows = foot[i::len(FOOT_CLUSTERS)]
        if not rows:
            continue
        targets = onto_site(g, create_points(g, rows, 1.0), actor)
        src = load(g, cluster)
        cp = g.add_node_of_type(unreal.PCGCopyPointsSettings)[0]
        for prop in ("apply_target_rotation_to_positions", "apply_target_scale_to_positions"):
            cp.get_settings().set_editor_property(prop, True)
        src.add_edge_to("Points", cp, "Source")
        targets.add_edge_to("Out", cp, "Target")
        cp.add_edge_to("Out", merge, "In")

    top = dressing.get("top", []) if TOP_DRESSING else []
    if top:
        onto_site(g, create_points(g, top, DRESSING_DENSITY), actor).add_edge_to("Out", merge, "In")
    merge.add_edge_to("Out", conform, "In")

    rock = density_filter(g, 0.75, 1.0)
    conform.add_edge_to("Out", rock, "In")
    rock.add_edge_to("Out", cliff_spawner, "In")
    if top:
        plants = density_filter(g, 0.0, 0.75)
        conform.add_edge_to("Out", plants, "In")
        jitter = g.add_node_of_type(unreal.PCGTransformPointsSettings)[0]
        js = jitter.get_settings()
        js.set_editor_property("rotation_min", unreal.Rotator(roll=0, pitch=0, yaw=0))
        js.set_editor_property("rotation_max", unreal.Rotator(roll=0, pitch=0, yaw=360))
        js.set_editor_property("scale_min", unreal.Vector(0.7, 0.7, 0.7))
        js.set_editor_property("scale_max", unreal.Vector(1.3, 1.3, 1.3))
        js.set_editor_property("uniform_scale", True)
        # absolute: the site's own scale (x0.3-0.6 since Oct 25) must not shrink the grass with the rock
        js.set_editor_property("absolute_scale", True)
        js.set_editor_property("offset_min", unreal.Vector(0, 0, -5))
        js.set_editor_property("offset_max", unreal.Vector(0, 0, -5))
        plants.add_edge_to("Out", jitter, "In")
        jitter.add_edge_to("Out", weighted_spawner(g), "In")
    eal.save_loaded_asset(g)
    log("%s <- %s: %d foot points in %d clusters, %d top points, embed %.0f cm" % (name, asset_name, len(foot), len(FOOT_CLUSTERS), len(top), embed_cm))


dressing_points = json.load(open(POINTS, encoding="utf-8"))
for graph, (asset, key, embed) in COMPOSITIONS.items():
    build(graph, asset, key, embed, dressing_points.get(key, {}))
