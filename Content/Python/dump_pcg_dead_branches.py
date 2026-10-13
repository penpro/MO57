"""List the nodes of the scatter graph whose work reaches no ENABLED sink (a node with no outgoing edge: a spawner, or the graph output). Their whole chain is computed and thrown away.

    python Tools/ue.py py --file Content/Python/dump_pcg_dead_branches.py

Why (2026-10-13): StaticMeshSpawner_3 (native black-locust clusters) and StaticMeshSpawner_5 (native Field_05 trees) are disabled, but the chains feeding them (CopyPoints x a points grid ->
transform -> voxel projection) still run for every cell -- a disabled node passes its input through, it does not stop the upstream work. Lines start with PCGDEAD.
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
g = unreal.load_asset(GRAPH)
nodes = list(g.get_editor_property("nodes"))
log = lambda m: unreal.log("PCGDEAD " + m)


def enabled(n):
    s = n.get_settings()
    try:
        return bool(s.get_editor_property("enabled")) if s else True
    except Exception:
        return True


def cls(n):
    s = n.get_settings()
    return s.get_class().get_name() if s else "?"


down, up = {}, {}
for n in nodes:
    down.setdefault(n.get_name(), [])
    for p in n.get_editor_property("output_pins"):
        lab = str(p.get_editor_property("properties").get_editor_property("label"))
        for e in p.get_editor_property("edges"):
            m = e.get_editor_property("output_pin").get_editor_property("node")
            down[n.get_name()].append((lab, m.get_name()))
            up.setdefault(m.get_name(), []).append((n.get_name(), lab))
byname = {n.get_name(): n for n in nodes}
out_node = g.get_editor_property("output_node") if hasattr(g, "get_editor_property") else None
sinks = [n for n in nodes if not down.get(n.get_name()) and enabled(n)]
if out_node:
    sinks.append(out_node)
live = set()
stack = [s.get_name() for s in sinks]
while stack:
    k = stack.pop()
    if k in live:
        continue
    if k in byname and not enabled(byname[k]) and down.get(k):
        pass   # a disabled pass-through node still forwards liveness (its input flows on)
    live.add(k)
    for u, _ in up.get(k, []):
        stack.append(u)
dead = [n for n in nodes if n.get_name() not in live]
log("%d nodes, %d enabled sinks, %d dead (their work reaches no enabled sink)" % (len(nodes), len(sinks), len(dead)))
for n in dead:
    log("  DEAD %-28s [%s]%s  <- %s  -> %s" % (n.get_name(), cls(n), "" if enabled(n) else " (disabled)",
        ", ".join("%s.%s" % u for u in up.get(n.get_name(), [])), ", ".join("%s:%s" % d for d in down.get(n.get_name(), []))))
# Live nodes that also feed a dead node: where the dead branch should be cut.
for n in nodes:
    k = n.get_name()
    if k in live:
        for lab, m in down.get(k, []):
            if m not in live:
                log("  CUT  %s.%s -> %s" % (k, lab, m))
