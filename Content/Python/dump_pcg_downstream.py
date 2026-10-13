"""Dump everything downstream of a PCG graph node (breadth-first, every output pin): node, class, the pin it leaves by, the pin it enters, and depth. No depth limit.

    python Tools/ue.py py --file Content/Python/dump_pcg_downstream.py            (edit START below, or set env MO_PCG_START="RandomChoice_3")

Why (2026-10-13): a depth-limited trace called RandomChoice_3's Discarded output "a dead end" when it fed the black-locust clusters; thinning items there doubled the clusters. Trace fully before
changing a split. Lines start with PCGDOWN.
"""
import os
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
START = os.environ.get("MO_PCG_START", "RandomChoice_3")

g = unreal.load_asset(GRAPH)
nodes = {n.get_name(): n for n in g.get_editor_property("nodes")}
log = lambda m: unreal.log("PCGDOWN " + m)


def cls(n):
    s = n.get_settings()
    return s.get_class().get_name() if s else "?"


def outs(n):
    for p in n.get_editor_property("output_pins"):
        lab = str(p.get_editor_property("properties").get_editor_property("label"))
        for e in p.get_editor_property("edges"):
            ip = e.get_editor_property("output_pin")
            yield lab, ip.get_editor_property("node"), str(ip.get_editor_property("properties").get_editor_property("label"))


seen = {START}
queue = [(nodes[START], 0)]
while queue:
    n, d = queue.pop(0)
    for lab, m, inlab in outs(n):
        enabled = m.get_settings().get_editor_property("enabled") if m.get_settings() else True
        log("%s%s.%s -> %s.%s  [%s]%s" % ("  " * d, n.get_name(), lab, m.get_name(), inlab, cls(m), "" if enabled else " (DISABLED)"))
        if m.get_name() not in seen:
            seen.add(m.get_name())
            queue.append((m, d + 1))
log("done: %d nodes downstream of %s" % (len(seen) - 1, START))
