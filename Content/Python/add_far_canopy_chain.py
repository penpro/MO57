"""Add the FAR-CANOPY tier to the scatter graph (MOPCG_StampScatter1): solid clump proxies (tag MOFar_Canopy, make_far_canopy.py) for the forest 3-9 km away, on 2 km cells.

    GetActorData -> HiGenGridSize (GRID512: 2048 m cells) -> WaitForVoxelWorld -> VoxelSampler (cell 2000 cm) -> MO Biome Spawner (FarCanopyOnly, MinDistanceFromViewerCm 300000)

(Cloned from add_far_tree_chain.py; the generation radius of this tier is the `radius51200` field, set by set_game_radii.py.)

The stand-ins are never DRAWN inside MinDistanceFromViewerCm: the biome spawner copies it to the component descriptor's InstanceMinDrawDistance, so a far cell that becomes near leaves no oversized ghost trees beside the real ones (and far tiers cast no shadow, are out of the ray-traced scene and ignore wind).
Set the cell's generation radius with Content/Python/set_game_radii.py (radius25600: the radius fields are named by the UNSCALED grid, see that script). Idempotent.

    python Tools/ue.py py --file Content/Python/add_far_tree_chain.py        (editor open, not in PIE)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
CELL_CM = 2000.0        # lattice cap: one candidate point per 20 m x 20 m (25 / ha); the sampler fills 72 % of it (18 / ha). (Was 14 m / 9 km: 1.3 M candidate points, 284 k clumps, +3 ms on the render thread and a minute of PCG work ahead of the near tiers.)
LATTICE_FILL = 0.72     # random thinning, not a grid (test MOFramework.PCG.BiomeSamplerLattice demands <= 0.9)
PPSM = LATTICE_FILL / ((CELL_CM / 100.0) ** 2)
MIN_DISTANCE_CM = 300000.0   # no clump within 3 km of the viewer at generation time (the stand-in tree tier owns the ground out to there)

g = unreal.load_asset(GRAPH)
nodes = g.get_editor_property("nodes")
byname = {n.get_name(): n for n in nodes}
log = lambda m: unreal.log("FARCANOPYCHAIN " + m)

for n in nodes:
    s = n.get_settings()
    if s and s.get_class().get_name() == "MOPCGBiomeSpawnerSettings" and "FAR_CANOPY_ONLY" in str(s.get_editor_property("species_filter")):
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
hi.get_settings().set_editor_property("hi_gen_grid_size", unreal.PCGHiGenGrid.GRID512)
wait = g.add_node_of_type(unreal.PCGWaitForVoxelWorldSettings)[0]
vox = g.add_node_of_type(unreal.PCGVoxelSamplerSettings)[0]
copy_props(vox.get_settings(), src_vox.get_settings())
vox.get_settings().set_editor_property("cell_size", CELL_CM)
vox.get_settings().set_editor_property("points_per_squared_meter", PPSM)
spawn = g.add_node_of_type(unreal.MOPCGBiomeSpawnerSettings)[0]
copy_props(spawn.get_settings(), src_spawn.get_settings())
ss = spawn.get_settings()
ss.set_editor_property("species_filter", unreal.MOBiomeSpeciesFilter.FAR_CANOPY_ONLY)
ss.set_editor_property("min_distance_from_viewer_cm", MIN_DISTANCE_CM)
ss.set_editor_property("input_points_per_hectare", PPSM * 10000.0)
get_actor.add_edge_to("Out", hi, target_pin(get_actor, src_hi))
hi.add_edge_to("Out", wait, target_pin(src_hi, src_wait))
wait.add_edge_to("Out", vox, target_pin(src_wait, src_vox))
vox.add_edge_to("Out", spawn, "In")
unreal.EditorAssetLibrary.save_loaded_asset(g)
log("added %s %s %s %s" % (hi.get_name(), wait.get_name(), vox.get_name(), spawn.get_name()))
