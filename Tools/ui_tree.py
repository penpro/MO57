"""Compact tree of a Widget Blueprint (wraps `ue.py ui dump`): type, name, [var], canvas rect / anchors, and the properties that matter for a restyle.

    python Tools/ui_tree.py /MOFramework/UI/CraftingSystem/WBP_CraftingMenu [more assets ...]
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def walk(n, d=0):
    s = n.get("slot", {}).get("layout_data", {})
    off, anc = s.get("offsets"), s.get("anchors")
    p = n.get("props", {})
    keep = {k: v for k, v in p.items() if k in ("text", "button_label", "width_override", "height_override", "brush_color", "style", "min_desired_width", "min_desired_height", "visibility")}
    print("  " * d + "%s %s%s%s%s %s" % (n["type"].split("/")[-1], n["name"], " [var]" if n.get("var") else "",
                                        " off=%s" % json.dumps(off) if off else "", " anc=%s" % json.dumps(anc) if anc else "", json.dumps(keep)[:150] if keep else ""))
    for c in n.get("children", []):
        walk(c, d + 1)


for asset in sys.argv[1:]:
    out = subprocess.run([sys.executable, os.path.join(ROOT, "Tools", "ue.py"), "ui", "dump", asset], capture_output=True, text=True, encoding="utf-8").stdout
    try:
        t = json.loads(out)
    except Exception:
        print("== %s: %s" % (asset, out[:200]))
        continue
    print("== %s (parent %s)" % (asset, t.get("parent")))
    walk(t["root"])
