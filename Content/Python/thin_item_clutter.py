"""Thin the native GROUND ITEM clutter (sticks, bark, stones, flint, cobbles) by about a factor of ten (2026-10-13, Wes: "an ass ton of ground spawn branches and bark ... cut those down by a factor of 10, that will help
free up some CPU").

Where they come from (found by tracing MOPCG_StampScatter1): VoxelSampler_0 (150 cm cells, 0.2 pts/m2 = 2000 / ha) -> MO Elevation Bands_1 (Band2) -> RandomChoice_3 (ratio 0.5: half the points become ITEMS) ->
RandomChoice_1 (ratio 0.765: Mesh Spawner_0 = Stick01 / Stone01 / Bark01 / FlintNodule01 / RiverCobble01 at equal weight; the rest Mesh Spawner_1 = Stick02). About 1000 item instances per hectare near the player,
on top of the biome tier's own ground cover (the biome spawner shares VoxelSampler_0 directly, so the sampler itself must NOT be thinned). The Discarded output of RandomChoice_3 goes nowhere (CopyPoints_1 ->
TransformPoints_6 -> VoxelProjection_1 is a dead end), so lowering its ratio is free.

Now: RandomChoice_3 ratio 0.5 -> 0.05 (all items x10 fewer), and Stick01 / Bark01 weigh 0.3 against 1.0 for the stones, so sticks + bark end up ~x20 fewer, stones / flint / cobbles ~x7, Stick02 ~x10.
Still ~36 sticks and ~20 stones per hectare: a stick every 15-20 m, plenty to forage, no longer a carpet.

    python Tools/ue.py py --file Content/Python/thin_item_clutter.py      (editor open, no inst instance)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
ITEM_RATIO = 0.05
WEIGHTS = {"Stick01": 0.3, "Bark01": 0.3}   # everything else keeps weight 1.0

g = unreal.load_asset(GRAPH)
nodes = {n.get_name(): n for n in g.get_editor_property("nodes")}
nodes["RandomChoice_3"].get_settings().set_editor_property("ratio", ITEM_RATIO)
unreal.log("ITEMCLUTTER RandomChoice_3 ratio -> %s" % nodes["RandomChoice_3"].get_settings().get_editor_property("ratio"))
s = nodes["MO Mesh Spawner_0"].get_settings()
items = s.get_editor_property("items_to_spawn")
rebuilt = []
for e in items:   # (struct elements read from the array are copies: rebuild the entries and set the whole array)
    n = unreal.MOPCGItemSpawnEntry()
    item = str(e.get_editor_property("item_id"))
    n.set_editor_property("item_id", e.get_editor_property("item_id"))
    n.set_editor_property("weight", WEIGHTS.get(item, e.get_editor_property("weight")))
    n.set_editor_property("min_quantity", e.get_editor_property("min_quantity"))
    n.set_editor_property("max_quantity", e.get_editor_property("max_quantity"))
    rebuilt.append(n)
s.set_editor_property("items_to_spawn", rebuilt)
unreal.log("ITEMCLUTTER Mesh Spawner_0 weights: %s" % [(str(e.get_editor_property("item_id")), e.get_editor_property("weight")) for e in s.get_editor_property("items_to_spawn")])
unreal.EditorAssetLibrary.save_loaded_asset(g)
unreal.log("ITEMCLUTTER saved")
