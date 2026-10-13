"""The game level's PCG generation radii (2026-10-12: "make the radius longer so it doesn't pop in so bad").

READ THIS BEFORE EDITING THE NUMBERS: the radius FIELDS are named by the UNSCALED grid (radius1600 = hi-gen grid 16), but the scatter graph runs with HiGenGridSizeMultiplier 4, so a node
set to GRID16 makes 64 m cells and looks its radius up in radius1600, GRID32 (128 m cells) in radius3200, GRID128 (512 m cells) in radius12800, GRID256 (1 km cells) in radius25600.
(The first version of this script set radius6400 / 51200 / 102400 -- fields no node of the graph uses -- so the "longer radius" never took effect: the engine default is twice the UNSCALED grid,
i.e. 32 m for the 64 m near tier, 512 m for the 1 km far-tree tier.) `MO.PCG.Rings` in a running game shows what really spawns at each distance.

Overrides (cm): near ground cover + native grass + items (GRID16) 180 m; shrubs / rocks / saplings (GRID32) 300 m; far grass AND the tree tier (GRID128) 1.4 km (the trees are Nanite:
the limit is generation cost, not draw cost); far-tree stand-ins (GRID256) 3.3 km; far-canopy clump proxies (GRID512, `add_far_canopy_chain.py`) 7.5 km. Every other field is put back to the engine default (twice its own grid). A lower FoliageQuality tier (pcg.Quality 0-2) gets a shorter reach: 35 / 50 / 75 % of these radii.

    python Tools/ue.py py --file Content/Python/set_game_radii.py        (editor open, not in PIE)
"""
import unreal

MAP = "/Game/VoxelExamples/PCGScattering/MOPCGScattering"
GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
OVERRIDES = {"radius1600": 18000.0, "radius3200": 30000.0, "radius12800": 150000.0, "radius25600": 330000.0, "radius51200": 750000.0}   # 2026-10-13: tree tier 1.5 km, stand-in trees 3.3 km, far-canopy clumps 7.5 km (GRID512: radius51200)
# Quality: the FoliageQuality scalability group sets pcg.Quality 0..4 (Low .. Cine); a lower tier gets a shorter reach (per-quality value of the same field).
QUALITY_SCALE = {0: 0.35, 1: 0.5, 2: 0.75, 3: 1.0, 4: 1.0}
FIELDS = ("radius400", "radius800", "radius1600", "radius3200", "radius6400", "radius12800", "radius25600", "radius51200", "radius102400", "radius204800")

graph = unreal.load_asset(GRAPH)
unreal.log("GAMERADII graph HiGenGridSizeMultiplier = %s" % graph.get_editor_property("hi_gen_grid_size_multiplier"))

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
done = []
for a in eas.get_all_level_actors():
    if a.get_class().get_name() != "PCGVolume":
        continue
    for c in a.get_components_by_class(unreal.ActorComponent):
        if c.get_class().get_name() != "PCGComponent":
            continue
        r = c.get_editor_property("generation_radii")
        for k in FIELDS:
            f = r.get_editor_property(k)
            base = OVERRIDES.get(k, 2.0 * float(k[len("radius"):]))
            f.set_editor_property("default", base)
            f.set_editor_property("per_quality", {q: base * sc for q, sc in QUALITY_SCALE.items()} if k in OVERRIDES else {})
            r.set_editor_property(k, f)
        c.set_editor_property("generation_radii", r)
        c.set_editor_property("override_generation_radii", True)
        done.append("%s override on: %s" % (a.get_name(), ", ".join("%s=%.0f" % (k, v) for k, v in OVERRIDES.items())))
unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log("GAMERADII %s" % done)
