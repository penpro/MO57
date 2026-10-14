"""Item icons from the items' own world meshes: export (editor) -> render (Blender, headless) -> import + wire (editor).

    python Tools/icon_studio.py --missing                 every item with a real world mesh and no icon (the default)
    python Tools/icon_studio.py --items Log01,Clay01      just these (must have a real mesh)
    python Tools/icon_studio.py --items Stick01 --replace re-render an item that already has an icon (overwrites <Id>Icon)
    python Tools/icon_studio.py --all --replace           rebuild every real-mesh item's icon, hand-made ones included
    python Tools/icon_studio.py --all --render-only       render to Saved/IconStudio only (compare before replacing anything)

Needs the editor running (bridge + MCP; PIE not running: saves are refused in PIE) and Blender 5.2. Work files go to
Saved/IconStudio/<ItemId>/ (mesh.fbx, textures, icon.png). Icons land in /MOFramework/Items/<Id>/<Id>Icon and the item row's
UI.IconSmall + UI.IconLarge point at them. Steps: Content/Python/icon_studio_export.py, Tools/icon_studio_blender.py,
Content/Python/icon_studio_import.py (see each for the why). Why this exists: Wes, 2026-10-14 -- icons are rebuilt from the
sourced meshes; the old Content-Browser-thumbnail icons were low side views that left long items as slivers.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Saved", "IconStudio").replace("\\", "/")
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
ITEMS_TABLE = "/MOFramework/Data/DT_Items.DT_Items"
MARK = "ICONJSON "


def ue_py(code, timeout=600):
    """Run editor python through the bridge; return the JSON the code printed after MARK."""
    proc = subprocess.run(UE + ["py", "--timeout", str(timeout), "-c", code], capture_output=True, text=True, cwd=ROOT)
    for line in proc.stdout.splitlines():
        if line.startswith(MARK):
            return json.loads(line[len(MARK):])
    sys.exit("editor step failed:\n" + proc.stdout[-2000:] + proc.stderr[-2000:])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--items", default="", help="comma-separated item ids")
    ap.add_argument("--missing", action="store_true", help="items with a real mesh and no icon (default)")
    ap.add_argument("--all", action="store_true", help="every item with a real mesh")
    ap.add_argument("--replace", action="store_true", help="overwrite existing <Id>Icon textures")
    ap.add_argument("--render-only", action="store_true", help="export + render only; import and wire nothing")
    ap.add_argument("--size", type=int, default=512)
    a = ap.parse_args()

    candidates = ue_py("import importlib, icon_studio_import as m; importlib.reload(m); out(%r + m.items_needing_icons(%s))"
                       % (MARK, bool(a.all or a.items)))
    wanted = [i for i in a.items.split(",") if i]
    items = {k: v for k, v in candidates.items() if not wanted or k in wanted}
    missing = [i for i in wanted if i not in candidates]
    if missing:
        print("no real world mesh (still the placeholder?): " + ", ".join(missing))
    if not items:
        sys.exit("nothing to do")
    print("icon studio: %d item(s): %s" % (len(items), ", ".join(sorted(items))))

    os.makedirs(OUT, exist_ok=True)
    ids = sorted(items)
    for start in range(0, len(ids), 10):   # chunks keep each bridge call well inside its timeout
        chunk = {i: items[i] for i in ids[start:start + 10]}
        report = ue_py("import importlib, json, icon_studio_export as e; importlib.reload(e); out(%r + json.dumps(e.run(%r, %r).splitlines()))"
                       % (MARK, chunk, OUT))
        print("\n".join("  export " + line for line in report))

    render = subprocess.run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(ROOT, "Tools", "icon_studio_blender.py"),
                             "--", OUT, ",".join(ids), str(a.size)], capture_output=True, text=True)
    print("\n".join("  " + l for l in render.stdout.splitlines() if l.startswith("ICONSTUDIO")))

    if a.render_only:
        print("render only: icons are in %s/<ItemId>/icon.png" % OUT)
        return
    placed = ue_py("import importlib, icon_studio_import as m; importlib.reload(m); out(%r + m.import_icons(%r, %r, %s))"
                   % (MARK, OUT, ids, a.replace))
    patch = {i: {"uI": {"IconSmall": p, "IconLarge": p}} for i, p in placed.items()}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(patch, f)
    rows = subprocess.run(UE + ["rows", "set", ITEMS_TABLE, "--file", f.name], capture_output=True, text=True, cwd=ROOT)
    print("\n".join(l for l in rows.stdout.splitlines() if "already exist" not in l)[-1500:])
    os.unlink(f.name)
    print("icons wired: %d of %d" % (len(placed), len(ids)))


if __name__ == "__main__":
    main()
