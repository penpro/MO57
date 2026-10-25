"""The highland crags + landmarks + granite summits, as a DT_Biomes patch (Wes, Oct 24 2026):
  "let's implement a 'sub biome' in the higher up areas where we have the cliffs made up of those huge rock formations ... clustering them should give some pretty nice results,
   also the largest rocks, and the cliff faces, let's have them render much further out ... also the rocky highlands material is very red and none of the rocks are red, let's shift
   that to something more granite looking"

    python Tools/ue.py rows get /MOFramework/Data/DT_Biomes.DT_Biomes <every row> > biomes.json      (rows list gives the names)
    python -I Tools/highland_crags_patch.py biomes.json patch.json
    python Tools/ue.py rows set /MOFramework/Data/DT_Biomes.DT_Biomes --file patch.json            (readback-verified)

What it writes (every biome row it changes is sent whole: rows set replaces nested arrays):
  1. LANDMARKS (FMOBiomeSpeciesEntry::bLandmark: the landmark tier, 1 km grid, ~2.5 km): every xlarge boulder (the ScaleBias-3 tier, all biomes) and every HugeSandstoneCliff face.
  2. THE CRAGS, a sub-biome of the highlands (RockyHighland, AlpineHeath, AlpineRock): the four TRELLIS cliff formations (/Game/ImageTo3D/Cliffs, their Granite variant) on ONE shared
     clump field (ClusterGroup 8, period 4 x 12000 cm = 480 m): they grow only in its cores (ClusterCore 0.72, ~7 % of the ground), where the capped compensation (x8) packs ~10 a
     hectare, so they stand together and overlap as one formation; the biome's large boulders (its ScaleBias-2 rocks) strew the ring round them (core 0.6). All landmarks, solid.
     Upright (closed rocks: a 30 m formation does not lean with the slope), any yaw, sunk 22 % of their height. A biome's factor scales the crags (the heath gets fewer).
  3. GRANITE SUMMITS: AlpineRock's ground -> VST_Cliff (rock_05: a grey speckled granite, already in the mega material for the cave walls) instead of VST_MI_IcelandicQuarryRock
     (a saturated red quarry scan under grey and white boulders). The red was AlpineRock's (>= 950 m), not RockyHighland's (MossyRockyGround is brown-green moss).
Idempotent: a rerun replaces the crag group (ClusterGroup 8) instead of adding it twice.
"""
import copy
import json
import sys

CRAG_GROUP = 8
CRAG_RADIUS_CM = 12000.0
CRAG_BIOMES = {"RockyHighland": 1.0, "AlpineHeath": 0.6, "AlpineRock": 1.2}
CLIFF_DENSITY = 0.3          # per hectare per formation mesh, authored (realised ~0.56x of it on average; ~2.4 / ha each inside a core)
BOULDER_RING_DENSITY = 0.3   # per hectare per boulder mesh round the crags
CLIFFS = ["Cliff_BroadCliffBlock01", "Cliff_SheerBlockTower01", "Cliff_LeaningSlabStack01", "Cliff_OverhangingButtress01"]
GRANITE_GROUND = "/Game/VoxelExamples/NaniteMaterials/Materials/Cliff/VST_Cliff.VST_Cliff"


def cliff_paths(name):
    base = "/Game/ImageTo3D/Cliffs/%s/%s/" % (name, name)
    return base + "StaticMeshes/%s.%s" % (name, name), base + "Materials/MI_%s_Granite.MI_%s_Granite" % (name, name)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    rows = json.load(open(src, encoding="utf-8"))
    if isinstance(rows, dict) and "rows" in rows:
        rows = rows["rows"]
    template = next(iter(rows.values()))["species"][0]
    if "bLandmark" not in template:
        raise SystemExit("the table has no bLandmark field yet: build the C++ (FMOBiomeSpeciesEntry) and read the rows again")

    patch, report = {}, []
    for biome, row in rows.items():
        species = row["species"]
        changed = False
        # 1. landmarks
        for e in species:
            wants = e["ScaleBias"] == 3 or "HugeSandstoneCliff" in e["Mesh"]
            if wants and not e["bLandmark"]:
                e["bLandmark"] = True
                changed = True
        marks = sum(1 for e in species if e["bLandmark"] and e["ClusterGroup"] != CRAG_GROUP)
        # 2. the crags
        if biome in CRAG_BIOMES:
            k = CRAG_BIOMES[biome]
            kept = [e for e in species if e["ClusterGroup"] != CRAG_GROUP]
            crags = []
            for name in CLIFFS:
                mesh, granite = cliff_paths(name)
                e = copy.deepcopy(template)
                e.update({"Mesh": mesh, "DensityPerHectare": round(CLIFF_DENSITY * k, 3), "ClusterRadius": CRAG_RADIUS_CM, "ClusterGroup": CRAG_GROUP, "ClusterCore": 0.72,
                          "HISMTag": "MOBiomeCover_Rock", "ResourceNodeId": "None", "bAlignToSurfaceNormal": False, "bFaceDownhill": False, "FaceYawDeg": 0, "SinkFraction": 0.22,
                          "SinkMinCm": 0, "BaseRadiusCm": 0, "bLieOnSlope": False, "LieShowFraction": 0.15, "MaxGroundReliefFraction": 0, "MinSlopeDeg": 0, "MaxSlopeDeg": 70,
                          "MaxRandomTiltDeg": 6, "bAutoSweepOnTerraform": False, "Collision": "Solid", "MinScale": 0.5, "MaxScale": 1.05, "ScaleBias": 1.5, "WidthScale": 1,
                          "bLandmark": True, "MaterialOverride": granite})
                crags.append(e)
            for b in [e for e in kept if e["ScaleBias"] == 2]:   # the biome's large boulders, strewn round the formations
                e = copy.deepcopy(b)
                e.update({"DensityPerHectare": round(BOULDER_RING_DENSITY * k, 3), "ClusterRadius": CRAG_RADIUS_CM, "ClusterGroup": CRAG_GROUP, "ClusterCore": 0.6,
                          "bLandmark": True, "MaxSlopeDeg": 55})
                crags.append(e)
            row["species"] = kept + crags
            changed = True
            report.append("%s: crags %d formation + %d boulder entries (x%.1f)" % (biome, len(CLIFFS), len(crags) - len(CLIFFS), k))
        if changed:
            patch.setdefault(biome, {})["species"] = row["species"]
        report.append("%s: %d landmark entries outside the crags" % (biome, marks))
    # 3. granite summits
    if rows["AlpineRock"]["groundSurfaceType"] != GRANITE_GROUND:
        patch.setdefault("AlpineRock", {})["groundSurfaceType"] = GRANITE_GROUND
        report.append("AlpineRock: ground %s -> VST_Cliff" % rows["AlpineRock"]["groundSurfaceType"].split(".")[-1])
    json.dump(patch, open(dst, "w", encoding="utf-8"), indent=1)
    print("CRAGS %d rows in the patch" % len(patch))
    print("\n".join(report))


if __name__ == "__main__":
    main()
