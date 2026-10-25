"""The western cliff face as a world-feature composition: one Desert Western Cliff Sheer XL 04 per member, seated by MO Conform To Ground (Wes,
Oct 25 2026: "i can't find any western cliffs anywhere ... add them to poi list and make a lot more of them in lined up clusters of 5-10").

The row (Tools/western_cliff_rows.py, WC_WesternCliffBand) lines its members up along the hillside (bClusterLine), each turned so the cliff's face
(+Y) looks downhill; this graph only places the cliff on its member's site: the Electric Dreams spire graph's core (DataFromActor -> Copy Points
onto the site -> MO Conform To Ground -> Static Mesh Spawner by the Mesh attribute) fed by one point at the site's origin carrying the mesh. Conform
To Ground gives the piece its own correction (its pivot is in the base band): its lowest point goes EmbedCm under the LOWEST ground beneath its
footprint, so the open back and base go into the hill and the face stands clear down to the slope below it.

    python Tools/ue.py py --file Content/Python/make_western_cliff_composition.py        (editor open, PIE NOT running)

Writes /Game/WorldFeatures/WesternCliffs/PCG_WesternCliffFace. Idempotent. WCLIFF lines.
"""
import unreal

TEMPLATE = "/Game/WorldFeatures/Cliffs/PCG_PL_LargeCliff_01b"
DEST = "/Game/WorldFeatures/WesternCliffs/PCG_WesternCliffFace"
MESH = "/Game/MS/3D/Desert_Western_Cliff_Sheer_Xl_04_xgknben/Smart/SM_Desert_Western_Cliff_Sheer_Xl_04_xgknben_RM_N1.SM_Desert_Western_Cliff_Sheer_Xl_04_xgknben_RM_N1"
EMBED_CM = 300.0   # actor units: x0.35-0.6 at the row's scales -> 1-1.8 m into the ground
KEEP = {"DataFromActor_1", "CopyPoints_2", "MOConformToGround_0", "StaticMeshSpawner_3"}

eal = unreal.EditorAssetLibrary
log = lambda m: out("WCLIFF " + m)

if not eal.does_asset_exist(DEST):
    eal.duplicate_asset(TEMPLATE, DEST)
g = unreal.load_asset(DEST)
for n in list(g.get_editor_property("nodes")):
    if n.get_name() not in KEEP:
        g.remove_node(n)
nodes = {n.get_name(): n for n in g.get_editor_property("nodes")}
actor, copy, conform, spawner = nodes["DataFromActor_1"], nodes["CopyPoints_2"], nodes["MOConformToGround_0"], nodes["StaticMeshSpawner_3"]
for prop in ("apply_target_rotation_to_positions", "apply_target_scale_to_positions"):
    copy.get_settings().set_editor_property(prop, True)
cs = conform.get_settings()
cs.set_editor_property("embed_cm", EMBED_CM)
cs.set_editor_property("base_spread", 1.0)

# One point at the site's origin (the cliff's own pivot), carrying the mesh.
create = g.add_node_of_type(unreal.PCGCreatePointsSettings)[0]
p = unreal.PCGPoint()
p.set_editor_property("transform", unreal.Transform())
p.set_editor_property("density", 1.0)
p.set_editor_property("seed", 1)
create.get_settings().set_editor_property("points_to_create", [p])
create.get_settings().set_editor_property("coordinate_space", unreal.PCGCoordinateSpace.WORLD)
add = g.add_node_of_type(unreal.PCGAddAttributeSettings)[0]
s = add.get_settings()
types = s.get_editor_property("attribute_types")
types.set_editor_property("type", unreal.PCGMetadataTypes.SOFT_OBJECT_PATH)
types.set_editor_property("soft_object_path_value", unreal.SoftObjectPath(MESH))
s.set_editor_property("attribute_types", types)
target = s.get_editor_property("output_target")
target.import_text('(Selection=Attribute,AttributeName="Mesh")')
s.set_editor_property("output_target", target)
create.add_edge_to("Out", add, "In")
add.add_edge_to("Out", copy, "Source")

# The core's wiring (the template has it; removed and added so a rerun never doubles an edge): the site onto the copy, the copy through
# Conform To Ground into the spawner.
for a, a_pin, b, b_pin in ((actor, "Out", copy, "Target"), (copy, "Out", conform, "In"), (conform, "Out", spawner, "In")):
    a.remove_edge_to(a_pin, b, b_pin)
    a.add_edge_to(a_pin, b, b_pin)
eal.save_loaded_asset(g)
check = s.get_editor_property("attribute_types")
log("%s: one %s per site, embed %.0f, Mesh attribute %s" % (DEST, MESH.split(".")[-1], EMBED_CM, check.get_editor_property("soft_object_path_value")))
