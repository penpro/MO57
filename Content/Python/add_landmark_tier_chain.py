"""Add the LANDMARK tier to the scatter graph (MOPCG_StampScatter1): the species with bLandmark (the xlarge boulders, the cliff faces and the crag formations) on 1 km cells out to ~2.5 km.

    GetActorData -> HiGenGridSize (GRID256: 1024 m cells) -> WaitForVoxelWorld -> VoxelSampler (cell 1500 cm) -> MO Biome Spawner (LandmarksOnly, no near floor)

Wes, Oct 24 2026: "the largest rocks, and the cliff faces, let's have them render much further out so they add some detail to the high lands at a distance". The large tier (GRID32) generates only
out to 300 m. Unlike the far tiers this one plants the REAL thing, not stand-ins: no MinDistanceFromViewerCm, so the instance you see from 2 km is the one you walk up to (a 30 m cliff swapping
for another as you approach would show); it keeps the large tier's shadows and collision (its spawner settings are copied from the large tier's node). GRID256 shares the far trees' radius
(radius25600 = 2.5 km, set_game_radii.py) and is generated during the load (the readiness far-grid hold starts at GRID512), so the boulders beside the spawn are there at the reveal.
The biome spawner's LargeOnly / TreesOnly / ground cover tiers leave landmarks out (EMOBiomeSpeciesFilter::LandmarksOnly), so this chain MUST exist or the landmarks vanish. Idempotent.

    python Tools/ue.py py --file Content/Python/add_landmark_tier_chain.py        (editor open, not in PIE)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
CELL_CM = 1500.0        # lattice cap: one candidate point per 15 m x 15 m (44 / ha); the sampler fills 72 % of it (32 / ha): a crag core asks ~10 / ha, the boulders 1-3 / ha
LATTICE_FILL = 0.72     # random thinning, not a grid (test MOFramework.PCG.BiomeSamplerLattice demands <= 0.9)
PPSM = LATTICE_FILL / ((CELL_CM / 100.0) ** 2)

g = unreal.load_asset(GRAPH)
nodes = g.get_editor_property("nodes")
byname = {n.get_name(): n for n in nodes}
log = lambda m: unreal.log("LANDMARKTIER " + m)

for n in nodes:
    s = n.get_settings()
    if s and s.get_class().get_name() == "MOPCGBiomeSpawnerSettings" and "LANDMARKS_ONLY" in str(s.get_editor_property("species_filter")):
        log("already present (%s)" % n.get_name())
        raise SystemExit

src_hi, src_wait, src_vox, src_spawn, get_actor = byname["HiGenGridSize_2"], byname["WaitForVoxelWorld_2"], byname["VoxelSampler_3"], byname["MO Biome Spawner_1"], byname["GetActorData_0"]
if "LARGE_ONLY" not in str(src_spawn.get_settings().get_editor_property("species_filter")):
    raise RuntimeError("MO Biome Spawner_1 is no longer the large tier's node (%s): pick the source node again" % src_spawn.get_settings().get_editor_property("species_filter"))


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
hi.get_settings().set_editor_property("hi_gen_grid_size", unreal.PCGHiGenGrid.GRID256)
wait = g.add_node_of_type(unreal.PCGWaitForVoxelWorldSettings)[0]
vox = g.add_node_of_type(unreal.PCGVoxelSamplerSettings)[0]
copy_props(vox.get_settings(), src_vox.get_settings())
vox.get_settings().set_editor_property("cell_size", CELL_CM)
vox.get_settings().set_editor_property("points_per_squared_meter", PPSM)
spawn = g.add_node_of_type(unreal.MOPCGBiomeSpawnerSettings)[0]
copy_props(spawn.get_settings(), src_spawn.get_settings())
ss = spawn.get_settings()
ss.set_editor_property("species_filter", unreal.MOBiomeSpeciesFilter.LANDMARKS_ONLY)
ss.set_editor_property("min_distance_from_viewer_cm", 0.0)
ss.set_editor_property("input_points_per_hectare", PPSM * 10000.0)
get_actor.add_edge_to("Out", hi, target_pin(get_actor, src_hi))
hi.add_edge_to("Out", wait, target_pin(src_hi, src_wait))
wait.add_edge_to("Out", vox, target_pin(src_wait, src_vox))
vox.add_edge_to("Out", spawn, "In")
unreal.EditorAssetLibrary.save_loaded_asset(g)
log("added %s %s %s %s (cell %.0f cm, %.1f points / ha, shadows %s)" % (hi.get_name(), wait.get_name(), vox.get_name(), spawn.get_name(), CELL_CM, PPSM * 10000.0,
                                                                      ss.get_editor_property("cast_shadows")))
