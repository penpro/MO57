"""Dump the Electric Dreams PCG graphs (forest, background = far trees, mountain trees, ground) as text: nodes, key settings, mesh selector entries with weights and the mesh sizes.

Run INSIDE the ED project (it has the graphs; MO57 only has a migrated slice of its assets):

    UnrealEditor-Cmd.exe D:\\UEProjects\\ElectricDreamsEnv\\ElectricDreamsEnv.uproject -unattended -nullrhi -nosplash -ExecCmds="py D:/UEProjects/MO57/Tools/ed_pcg_dump.py;Quit"

Writes D:/UEProjects/MO57/Saved/EDDump/<graph>.txt and graphs.txt (the index).
"""
import os

import unreal

OUT = "D:/UEProjects/MO57/Saved/EDDump/"
os.makedirs(OUT, exist_ok=True)
GRAPHS = ["/Game/PCG/Graphs/Forest/PCGDemo_Forest", "/Game/PCG/Graphs/Forest/PCGDemo_Background", "/Game/PCG/Graphs/Forest/PCGDemo_MountainTrees", "/Game/PCG/Graphs/Forest/PCGDemo_ForestCliffs",
          "/Game/PCG/Graphs/Forest/PCGDemo_Background_PlacementSubGraph", "/Game/PCG/Graphs/Forest/PCGDemo_Background_VariantsSubGraph", "/Game/PCG/Graphs/Ground/PCGDemo_Ground"]
SKIP = set(dir(unreal.PCGSettings))


def val(v):
    s = str(v)
    return s if len(s) < 150 else s[:150] + "..."


def props(obj):
    out = {}
    for a in dir(obj):
        if a.startswith("_") or a in SKIP:
            continue
        try:
            v = getattr(obj, a)
        except Exception:
            continue
        if callable(v):
            continue
        out[a] = val(v)
    return out


def dump_graph(path):
    g = unreal.load_asset(path)
    if not g:
        return "missing %s\n" % path
    lines = ["GRAPH %s" % path]
    nodes = g.get_editor_property("nodes")
    lines.append("nodes=%d" % len(nodes))
    for n in nodes:
        s = n.get_settings()
        if not s:
            lines.append("NODE %s (no settings)" % n.get_name())
            continue
        cls = s.get_class().get_name()
        lines.append("NODE %-34s %s" % (n.get_name()[:34], cls))
        p = props(s)
        for k, v in p.items():
            lines.append("      %s = %s" % (k, v))
        if cls == "PCGStaticMeshSpawnerSettings":
            msp = s.get_editor_property("mesh_selector_parameters")
            try:
                for e in msp.get_editor_property("mesh_entries"):
                    d = e.get_editor_property("descriptor")
                    m = d.get_editor_property("static_mesh")
                    ext = m.get_bounds().box_extent if m else None
                    lines.append("      ENTRY w=%s mesh=%s extent=%s" % (e.get_editor_property("weight"), m.get_path_name() if m else None, ("(%.0f,%.0f,%.0f)" % (ext.x, ext.y, ext.z)) if ext else ""))
            except Exception as ex:
                lines.append("      ENTRIES unreadable: %s" % ex)
        if cls == "PCGSubgraphSettings":
            try:
                sg = s.get_subgraph()   # the property itself is protected; the accessor is exposed
                lines.append("      SUBGRAPH %s" % (sg.get_path_name() if sg else None))
            except Exception as ex:
                lines.append("      SUBGRAPH unreadable: %s" % ex)
    return "\n".join(lines) + "\n"


index = []
for gp in GRAPHS:
    txt = dump_graph(gp)
    name = gp.split("/")[-1]
    with open(OUT + name + ".txt", "w", encoding="utf-8") as f:
        f.write(txt)
    index.append("%s %d bytes" % (name, len(txt)))
with open(OUT + "graphs.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(index))
unreal.log("EDDUMP done %s" % index)
