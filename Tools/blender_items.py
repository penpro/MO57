"""Blender-made items into the game (Nov 2 2026; Wes: "take a few items that need done and see if you can create some things, especially
things like the crude bow that also needs animation"). Per item:
  1. Blender builds it from measurements and bakes its textures (Tools/blender_items_blender.py) into Saved/BlenderItems/<ItemId>/;
  2. the editor imports it into /Game/BlenderItems/<ItemId>/ (Content/Python/import_blender_items.py);
  3. item_mesh_prepare gives the world mesh Nanite and a hull;
  4. a bow is fitted to the survivor's left hand at her full draw (Content/Python/bow_hand_fit.py: the handle in the hollow of her fist,
     her fingers closing on it as the game closes them);
  5. the row patch (world mesh; a bow's held skeletal mesh, draw clip, hand transform and grip) goes to Saved/BlenderItems/rows.json, and with
     --rows into DT_Items through `ue.py rows set` (readback-verified). The icon is icon_studio.py's job afterwards.

    python Tools/blender_items.py SimpleBow01 [SinewBackedBow01 ...] [--blender-only] [--no-blender] [--rows] [--res N]

Needs Blender 5.2; steps 2-5 need the editor running (bridge; PIE not running). Items it knows: the BOWS and ARROWS tables in
Tools/blender_items_blender.py. Verify a bow in the game with Tools/check_bow.py.
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
OUT = os.path.join(ROOT, "Saved", "BlenderItems")
TABLE = "/MOFramework/Data/DT_Items.DT_Items"
GRIP_POSE = "/MOFramework/Characters/Bow/RMB_Loop_MH.RMB_Loop_MH"   # bow_hand_fit.CLIP / CLIP_T: the fist the seat is fitted in
GRIP_POSE_TIME = 0.33


def ue_py(code, timeout=900):
    proc = subprocess.run(UE + ["py", "--timeout", str(timeout), "-c", code], capture_output=True, text=True, cwd=ROOT)
    for line in proc.stdout.splitlines():
        if line.startswith("{"):
            return json.loads(line)
    sys.exit("editor step failed:\n" + proc.stdout[-3000:] + proc.stderr[-2000:])


def blender(item, res):
    work = os.path.join(OUT, item)
    os.makedirs(work, exist_ok=True)
    args = [BLENDER, "-b", "--factory-startup", "--python", os.path.join(ROOT, "Tools", "blender_items_blender.py"), "--", item, work]
    if res:
        args += ["--res", str(res)]
    proc = subprocess.run(args, capture_output=True, text=True)
    lines = [l for l in proc.stdout.splitlines() if l.startswith("BLENDERITEMS")]
    print("\n".join(lines) or proc.stdout[-3000:] + proc.stderr[-2000:], flush=True)
    if not os.path.isfile(os.path.join(work, item + ".json")):
        sys.exit("blender wrote nothing for " + item)


def transform(rel):
    q, t = rel["rot"], rel["loc"]
    return {"Rotation": {"X": q[0], "Y": q[1], "Z": q[2], "W": q[3]}, "Translation": {"X": t[0], "Y": t[1], "Z": t[2]},
            "Scale3D": {"X": 1, "Y": 1, "Z": 1}}


def editor(item):
    result = ue_py("import importlib, import_blender_items as b; importlib.reload(b); out(b.run(%r))" % item)
    print("import:", json.dumps(result), flush=True)
    prep = subprocess.run(UE + ["py", "--timeout", "600", "-c", "import importlib, item_mesh_prepare as p; importlib.reload(p); out(p.prepare([%r]))"
                                % result["static_mesh"].split(".")[0]], capture_output=True, text=True, cwd=ROOT)
    print("\n".join(l for l in prep.stdout.splitlines() if "ITEMPREP" in l and not l.startswith("[")), flush=True)
    row = {"worldVisual": {"StaticMesh": result["static_mesh"]}}
    if result.get("skeletal_mesh"):
        with open(os.path.join(OUT, item, item + ".json")) as f:
            grip = json.load(f).get("grip_cm")
        if not grip:
            sys.exit("%s.json has no grip_cm: rebuild the bow in Blender (it predates Nov 2's grip)" % item)
        fit = ue_py("import importlib, bow_hand_fit as f; importlib.reload(f); out(f.fit(%r, %r))" % (result["draw"].split(".")[0], tuple(grip)))
        print("fit:", json.dumps(fit), flush=True)
        hand = transform(fit["relative_to_hand_l"])
        # a bow belongs in the left hand (her draws are right-handed); the right hand gets the same seat until a left-handed draw exists. Her
        # fingers close on its handle toward the fist of the full draw the seat came from.
        row["heldVisual"] = {"SkeletalMesh": result["skeletal_mesh"], "DrawAnimation": result["draw"], "bMirrorForLeftHand": False,
                             "LeftHandTransform": hand, "RightHandTransform": hand,
                             "GripHalfSize": {"X": grip[0], "Y": grip[1], "Z": grip[2]}, "GripPose": GRIP_POSE, "GripPoseTime": GRIP_POSE_TIME}
    return row


def main():
    items = [a for a in sys.argv[1:] if not a.startswith("--") and not a.isdigit()]
    res = int(sys.argv[sys.argv.index("--res") + 1]) if "--res" in sys.argv else 0
    if not items:
        sys.exit(__doc__)
    rows = {}
    for item in items:
        if "--no-blender" not in sys.argv:
            blender(item, res)
        if "--blender-only" not in sys.argv:
            rows[item] = editor(item)
    if rows:
        path = os.path.join(OUT, "rows.json")
        with open(path, "w") as f:
            json.dump(rows, f, indent=1)
        print("row patch:", path, flush=True)
        if "--rows" in sys.argv:
            env = dict(os.environ, MSYS_NO_PATHCONV="1")
            proc = subprocess.run(UE + ["rows", "set", TABLE, "--file", path], capture_output=True, text=True, cwd=ROOT, env=env)
            print(proc.stdout[-2000:] + proc.stderr[-1000:], flush=True)


if __name__ == "__main__":
    main()
