"""BR25-2 (Wes, Oct 24 2026: "remove all open faced geometry assets, log them for rebuilding in trellis to be closed geometry"): take the OPEN rock and
cliff scans out of the biome palettes, keeping a record to put their closed rebuilds back with.

    python Tools/ue.py py --file Content/Python/open_geometry_probe.py           (measures; writes Saved/open_geometry.json)
    python Tools/ue.py rows get /MOFramework/Data/DT_Biomes.DT_Biomes <every row> > biomes.json
    python -I Tools/remove_open_geometry.py biomes.json Saved/open_geometry.json patch.json record.json
    python Tools/ue.py rows set /MOFramework/Data/DT_Biomes.DT_Biomes --file patch.json            (readback-verified)

Removed: every species (main and water-edge lists) tagged as a rock (MOResource_Rock / MOBiomeCover_Rock) whose mesh the probe measured open
(openness >= 0.2). Kept: the leaf-litter and stone debris (MOBiomeCover_Debris: flat by nature, not a scan to rebuild), every closed rock.
record.json lists each removed entry whole (biome, list, the entry), so a closed rebuild goes back as the same species with its mesh swapped.
Idempotent: a second run finds nothing left to remove.
"""
import json
import sys

ROCK_TAGS = ("MOResource_Rock", "MOBiomeCover_Rock")
OPEN_AT = 0.2


def main():
    src, probe, dst, record_path = sys.argv[1:5]
    rows = json.load(open(src, encoding="utf-8"))
    if isinstance(rows, dict) and "rows" in rows:
        rows = rows["rows"]
    measured = json.load(open(probe, encoding="utf-8"))
    open_meshes = {m["mesh"]: m for m in measured if m["openness"] >= OPEN_AT}
    patch, record = {}, []
    for biome, row in rows.items():
        for field in ("species", "waterEdgeSpecies"):
            entries = row.get(field) or []
            kept = []
            for e in entries:
                if e["HISMTag"] in ROCK_TAGS and e["Mesh"] in open_meshes:
                    record.append({"biome": biome, "list": field, "openness": open_meshes[e["Mesh"]]["openness"], "kind": open_meshes[e["Mesh"]]["kind"], "entry": e})
                else:
                    kept.append(e)
            if len(kept) != len(entries):
                patch.setdefault(biome, {})[field] = kept
    json.dump(patch, open(dst, "w", encoding="utf-8"), indent=1)
    json.dump(record, open(record_path, "w", encoding="utf-8"), indent=1)
    meshes = sorted({r["entry"]["Mesh"] for r in record})
    print("OPENREMOVE %d entries of %d meshes from %d rows" % (len(record), len(meshes), len(patch)))
    for m in meshes:
        print("  %s (%s)" % (m.split(".")[-1], ", ".join(sorted({r["biome"] for r in record if r["entry"]["Mesh"] == m}))))


if __name__ == "__main__":
    main()
