"""Kitbash an item mesh: one scan edited in Blender becomes a new mesh for a related item (a stick sharpened into a spear).

    python Tools/kitbash.py <source mesh> <new mesh> taper <fraction> [max|min]
    python Tools/kitbash.py <source mesh> <new mesh> cut <fraction> [max|min]
    python Tools/kitbash.py <shaft mesh> <new mesh> attach <head mesh> <head cm> <tip|axe|adze> [embed] [shaft scale x,y,z]
      e.g. python Tools/kitbash.py /Game/Fab/Megascans/3D/X/Medium/x/StaticMeshes/x /Game/Kitbash/WoodenSpear/SM_WoodenSpear taper 0.12
           python Tools/kitbash.py <stick> /Game/Kitbash/FlintSpear/SM_FlintSpear attach <flint point> 9 tip 0.25 1.5,0.45,0.45

Needs the editor running (bridge; PIE not running) and Blender 5.2. Steps: Content/Python/kitbash.py exports the source (and an
attached head) to Saved/Kitbash/<name>/; Tools/kitbash_blender.py applies the op (see there) and writes edit.fbx; kitbash.py
imports it at <new mesh> on the sources' material instances; item_mesh_prepare gives it Nanite / a convex hull. Wire it to an
item with `ue.py rows set` (an attach result already has the shaft scale baked: use a uniform row scale), render the icon with
icon_studio.py. Why (2026-10-14): many primitive items are a scanned object plus one cut (a sharpened stick) or two scans put
together (a flint point on a shaft), and Fab has no free scan of the finished thing.
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


def export(mesh, fbx):
    return ue_py("import importlib, kitbash as k; importlib.reload(k); out(%r + k.export(%r, %r))" % (MARK, mesh, fbx))


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    src, dest, op = sys.argv[1:4]
    rest = sys.argv[4:]
    work = os.path.join(ROOT, "Saved", "Kitbash", dest.rsplit("/", 1)[1]).replace("\\", "/")
    os.makedirs(work, exist_ok=True)
    src_fbx, edit_fbx = work + "/src.fbx", work + "/edit.fbx"
    print("export:", export(src, src_fbx))
    sources = [src]
    if op == "attach":
        if len(rest) < 3:
            sys.exit(__doc__)
        head, head_cm, mode = rest[0], rest[1], rest[2]
        embed = rest[3] if len(rest) > 3 else "0.2"
        shaft_scale = rest[4] if len(rest) > 4 else "1,1,1"
        head_fbx = work + "/head.fbx"
        print("export head:", export(head, head_fbx))
        sources.append(head)
        op_args = ["attach", head_fbx, head_cm, mode, embed, shaft_scale]
    else:
        op_args = [op, rest[0], rest[1] if len(rest) > 1 else "max"]
    if os.path.exists(edit_fbx):
        os.remove(edit_fbx)
    blender = subprocess.run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(ROOT, "Tools", "kitbash_blender.py"),
                              "--", src_fbx, edit_fbx] + op_args, capture_output=True, text=True)
    lines = [l for l in blender.stdout.splitlines() if l.startswith("KITBASH")]
    print("\n".join(lines) or blender.stdout[-2000:] + blender.stderr[-2000:])
    if not os.path.isfile(edit_fbx):
        sys.exit("blender step wrote nothing")
    result = json.loads(ue_py("import importlib, kitbash as k; importlib.reload(k); out(%r + k.import_edit(%r, %r, %r))"
                              % (MARK, edit_fbx, dest, sources)))
    print("import:", result)
    if "error" in result:
        sys.exit(1)
    prep = subprocess.run(UE + ["py", "--timeout", "600", "-c",
                                "import importlib, item_mesh_prepare as p; importlib.reload(p); out(p.prepare([%r]))" % dest],
                          capture_output=True, text=True, cwd=ROOT)
    print("\n".join(l for l in prep.stdout.splitlines() if not l.startswith("[")))


if __name__ == "__main__":
    main()
