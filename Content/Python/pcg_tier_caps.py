"""Set a biome-spawner tier's candidate rate: the MO Biome Spawner's InputPointsPerHectare AND the voxel sampler that feeds it.

    python Tools/ue.py py -c "import pcg_tier_caps as c; out(c.show())"
    python Tools/ue.py py -c "import pcg_tier_caps as c; out(c.set_tier('MO Biome Spawner_5', 300, 500))"   # 300/ha on a 5 m lattice

Why both: the spawner turns each input point into at most one plant with p = density / InputPointsPerHectare, so the sampler's real
rate (points_per_squared_meter, at most one point per cell_size lattice cell) and the spawner's InputPointsPerHectare must agree,
or every species is mis-scaled. Rule (Oct 11): cell_size <= ~70 / sqrt(points per m^2) cm, so the lattice never caps the rate.
Edits MOPCG_StampScatter1 (not during PIE) and saves it. Output lines start with PCGCAPS.
"""
import math

import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"


def _graph():
    return unreal.load_asset(GRAPH)


def _upstream_sampler(graph, spawner):
    """Walk input edges back from the spawner to the first PCGVoxelSamplerSettings node."""
    seen, frontier = set(), [spawner]
    while frontier:
        node = frontier.pop()
        for pin in node.get_editor_property("input_pins"):
            for edge in pin.get_editor_property("edges"):
                src = edge.get_editor_property("input_pin").get_outer()
                if src in seen:
                    continue
                seen.add(src)
                settings = src.get_settings()
                if settings and settings.get_class().get_name() == "PCGVoxelSamplerSettings":
                    return src
                frontier.append(src)
    return None


def _spawners(graph):
    return [n for n in graph.get_editor_property("nodes")
            if n.get_settings() and n.get_settings().get_class().get_name() == "MOPCGBiomeSpawnerSettings"]


def show():
    g = _graph()
    lines = []
    for n in _spawners(g):
        s = n.get_settings()
        sampler = _upstream_sampler(g, n)
        ss = sampler.get_settings() if sampler else None
        lines.append("PCGCAPS %s filter=%s input=%.0f/ha sampler=%s cell=%s pts/m2=%s (=%.0f/ha)" % (
            n.get_name(), s.get_editor_property("species_filter"), s.get_editor_property("input_points_per_hectare"),
            sampler.get_name() if sampler else None, ss.get_editor_property("cell_size") if ss else None,
            ss.get_editor_property("points_per_squared_meter") if ss else None,
            ss.get_editor_property("points_per_squared_meter") * 10000.0 if ss else 0.0))
    return "\n".join(lines)


def set_tier(spawner_name, per_hectare, cell_cm=None):
    g = _graph()
    node = next((n for n in _spawners(g) if n.get_name() == spawner_name), None)
    if node is None:
        return "PCGCAPS no spawner named %s" % spawner_name
    sampler = _upstream_sampler(g, node)
    if sampler is None:
        return "PCGCAPS %s: no voxel sampler upstream" % spawner_name
    pts = per_hectare / 10000.0
    cell = cell_cm if cell_cm else math.floor(70.0 / math.sqrt(pts))
    lattice_max = 1e8 / (cell * cell)   # one point per cell; 1 ha = 1e8 cm^2
    if lattice_max < per_hectare:
        return "PCGCAPS refused: a %d cm lattice holds at most %.0f/ha < %.0f/ha" % (cell, lattice_max, per_hectare)
    node.get_settings().set_editor_property("input_points_per_hectare", float(per_hectare))
    sampler.get_settings().set_editor_property("points_per_squared_meter", pts)
    sampler.get_settings().set_editor_property("cell_size", float(cell))
    unreal.EditorAssetLibrary.save_loaded_asset(g, False)
    return "PCGCAPS %s: input %.0f/ha, %s %.4f/m2 on a %d cm lattice (saved)" % (spawner_name, per_hectare, sampler.get_name(), pts, cell)
