"""Dress lake and river shores per biome: add boulders, a fallen log and lakeside saplings to DT_Biomes' WaterEdgeSpecies.

    python Tools/shore_dressing.py plan     # print what would be added per biome
    python Tools/shore_dressing.py apply    # rows set the live DT_Biomes (editor up, PIE off), readback-verified

The shore tier (MO Biome Spawner_2, WaterEdgeOnly) plants each biome's WaterEdgeSpecies on dry ground within WaterEdgeBandCm
ABOVE the waterline (1.2 m: ~5 m inland on the default 0.22 bank), density fading toward the top of the band (ShoreFactor).
Until Oct 14 every list was the same reed-grass + flowers + pebbles strip, so every lake looked alike (bug #29, "lakes
unadorned"). Each added species copies its tested settings (collision, sink, alignment, scale) from the palette entry that
already uses that mesh (the species gallery contact test passes for all of them); only density and scale are set here.
Idempotent: a mesh already in a biome's water-edge list is left alone.
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
TABLE = "/MOFramework/Data/DT_Biomes.DT_Biomes"
ITEMS = "/MOFramework/Data/DT_Items.DT_Items"

ROCK_A = "Beach_Rock_wd5mbhd"            # rounded water-worn rock
ROCK_MOSSY = "MossyForestBoulder_01"
SAPLING_ALDER = "BlackAlder_Sapling_02"
SAPLING_HAZEL = "CommonHazel_Sapling_01"
LOG = "LargeLog01"                        # the fallen-log item's mesh (Old Wooden Log scan), decor here

# The band is a ~5 m strip, so a hectare of it is ~2 km of shoreline: STRIP_SCALE turns "how many per 2 km if the strip were full" into what is authored.
# The reed grass fills the slot (27k/ha asked of a 20k/ha tier, so everything is prorated ~0.7) and ShoreFactor fades toward the band top (~0.5 average):
# 30 x 10 = 300/ha of rocks is ~5 rocks per 100 m of shore, alders ~5 per 100 m, a log every ~100 m.
STRIP_SCALE = 10.0
# per biome: (mesh key, density per ha of a FULL strip before STRIP_SCALE, min scale, max scale)
DRESSING = {
    "TemperateForest": [(ROCK_A, 30, 0.6, 1.3), (ROCK_MOSSY, 14, 0.5, 1.1), (LOG, 6, 0.55, 0.85), (SAPLING_ALDER, 30, 0.8, 1.2), (SAPLING_HAZEL, 14, 0.8, 1.2)],
    "Meadow":          [(ROCK_A, 26, 0.6, 1.3), (LOG, 4, 0.55, 0.85), (SAPLING_ALDER, 22, 0.8, 1.2), (SAPLING_HAZEL, 10, 0.8, 1.2)],
    "LocustWoodland":  [(ROCK_A, 28, 0.6, 1.3), (ROCK_MOSSY, 10, 0.5, 1.1), (LOG, 6, 0.55, 0.85), (SAPLING_ALDER, 24, 0.8, 1.2)],
    "Rainforest":      [(ROCK_MOSSY, 22, 0.5, 1.2), (LOG, 8, 0.55, 0.85)],
    "DryScrub":        [(ROCK_A, 34, 0.6, 1.4), (LOG, 3, 0.5, 0.8)],
    "AlpineHeath":     [(ROCK_A, 40, 0.6, 1.5), (ROCK_MOSSY, 16, 0.5, 1.2)],
    "RockyHighland":   [(ROCK_A, 44, 0.6, 1.5), (ROCK_MOSSY, 14, 0.5, 1.2)],
}


def ue(*args):
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    out = subprocess.run(UE + list(args), capture_output=True, text=True, cwd=ROOT, env=env).stdout
    return out


def rows_get(table, names):
    out = ue("rows", "get", table, *names)
    return json.loads(out[out.find("{"):])


def templates(live):
    """mesh key -> a species dict already authored in some palette (tested settings)."""
    found = {}
    for row in live.values():
        for sp in row.get("species", []) + row.get("waterEdgeSpecies", []):
            for key in (ROCK_A, ROCK_MOSSY, SAPLING_ALDER, SAPLING_HAZEL):
                if key in sp.get("Mesh", "") and key not in found and not sp.get("HISMTag", "").startswith("MOFar_"):
                    found[key] = sp
    return found


def log_species(rock_template):
    item = rows_get(ITEMS, [LOG])[LOG]
    sp = json.loads(json.dumps(rock_template))
    sp.update({"Mesh": item["worldVisual"]["StaticMesh"], "HISMTag": "MOBiomeCover_Rock", "ResourceNodeId": "None",
               "bAlignToSurfaceNormal": True, "SinkFraction": 0.15, "MaxRandomTiltDeg": 4.0, "ClusterGroup": -1, "ClusterRadius": 0.0})
    return sp


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "plan"
    names = ["TemperateForest", "Meadow", "RockyHighland", "ShoreStrand", "DryScrub", "LocustWoodland", "AlpineHeath", "Rainforest",
             "AlpineRock", "ElectricForest", "NordicStrand"]
    live = rows_get(TABLE, names)
    tpl = templates(live)
    missing = [k for k in (ROCK_A, ROCK_MOSSY, SAPLING_ALDER, SAPLING_HAZEL) if k not in tpl]
    if missing:
        sys.exit("no palette template for: %s" % missing)
    tpl[LOG] = log_species(tpl[ROCK_A])
    patch = {}
    for biome, adds in DRESSING.items():
        edge = list(live[biome].get("waterEdgeSpecies", []))
        have = {sp.get("Mesh") for sp in edge}
        added = []
        for key, density, smin, smax in adds:
            sp = json.loads(json.dumps(tpl[key]))
            if sp["Mesh"] in have:
                continue
            sp.update({"DensityPerHectare": float(density) * STRIP_SCALE, "MinScale": smin, "MaxScale": smax, "MinSlopeDeg": 0.0, "MaxSlopeDeg": 35.0})
            edge.append(sp)
            added.append("%s %d/ha" % (key, density * STRIP_SCALE))
        print("%-15s + %s" % (biome, ", ".join(added) or "nothing (already dressed)"))
        if added:
            patch[biome] = {"waterEdgeSpecies": edge}
    if mode == "apply" and patch:
        path = os.path.join(ROOT, "Saved", "shore_dressing_patch.json")
        json.dump(patch, open(path, "w"))
        res = ue("rows", "set", TABLE, "--file", path)
        print("\n".join(l for l in res.splitlines() if "already exist" not in l)[-600:])


if __name__ == "__main__":
    main()
