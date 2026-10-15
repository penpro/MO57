"""Thin the native GROUND ITEM clutter (sticks, bark, stones, flint, cobbles) by about a factor of ten (2026-10-13, Wes: "an ass ton of ground spawn branches and bark ... cut those down by a factor of 10, that will help
free up some CPU").

Where they come from (found by tracing MOPCG_StampScatter1): VoxelSampler_0 (150 cm cells, 0.2 pts/m2 = 2000 / ha) -> MO Elevation Bands_1 (Band2) -> RandomChoice_3 (ratio 0.5) -> Chosen -> RandomChoice_1 (ratio 0.765:
Mesh Spawner_0 = Stick01 / Stone01 / Bark01 / FlintNodule01 / RiverCobble01; the rest Mesh Spawner_1 = Stick02). About 1000 item instances per hectare near the player, on top of the biome tier's own ground cover
(the biome spawner shares VoxelSampler_0 directly, so the sampler itself must NOT be thinned).

RandomChoice_3's Discarded half fed CopyPoints_1 (x CreatePointsGrid_1) -> TransformPoints_6 -> VoxelProjection_1 -> StaticMeshSpawner_3, the native black-locust clusters -- a spawner that is
DISABLED (the biome tier places locusts), so the branch spawned nothing but was still computed: the first version of this script lowered RandomChoice_3 to 0.05 and sent 95 % of the points
into that wasted work (the packaged reveal: 47.8 s vs ~40 s). The branch is now cut at its root (cut_pcg_dead_branches.py; test MOFramework.PCG.NoDeadBranches).

Now: RandomChoice_3 at 0.5, and a Random Choice of its own (ratio ITEM_RATIO, Discarded output off) between RandomChoice_3.Chosen and RandomChoice_1:
0.5 x 0.1 = 5 % of the points become items (x10 fewer). Stick01 / Bark01 weigh 0.3 against 1.0 for the stones, so sticks + bark end up ~x20 fewer, stones / flint / cobbles ~x7, Stick02 ~x10.
Still ~36 sticks and ~20 stones per hectare: a stick every 15-20 m, plenty to forage, no longer a carpet. Test: MOFramework.PCG.ItemThinning.

    python Tools/ue.py py --file Content/Python/thin_item_clutter.py      (editor open, no inst instance)

Idempotent (finds the thinning node if it is already there). Never deletes a node (deleting from editor Python crashed the editor, 2026-10-13): edges only.
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
SPLIT_RATIO = 0.5       # RandomChoice_3: items vs the locust-cluster branch (authored value)
ITEM_RATIO = 0.1 * 2000.0 / 6000.0   # the item thinning node: 0.1 of a 2000/ha sampler; VoxelSampler_0 went to 6000/ha on 2026-10-14 (ground cover tier, pcg_tier_caps.py), same items/ha
THIN_SEED = 7331        # its own seed: not correlated with RandomChoice_3's pick
WEIGHTS = {"Stick01": 0.3, "Bark01": 0.3}   # everything else keeps weight 1.0
log = lambda m: unreal.log("ITEMCLUTTER " + m)

g = unreal.load_asset(GRAPH)
nodes = {n.get_name(): n for n in g.get_editor_property("nodes")}
rc3, rc1 = nodes["RandomChoice_3"], nodes["RandomChoice_1"]


def downstream(node, label):
    """[(node, input pin label)] that an output pin of `node` feeds."""
    out = []
    for p in node.get_editor_property("output_pins"):
        if str(p.get_editor_property("properties").get_editor_property("label")) != label:
            continue
        for e in p.get_editor_property("edges"):
            ip = e.get_editor_property("output_pin")
            out.append((ip.get_editor_property("node"), str(ip.get_editor_property("properties").get_editor_property("label"))))
    return out


def is_random_choice(node):
    s = node.get_settings()
    return s is not None and s.get_class().get_name() == "PCGRandomChoiceSettings"


s3 = rc3.get_settings()
s3.set_editor_property("fixed_mode", False)
s3.set_editor_property("ratio", SPLIT_RATIO)
log("RandomChoice_3 fixed_mode=%s ratio=%s (Discarded feeds: %s -- should be nothing, see cut_pcg_dead_branches.py)" % (
    s3.get_editor_property("fixed_mode"), s3.get_editor_property("ratio"), [n.get_name() for n, _ in downstream(rc3, "Discarded")]))

chosen = downstream(rc3, "Chosen")
log("RandomChoice_3.Chosen feeds %s" % [(n.get_name(), l) for n, l in chosen])
if len(chosen) == 1 and chosen[0][0].get_name() == rc1.get_name():
    rc1_pin = chosen[0][1]
    thin = g.add_node_of_type(unreal.PCGRandomChoiceSettings)[0]
    removed = g.remove_edge(rc3, "Chosen", rc1, rc1_pin)
    rc3.add_edge_to("Chosen", thin, "In")
    thin.add_edge_to("Chosen", rc1, rc1_pin)
    log("inserted %s between RandomChoice_3.Chosen and RandomChoice_1.%s (old edge removed: %s)" % (thin.get_name(), rc1_pin, removed))
elif len(chosen) == 1 and is_random_choice(chosen[0][0]):
    thin = chosen[0][0]
    log("thinning node already present: %s" % thin.get_name())
else:
    raise RuntimeError("unexpected wiring after RandomChoice_3.Chosen: %s" % [(n.get_name(), l) for n, l in chosen])

ts = thin.get_settings()
ts.set_editor_property("fixed_mode", False)
ts.set_editor_property("ratio", ITEM_RATIO)
ts.set_editor_property("output_discarded_entries", False)
ts.set_editor_property("seed", THIN_SEED)
log("%s fixed_mode=%s ratio=%s output_discarded=%s seed=%s -> %s" % (thin.get_name(), ts.get_editor_property("fixed_mode"), ts.get_editor_property("ratio"),
    ts.get_editor_property("output_discarded_entries"), ts.get_editor_property("seed"), [(n.get_name(), l) for n, l in downstream(thin, "Chosen")]))

s = nodes["MO Mesh Spawner_0"].get_settings()
rebuilt = []
for e in s.get_editor_property("items_to_spawn"):   # (struct elements read from the array are copies: rebuild the entries and set the whole array)
    n = unreal.MOPCGItemSpawnEntry()
    item = str(e.get_editor_property("item_id"))
    n.set_editor_property("item_id", e.get_editor_property("item_id"))
    n.set_editor_property("weight", WEIGHTS.get(item, e.get_editor_property("weight")))
    n.set_editor_property("min_quantity", e.get_editor_property("min_quantity"))
    n.set_editor_property("max_quantity", e.get_editor_property("max_quantity"))
    rebuilt.append(n)
s.set_editor_property("items_to_spawn", rebuilt)
log("Mesh Spawner_0 weights: %s" % [(str(e.get_editor_property("item_id")), e.get_editor_property("weight")) for e in s.get_editor_property("items_to_spawn")])
unreal.EditorAssetLibrary.save_loaded_asset(g)
log("saved")
