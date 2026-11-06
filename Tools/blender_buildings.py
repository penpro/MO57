"""Building pieces made in Blender into the game (Nov 2 2026; Wes: "let's flesh out a lot of the building system and remember to have the
assets 'pop in' as building goes on"). Per piece (Tools/building_pieces.py PIECES):
  1. Blender builds its parts in build order (Tools/blender_buildings_blender.py) into Saved/BlenderBuildings/<Piece>/;
  2. the editor imports them, lays a cobble piece's stones and makes BP_Piece_<Piece> (Content/Python/import_blender_buildings.py);
  3. with --rows, the recipe rows go into the Recipes table and the new materials' rows into DT_Items (`ue.py rows set`, readback-verified).

    python Tools/blender_buildings.py [Piece ...] [--surfaces[=Name,Name]] [--blender-only] [--no-blender] [--rows]

No pieces = all of them. --surfaces bakes the tiling surfaces again first and imports them (=Name,Name: only those). Blender 5.2; steps 2-3 need the editor (bridge,
PIE not running). Verify in the game with Tools/check_buildings.py.
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import building_pieces as BP   # noqa: E402

BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
OUT = os.path.join(ROOT, "Saved", "BlenderBuildings")
RECIPES = "/MOFramework/Data/Recipes.Recipes"
ITEMS = "/MOFramework/Data/DT_Items.DT_Items"
ENV = dict(os.environ, MSYS_NO_PATHCONV="1")

# The two new materials (building_pieces.NEW_ITEMS). FieldStone01 wears the scanned forest boulder at a building stone's size.
ITEM_ROWS = {
    "Moss01": {
        "itemId": "Moss01", "displayName": "Moss", "shortDescription": "An armful of moss.",
        "description": "Moss stripped from a rock by the armful, damp and springy. Packed into the gaps of a wattle or log wall it stops the "
                       "draught -- the chinking of northern houses for thousands of years -- and it makes a soft bed.",
        "itemType": "Material", "weight": 0.5, "maxStackSize": 20, "baseValue": 1,
        "worldVisual": {"StaticMesh": "/Game/BlenderBuildings/Moss01/SM_Moss01.SM_Moss01",
                        "RelativeTransform": {"Scale3D": {"X": 1, "Y": 1, "Z": 1}}},
    },
    "FieldStone01": {
        "itemId": "FieldStone01", "displayName": "Building Stone", "shortDescription": "A heavy field stone, good for walling.",
        "description": "A stone the size of a loaf, about twelve kilograms, knocked off a boulder or picked from a field: what dry walls, "
                       "cobble footings and stone walls are laid from. One at a time is all anyone carries far.",
        "itemType": "Material", "weight": 12.0, "maxStackSize": 4, "baseValue": 1,
        "worldVisual": {"StaticMesh": "/Game/Fab/Megascans/3D/Forest_Boulder_wgosajs/Medium/wgosajs_tier_2/StaticMeshes/wgosajs_tier_2.wgosajs_tier_2",
                        "RelativeTransform": {"Scale3D": {"X": 0.12, "Y": 0.12, "Z": 0.12}}},
    },
}


def blender(job, *extra):
    out = os.path.join(OUT, {"surfaces": "Surfaces", "supports": "Supports"}.get(job, job))
    proc = subprocess.run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(ROOT, "Tools", "blender_buildings_blender.py"), "--",
                           job, out] + list(extra), capture_output=True, text=True)
    lines = [l for l in proc.stdout.splitlines() if l.startswith("BLENDERBUILD")]
    print("\n".join(lines) or proc.stdout[-3000:] + proc.stderr[-2000:], flush=True)
    if not any("done" in l for l in lines):
        sys.exit("blender failed for " + job)


def ue_py(code, timeout=3600):
    proc = subprocess.run(UE + ["py", "--timeout", str(timeout), "-c", code], capture_output=True, text=True, cwd=ROOT)
    for line in proc.stdout.splitlines():
        if line.startswith("{") or line.startswith('"'):
            return json.loads(line)
    sys.exit("editor step failed:\n" + proc.stdout[-3000:] + proc.stderr[-2000:])


def camel(row):
    """The table's top-level fields are camelCase in `rows get` (nested structs keep their names)."""
    return {(k[0].lower() + k[1:]): v for k, v in row.items()}


def rows_set(table, rows, name):
    path = os.path.join(OUT, name)
    with open(path, "w") as f:
        json.dump(rows, f, indent=1)
    proc = subprocess.run(UE + ["rows", "set", table, "--file", path], capture_output=True, text=True, cwd=ROOT, env=ENV)
    print(proc.stdout[-2500:] + proc.stderr[-800:], flush=True)


def main():
    pieces = [a for a in sys.argv[1:] if not a.startswith("--")] or list(BP.PIECES)
    only = next((a.split("=", 1)[1].split(",") for a in sys.argv[1:] if a.startswith("--surfaces=")), None)
    do_surfaces = "--surfaces" in sys.argv or only is not None
    unknown = [p for p in pieces if p not in BP.PIECES]
    if unknown:
        sys.exit("unknown pieces: %s" % unknown)
    if "--no-blender" not in sys.argv:
        if do_surfaces:
            blender("surfaces", *(["--only", ",".join(only)] if only else []))
        blender("supports")
        for p in pieces:
            blender(p)
        blender("Moss01")
    if "--blender-only" in sys.argv:
        return
    report = ue_py("import importlib, import_blender_buildings as b; importlib.reload(b); out(b.run(%r, do_surfaces=%r, only_surfaces=%r))"
                   % (pieces, do_surfaces, only))
    print(json.dumps(report["pieces"], indent=1), flush=True)
    ue_py("import importlib, import_blender_buildings as b; importlib.reload(b); import json; out(json.dumps(b.moss_item()))")
    if "--rows" in sys.argv:
        with open(report["recipes"]) as f:
            recipes = json.load(f)
        rows = {}
        for name, row in recipes.items():
            spec = next(s for s in BP.PIECES.values() if s["recipe"] == name)
            r = camel(row)
            if spec["frame"] == "shelter":
                r["category"] = "Shelter"
                r["housingCapacity"] = 4 if "Tent" in spec["display"] else 2
            rows[name] = r
        rows_set(RECIPES, rows, "recipes_rows.json")
        rows_set(ITEMS, ITEM_ROWS, "items_rows.json")


if __name__ == "__main__":
    main()
