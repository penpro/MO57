"""BR25-1 (Wes, Oct 24 2026): "a lot of the rocks in the rocky alpine area are still way oversized ... let's limit the oversizing to like.... 5times? they start
to look pretty crappy when they're huge". Every rock species of DT_Biomes (HISMTag MOResource_Rock / MOBiomeCover_Rock, main and water-edge lists) whose scale
range tops out above x5 is scaled down so it tops out at x5, its proportions kept (AlpineRock's xlarge x16.8-33.6 -> x2.5-5; its x3 base rocks x2.4-6.6 ->
x1.8-5). The cliff faces and formations stay as they are (cliffs, not rocks: HugeSandstoneCliff tops out at x2.6, the TRELLIS formations at x1.05).

    python Tools/ue.py rows get /MOFramework/Data/DT_Biomes.DT_Biomes <every row> > biomes.json      (rows list gives the names)
    python -I Tools/rock_scale_cap.py biomes.json patch.json [max=5]
    python Tools/ue.py rows set /MOFramework/Data/DT_Biomes.DT_Biomes --file patch.json            (readback-verified)
Idempotent: a capped range tops out at exactly the cap and is left alone.
"""
import json
import sys

ROCK_TAGS = ("MOResource_Rock", "MOBiomeCover_Rock")


def main():
    src, dst = sys.argv[1], sys.argv[2]
    cap = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0
    rows = json.load(open(src, encoding="utf-8"))
    if isinstance(rows, dict) and "rows" in rows:
        rows = rows["rows"]
    patch, report = {}, []
    for biome, row in rows.items():
        for field in ("species", "waterEdgeSpecies"):
            changed = False
            for e in row.get(field) or []:
                if e["HISMTag"] not in ROCK_TAGS or "Cliff" in e["Mesh"] or e["MaxScale"] <= cap + 1e-6:
                    continue
                k = cap / e["MaxScale"]
                old = (e["MinScale"], e["MaxScale"])
                e["MinScale"], e["MaxScale"] = round(e["MinScale"] * k, 3), cap
                report.append("%s.%s %s: x%.2f-%.2f -> x%.2f-%.2f" % (biome, field, e["Mesh"].split(".")[-1][:36], old[0], old[1], e["MinScale"], e["MaxScale"]))
                changed = True
            if changed:
                patch.setdefault(biome, {})[field] = row[field]
    json.dump(patch, open(dst, "w", encoding="utf-8"), indent=1)
    print("ROCKCAP x%.1f: %d entries in %d rows" % (cap, len(report), len(patch)))
    print("\n".join(report))


if __name__ == "__main__":
    main()
