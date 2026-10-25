"""The western cliff band as a world feature (a POI) -- DT_WorldFeatures row WC_WesternCliffBand (Wes, Oct 25 2026: "i can't find any western cliffs
anywhere ... add them to poi list and make a lot more of them in lined up clusters of 5-10").

    python Tools/ue.py rows get /MOFramework/Data/DT_WorldFeatures.DT_WorldFeatures ED_RockSpire > spire.json
    python -I Tools/western_cliff_rows.py spire.json patch.json
    python Tools/ue.py rows set /MOFramework/Data/DT_WorldFeatures.DT_WorldFeatures --file patch.json        (readback-verified)

The row is the Electric Dreams spire row (ED_RockSpire, the template: every field the struct has) with its own graph and placement:
  - the Desert Western Cliff Sheer XL 04 (218 x 103 x 81 m at x1) at x0.35-0.6: 76-131 m wide, 28-49 m high faces;
  - 5-10 of them in a LINE along the hillside (bClusterLine), their cores (0.7 of the half-width footprint) touching, so the faces overlap into
    one cliff band 0.4-1 km long, each face turned downhill (bAlignToSlope; the mesh faces +Y, AuthoredDownhillYawDeg 90);
  - steep highland ground (25-60 deg over each member's footprint), half a band a square kilometre where it qualifies, seen to 3 km;
  - the PCG species western cliffs (Tools/highland_crags_patch.py, step 5) stay as the rare single faces.
"""
import copy
import json
import sys

GRAPH = "/Game/WorldFeatures/WesternCliffs/PCG_WesternCliffFace"
ROW = {
    "displayName": "Western cliff band", "biomes": ["AlpineRock", "RockyHighland", "AlpineHeath"], "densityPerKm2": 0.5, "spacingCm": 140000,
    "footprintRadiusCm": 11000, "coreFraction": 0.7, "priority": 30, "slopeMinDeg": 25, "slopeMaxDeg": 60, "maxFootprintReliefCm": 2500,
    "bAlignToSlope": True, "authoredDownhillYawDeg": 90, "alignYawJitterDeg": 8, "sinkCm": 0, "minScale": 0.35, "maxScale": 0.6,
    "clusterCountMin": 5, "clusterCountMax": 10, "clusterRadiusCm": 60000, "clusterScaleBias": 1, "bClusterLine": True,
    "spawnDistanceCm": 300000, "minHeightAboveSeaCm": 3000, "maxHeightAboveSeaCm": 100000, "nearFeatureIds": [], "nearWithinCm": 0,
}


def main():
    src, dst = sys.argv[1], sys.argv[2]
    rows = json.load(open(src, encoding="utf-8"))
    rows = rows.get("rows", rows)
    row = copy.deepcopy(rows["ED_RockSpire"])
    if "bClusterLine" not in row:
        raise SystemExit("the table has no bClusterLine field: build the C++ (FMOWorldFeatureDefinitionRow) and read the row again")
    row.update(ROW)
    row["actorProperties"] = {"Variants": "(\"%s.%s\")" % (GRAPH, GRAPH.split("/")[-1])}
    row.update({"bEnabled": True, "policy": "Many", "authoredGround": {"OriginCm": {"X": 0, "Y": 0}, "CellCm": 100, "NumX": 0, "NumY": 0, "HeightsCm": [], "Weights": []}})
    json.dump({"WC_WesternCliffBand": row}, open(dst, "w", encoding="utf-8"), indent=1)
    print("WCROW WC_WesternCliffBand: lines of %d-%d faces x%.2f-%.2f, %.1f / km2, %s" % (ROW["clusterCountMin"], ROW["clusterCountMax"], ROW["minScale"],
                                                                                    ROW["maxScale"], ROW["densityPerKm2"], ",".join(ROW["biomes"])))


if __name__ == "__main__":
    main()
