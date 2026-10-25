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
  4. THE HUGE NORDIC COASTAL CLIFF (Oct 24 night, from Wes's Dark Ruins Megascans sample: /Game/MS/3D/Huge_Nordic_Coastal_Cliff_venrdcgga/Smart, 14 x 8 x
     9 m, open only underneath -- "we can handle the larger taller ones that have only the bottom open"): a fifth crag formation in its own Smart
     material, and NordicStrand's rocky coast gets clusters of it (ClusterGroup 9). Landmarks, solid, upright, the open base buried.
  5. THE WESTERN CLIFF FACES (Oct 24 night; Wes: "the big western cliffs are ENORMOUS and could help really get some good looking cliffs without a shit
     ton of cpu calls"): the Dark Ruins Desert Western Cliff Sheer XL 04 (218 x 103 x 81 m, grey sheer rock, open behind and below) on the steep
     ground of AlpineRock and RockyHighland, x0.4-0.7, its face (+Y, FaceYawDeg 90) turned downhill: the spawner sinks it by the ground's drop to its
     face edge (FaceDropCm), so the back and the open base go into the hill and the front stands as a 30-55 m cliff. Landmarks, solid (real
     surface: Content/Python/fix_mesh_collision.py), rare (one a square kilometre or two).
Idempotent: a rerun replaces the crag group (ClusterGroup 8), the coastal group (9) and the western cliff entries instead of adding them twice.
"""
import copy
import json
import sys

CRAG_GROUP = 8
CRAG_RADIUS_CM = 12000.0
CRAG_BIOMES = {"RockyHighland": 1.0, "AlpineHeath": 0.6, "AlpineRock": 1.2}
# Oct 25 2026 evening (BR25-15), Wes of the TRELLIS formations: "still look like garbage up close and they have holes in them where you can see
# light through them ... scale them down and make more of them in clusters": x0.4-0.8 (was 0.8-1.6, which had doubled the texture stretch) and
# 2.5x as many; their materials are two-sided (Content/Python/trellis_cliffs_two_sided.py: no light through the holes). The Nordic cliff -- his
# reference for good, "properly scaled and not sticking out of the ground" -- keeps its size and its own density.
CLIFF_DENSITY = 0.75         # per hectare per TRELLIS formation mesh, authored (realised ~0.56x of it on average)
NORDIC_CLIFF_DENSITY = 0.3   # per hectare, the Nordic cliff in the crags
BOULDER_RING_DENSITY = 0.3   # per hectare per boulder mesh round the crags
# The formations' size (their meshes are 23-30 m): x0.5-1.05 until Oct 25, then x0.8-1.6, 24-48 m (Wes: "they're also not very large
# compared to what i thought they were going to be"; the rock detail layer keeps them crisp up close at any size: rock_detail_layer.py).
CRAG_MIN_SCALE = 0.4
CRAG_MAX_SCALE = 0.8
CLIFFS = ["Cliff_BroadCliffBlock01", "Cliff_SheerBlockTower01", "Cliff_LeaningSlabStack01", "Cliff_OverhangingButtress01"]
NORDIC_CLIFF = "/Game/MS/3D/Huge_Nordic_Coastal_Cliff_venrdcgga/Smart/SM_venrdcgga_RM_N1.SM_venrdcgga_RM_N1"
COAST_GROUP = 9
COAST_BIOMES = {"NordicStrand": 0.35}   # per hectare, authored (in clusters: ~2-3 a hectare inside one)
WESTERN_CLIFF = "/Game/MS/3D/Desert_Western_Cliff_Sheer_Xl_04_xgknben/Smart/SM_Desert_Western_Cliff_Sheer_Xl_04_xgknben_RM_N1.SM_Desert_Western_Cliff_Sheer_Xl_04_xgknben_RM_N1"
WESTERN_BIOMES = {"AlpineRock": 0.012, "RockyHighland": 0.008}   # per hectare, authored
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
                          "MaxRandomTiltDeg": 6, "bAutoSweepOnTerraform": False, "Collision": "Solid", "MinScale": CRAG_MIN_SCALE, "MaxScale": CRAG_MAX_SCALE, "ScaleBias": 1.5, "WidthScale": 1,
                          "bLandmark": True, "MaterialOverride": granite})
                crags.append(e)
            e = copy.deepcopy(crags[0])   # the Dark Ruins cliff: a fifth formation, its own (Smart) material, 11-22 m
            e.update({"Mesh": NORDIC_CLIFF, "MaterialOverride": "None", "DensityPerHectare": round(NORDIC_CLIFF_DENSITY * k, 3),
                      "MinScale": 0.8, "MaxScale": 1.6, "SinkFraction": 0.2})
            crags.append(e)
            for b in [e for e in kept if e["ScaleBias"] == 2]:   # the biome's large boulders, strewn round the formations
                e = copy.deepcopy(b)
                e.update({"DensityPerHectare": round(BOULDER_RING_DENSITY * k, 3), "ClusterRadius": CRAG_RADIUS_CM, "ClusterGroup": CRAG_GROUP, "ClusterCore": 0.6,
                          "bLandmark": True, "MaxSlopeDeg": 55})
                crags.append(e)
            row["species"] = kept + crags
            changed = True
            report.append("%s: crags %d formation + %d boulder entries (x%.1f)" % (biome, len(CLIFFS) + 1, len(crags) - len(CLIFFS) - 1, k))
        # 4. the Nordic coastal cliffs
        if biome in COAST_BIOMES:
            kept = [e for e in row["species"] if e["ClusterGroup"] != COAST_GROUP]
            e = copy.deepcopy(template)
            e.update({"Mesh": NORDIC_CLIFF, "DensityPerHectare": COAST_BIOMES[biome], "ClusterRadius": 8000.0, "ClusterGroup": COAST_GROUP, "ClusterCore": 0.65,
                      "HISMTag": "MOBiomeCover_Rock", "ResourceNodeId": "None", "bAlignToSurfaceNormal": False, "bFaceDownhill": False, "FaceYawDeg": 0,
                      "SinkFraction": 0.2, "SinkMinCm": 0, "BaseRadiusCm": 0, "bLieOnSlope": False, "LieShowFraction": 0.15, "MaxGroundReliefFraction": 0,
                      "MinSlopeDeg": 0, "MaxSlopeDeg": 55, "MaxRandomTiltDeg": 6, "bAutoSweepOnTerraform": False, "Collision": "Solid", "MinScale": 0.7,
                      "MaxScale": 1.4, "ScaleBias": 1.5, "WidthScale": 1, "bLandmark": True, "MaterialOverride": "None"})
            row["species"] = kept + [e]
            changed = True
            report.append("%s: Nordic coastal cliffs (group %d, %.2f / ha)" % (biome, COAST_GROUP, COAST_BIOMES[biome]))
        # 5. the western cliff faces
        if biome in WESTERN_BIOMES:
            kept = [e for e in row["species"] if e["Mesh"] != WESTERN_CLIFF]
            e = copy.deepcopy(template)
            e.update({"Mesh": WESTERN_CLIFF, "DensityPerHectare": WESTERN_BIOMES[biome], "ClusterRadius": 0, "ClusterGroup": -1, "ClusterCore": 0.5,
                      "HISMTag": "MOBiomeCover_Rock", "ResourceNodeId": "None", "bAlignToSurfaceNormal": False, "bFaceDownhill": True, "FaceYawDeg": 90,
                      "SinkFraction": 0.2, "SinkMinCm": 0, "BaseRadiusCm": 0, "bLieOnSlope": False, "LieShowFraction": 0.15, "MaxGroundReliefFraction": 0,
                      "MinSlopeDeg": 32, "MaxSlopeDeg": 65, "MaxRandomTiltDeg": 0, "bAutoSweepOnTerraform": False, "Collision": "Solid", "MinScale": 0.4,
                      "MaxScale": 0.7, "ScaleBias": 1, "WidthScale": 1, "bLandmark": True, "MaterialOverride": "None"})
            row["species"] = kept + [e]
            changed = True
            report.append("%s: western cliff faces (%.3f / ha)" % (biome, WESTERN_BIOMES[biome]))
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
