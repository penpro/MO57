"""Add the TREE tier to the scatter graph (MOPCG_StampScatter1): trees get their own hierarchical grid so the forest reaches far past the undergrowth, rocks and shrubs of the 128 m tier.

    GetActorData -> HiGenGridSize (GRID128: 512 m cells) -> WaitForVoxelWorld -> VoxelSampler (cell 300 cm) -> MO Biome Spawner (TreesOnly)

The biome spawner's LargeOnly tier no longer plants trees (EMOBiomeSpeciesFilter::TreesOnly takes them), so this chain MUST exist or the world has no trees. Idempotent.

    python Tools/ue.py py --file Content/Python/add_tree_tier_chain.py        (editor open, not in PIE)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
CELL_CM = 300.0   # lattice cap: one candidate point per 3 m x 3 m (1111 / ha); the sampler fills 72 % of it (800 / ha): a rainforest asks 556 trees / ha
LATTICE_FILL = 0.72   # random thinning, not a grid (test MOFramework.PCG.BiomeSamplerLattice demands <= 0.9)
PPSM = LATTICE_FILL / ((CELL_CM / 100.0) ** 2)

g = unreal.load_asset(GRAPH)
nodes = g.get_editor_property("nodes")
byname = {n.get_name(): n for n in nodes}
log = lambda m: unreal.log("TREETIER " + m)

for n in nodes:
    s = n.get_settings()
    if s and s.get_class().get_name() == "MOPCGBiomeSpawnerSettings" and "TREES_ONLY" in str(s.get_editor_property("species_filter")):
        log("already present (%s)" % n.get_name())
        raise SystemExit

src_hi, src_wait, src_vox, src_spawn, get_actor = byname["HiGenGridSize_2"], byname["WaitForVoxelWorld_2"], byname["VoxelSampler_3"], byname["MO Biome Spawner_1"], byname["GetActorData_0"]


def copy_props(dst, src):
    for a in dir(src):
        if a.startswith("_") or a in dir(unreal.PCGSettings):
            continue
        try:
            v = getattr(src, a)
        except Exception:
            continue
        if callable(v):
            continue
        try:
            setattr(dst, a, v)
        except Exception:
            pass


def target_pin(src_node, dst_node):
    for p in src_node.get_editor_property("output_pins"):
        for e in p.get_editor_property("edges"):
            ip = e.get_editor_property("output_pin")
            if ip.get_editor_property("node").get_name() == dst_node.get_name():
                return str(ip.get_editor_property("properties").get_editor_property("label"))
    return "In"


hi = g.add_node_of_type(unreal.PCGHiGenGridSizeSettings)[0]
hi.get_settings().set_editor_property("hi_gen_grid_size", src_hi.get_settings().get_editor_property("hi_gen_grid_size"))
wait = g.add_node_of_type(unreal.PCGWaitForVoxelWorldSettings)[0]
vox = g.add_node_of_type(unreal.PCGVoxelSamplerSettings)[0]
copy_props(vox.get_settings(), src_vox.get_settings())
vox.get_settings().set_editor_property("cell_size", CELL_CM)
vox.get_settings().set_editor_property("points_per_squared_meter", PPSM)
spawn = g.add_node_of_type(unreal.MOPCGBiomeSpawnerSettings)[0]
copy_props(spawn.get_settings(), src_spawn.get_settings())
ss = spawn.get_settings()
ss.set_editor_property("species_filter", unreal.MOBiomeSpeciesFilter.TREES_ONLY)
ss.set_editor_property("input_points_per_hectare", PPSM * 10000.0)
get_actor.add_edge_to("Out", hi, target_pin(get_actor, src_hi))
hi.add_edge_to("Out", wait, target_pin(src_hi, src_wait))
wait.add_edge_to("Out", vox, target_pin(src_wait, src_vox))
vox.add_edge_to("Out", spawn, "In")
unreal.EditorAssetLibrary.save_loaded_asset(g)
log("added %s %s %s %s" % (hi.get_name(), wait.get_name(), vox.get_name(), spawn.get_name()))
