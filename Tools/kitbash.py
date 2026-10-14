"""Kitbash an item mesh: one scan edited in Blender becomes a new mesh for a related item (a stick sharpened into a spear).

    python Tools/kitbash.py <source mesh> <new mesh> taper <fraction> [max|min]
    python Tools/kitbash.py <source mesh> <new mesh> cut <fraction> [max|min]
      e.g. python Tools/kitbash.py /Game/Fab/Megascans/3D/X/Medium/x/StaticMeshes/x /Game/Kitbash/WoodenSpear/SM_WoodenSpear taper 0.12

Needs the editor running (bridge; PIE not running) and Blender 5.2. Steps: Content/Python/kitbash.py exports the source to
Saved/Kitbash/<name>/src.fbx; Tools/kitbash_blender.py applies the op (see there) and writes edit.fbx; kitbash.py imports it at
<new mesh> with the source's material instances; item_mesh_prepare gives it Nanite / a convex hull. Wire it to an item with
`ue.py rows set`, render the icon with icon_studio.py. Why (2026-10-14): several primitive items are a scanned object plus one
cut (a sharpened stick, an antler tine) and Fab has no free scan of the finished thing.
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
MARK = "KITJSON "


def ue_py(code):
    proc = subprocess.run(UE + ["py", "--timeout", "600", "-c", code], capture_output=True, text=True, cwd=ROOT)
    for line in proc.stdout.splitlines():
        if line.startswith(MARK):
            return line[len(MARK):]
    sys.exit("editor step failed:\n" + proc.stdout[-2000:] + proc.stderr[-2000:])


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    src, dest, op, fraction = sys.argv[1:5]
    end = sys.argv[5] if len(sys.argv) > 5 else "max"
    work = os.path.join(ROOT, "Saved", "Kitbash", dest.rsplit("/", 1)[1]).replace("\\", "/")
    os.makedirs(work, exist_ok=True)
    src_fbx, edit_fbx = work + "/src.fbx", work + "/edit.fbx"
    print("export:", ue_py("import importlib, kitbash as k; importlib.reload(k); out(%r + k.export(%r, %r))" % (MARK, src, src_fbx)))
    blender = subprocess.run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(ROOT, "Tools", "kitbash_blender.py"),
                              "--", src_fbx, edit_fbx, op, fraction, end], capture_output=True, text=True)
    lines = [l for l in blender.stdout.splitlines() if l.startswith("KITBASH")]
    print("\n".join(lines) or blender.stdout[-2000:] + blender.stderr[-2000:])
    if not os.path.isfile(edit_fbx):
        sys.exit("blender step wrote nothing")
    result = json.loads(ue_py("import importlib, kitbash as k; importlib.reload(k); out(%r + k.import_edit(%r, %r, %r))"
                              % (MARK, edit_fbx, dest, src)))
    print("import:", result)
    if "error" in result:
        sys.exit(1)
    prep = subprocess.run(UE + ["py", "--timeout", "600", "-c",
                                "import importlib, item_mesh_prepare as p; importlib.reload(p); out(p.prepare([%r]))" % dest],
                          capture_output=True, text=True, cwd=ROOT)
    print("\n".join(l for l in prep.stdout.splitlines() if not l.startswith("[")))


if __name__ == "__main__":
    main()
