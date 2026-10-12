"""Add the FAR GRASS tier to the scatter graph (MOPCG_StampScatter1): a second ground-cover chain on the 512 m hierarchical grid, so far tufts reach ~1 km while the dense near tier stays at ~130 m.

    GetActorData -> HiGenGridSize (GRID128) -> WaitForVoxelWorld -> VoxelSampler (cell 400 cm) -> MO Biome Spawner (FarGroundCoverOnly, MinDistanceFromViewerCm 10000)

The tier is data-driven: the biome rows carry MOFar_Grass species (Content/Python/biome_palettes.py, far_grass()). Idempotent (skips when a spawner with that filter exists).

    python Tools/ue.py py --file Content/Python/add_far_grass_chain.py        (editor open, not in PIE)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
FAR_CELL_CM = 500.0           # sampler lattice: at most one point per 5 m x 5 m (0.04 / m^2 = 400 / ha) -- far tufts are sparse
LATTICE_FILL = 0.72           # the sampler asks for this fraction of the lattice cap: random thinning, not a grid (test MOFramework.PCG.BiomeSamplerLattice demands <= 0.9)
FAR_MIN_DISTANCE_CM = 10000.0  # nothing closer than 100 m to the viewer at generation time (the near tier's zone)

g = unreal.load_asset(GRAPH)
nodes = g.get_editor_property("nodes")
byname = {n.get_name(): n for n in nodes}


def log(m):
    unreal.log("FARGRASS " + m)


for n in nodes:
    s = n.get_settings()
    if s and s.get_class().get_name() == "MOPCGBiomeSpawnerSettings" and "FAR_GROUND_COVER_ONLY" in str(s.get_editor_property("species_filter")):
        log("already present (%s): nothing to do" % n.get_name())
        raise SystemExit

src_hi = byname["HiGenGridSize_2"]      # a GRID128 node
src_wait = byname["WaitForVoxelWorld_2"]
src_vox = byname["VoxelSampler_3"]
src_spawn = byname["MO Biome Spawner_1"]
get_actor = byname["GetActorData_0"]


def add(settings_cls):
    node, settings = g.add_node_of_type(settings_cls)
    return node


def copy_props(dst, src, skip=()):
    for a in dir(src):
        if a.startswith("_") or a in skip or a in dir(unreal.PCGSettings):
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


hi = add(unreal.PCGHiGenGridSizeSettings)
hi.get_settings().set_editor_property("hi_gen_grid_size", src_hi.get_settings().get_editor_property("hi_gen_grid_size"))
wait = add(unreal.PCGWaitForVoxelWorldSettings)
vox = add(unreal.PCGVoxelSamplerSettings)
copy_props(vox.get_settings(), src_vox.get_settings())
vox.get_settings().set_editor_property("cell_size", FAR_CELL_CM)
PPSM = LATTICE_FILL / ((FAR_CELL_CM / 100.0) ** 2)
vox.get_settings().set_editor_property("points_per_squared_meter", PPSM)   # a fraction of the lattice cap
spawn = add(unreal.MOPCGBiomeSpawnerSettings)
copy_props(spawn.get_settings(), src_spawn.get_settings())
ss = spawn.get_settings()
ss.set_editor_property("species_filter", unreal.MOBiomeSpeciesFilter.FAR_GROUND_COVER_ONLY)
ss.set_editor_property("min_distance_from_viewer_cm", FAR_MIN_DISTANCE_CM)
# the sampler emits PPSM points per m^2 => PPSM * 10000 per hectare: the spawner's acceptance divides by this
ss.set_editor_property("input_points_per_hectare", PPSM * 10000.0)

def target_pin(src_node, dst_node):
    """The input pin label of dst_node that an existing edge from src_node ends at (so the new chain is wired exactly like the old one)."""
    for p in src_node.get_editor_property("output_pins"):
        for e in p.get_editor_property("edges"):
            ip = e.get_editor_property("output_pin")
            if ip.get_editor_property("node").get_name() == dst_node.get_name():
                return str(ip.get_editor_property("properties").get_editor_property("label"))
    return "In"


get_actor.add_edge_to("Out", hi, target_pin(get_actor, src_hi))
hi.add_edge_to("Out", wait, target_pin(src_hi, src_wait))
wait.add_edge_to("Out", vox, target_pin(src_wait, src_vox))
vox.add_edge_to("Out", spawn, target_pin(src_vox, byname["MO Elevation Bands_2"]))
log("pins: actor->hi=%s hi->wait=%s wait->vox=%s vox->spawn=%s" % (target_pin(get_actor, src_hi), target_pin(src_hi, src_wait), target_pin(src_wait, src_vox), target_pin(src_vox, byname["MO Elevation Bands_2"])))
log("added GRID128 chain: hi=%s wait=%s vox=%s spawn=%s" % (hi.get_name(), wait.get_name(), vox.get_name(), spawn.get_name()))
unreal.EditorAssetLibrary.save_loaded_asset(g)
