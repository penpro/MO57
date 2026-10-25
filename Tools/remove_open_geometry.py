"""BR25-2 (Wes, Oct 24 2026: "remove all open faced geometry assets, log them for rebuilding in trellis to be closed geometry"): take the OPEN rock and
cliff scans out of the biome palettes, keeping a record to put their closed rebuilds back with.

    python Tools/ue.py py --file Content/Python/open_geometry_probe.py           (measures; writes Saved/open_geometry.json)
    python Tools/ue.py rows get /MOFramework/Data/DT_Biomes.DT_Biomes <every row> > biomes.json
    python -I Tools/remove_open_geometry.py biomes.json Saved/open_geometry.json patch.json record.json
    python Tools/ue.py rows set /MOFramework/Data/DT_Biomes.DT_Biomes --file patch.json            (readback-verified)

Removed: every species (main and water-edge lists) tagged as a rock (MOResource_Rock / MOBiomeCover_Rock) whose mesh the probe measured open
(openness >= 0.2) -- unless it is TALL with only its BOTTOM open (Wes, Oct 24 2026: "we can handle the larger taller ones that have only the bottom
open, the shallow depth ones are hard to get to sit properly in the terrain so avoid those ones"): the open side faces down (the probe's "up" >= 0.7)
and its height is at least TALL_FRACTION of its narrower horizontal side. Kept too: the leaf-litter and stone debris (MOBiomeCover_Debris: flat by
nature), every closed rock.
record.json lists each removed entry whole (biome, list, the entry), so a closed rebuild goes back as the same species with its mesh swapped. An entry
of an EXISTING record whose mesh now passes is put back (appended to its list) and dropped from the record.
Idempotent: a second run finds nothing left to remove or restore.
"""
import json
import sys

ROCK_TAGS = ("MOResource_Rock", "MOBiomeCover_Rock")
OPEN_AT = 0.2
TALL_FRACTION = 0.4


def keeps(m):
    """A measured mesh that stays: closed, or tall with only its bottom open (the ground hides it)."""
    if m["openness"] < OPEN_AT:
        return True
    x, y, z = m["size_cm"]
    return m["up"] >= 0.7 and z >= TALL_FRACTION * min(x, y)


def main():
    src, probe, dst, record_path = sys.argv[1:5]
    rows = json.load(open(src, encoding="utf-8"))
    if isinstance(rows, dict) and "rows" in rows:
        rows = rows["rows"]
    measured = json.load(open(probe, encoding="utf-8"))
    open_meshes = {m["mesh"]: m for m in measured if not keeps(m)}
    kept_meshes = {m["mesh"] for m in measured if keeps(m)}
    try:
        previous = json.load(open(record_path, encoding="utf-8"))
    except (OSError, ValueError):
        previous = []
    patch, record, restored = {}, [], []
    for biome, row in rows.items():
        for field in ("species", "waterEdgeSpecies"):
            entries = row.get(field) or []
            kept = []
            for e in entries:
                if e["HISMTag"] in ROCK_TAGS and e["Mesh"] in open_meshes:
                    record.append({"biome": biome, "list": field, "openness": open_meshes[e["Mesh"]]["openness"], "kind": open_meshes[e["Mesh"]]["kind"], "entry": e})
                else:
                    kept.append(e)
            for r in previous:   # put back what the rule now keeps
                if r["biome"] == biome and r["list"] == field and r["entry"]["Mesh"] in kept_meshes and r["entry"] not in kept:
                    kept.append(r["entry"])
                    restored.append(r)
            if kept != entries:
                patch.setdefault(biome, {})[field] = kept
    json.dump(patch, open(dst, "w", encoding="utf-8"), indent=1)
    record += [r for r in previous if r["entry"]["Mesh"] not in kept_meshes and r not in record]
    json.dump(record, open(record_path, "w", encoding="utf-8"), indent=1)
    if restored:
        print("OPENRESTORE %d entries put back: %s" % (len(restored), ", ".join(sorted({r["entry"]["Mesh"].split(".")[-1] for r in restored}))))
    meshes = sorted({r["entry"]["Mesh"] for r in record})
    print("OPENREMOVE %d entries of %d meshes from %d rows" % (len(record), len(meshes), len(patch)))
    for m in meshes:
        print("  %s (%s)" % (m.split(".")[-1], ", ".join(sorted({r["biome"] for r in record if r["entry"]["Mesh"] == m}))))


if __name__ == "__main__":
    main()
