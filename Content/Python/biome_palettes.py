"""The biome palettes of DT_Biomes as DATA: what grows where, and how thick. One place to read and tune the look of the land.

Apply with the editor open:
    python Tools/ue.py py --file Content/Python/biome_palettes.py          (writes Saved/Logs/biome_palettes.json and prints the plan)
    python Tools/ue.py rows set /MOFramework/Data/DT_Biomes.DT_Biomes --file Saved/Logs/biome_palettes.json     (existing rows: species arrays are replaced wholesale)
The new biome (AlpineHeath) is added by apply_biome_palettes() below through the in-editor DataTable API.

Why this exists (2026-10-10, "make the terrain feel more alive and full"): before this the forest was three tree meshes and one grass, the meadow two grasses, and the world had two
flower meshes out of ~110 available. The packs ship far more: BlackAlder (9 forest + 5 field trees, 6 saplings, 2 seedlings), CommonHazel (a full shrub / sapling / seedling set),
20 meadow-flower families, 8 wild-grass and 6 dune-grass variants, dead shrubs, raspberry and locust variants.

2026-10-10 (later): generator v5 -- per-biome RELIEF (reliefScale = hills, mountainScale = ranges; prairies flat, cool moist highlands tall), sand under the shore (VST_MI_Sand), a RAINFOREST biome
modelled on the layering of Epic's PCG Biome Sample (emergent giants -> canopy -> saplings -> seedlings -> shrubs -> ground cover, each layer its own clump field, boulders and fallen wood on the
floor), six more Megascans rocks, driftwood on the beaches, and walk-through Black Locust (every variant is a knee-high plant that the resource table calls a Tree, so it blocked like an oak; I first
blamed the saplings, wrongly -- see species()).

Conventions (see FMOBiomeSpeciesEntry): density is instances per hectare at full biome strength; ClusterGroup members share one clump field (canopy trees at the clump core, undergrowth in
the ring around it); species with a ResourceNodeId are HARVESTABLE and take their collision from the resource type (trees / rocks block the pawn, bushes do not); species without one are
decorative ground cover: no collision, swept away when the ground under them is dug.
"""
import json

GAME = "/Game"
BA = GAME + "/BlackAlder/Geometry/PivotPainter/SM_BlackAlder_%s_PP.SM_BlackAlder_%s_PP"
CH = GAME + "/CommonHazel/Geometry/PivotPainter/SM_CommonHazel_%s_PP.SM_CommonHazel_%s_PP"
FL = GAME + "/PN_MeadowFlowers/Meshes/flowerMesh/flower_%s.flower_%s"
MEGA = GAME + "/Fab/Megascans/Plants/%s/Medium/%s_tier_2/StaticMeshes/SM_%s_Var%s.SM_%s_Var%s"
ROCK = GAME + "/Fab/Megascans/3D/%s/Medium/%s_tier_2/StaticMeshes/%s_tier_2.%s_tier_2"


def ba(name):
    return BA % (name, name)


def ch(name):
    return CH % (name, name)


def fl(name):
    return FL % (name, name)


def wild_grass(v):
    return MEGA % ("Wild_Grass_vlkhcbxia", "vlkhcbxia", "vlkhcbxia", v, "vlkhcbxia", v)


def dune_grass(v):
    return MEGA % ("Grass_smdrailr", "smdrailr", "smdrailr", v, "smdrailr", v)


def dead_shrub(v):
    return MEGA % ("Dead_Shrubs_vdknafgha", "vdknafgha", "vdknafgha", v, "vdknafgha", v)


def raspberry(v):
    return MEGA % ("Raspberry_wf0oefeja", "wf0oefeja", "wf0oefeja", v, "wf0oefeja", v)


def locust(v):
    return MEGA % ("Black_Locust_wdvhberja", "wdvhberja", "wdvhberja", v, "wdvhberja", v)


def rock(folder, code):
    return ROCK % (folder, code, code, code)


def species(path, density, res="None", tag="", cluster=0.0, group=-1, core=0.6, align=False, tilt=8.0, sweep=False, smin=0.85, smax=1.25, collision="Auto"):
    # Every Black Locust variant is a GROUND-LEVEL plant (0.5 - 1.5 m tall, up to 1.8 m wide) that the resource table calls a Tree, so by resource type it blocks the pawn like an oak: you walk
    # into a knee-high bush and stop. They are walk-through (found 2026-10-10 by Wes: "a small locust bush, or a collection of them"; I had first blamed the saplings and been wrong).
    if collision == "Auto" and "wdvhberja" in path:
        collision = "Passable"
    return {"Mesh": path, "DensityPerHectare": density, "ClusterRadius": cluster, "ClusterGroup": group, "ClusterCore": core, "HISMTag": tag or "None",
            "ResourceNodeId": res, "bAlignToSurfaceNormal": align, "MaxRandomTiltDeg": tilt, "bAutoSweepOnTerraform": sweep, "MinScale": smin, "MaxScale": smax, "Collision": collision}


def tree(name, density, group=0, core=0.7, res="BlackAlder", smin=0.8, smax=1.3):
    # (Saplings block like trees again: they were never the "bush that stops you" -- that was the locust, see species().)
    return species(ba(name), density, res=res, tag="MOResource_Tree", cluster=3000.0, group=group, core=core, smin=smin, smax=smax)


LIB = GAME + "/Megascans/3D_Assets/%s/%s.%s"
DRIFT = GAME + "/Fab/Megascans/3D/%s/Medium/%s_tier_2/StaticMeshes/%s_tier_2.%s_tier_2"


def lib_rock(folder, mesh, density, res="ForestBoulder", cluster=1500.0, smin=0.7, smax=1.4):
    """A rock from the local Megascans library (Content/Megascans/3D_Assets): a harvestable boulder (solid by its resource type)."""
    return species(LIB % (folder, mesh, mesh), density, res=res, tag="MOResource_Rock", cluster=cluster, align=True, tilt=0.0, smin=smin, smax=smax)


def drift(folder, code, density, cluster=2500.0, collision="Passable", smin=0.8, smax=1.3):
    """Fallen wood / driftwood: decorative (no resource node), swept away when the ground is dug."""
    return species(DRIFT % (folder, code, code, code), density, tag="MOBiomeCover_Debris", cluster=cluster, align=True, tilt=4.0, sweep=True, smin=smin, smax=smax, collision=collision)


def undergrowth(path, density, group=0, core=0.45, smin=0.7, smax=1.1, tag="MOBiomeCover_Shrub", cluster=2500.0):
    """Decorative woody undergrowth (seedlings, hazel): clumps with the canopy, no collision, swept by digging."""
    return species(path, density, tag=tag, cluster=cluster, group=group, core=core, sweep=True, smin=smin, smax=smax)


def cover(path, density, cluster=0.0, tag="MOBiomeCover_Grass", smin=0.8, smax=1.2):
    return species(path, density, tag=tag, cluster=cluster, sweep=True, smin=smin, smax=smax)


def flower(code, density, cluster=1800.0, smin=0.9, smax=1.4):
    return species(fl(code), density, tag="MOBiomeCover_Flower", cluster=cluster, sweep=True, smin=smin, smax=smax, tilt=6.0)


def boulder(kind, density, cluster=1500.0, smin=0.5, smax=1.5):
    if kind == "forest":
        return species(rock("Forest_Boulder_wgosajs", "wgosajs"), density, res="ForestBoulder", tag="MOResource_Rock", cluster=cluster, align=True, tilt=0.0, smin=smin, smax=smax)
    paths = {"beach_a": rock("Beach_Boulder_ukopfbks", "ukopfbks"), "beach_b": rock("Beach_Rock_wc2hbfr", "wc2hbfr"), "beach_c": rock("Beach_Rock_wd5mbhd", "wd5mbhd")}
    return species(paths[kind], density, res="BeachBoulder", tag="MOResource_Rock", cluster=cluster, align=True, tilt=0.0, smin=smin, smax=smax)


PALETTES = {
    "TemperateForest": [
        # canopy: five different trees, tall to short, in clumps
        tree("Forest_01", 70), tree("Forest_02", 50, core=0.75), tree("Forest_03", 45, core=0.7), tree("Forest_06", 30, core=0.6), tree("Forest_07", 22, core=0.55),
        # the growth stages under them: saplings are harvestable and solid, seedlings and hazel are cover
        tree("Sapling_01", 34, core=0.5, smin=0.7, smax=1.1), tree("Sapling_03", 26, core=0.5, smin=0.7, smax=1.1), tree("Sapling_04", 20, core=0.45, smin=0.7, smax=1.1),
        undergrowth(ba("Seedling_01"), 45), undergrowth(ba("Seedling_02"), 30), undergrowth(ch("Forest_03"), 14, group=3, core=0.5, smin=0.8, smax=1.2),
        undergrowth(ch("Sapling_02"), 24, group=3, core=0.4), undergrowth(ch("Sapling_04"), 28, group=3, core=0.4),
        # the floor
        cover(wild_grass("F"), 380, 800.0), cover(wild_grass("D"), 140, 1200.0), cover(wild_grass("E"), 110, 1500.0),
        flower("11_02", 70, 1400.0), flower("19_03", 90, 1200.0), flower("16_03", 40, 1600.0),
        boulder("forest", 3.0, 2500.0, 0.6, 1.6),
    ],
    "Meadow": [
        cover(wild_grass("G"), 760, 0.0, smin=0.9, smax=1.3), cover(wild_grass("H"), 520, 0.0, smin=0.9, smax=1.3), cover(wild_grass("B"), 160, 1400.0), cover(dune_grass("C"), 140, 1800.0),
        # flowers: six families in separate drifts, so a meadow reads as patches of colour
        flower("01_02", 120, 1700.0), flower("02_03", 90, 2000.0), flower("05_02", 110, 1500.0), flower("07_03", 95, 1800.0), flower("08_02", 85, 2100.0), flower("10_03", 28, 2400.0),
        flower("17_02", 50, 2000.0),
        # lone trees and thickets standing in the grass
        tree("Field_01", 1.5, group=1, core=0.85, smin=0.9, smax=1.4), tree("Field_02", 1.0, group=1, core=0.85, smin=0.9, smax=1.3), tree("Field_04", 1.2, group=1, core=0.85, smin=0.9, smax=1.3),
        species(ba("Sapling_02"), 4.0, res="BlackAlder", tag="MOResource_Tree", cluster=4000.0, group=1, core=0.62, smin=0.6, smax=1.0),
        undergrowth(ch("Field_03"), 1.4, group=1, core=0.8, smin=0.8, smax=1.2), undergrowth(ch("Sapling_01"), 6.0, group=1, core=0.6),
        species(raspberry("A"), 6.0, res="RaspberryBush", tag="MOResource_Bush", cluster=2200.0, group=1, core=0.55, smin=0.8, smax=1.2),
        boulder("forest", 1.6, 3000.0, 0.5, 1.3),
    ],
    "RockyHighland": [
        boulder("forest", 25.0, 2000.0, 0.6, 1.8), boulder("beach_c", 40.0, 1200.0, 0.5, 1.5), boulder("beach_b", 30.0, 1600.0, 0.5, 1.4),
        # hardy stunted trees and scrub between the rocks
        tree("Forest_08", 5.0, group=-1, core=0.6, smin=0.7, smax=1.1), tree("Forest_09", 4.0, group=-1, core=0.6, smin=0.7, smax=1.1),
        species(ba("Sapling_02"), 8.0, res="BlackAlder", tag="MOResource_Tree", cluster=0.0, smin=0.6, smax=1.0),
        species(dead_shrub("G"), 14.0, res="DeadShrub", tag="MOResource_Bush", cluster=1500.0, smin=0.7, smax=1.2), species(dead_shrub("E"), 16.0, res="DeadShrub", tag="MOResource_Bush", cluster=1500.0),
        cover(dune_grass("B"), 140, 1200.0), cover(dune_grass("E"), 90, 1600.0),
        flower("09_02", 30, 1800.0), flower("20_02", 26, 2000.0),
        lib_rock("Nordic_Forest_Cluster_Rock_Medium_xfplcbf", "S_Nordic_Forest_Cluster_Rock_Medium_xfplcbf_lod3_Var1", 6.0, smin=0.8, smax=1.6),
        lib_rock("Nordic_Forest_Rock_Small_xiemca1", "S_Nordic_Forest_Rock_Small_xiemca1_lod3", 14.0), lib_rock("Nordic_Forest_Rock_Small_xjiifd1", "S_Nordic_Forest_Rock_Small_xjiifd1_lod3", 14.0),
    ],
    "ShoreStrand": [
        boulder("beach_b", 18.0, 1500.0, 0.5, 1.4), boulder("beach_a", 6.0, 2500.0, 0.6, 1.6), boulder("beach_c", 12.0, 1800.0, 0.5, 1.3),
        cover(wild_grass("A"), 120, 900.0, smin=0.7, smax=1.1), cover(dune_grass("A"), 220, 900.0, smin=0.8, smax=1.2), cover(dune_grass("C"), 130, 1300.0),
        species(dead_shrub("D"), 22.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0, smin=0.8, smax=1.3), species(dead_shrub("B"), 16.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0),
        flower("03_02", 70, 1400.0, 1.0, 1.6), flower("13_03", 60, 1600.0, 1.0, 1.5), flower("15_03", 50, 1800.0, 1.0, 1.5),
        # the library's beach finds: sandstone and smooth stones on the sand, wood the sea left behind
        lib_rock("Sand_Rock_vd5scco", "S_Sand_Rock_vd5scco_lod3_Var1", 26.0, res="BeachBoulder", cluster=2000.0, smin=0.8, smax=2.2),
        lib_rock("Stone_vd3edgg", "S_Stone_vd3edgg_lod3", 34.0, res="BeachBoulder", cluster=1400.0, smin=0.7, smax=1.8),
        drift("Broken_Tree_Bark_wjhiecy", "wjhiecy", 2.2, 3500.0, collision="Solid", smin=0.7, smax=1.2),
        drift("Bent_Branch_paErt", "paErt", 9.0, 2200.0), drift("Common_Spruce_Branch_uehjfcofa", "uehjfcofa", 6.0, 2200.0),
    ],
    "DryScrub": [
        species(dead_shrub("A"), 55.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0, group=1), species(dead_shrub("C"), 38.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0, group=1),
        species(dead_shrub("F"), 26.0, res="DeadShrub", tag="MOResource_Bush", cluster=2000.0, group=1), species(dead_shrub("H"), 18.0, res="DeadShrub", tag="MOResource_Bush", cluster=2000.0, group=1),
        species(locust("B"), 3.0, res="BlackLocust", tag="MOResource_Tree", smin=0.8, smax=1.15), species(locust("H"), 6.0, res="BlackLocust", tag="MOResource_Tree", cluster=2500.0, smin=0.8, smax=1.2),
        cover(dune_grass("B"), 440, 0.0), cover(dune_grass("D"), 150, 1500.0), cover(dune_grass("F"), 110, 1800.0),
        flower("09_05", 50, 2000.0), flower("16_04", 40, 1800.0), flower("19_04", 45, 1600.0), flower("20_03", 30, 2200.0),
        boulder("beach_b", 4.0, 2500.0, 0.5, 1.2), boulder("forest", 1.2, 3000.0, 0.5, 1.2),
        lib_rock("Sand_Rock_vd5scco", "S_Sand_Rock_vd5scco_lod3_Var1", 10.0, res="BeachBoulder", cluster=2200.0, smin=0.8, smax=2.0),
    ],
    "LocustWoodland": [
        species(locust("A"), 62, res="BlackLocust", tag="MOResource_Tree", cluster=3000.0, group=2, core=0.7, smin=0.8, smax=1.2),
        species(locust("C"), 42, res="BlackLocust", tag="MOResource_Tree", cluster=3000.0, group=2, core=0.75, smin=0.8, smax=1.2),
        species(locust("E"), 28, res="BlackLocust", tag="MOResource_Tree", cluster=3000.0, group=2, core=0.5, smin=0.6, smax=1.0),
        species(locust("G"), 30, res="BlackLocust", tag="MOResource_Tree", cluster=3000.0, group=2, core=0.55, smin=0.7, smax=1.1),
        species(locust("D"), 40, tag="MOBiomeCover_Shrub", cluster=2500.0, group=2, core=0.4, sweep=True, smin=0.7, smax=1.2),
        species(raspberry("B"), 32, res="RaspberryBush", tag="MOResource_Bush", cluster=2000.0, group=2, core=0.45), species(raspberry("D"), 20, res="RaspberryBush", tag="MOResource_Bush", cluster=2200.0, group=2, core=0.4),
        undergrowth(ch("Forest_04"), 10, group=2, core=0.55, smin=0.8, smax=1.2), undergrowth(ch("Sapling_03"), 18, group=2, core=0.4),
        cover(wild_grass("C"), 280, 800.0), cover(wild_grass("E"), 140, 1400.0),
        flower("12_02", 55, 1700.0), flower("17_04", 45, 2000.0), flower("18_03", 40, 1900.0),
        boulder("forest", 2.5, 2500.0, 0.6, 1.5),
        lib_rock("Nordic_Forest_Rock_Small_xiemca1", "S_Nordic_Forest_Rock_Small_xiemca1_lod3", 4.0, cluster=2400.0),
        lib_rock("Mossy_Forest_Boulder_wc2hbjw", "S_Mossy_Forest_Boulder_wc2hbjw_lod3_Var1", 2.0, cluster=2600.0),
    ],
}

# A new biome for the doubled relief: high ground that is not steep enough to be bare rock. Gravel under foot, wind-cut scrub and boulders, tough grass, a few late flowers.
ALPINE = {
    "displayName": "Alpine Heath",
    "heightMin": 32000.0, "heightMax": 100000.0, "slopeMinDeg": 0.0, "slopeMaxDeg": 40.0, "moistureMin": 0.0, "moistureMax": 1.0, "temperatureMin": 0.0, "temperatureMax": 1.0,
    "priority": 14, "groundSurfaceType": "/Game/VoxelExamples/NaniteMaterials/Materials/VST_MI_Gravel.VST_MI_Gravel", "edgeBlendWidth": 9000.0,
    "species": [
        boulder("forest", 14.0, 2500.0, 0.6, 1.7), boulder("beach_c", 20.0, 1500.0, 0.5, 1.4), boulder("beach_b", 14.0, 1800.0, 0.5, 1.3),
        tree("Forest_08", 3.0, group=-1, core=0.6, smin=0.7, smax=1.0), tree("Forest_09", 2.5, group=-1, core=0.6, smin=0.7, smax=1.0),
        species(ba("Sapling_05"), 14.0, res="BlackAlder", tag="MOResource_Tree", cluster=2400.0, smin=0.6, smax=1.0),
        species(dead_shrub("G"), 24.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0), species(dead_shrub("E"), 20.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0),
        cover(dune_grass("B"), 300, 0.0), cover(dune_grass("E"), 160, 1600.0), cover(wild_grass("B"), 90, 2000.0),
        flower("10_01", 22, 2400.0), flower("09_02", 40, 1800.0), flower("20_02", 40, 2000.0), flower("16_02", 30, 2200.0),
    ],
}


def sapling(name, density, group, core):
    return tree(name, density, group=group, core=core, smin=0.7, smax=1.15)


def hazel(name, density, group, core, smin=0.8, smax=1.3, cluster=2500.0):
    return undergrowth(ch(name), density, group=group, core=core, smin=smin, smax=smax, cluster=cluster)


# RAINFOREST: moist AND warm. The layering is the PCG Biome Sample's: parent assets (the canopy) with child assets scattered around them (saplings, then seedlings, then boulders), each layer
# in its own clump field so the stand has rooms and clearings -- here as ClusterGroups: group 4 = the tree stand (giants at the clump cores, young trees in the ring around them), group 5 = the
# shrub thicket. Emergent giants stand a head above a dense canopy; the floor is dark (VST_MI_JungleFloor), thick with undergrowth, mossy boulders and fallen wood.
RAINFOREST = {
    "displayName": "Rainforest",
    "heightMin": -100000.0, "heightMax": 100000.0, "slopeMinDeg": 0.0, "slopeMaxDeg": 42.0, "moistureMin": 0.70, "moistureMax": 1.0, "temperatureMin": 0.55, "temperatureMax": 1.0,
    "priority": 12, "groundSurfaceType": "/Game/VoxelExamples/NaniteMaterials/Materials/JungleFloor/VST_MI_JungleFloor.VST_MI_JungleFloor", "edgeBlendWidth": 7000.0,
    "reliefScale": 1.1, "mountainScale": 1.0,
    "species": [
        # emergent giants
        tree("Forest_01", 26, group=4, core=0.84, smin=1.5, smax=2.2), tree("Forest_02", 22, group=4, core=0.84, smin=1.5, smax=2.1),
        # the canopy, dense and overlapping
        tree("Forest_03", 60, group=4, core=0.70, smin=1.0, smax=1.5), tree("Forest_04", 46, group=4, core=0.68, smin=1.0, smax=1.5), tree("Forest_05", 40, group=4, core=0.68, smin=1.0, smax=1.5),
        tree("Forest_06", 46, group=4, core=0.64, smin=1.0, smax=1.45), tree("Forest_07", 40, group=4, core=0.62, smin=0.95, smax=1.4),
        tree("Forest_08", 30, group=4, core=0.60, smin=0.9, smax=1.3), tree("Forest_09", 28, group=4, core=0.60, smin=0.9, smax=1.3),
        # young trees in the ring around each stand (harvestable, walk-through)
        sapling("Sapling_01", 46, 4, 0.46), sapling("Sapling_02", 40, 4, 0.44), sapling("Sapling_03", 40, 4, 0.44), sapling("Sapling_04", 36, 4, 0.42),
        sapling("Sapling_05", 30, 4, 0.42), sapling("Sapling_06", 26, 4, 0.40),
        undergrowth(ba("Seedling_01"), 90, group=4, core=0.34), undergrowth(ba("Seedling_02"), 70, group=4, core=0.34),
        # the shrub thicket (its own clump field)
        hazel("Forest_01", 22, 5, 0.50), hazel("Forest_02", 22, 5, 0.50), hazel("Forest_03", 28, 5, 0.46), hazel("Forest_04", 28, 5, 0.46),
        hazel("Sapling_01", 40, 5, 0.34), hazel("Sapling_02", 40, 5, 0.34), hazel("Sapling_03", 36, 5, 0.32), hazel("Sapling_04", 36, 5, 0.32),
        hazel("Seedling_01", 60, 5, 0.26, 0.7, 1.1), hazel("Seedling_02", 60, 5, 0.26, 0.7, 1.1), hazel("Seedling_03", 50, 5, 0.26, 0.7, 1.1), hazel("Seedling_04", 50, 5, 0.26, 0.7, 1.1),
        # ground cover: lush, tall, in drifts
        cover(wild_grass("F"), 420, 700.0, smin=0.9, smax=1.5), cover(wild_grass("D"), 260, 1000.0, smin=0.9, smax=1.5), cover(wild_grass("E"), 200, 1400.0, smin=0.9, smax=1.5),
        cover(wild_grass("G"), 300, 900.0, smin=0.9, smax=1.5), cover(wild_grass("H"), 200, 1200.0, smin=0.9, smax=1.5),
        flower("03_02", 90, 1300.0), flower("11_02", 110, 1200.0), flower("16_03", 70, 1500.0), flower("18_03", 90, 1400.0), flower("19_03", 120, 1100.0),
        flower("05_02", 80, 1500.0), flower("02_03", 70, 1600.0), flower("07_03", 60, 1700.0), flower("15_03", 60, 1700.0),
        # the floor: mossy boulders (a jungle-tagged scan), fallen trunks and branches
        lib_rock("Mossy_Forest_Boulder_wc2hbjw", "S_Mossy_Forest_Boulder_wc2hbjw_lod3_Var1", 7.0, cluster=2600.0, smin=0.8, smax=1.9),
        lib_rock("Nordic_Forest_Rock_Small_xjiifd1", "S_Nordic_Forest_Rock_Small_xjiifd1_lod3", 5.0, cluster=2000.0),
        drift("Broken_Tree_Bark_wjhiecy", "wjhiecy", 3.0, 3000.0, collision="Solid", smin=0.8, smax=1.5),
        drift("Bent_Branch_paErt", "paErt", 10.0, 2400.0), drift("Common_Spruce_Branch_uehjfcofa", "uehjfcofa", 8.0, 2400.0),
    ],
}

# The decorative layers (grass, flowers, shrubs, seedlings) are thinned to 70%: the first draft was 3350 instances per hectare, 2.6x the forest, and every one is a HISM instance the GPU draws.
for _s in RAINFOREST["species"]:
    if _s["ResourceNodeId"] == "None" and _s["HISMTag"] in ("MOBiomeCover_Grass", "MOBiomeCover_Flower", "MOBiomeCover_Shrub"):
        _s["DensityPerHectare"] = round(_s["DensityPerHectare"] * 0.7, 1)

# Band / relief / ground changes for existing rows (merged onto the row: only these keys change). Priorities now: Rocky 20, Alpine 14, Shore 13, Rainforest 12, Locust 11, Forest 10, Dry 8, Meadow 1.
ROW_EDITS = {
    "ShoreStrand": {"heightMin": -100000.0, "heightMax": 400.0, "priority": 13, "edgeBlendWidth": 4500.0,
                    "groundSurfaceType": "/Game/VoxelExamples/NaniteMaterials/Materials/Sand/VST_MI_Sand.VST_MI_Sand"},
    "Meadow": {"reliefScale": 0.25, "mountainScale": 0.03},      # prairie: gently rolling, no mountains
    "DryScrub": {"reliefScale": 0.35, "mountainScale": 0.15},    # arid plain with low foothills
    "TemperateForest": {"reliefScale": 1.0, "mountainScale": 1.0},
    "LocustWoodland": {"reliefScale": 1.2, "mountainScale": 1.6},   # cool and moist: the dramatic highlands
}


def write_plan():
    out = {name: {"species": sp} for name, sp in PALETTES.items()}
    for name, edit in ROW_EDITS.items():
        out.setdefault(name, {}).update(edit)
    out["AlpineHeath"] = ALPINE
    out["Rainforest"] = RAINFOREST
    with open("D:/UEProjects/MO57/Saved/Logs/biome_palettes.json", "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
    for name, sp in list(PALETTES.items()) + [("AlpineHeath", ALPINE["species"]), ("Rainforest", RAINFOREST["species"])]:
        total = sum(s["DensityPerHectare"] for s in sp)
        trees = sum(s["DensityPerHectare"] for s in sp if s["HISMTag"] == "MOResource_Tree")
        print("PALETTE %-16s %2d species, %5.0f instances/ha (%4.0f trees)" % (name, len(sp), total, trees))


if __name__ == "__main__" or True:
    write_plan()
