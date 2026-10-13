"""Cut the scatter graph's DEAD branches at their root: every edge that carries data into a chain whose work reaches no ENABLED sink (a node with no outgoing edge: a spawner).

    python Tools/ue.py py --file Content/Python/cut_pcg_dead_branches.py          (editor open, not in PIE, no inst instance)

Why (2026-10-13): a disabled PCG node passes its input through; it does NOT stop the upstream work. Three native chains end in disabled spawners and were still computed for every cell:
  * RandomChoice_3.Discarded -> CopyPoints_1 (x CreatePointsGrid_1) -> TransformPoints_6 -> VoxelProjection_1 -> StaticMeshSpawner_3 (native black-locust clusters, biome-blind;
    the biome tier places locusts now). Measured: the packaged new-game reveal took 47.8 s with this branch fed 95 % of the item points vs ~40 s at 50 % (same seed, same build).
  * GetActorData_0 -> HiGenGridSize_1 (GRID2048) -> VoxelSampler_1 -> MO Distance Culling_0 -> MO Elevation Bands_0 -> TransformPoints_4 -> StaticMeshSpawner_5 (native Field_05 trees).
  * GetActorData_0 -> HiGenGridSize_2 -> WaitForVoxelWorld_2 -> VoxelSampler_3 -> MO Elevation Bands_2 -> TransformPoints_9 -> MO Resource Spawner_0 (native resource spawner).
The nodes stay in the graph (deleting nodes from editor Python crashed the editor, 2026-10-13); re-enabling one of those spawners means re-connecting its chain on purpose.
Cut at the ROOT only (the edge from live data into the dead chain). TRAP, measured in the packaged build: a Hierarchical Generation Grid Size node left with no edge at all (a first version
of this script peeled the chain edge by edge) stopped ALL runtime generation of the graph, silently. A source is a node with no input PINS, not one whose inputs were cut.
A RandomChoice whose Discarded pin ends up unconnected stops building that output (bOutputDiscardedEntries off). Idempotent. Test: MOFramework.PCG.NoDeadBranches.
Lines start with PCGCUT.
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
g = unreal.load_asset(GRAPH)
log = lambda m: unreal.log("PCGCUT " + m)


def enabled(n):
    s = n.get_settings()
    try:
        return bool(s.get_editor_property("enabled")) if s else True
    except Exception:
        return True


def edges():
    out = []
    for n in g.get_editor_property("nodes"):
        for p in n.get_editor_property("output_pins"):
            lab = str(p.get_editor_property("properties").get_editor_property("label"))
            for e in p.get_editor_property("edges"):
                ip = e.get_editor_property("output_pin")
                out.append((n, lab, ip.get_editor_property("node"), str(ip.get_editor_property("properties").get_editor_property("label"))))
    return out


def live_set(es):
    nodes = list(g.get_editor_property("nodes"))
    has_out = {u.get_name() for u, _, _, _ in es}
    up = {}
    for u, _, v, _ in es:
        up.setdefault(v.get_name(), []).append(u.get_name())
    stack = [n.get_name() for n in nodes if n.get_name() not in has_out and enabled(n)]
    live = set()
    while stack:
        k = stack.pop()
        if k not in live:
            live.add(k)
            stack.extend(up.get(k, []))
    return live


def is_source(n):
    """A node that MAKES data with no input pins at all (Get Actor Data, Create Points Grid). A node whose input pins were cut produces nothing -- not a source."""
    return len(n.get_editor_property("input_pins")) == 0


es = edges()
live = live_set(es)
cut = 0
for u, ulab, v, vlab in es:
    if v.get_name() not in live and (u.get_name() in live or is_source(u)):
        ok = g.remove_edge(u, ulab, v, vlab)
        log("cut %s.%s -> %s.%s (%s)" % (u.get_name(), ulab, v.get_name(), vlab, "removed" if ok else "REMOVE FAILED"))
        cut += 1 if ok else 0
es = edges()
for n in g.get_editor_property("nodes"):
    s = n.get_settings()
    if s and s.get_class().get_name() == "PCGRandomChoiceSettings":
        fed = any(u is n and lab == "Discarded" for u, lab, _, _ in es)
        if not fed and s.get_editor_property("output_discarded_entries"):
            s.set_editor_property("output_discarded_entries", False)
            log("%s: Discarded is unconnected -> output_discarded_entries off" % n.get_name())
live = live_set(es)
fed_dead = [(u.get_name(), v.get_name()) for u, _, v, _ in es if v.get_name() not in live and (u.get_name() in live or is_source(u))]
log("%d edge(s) cut; edges still feeding a dead chain: %s" % (cut, fed_dead or "none"))
unreal.EditorAssetLibrary.save_loaded_asset(g)
log("saved")
