"""DT_WorldFeatures rows for the Dark Ruins cliff compositions (Content/Python/make_dr_compositions.py builds their graphs).

    python Tools/ue.py rows get /MOFramework/Data/DT_WorldFeatures.DT_WorldFeatures ED_RockSpire > spire.json
    python -I Tools/dr_worldfeature_rows.py spire.json patch.json
    python Tools/ue.py rows set /MOFramework/Data/DT_WorldFeatures.DT_WorldFeatures --file patch.json        (readback-verified; keeps every row's maps)

Each row is the Electric Dreams spire row (ED_RockSpire, the template: every field the struct has) with its own graph, biomes, density and
size. Wes, Oct 25 2026: "the huge cliffs we just imported look pretty good but we need to have more of them".
  DR_NordicMountain  Mountain_01: nine Huge / Massive Nordic Coastal Cliff pieces stacked into a ~60 m mountain face (57 x 32 m of pivots)
  DR_NordicTallPath  TallPath_01: a ~52 m stack of massive Nordic cliffs with a rock arch, led up to the mountains (NearFeatureIds)
Both in the highlands and on the Nordic coast, in clusters (several stacks together read as one massif), seen out to 2.5 km. Not aligned
to the slope: the stacks are rock all round, any yaw stands.

Oct 25 2026, Wes: "the composite cliffs are scaled way too high and look horrible close up and just look like a ball": x0.7-1.5 -> x0.3-0.55
(mountain, 18-33 m) and x0.35-0.6 (stack, 18-31 m), clusters packed tighter, flatter ground required (their relief limits are absolute).
Footprints MEASURED (PIE, Oct 25: every rock instance's bounds round the site, in actor units): the mountain's rock reaches 5860 (p90) / 7940
(max, the foot talus), the stack's 1650 / 4190; the spire template's 3600 x core 0.45 and 1800 x 0.5 let the forest grow inside the rock. Now
the core (no trees: footprint x coreFraction) covers the rock and trees start at the footprint's edge.
"""
import copy
import json
import sys

GRAPHS = "/Game/WorldFeatures/DarkRuins/"
# OFF since Oct 25 2026 evening (BR25-16): smaller, in the artist's layout, dressed -- and still, Wes: "nordic cliffs are a disaster, the textures
# look like garbage close up, like they're scaled up too far, and they're floating WAY above ground" (an arch of Mountain_01 showing its open
# underside from the slope below; the sample's pieces at x2-5 of their own, the texture stretched). The single Nordic cliff in the crags is his
# reference for good ("these cliffs look pretty good ... properly scaled and not sticking out of the ground"). The rows and graphs stay for a rework.
ENABLED = False
ROWS = {
    "DR_NordicMountain": {
        "displayName": "Nordic cliff mountain (Dark Ruins)", "graph": "PCG_DR_NordicMountain",
        "biomes": ["RockyHighland", "AlpineRock", "AlpineHeath", "NordicStrand"], "densityPerKm2": 2, "spacingCm": 45000,
        "footprintRadiusCm": 7500, "priority": 25, "slopeMaxDeg": 28, "maxFootprintReliefCm": 1500, "minScale": 0.3, "maxScale": 0.55,
        "spawnDistanceCm": 250000, "clusterCountMin": 2, "clusterCountMax": 5, "clusterRadiusCm": 9000, "clusterScaleBias": 1.4,
        "coreFraction": 0.8, "nearFeatureIds": [], "nearWithinCm": 0,
    },
    "DR_NordicTallPath": {
        "displayName": "Nordic cliff stack with an arch (Dark Ruins)", "graph": "PCG_DR_NordicTallPath",
        "biomes": ["RockyHighland", "AlpineRock", "AlpineHeath", "NordicStrand"], "densityPerKm2": 3, "spacingCm": 30000,
        "footprintRadiusCm": 3500, "priority": 22, "slopeMaxDeg": 28, "maxFootprintReliefCm": 1200, "minScale": 0.35, "maxScale": 0.6,
        "spawnDistanceCm": 250000, "clusterCountMin": 1, "clusterCountMax": 3, "clusterRadiusCm": 6000, "clusterScaleBias": 1.4,
        "coreFraction": 0.6, "nearFeatureIds": ["DR_NordicMountain"], "nearWithinCm": 40000,
    },
}


def main():
    src, dst = sys.argv[1], sys.argv[2]
    rows = json.load(open(src, encoding="utf-8"))
    rows = rows.get("rows", rows)
    template = rows["ED_RockSpire"]
    patch = {}
    for name, spec in ROWS.items():
        row = copy.deepcopy(template)
        graph = GRAPHS + spec["graph"]
        row.update({k: v for k, v in spec.items() if k != "graph"})
        row["actorProperties"] = {"Variants": "(\"%s.%s\")" % (graph, spec["graph"])}
        row.update({"bEnabled": ENABLED, "policy": "Many", "slopeMinDeg": 0, "bAlignToSlope": False, "authoredDownhillYawDeg": 0,
                    "alignYawJitterDeg": 0, "sinkCm": 0, "minHeightAboveSeaCm": 200, "maxHeightAboveSeaCm": 100000,
                    "authoredGround": {"OriginCm": {"X": 0, "Y": 0}, "CellCm": 100, "NumX": 0, "NumY": 0, "HeightsCm": [], "Weights": []}})
        patch[name] = row
        print("DRROW %s: %s, %s / km2 in clusters of %d-%d, x%.1f-%.1f, %s" % (name, spec["graph"], spec["densityPerKm2"], spec["clusterCountMin"],
                                                                            spec["clusterCountMax"], spec["minScale"], spec["maxScale"], ",".join(spec["biomes"])))
    json.dump(patch, open(dst, "w", encoding="utf-8"), indent=1)


if __name__ == "__main__":
    main()
