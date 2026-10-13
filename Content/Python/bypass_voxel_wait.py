"""EXPERIMENT (Oct 13 2026): let the NEAR PCG tiers sample the voxel layers without waiting for the voxel world's first full render.

    python Tools/ue.py py --file Content/Python/bypass_voxel_wait.py          (APPLY = False below: report only)

Why: every chain of MOPCG_StampScatter1 runs GetActorData -> HiGenGridSize -> Wait For Voxel World -> Voxel Sampler -> ... The wait node pauses PCG until the
voxel world's NEW STATE is ready to render (PCGWaitForVoxelWorld.cpp). At a new game that is the whole first render of the terrain around the pawn: ~10 s at
960x540, ~14 s at 1920x1080, during which no PCG cell produces anything -- the near tiers then arrive just before the 15 s vegetation cap. The Voxel Sampler
itself reads FVoxelLayers (the world's stamp registry) with a dependency collector, not the render state.
This script reroutes HiGenGridSize -> Voxel Sampler directly for the chains on the grids in BYPASS_GRIDS and leaves the wait node with no edges (an isolated
WAIT node is harmless; an isolated HiGenGridSize node is NOT -- it stopped all generation once, see cut_pcg_dead_branches.py -- so the grid node always
keeps its edge to the sampler). Measure READY and the placement (MO.PCG.Census sink / tilt) before keeping it; revert by restoring the graph from git.
Lines start with PCGWAIT.

RESULT (Oct 13 2026, packaged, 1920x1080): NO GAIN -- READY 24.6 s bypassed vs 24.5 s, the near cells arrived with the voxel state either way. The
real wait was the voxel world's own state (far chunks resolving sub-voxel lakes: FMOWorldGen::IsWaterTierResolved). Kept as a report-only tool
(APPLY = False); the graph keeps its wait nodes.
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
APPLY = False
BYPASS_GRIDS = ("GRID16", "GRID32", "GRID128")   # the near tiers: ground cover, shrubs / rocks, trees (the far tiers keep their wait)

g = unreal.load_asset(GRAPH)
log = lambda m: unreal.log("PCGWAIT " + m)


def cls(n):
    s = n.get_settings()
    return s.get_class().get_name() if s else "?"


def outs(n):
    r = []
    for p in n.get_editor_property("output_pins"):
        lab = str(p.get_editor_property("properties").get_editor_property("label"))
        for e in p.get_editor_property("edges"):
            ip = e.get_editor_property("output_pin")
            r.append((lab, ip.get_editor_property("node"), str(ip.get_editor_property("properties").get_editor_property("label"))))
    return r


nodes = list(g.get_editor_property("nodes"))
changed = 0
for w in nodes:
    if cls(w) != "PCGWaitForVoxelWorldSettings":
        continue
    ups = [(u, lab, ilab) for u in nodes for lab, v, ilab in outs(u) if v is w]
    downs = outs(w)
    for u, ulab, wlab in ups:
        if cls(u) != "PCGHiGenGridSizeSettings":
            log("%s: upstream %s is not a grid node -- left alone" % (w.get_name(), u.get_name()))
            continue
        grid = str(u.get_settings().get_editor_property("hi_gen_grid_size")).split(".")[-1].split(":")[0]
        log("%s: %s (%s) -> %s -> %s" % (w.get_name(), u.get_name(), grid, w.get_name(), [d.get_name() for _, d, _ in downs]))
        if grid not in BYPASS_GRIDS or not APPLY:
            continue
        for dlab, d, dilab in downs:
            u.add_edge_to(ulab, d, dilab)                 # grid -> sampler first: the grid node never loses all its edges
            g.remove_edge(w, dlab, d, dilab)
        g.remove_edge(u, ulab, w, wlab)
        changed += 1
        log("  bypassed")
if APPLY and changed:
    unreal.EditorAssetLibrary.save_loaded_asset(g)
log("%d chain(s) bypassed (APPLY=%s)" % (changed, APPLY))
