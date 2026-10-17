"""The biome palettes of DT_Biomes as DATA: what grows where, and how thick. One place to read and tune the look of the land.

Apply with the editor open:
    python Tools/ue.py py --file Content/Python/biome_palettes.py          (writes Saved/Logs/biome_palettes.json and prints the plan)
    python Tools/ue.py rows set /MOFramework/Data/DT_Biomes.DT_Biomes --file Saved/Logs/biome_palettes.json     (existing rows: species arrays are replaced wholesale)
The new biome (AlpineHeath) is added by apply_biome_palettes() below through the in-editor DataTable API.

WARNING (Oct 16 2026): this script has DRIFTED from the live DT_Biomes -- Meadow, RockyHighland, DryScrub and AlpineHeath each have one more species live, and the
Rainforest hazel densities live are ~0.7x the values here (tuned in the table later). Applying the plan would REVERT those. Diff the plan against `ue.py rows get`
first, or make targeted edits on the live rows (the Oct 16 alder burial was applied that way: 42 entries, nothing else touched).

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


ALDER_TREE_EXTRA_SINK_CM = 25.0   # see species()


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


def species(path, density, res="None", tag="", cluster=0.0, group=-1, core=0.6, align=False, tilt=8.0, sweep=False, smin=0.85, smax=1.25, collision="Auto", sink=-1.0, sink_cm=0.0,
            min_slope=0.0, max_slope=90.0):
    # Every Black Locust variant is a GROUND-LEVEL plant (0.5 - 1.5 m tall, up to 1.8 m wide) that the resource table calls a Tree, so by resource type it blocks the pawn like an oak: you walk
    # into a knee-high bush and stop. They are walk-through (found 2026-10-10 by Wes: "a small locust bush, or a collection of them"; I had first blamed the saplings and been wrong).
    if collision == "Auto" and "wdvhberja" in path:
        collision = "Passable"
    if "/BlackAlder/" in path and ("_Forest_" in path or "_Field_" in path):
        # Black Alder TREES floated (Wes, Oct 16 2026: "seated another 25cm or so into the ground"). The meshes are not the cause -- every base sits 0 - 44 cm
        # under its pivot -- but trees are not tilted to the slope, so the downhill side of the wide root flare shows. Not the saplings / seedlings (0.4 - 4 m):
        # 25 cm buries too much of them.
        sink_cm += ALDER_TREE_EXTRA_SINK_CM
    if tag == "MOResource_Tree":
        # Trees of one species stood as one even wall (playtest 2026-10-14 #14: "more variation in the sizes ... like 75% to 125%"): every tree spans at least
        # 75 - 125 % of its mid size (a wider authored range is kept; far stand-ins derive from these and inherit it).
        mid = 0.5 * (smin + smax)
        smin, smax = round(min(smin, 0.75 * mid), 3), round(max(smax, 1.25 * mid), 3)
    return {"Mesh": path, "DensityPerHectare": density, "ClusterRadius": cluster, "ClusterGroup": group, "ClusterCore": core, "HISMTag": tag or "None",
            "ResourceNodeId": res, "bAlignToSurfaceNormal": align, "MaxRandomTiltDeg": tilt, "bAutoSweepOnTerraform": sweep, "MinScale": smin, "MaxScale": smax, "Collision": collision,
            "SinkFraction": sink, "SinkMinCm": sink_cm, "MinSlopeDeg": min_slope, "MaxSlopeDeg": max_slope}   # sink: fraction of the mesh height buried along its up axis (-1 = auto: 0.25 for aligned rocks / debris, 0 for plants); sink_cm: extra burial


def tree(name, density, group=0, core=0.7, res="BlackAlder", smin=0.8, smax=1.3):
    # (Saplings block like trees again: they were never the "bush that stops you" -- that was the locust, see species().)
    return species(ba(name), density, res=res, tag="MOResource_Tree", cluster=3000.0, group=group, core=core, smin=smin, smax=smax)


LIB = GAME + "/Megascans/3D_Assets/%s/%s.%s"
DRIFT = GAME + "/Fab/Megascans/3D/%s/Medium/%s_tier_2/StaticMeshes/%s_tier_2.%s_tier_2"


def lib_rock(folder, mesh, density, res="ForestBoulder", cluster=1500.0, smin=0.7, smax=1.4):
    """A rock from the local Megascans library (Content/Megascans/3D_Assets): a harvestable boulder (solid by its resource type)."""
    return species(LIB % (folder, mesh, mesh), density, res=res, tag="MOResource_Rock", cluster=cluster, align=True, tilt=0.0, smin=smin, smax=smax)


# Scans that moved into an item's art folder when the item got that mesh (the old path is a redirector; the table stores the new one).
MOVED = {"wjhiecy": "/MOFramework/Items/Bark01/Medium/wjhiecy_tier_2/StaticMeshes/wjhiecy_tier_2.wjhiecy_tier_2"}


def drift(folder, code, density, cluster=2500.0, collision="Passable", smin=0.8, smax=1.3):
    """Fallen wood / driftwood: decorative (no resource node), swept away when the ground is dug."""
    return species(MOVED.get(code, DRIFT % (folder, code, code, code)), density, tag="MOBiomeCover_Debris", cluster=cluster, align=True, tilt=4.0, sweep=True, smin=smin, smax=smax, collision=collision)


def undergrowth(path, density, group=0, core=0.45, smin=0.7, smax=1.1, tag="MOBiomeCover_Shrub", cluster=2500.0):
    """Decorative woody undergrowth (seedlings, hazel): clumps with the canopy, swept by digging. Walk-through, EXCEPT saplings: a 1.5-2 m sapling is a trunk you cannot walk through (2026-10-12: Wes
    found hazel saplings without collision; they are decorative, so by resource type they had none; their meshes carry convex collision shapes, so Solid is enough)."""
    # A hazel "Forest" variant is a 4-5 m multi-stem small tree, not undergrowth you wade through (playtest 2026-10-14 #9: "these medium sized trees don't have
    # collision"): Solid, and fix_mesh_collision.py gives it a CORE capsule (the stems stop you, the outer twigs do not).
    solid = "Sapling" in path or "CommonHazel_Forest" in path
    # Common Hazel is a harvest source (DT_Resources CommonHazel: hazelnuts, green branches), not decoration: E on the commonest bush around a
    # woodland spawn did nothing (Wes, Oct 15 2026: everything in the world gets an action; plain cover forages).
    res = "CommonHazel" if "CommonHazel" in path else "None"
    if res != "None":
        tag = "MOResource_Bush"
    return species(path, density, res=res, tag=tag, cluster=cluster, group=group, core=core, sweep=True, smin=smin, smax=smax, collision="Solid" if solid else "Auto")


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
        # scrubland, not a plain: shrubs in thickets (clump group 1) with tufts of dry grass between them; at the old densities (55 / 38 / 26 / 18 per ha and 440 tufts) the biome read as bare brown dirt
        species(dead_shrub("A"), 110.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0, group=1), species(dead_shrub("C"), 76.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0, group=1),
        species(dead_shrub("F"), 52.0, res="DeadShrub", tag="MOResource_Bush", cluster=2000.0, group=1), species(dead_shrub("H"), 36.0, res="DeadShrub", tag="MOResource_Bush", cluster=2000.0, group=1),
        species(dead_shrub("E"), 40.0, res="DeadShrub", tag="MOResource_Bush", cluster=2200.0, group=1), species(dead_shrub("G"), 30.0, res="DeadShrub", tag="MOResource_Bush", cluster=2200.0, group=1),
        species(locust("B"), 12.0, res="BlackLocust", tag="MOResource_Tree", cluster=2600.0, group=1, core=0.5, smin=0.8, smax=1.15), species(locust("H"), 18.0, res="BlackLocust", tag="MOResource_Tree", cluster=2600.0, group=1, core=0.5, smin=0.8, smax=1.2),
        species(locust("D"), 30.0, tag="MOBiomeCover_Shrub", cluster=2600.0, group=1, core=0.45, sweep=True, smin=0.7, smax=1.3),   # waist-high thickets, walk-through: the visible mass of the scrub
        cover(dune_grass("B"), 900, 0.0), cover(dune_grass("D"), 380, 1500.0), cover(dune_grass("F"), 300, 1800.0), cover(wild_grass("B"), 120, 2200.0),
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
    "priority": 14, "groundSurfaceType": "/Game/VoxelExamples/NaniteMaterials/Materials/VST_MI_Gravel.VST_MI_Gravel", "edgeBlendWidth": 9000.0,   # (ground replaced below)
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

# Playtest 2026-10-14 #5 / #6 (Rainforest: "dense forest, grass still brightly lit, trees VERY dense"; frame rate halved there, 31-37 ms Shipping 1440p):
# the canopy trees x0.65 (the emergent giants Forest_01/02 stay), saplings x0.75, the lush grass drifts x0.5 -- a closed canopy shades the floor out;
# ferns take that floor instead (appended below, after fern() exists). The scatter graph's native LAWN grass no longer runs here either
# (Content/Python/native_grass_biomes.py): that grass glowed under the canopy.
for _s in RAINFOREST["species"]:
    _m = _s["Mesh"]
    if _s["HISMTag"] == "MOResource_Tree" and "_Forest_0" in _m and "_Forest_01" not in _m and "_Forest_02" not in _m:
        _s["DensityPerHectare"] = round(_s["DensityPerHectare"] * 0.65, 1)
    elif _s["HISMTag"] == "MOResource_Tree" and "Sapling" in _m:
        _s["DensityPerHectare"] = round(_s["DensityPerHectare"] * 0.75, 1)
    elif _s["HISMTag"] == "MOBiomeCover_Grass":
        _s["DensityPerHectare"] = round(_s["DensityPerHectare"] * 0.5, 1)

# Band / relief / ground changes for existing rows (merged onto the row: only these keys change). Priorities now: Rocky 20, Alpine 14, Shore 13, Rainforest 12, Locust 11, Forest 10, Dry 8, Meadow 1.
ROW_EDITS = {
    "ShoreStrand": {"heightMin": -100000.0, "heightMax": 400.0, "priority": 13, "edgeBlendWidth": 4500.0,
                    "groundSurfaceType": "/Game/VoxelExamples/NaniteMaterials/Materials/Sand/VST_MI_Sand.VST_MI_Sand"},
    "Meadow": {"reliefScale": 0.25, "mountainScale": 0.03},      # prairie: gently rolling, no mountains
    "DryScrub": {"reliefScale": 0.35, "mountainScale": 0.15},    # arid plain with low foothills
    "TemperateForest": {"reliefScale": 1.0, "mountainScale": 1.0},
    "LocustWoodland": {"reliefScale": 1.2, "mountainScale": 1.6},   # cool and moist: the dramatic highlands
}


# --- MOUNTAINS (2026-10-12, Wes: "the mountains are really shitty looking, a lot of grass but not much else") ---------------------------------------------------------------------------------
# The Electric Dreams migration brought Nanite scans that fit steep ground: boulders (collision added by prep_rock_meshes.py), flat rock PLATES (ErodedRockyGround, SmoothRockShelf, MossyRocks,
# SandstoneRockyGround: lie flush in the slope, edges buried), 11-22 m sandstone cliff slabs and the SmallStonesPack scree. Everything aligned to the terrain normal and buried (spawner sink math);
# slopes get rock, scree and scrub, the grass drops to patches.
ED = GAME + "/Megascans/3D_Assets/%s/%s.%s"


def surface(name):
    """A voxel surface type made from an Electric Dreams ground scan (Content/Python/ed_surfaces_import.py)."""
    return "/Game/VoxelExamples/NaniteMaterials/Materials/%s/VST_MI_%s.VST_MI_%s" % (name, name, name)


def ed(folder, mesh):
    return ED % (folder, mesh, mesh)


def ed_rock(folder, mesh, density, res="ForestBoulder", cluster=1800.0, smin=0.8, smax=1.8, sink=-1.0, sink_cm=0.0, collision="Auto"):
    """A solid scanned rock (harvestable stone, blocks the pawn by its resource type), aligned to the surface and buried a quarter of its height."""
    return species(ed(folder, mesh), density, res=res, tag="MOResource_Rock", cluster=cluster, align=True, tilt=0.0, smin=smin, smax=smax, sink=sink, sink_cm=sink_cm, collision=collision)


def ed_plate(folder, mesh, density, cluster=2600.0, smin=0.9, smax=1.7, sink_cm=14.0, collision="Solid"):
    """A flat rock slab (open geometry, 0.2-0.5 m thick, 2-7 m wide): decorative, aligned to the slope and sunk by half its thickness so neither its edge nor its open underside shows.
    SOLID: half-sunk, a plate still stands 20-54 cm out of the ground, and a walk-through one swallowed the player's feet (playtest reports 2026-10-14 #2 / #3 / #18; the species gallery's
    contact test measures it: MO.Gallery <Biome> + MO.Gallery.Test). Only plates thinner than ~20 cm above ground stay walk-through (collision="Passable" at the call)."""
    return species(ed(folder, mesh), density, tag="MOBiomeCover_Rock", cluster=cluster, align=True, tilt=0.0, sweep=True, smin=smin, smax=smax, sink=0.5, sink_cm=sink_cm, collision=collision,
                   max_slope=28.0)   # steeper and the aligned plate stands on its edge, hollow back showing (playtest 2026-10-14 #1 / #26)


def ed_cliff(mesh, density, cluster=6000.0, smin=0.7, smax=1.3, folder="HugeSandstoneCliff"):
    """A 11-22 m cliff slab (sandstone by default, `folder` picks another): solid, aligned, buried a fifth of its height."""
    return species(ed(folder, mesh), density, tag="MOBiomeCover_Rock", cluster=cluster, align=True, tilt=0.0, smin=smin, smax=smax, sink=0.2, collision="Solid",
                   min_slope=25.0)   # a cliff on gentle ground is a monolith (playtest 2026-10-14 #26)


def scree(density_each, smin=1.2, smax=2.8):
    """SmallStonesPack 08-14 (10-18 cm stones, the low-poly variants): a talus of them, a little embedded."""
    return [species(ed("SmallStonesPack", "SM_SmallStonesPack_%02d" % i), density_each, tag="MOBiomeCover_Debris", cluster=900.0, align=True, tilt=0.0, sweep=True, smin=smin, smax=smax, sink=0.3, collision="Passable")
            for i in range(8, 15)]


def mountain_rocks(k=1.0, big=1.0, plates=1.0):
    """The shared rock set, scaled: k = boulders, big = the 2-5 m rocks, plates = ground slabs."""
    return [
        ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_02", 26.0 * k, cluster=1600.0, smin=0.8, smax=2.0),
        ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_05", 20.0 * k, cluster=1800.0, smin=0.8, smax=2.0),
        ed_rock("LichenedForestBoulder", "SM_LichenedForestBoulder_01", 16.0 * k, cluster=1600.0, smin=0.9, smax=2.2),
        ed_rock("MossyForestRock", "SM_MossyForestRock_02", 12.0 * k, cluster=2000.0, smin=0.8, smax=1.8),
        ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_04", 2.0 * big, cluster=3200.0, smin=0.5, smax=1.1),
        ed_rock("NordicBeachRocks", "SM_NordicBeachRocks_01", 1.5 * big, res="BeachBoulder", cluster=3200.0, smin=0.6, smax=1.2),
        # dark / grey slabs carry the ground; the PALE ones (ErodedRockyGround_03 pink-white, SmoothRockShelf cream, SandstoneRockyGround) read as white paper scraps on dark volcanic rock and green moss
        # (seen on the Alpine Rock / Heath slopes, 2026-10-12), so they are an accent, not the base. TundraMossyBoulder is the sample's dark moss-skinned slab boulder.
        ed_plate("ErodedRockyGround", "SM_ErodedRockyGround_01", 12.0 * plates), ed_plate("MossyRocks", "SM_MossyRocks_02", 10.0 * plates),
        ed_plate("ErodedRockyGround", "SM_ErodedRockyGround_03", 2.5 * plates), ed_plate("SmoothRockShelf", "SM_SmoothRockShelf_01", 2.0 * plates),
        ed_plate("SandstoneRockyGround", "SM_SandstoneRockyGround_01", 2.5 * plates, sink_cm=8.0, collision="Passable"),   # 11 cm above ground: stepped over
        ed_rock("TundraMossyBoulder", "SM_TundraMossyBoulder_01", 3.0 * k, cluster=2800.0, smin=0.5, smax=1.1),
    ]


def thin(species_list, factor, tags=("MOBiomeCover_Grass", "MOBiomeCover_Flower")):
    for sp in species_list:
        if sp["HISMTag"] in tags:
            sp["DensityPerHectare"] = round(sp["DensityPerHectare"] * factor, 1)
    return species_list


# steep ground (slope >= 28 deg, any height): rock first. The old list kept for the plants (trees / scrub / grass at a third), the rocks replaced.
PALETTES["RockyHighland"] = (mountain_rocks(1.0, 1.0, 1.0) + scree(55.0)
    + [ed_cliff("SM_HugeSandstoneCliff_07", 0.25), ed_cliff("SM_HugeSandstoneCliff_04", 0.2)]
    + thin([sp for sp in PALETTES["RockyHighland"] if sp["HISMTag"] != "MOResource_Rock"], 0.4, ("MOBiomeCover_Grass", "MOBiomeCover_Flower", "MOResource_Tree", "MOResource_Bush")))

# alpine heath (>= 320 m, gentler ground): scrub and heath, boulders and some slabs
ALPINE["species"] = (mountain_rocks(0.4, 0.5, 0.5) + scree(20.0) + [sp for sp in ALPINE["species"] if sp["HISMTag"] != "MOResource_Rock"]
    # the heath itself: seedlings, low scrub and flowers between the tussocks (the first pass was gravel and boulders: a desert)
    + [undergrowth(ba("Seedling_01"), 70, group=-1, core=0.0, smin=0.8, smax=1.4, cluster=2200.0), undergrowth(ba("Seedling_02"), 45, group=-1, core=0.0, smin=0.8, smax=1.3, cluster=2600.0),
       flower("19_03", 110, 1500.0), flower("11_02", 80, 1700.0), flower("12_02", 70, 1800.0), flower("17_04", 50, 2000.0), flower("09_02", 60, 1600.0),
       species(dead_shrub("A"), 40.0, res="DeadShrub", tag="MOResource_Bush", cluster=1600.0, smin=0.8, smax=1.4), species(dead_shrub("C"), 26.0, res="DeadShrub", tag="MOResource_Bush", cluster=1800.0)])

# alpine rock (>= 950 m): bare rock. Scree, slabs, big boulders; no soil plants. (Row exists in DT_Biomes; the palette lives here so it is DATA like the others.)
# ground surfaces from the Electric Dreams scans (2026-10-12): the biomes' ground was a handful of brown / grey / pale materials
ROW_EDITS["RockyHighland"] = {"groundSurfaceType": surface("MossyRockyGround")}
ROW_EDITS["Meadow"] = {**ROW_EDITS["Meadow"], "groundSurfaceType": surface("MossyGrass")}
ROW_EDITS["DryScrub"] = {**ROW_EDITS["DryScrub"], "groundSurfaceType": surface("DriedGrassOnDampSoil")}
ALPINE["groundSurfaceType"] = surface("NordicMoss")   # tundra: moss over the gravel (was pale gravel: it read as desert)
# the sample's tundra rocky-ground slab for the heath (its moss-skinned boulder is part of mountain_rocks)
ALPINE["species"] = ALPINE["species"] + [ed_plate("TundraRockyGround", "SM_TundraRockyGround_01", 3.0)]

ALPINE_ROCK = {
    "displayName": "Alpine Rock",
    "heightMin": 95000.0, "heightMax": 100000000.0, "slopeMinDeg": 0.0, "slopeMaxDeg": 90.0, "moistureMin": 0.0, "moistureMax": 1.0, "temperatureMin": 0.0, "temperatureMax": 1.0,
    "priority": 22, "groundSurfaceType": surface("IcelandicQuarryRock"), "edgeBlendWidth": 5000.0,   # dark volcanic rock above the heath
    "species": mountain_rocks(0.5, 1.4, 1.2) + scree(95.0, 1.4, 3.2)
    + [boulder("forest", 8.0, 2000.0, 0.8, 2.2), boulder("beach_a", 5.0, 2600.0, 0.8, 2.0), ed_cliff("SM_HugeSandstoneCliff_04", 0.15)],
}

# --- ELECTRIC FOREST (2026-10-12, Wes: "try to make a 1:1 copy of the electric dreams demo as a biome") ---------------------------------------------------------------------------------------
# Composition read from the sample's own PCG graphs (Saved/EDDump/PCGDemo_Forest.txt via Tools/ed_pcg_dump.py): European hornbeam in every growth stage (Forest_01-10 canopy, Field_01-04 lone giants,
# Saplings, Seedlings), elderberry and fern understory, dead leaves, mossy forest boulders / rocks, forest roots, rock shelves and formations, standing dead trees. The sample's densities come from
# noise-driven stand masks (410 nodes); here the same cast is placed through the biome table with clump groups: group 6 = the stand (tall cores, young ring), group 7 = lone giants in clearings.
HB = GAME + "/Megascans/3D_Plants/EuropeanHornbeam/Geometry/SimpleWind/SM_EuropeanHornbeam_%s.SM_EuropeanHornbeam_%s"
ELD = GAME + "/Megascans/3D_Plants/Elderberry/SM_Elderberry_%s.SM_Elderberry_%s"
FERN = GAME + "/Megascans/3D_Plants/Fern/SM_Fern_%s.SM_Fern_%s"
LEAVES = GAME + "/Megascans/3D_Plants/DeadLeaves/SM_DeadLeaves_%s.SM_DeadLeaves_%s"


# The hornbeam set could not be COOKED at first (2026-10-12): M_Foliage took its shading model from MaterialAttributes, which trips the 5.8 shader-map consistency check (missing FAnisotropyPS,
# see fix_substrate_shading_model_pin.py). Content/Python/fix_hornbeam_masters.py splits it into a DefaultLit and a TwoSidedFoliage master (constant shading models) and sorts the instances,
# and a package passes with the hornbeams. False falls back to the BlackAlder shapes (same roles) if a re-migration ever brings the problem back before that script is re-run.
HORNBEAM_COOKS = True
HB_TO_BA = {"Forest_10": "Forest_09", "Sapling_07": "Sapling_03", "Sapling_08": "Sapling_04", "Seedling_03": "Seedling_01", "Seedling_04": "Seedling_02", "Seedling_05": "Seedling_02", "Field_04": "Field_05"}


def hb(name):
    if not HORNBEAM_COOKS:
        return ba(HB_TO_BA.get(name, name))
    return HB % (name, name)


def hb_tree(name, density, group=6, core=0.7, smin=0.85, smax=1.3, cluster=3000.0):
    return species(hb(name), density, res="BlackAlder", tag="MOResource_Tree", cluster=cluster, group=group, core=core, smin=smin, smax=smax)


def eld(n, density, group=6, core=0.3, cluster=2200.0):
    """Elderberry: a 1.5-3 m berry shrub you gather from and cannot walk through (playtest 2026-10-14 #21: "berry bushes have 0 collision, would like to
    harvest berries / interact"). A harvestable Bush resource (DT_Resources ElderberryBush: Gather Berries / Gather Twigs, kept on harvest), Solid:
    fix_mesh_collision.py gives it a core capsule, so the stems stop you and the outer twigs do not."""
    return species(ELD % (n, n), density, res="ElderberryBush", tag="MOResource_Bush", cluster=cluster, group=group, core=core, sweep=True, smin=0.8, smax=1.35,
                   collision="Solid")


def fern(n, density, cluster=1800.0, tag="MOBiomeCover_Shrub", smin=0.8, smax=1.4):
    """tag MOBiomeCover_Fern puts a fern in the NEAR ground-cover tier (any tag outside the spawner's large list: trees / rocks / bushes / shrubs): the large
    tier caps at ~1200 candidates/ha and a forest's trees, saplings and hazel already fill it, so floor ferns tagged as shrubs were realised at a fraction."""
    return undergrowth(FERN % (n, n), density, group=-1, core=0.0, smin=smin, smax=smax, cluster=cluster, tag=tag)


def leaves(n, density, cluster=1500.0, group=6, core=0.6):
    """Leaf litter lies UNDER trees: it shares the canopy's clump field (group 6 = the hornbeam stand) at its cores. Scattered on its own it made lone discs of
    leaves on open ground (playtest 2026-10-14 #17: "be sure the pcg is centered around one larger asset")."""
    return species(LEAVES % (n, n), density, tag="MOBiomeCover_Debris", cluster=cluster, group=group, core=core, align=True, tilt=0.0, sweep=True, smin=0.9, smax=1.6,
                   collision="Passable", sink=0.0)


ELECTRIC_FOREST = {
    "displayName": "Electric Forest",
    "heightMin": -100000.0, "heightMax": 30000.0, "slopeMinDeg": 0.0, "slopeMaxDeg": 32.0, "moistureMin": 0.42, "moistureMax": 0.66, "temperatureMin": 0.28, "temperatureMax": 0.62,
    "priority": 15, "groundSurfaceType": surface("ForestLitter"), "edgeBlendWidth": 7000.0,
    "reliefScale": 0.9, "mountainScale": 0.8, "footstepSet": "LeafCrunch",
    "species": [
        # the stand: weights follow the sample's (Forest_10 the commonest, Forest_07 the tall ones, 01-04 scattered tall trees)
        hb_tree("Forest_10", 34, core=0.60, smin=0.9, smax=1.3), hb_tree("Forest_09", 14, core=0.62), hb_tree("Forest_08", 10, core=0.64), hb_tree("Forest_07", 20, core=0.70),
        hb_tree("Forest_06", 8, core=0.66), hb_tree("Forest_05", 8, core=0.66), hb_tree("Forest_04", 12, core=0.74), hb_tree("Forest_03", 12, core=0.74),
        hb_tree("Forest_02", 12, core=0.76), hb_tree("Forest_01", 12, core=0.76),
        # lone giants in the clearings
        hb_tree("Field_01", 1.5, group=7, core=0.85, smin=0.9, smax=1.3), hb_tree("Field_02", 2.0, group=7, core=0.85, smin=0.9, smax=1.3),
        hb_tree("Field_03", 2.0, group=7, core=0.85, smin=0.9, smax=1.3), hb_tree("Field_04", 1.5, group=7, core=0.85, smin=0.9, smax=1.3),
        # young trees ring the stand (harvestable, solid), seedlings and elderberry fill the gaps (decorative, walk-through)
        hb_tree("Sapling_02", 22, core=0.46, smin=0.8, smax=1.2), hb_tree("Sapling_06", 16, core=0.46, smin=0.8, smax=1.2), hb_tree("Sapling_01", 12, core=0.44, smin=0.8, smax=1.2),
        hb_tree("Sapling_07", 10, core=0.44, smin=0.8, smax=1.2),
        undergrowth(hb("Seedling_01"), 52, group=6, core=0.32), undergrowth(hb("Seedling_03"), 36, group=6, core=0.32), undergrowth(hb("Seedling_02"), 30, group=6, core=0.30),
        undergrowth(hb("Seedling_04"), 24, group=6, core=0.30),
        eld("02", 28), eld("03", 30), eld("07", 17), eld("08", 17), eld("04", 14), eld("05", 11),
        fern("01", 190), fern("02", 110), fern("03", 90), fern("04", 60),
        # the floor
        leaves("01", 60), leaves("12", 30), leaves("05", 24), leaves("08", 20),
        cover(wild_grass("F"), 140, 900.0), cover(wild_grass("D"), 70, 1300.0),
        flower("11_02", 24, 1600.0), flower("19_03", 30, 1500.0),
        # mossy boulders and rocks, roots, rock shelves and formations (aligned + buried by the spawner)
        ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_03", 12.0, cluster=1800.0, smin=0.8, smax=1.8), ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_02", 8.0, cluster=1800.0, smin=0.8, smax=1.8),
        ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_06", 6.0, cluster=2000.0, smin=0.8, smax=1.8), ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_04", 2.0, cluster=3200.0, smin=0.6, smax=1.1),
        ed_rock("MossyForestRock", "SM_MossyForestRock_02", 10.0, cluster=2000.0, smin=0.8, smax=1.7), ed_rock("LichenedForestBoulder", "SM_LichenedForestBoulder_01", 6.0, cluster=1800.0, smin=0.9, smax=2.0),
        ed_plate("ForestRoots", "SM_ForestRoots_01", 6.0, cluster=2200.0, smin=0.9, smax=1.5, sink_cm=4.0), ed_plate("ForestRoots", "SM_ForestRoots_02", 5.0, cluster=2200.0, smin=0.9, smax=1.5, sink_cm=3.0),
        ed_plate("SmoothRockShelf", "SM_SmoothRockShelf_01", 3.0), ed_plate("MossyRocks", "SM_MossyRocks_01", 5.0), ed_plate("RockyForestPath", "SM_RockyForestPath_01", 2.0, sink_cm=6.0),
        ed_plate("ForestRockFormation", "SM_ForestRockFormation_05", 1.5, cluster=4000.0), ed_plate("ForestRockFormation", "SM_ForestRockFormation_06", 1.5, cluster=4000.0),
        # standing dead trees (decorative, but solid: a trunk you cannot walk through)
        species(ed("DeadTree", "SM_DeadTree_01"), 1.2, tag="MOBiomeCover_Shrub", cluster=3000.0, sweep=True, smin=0.9, smax=1.3, tilt=3.0, collision="Solid", sink_cm=35.0),   # root flare floated (#20)
        species(ed("DeadTree", "SM_DeadTree_02"), 1.2, tag="MOBiomeCover_Shrub", cluster=3000.0, sweep=True, smin=0.9, smax=1.3, tilt=3.0, collision="Solid", sink_cm=35.0),
    ],
}

# --- NORDIC STRAND (2026-10-12, Wes: "some very distinct biomes from megascans stuff like nordic beach") ----------------------------------------------------------------------------------------
# Cold shores (the sea-level band ShoreStrand holds, but cool): dark volcanic ground, big wet-black beach boulders, smooth rock shelves, a talus of stones, hardy dune grass, driftwood and a few rock
# pillars standing in the shallows' edge. Warm shores keep the sand (ShoreStrand, priority 13); this wins where it is cold (priority 14).
NORDIC_STRAND = {
    "displayName": "Nordic Strand",
    "heightMin": -100000.0, "heightMax": 450.0, "slopeMinDeg": 0.0, "slopeMaxDeg": 38.0, "moistureMin": 0.0, "moistureMax": 1.0, "temperatureMin": 0.0, "temperatureMax": 0.34,
    "priority": 14, "groundSurfaceType": surface("DarkPebbles"), "edgeBlendWidth": 4500.0, "reliefScale": 0.5, "mountainScale": 0.35, "footstepSet": "StoneStep",   # a stony shore: gentle relief (at 1.0 it grew 5-10 m smooth domes)
    "species": [
        ed_rock("NordicBeachRocks", "SM_NordicBeachRocks_01", 12.0, res="BeachBoulder", cluster=2200.0, smin=0.8, smax=1.9), ed_rock("NordicBeachRock", "SM_NordicBeachRock_01", 9.0, res="BeachBoulder", cluster=1800.0, smin=0.8, smax=1.8),
        ed_rock("MossyForestBoulder", "SM_MossyForestBoulder_01", 3.0, cluster=2600.0, smin=0.8, smax=1.6),
        boulder("beach_b", 14.0, 1500.0, 0.6, 1.5), boulder("beach_c", 10.0, 1800.0, 0.6, 1.4), boulder("beach_a", 5.0, 2600.0, 0.7, 1.7),
        ed_plate("SmoothRockShelf", "SM_SmoothRockShelf_01", 5.0), ed_plate("MossyRocks", "SM_MossyRocks_01", 3.0), ed_plate("ErodedRockyGround", "SM_ErodedRockyGround_01", 5.0),
        # the sample's Nordic coast set (migrated 2026-10-12): house-sized wet boulders, rocky-ground slabs, one coastal cliff in a few hectares
        ed_rock("NordicBoulder", "SM_NordicBoulder_01", 1.4, res="BeachBoulder", cluster=3600.0, smin=0.45, smax=0.9), ed_rock("NordicBoulder", "SM_NordicBoulder_02", 1.4, res="BeachBoulder", cluster=3600.0, smin=0.45, smax=0.9),
        ed_plate("NordicBeachRockyGround", "SM_NordicBeachRockyGround_01", 5.0), ed_plate("NordicBeachRockyGround", "SM_NordicBeachRockyGround_02", 7.0, collision="Passable"),   # 16 cm: stepped over
        ed_cliff("SM_MassiveNordicCoastalCliff_01", 0.18, cluster=7000.0, smin=0.45, smax=0.8, folder="MassiveNordicCoastalCliff"),
    ] + scree(48.0, 1.0, 2.4) + [
        cover(dune_grass("A"), 150, 1000.0, smin=0.8, smax=1.2), cover(dune_grass("C"), 90, 1400.0), cover(wild_grass("A"), 40, 1400.0, smin=0.7, smax=1.1),
        flower("10_01", 18, 2200.0, 1.0, 1.5), flower("20_02", 20, 2000.0, 1.0, 1.4),
        lib_rock("Stone_vd3edgg", "S_Stone_vd3edgg_lod3", 26.0, res="BeachBoulder", cluster=1400.0, smin=0.7, smax=1.8),
        drift("Broken_Tree_Bark_wjhiecy", "wjhiecy", 2.0, 3500.0, collision="Solid", smin=0.7, smax=1.2),
        drift("Bent_Branch_paErt", "paErt", 8.0, 2200.0), drift("Common_Spruce_Branch_uehjfcofa", "uehjfcofa", 5.0, 2200.0),
    ],
}

# --- grass tiers (2026-10-12, Wes: "thin out the grass quite a bit"; "a near grass and a far grass so the far grass can extend a lot further and hide the popping in and out") ---------------
# NEAR grass (tag MOBiomeCover_Grass, placed on the 64 m grid within ~130 m) is thinned to GRASS_THIN of what the palettes above author. The FAR tier (tag MOFar_Grass, placed on the 512 m grid out to
# ~1 km, never closer than the spawner's MinDistanceFromViewerCm) repeats every grass species of a biome as a sparse stand of larger tufts: coverage at distance for a fraction of the instances.
GRASS_THIN = 0.55
FAR_RATIO = 0.16     # far tufts per hectare as a fraction of the (thinned) near density of the same species
FAR_SCALE = 2.1      # far tufts are this much larger than the near ones (one big tuft stands in for several)


def far_grass(species_list):
    far = []
    for sp in species_list:
        if sp["HISMTag"] == "MOBiomeCover_Grass":
            f = dict(sp)
            f["HISMTag"] = "MOFar_Grass"
            f["DensityPerHectare"] = round(sp["DensityPerHectare"] * FAR_RATIO, 1)
            f["ClusterRadius"] = sp["ClusterRadius"] * 2.5
            f["MinScale"] = round(sp["MinScale"] * FAR_SCALE, 3)
            f["MaxScale"] = round(sp["MaxScale"] * FAR_SCALE, 3)
            f["bAutoSweepOnTerraform"] = True
            far.append(f)
    return far


# FAR TREES (2026-10-12, Wes: "a fuck ton of proper distance trees"): every tall tree of a biome also stands as a sparser, larger stand-in (tag MOFar_Tree) on the far-tree tier (1 km cells out to
# ~4 km, node chain add_far_tree_chain.py). Bigger crowns at a fraction of the density keep the canopy closed from a distance for a fraction of the instances. The stand-ins are decorative (no
# resource node, no collision) and are never drawn inside the node's MinDistanceFromViewerCm (descriptor InstanceMinDrawDistance), where the real trees stand.
FAR_TREE_RATIO = 0.55
FAR_TREE_SCALE = 1.1    # (was 1.5: the stand-ins read as GIANTS next to the real trees that replace them at the hand-off ring -- Wes saw them pop out and the small real ones pop in; the real
                        #  tier averages x1.05, so the stand-ins are only a touch bigger, to carry a closed canopy at 55 % of the density)
FAR_TREE_CAP = 100.0   # stand-ins per hectare at most (the far sampler offers ~112 candidate points / ha)

# The THIRD far tier (MOFar_Canopy, 3 km out to 9 km): solid low-poly CLUMP proxies (Content/Python/make_far_canopy.py). A real tree out there is 2-6 px of aliasing masked leaves; a clump is a soft dark mass.
# One clump stands for several trees: about 15 per hectare of forest (the 20 m lattice offers ~18), wider than a tree and low (see Tools/gen_far_canopy_meshes.py).
FAR_CANOPY_CAP = 15.0
FAR_CANOPY_MIN_TREES = 40.0     # tall trees per hectare a biome needs before it gets clumps
FAR_CANOPY_PER_TREE = 0.16     # clumps per tall-tree-per-hectare, until the cap (a 217 /ha temperate forest hits the cap; a sparse meadow grove stays sparse)
CANOPY_SMIN, CANOPY_SMAX = 1.0, 1.5
PROXY = GAME + "/Penumbra/FarCanopy/SM_FarCanopy_%s.SM_FarCanopy_%s"
PROXY_BY_TREE = {"Field_01": "Round", "Field_02": "Round", "Field_04": "Mid", "Forest_01": "Tall", "Forest_02": "Tall", "Forest_03": "Tall", "Forest_06": "Small", "Forest_07": "Small",
                 "Forest_08": "Small", "Forest_09": "Small"}   # BlackAlder sizes measured in the editor (Tools: Saved/Tmp/tree_bounds.py); anything else (hornbeams, hazel) -> Mid


def proxy_for(path):
    for key, name in PROXY_BY_TREE.items():
        if "BlackAlder_" + key + "_" in path:
            return name
    return "Mid"


def far_trees(species_list):
    far = []
    for sp in species_list:
        path = sp["Mesh"]
        if sp["HISMTag"] != "MOResource_Tree" or "Sapling" in path or "Seedling" in path or "wdvhberja" in path:   # (Black Locust is a knee-high plant: no stand-in)
            continue
        f = dict(sp)
        f["HISMTag"] = "MOFar_Tree"
        f["ResourceNodeId"] = "None"
        f["Collision"] = "Passable"
        f["bAutoSweepOnTerraform"] = True
        f["ClusterRadius"] = sp["ClusterRadius"] * 1.4
        f["MinScale"] = round(sp["MinScale"] * FAR_TREE_SCALE, 3)
        f["MaxScale"] = round(sp["MaxScale"] * FAR_TREE_SCALE, 3)
        f["DensityPerHectare"] = sp["DensityPerHectare"] * FAR_TREE_RATIO
        far.append(f)
    total = sum(f["DensityPerHectare"] for f in far)
    k = min(1.0, FAR_TREE_CAP / total) if total > 0 else 1.0
    for f in far:
        f["DensityPerHectare"] = round(f["DensityPerHectare"] * k, 2)
    return far


def far_canopy(species_list):
    """The MOFar_Canopy stand-ins of a biome: its tall trees' density, folded onto the four clump proxies by size."""
    per_proxy = {}
    for sp in species_list:
        path = sp["Mesh"]
        if sp["HISMTag"] != "MOResource_Tree" or "Sapling" in path or "Seedling" in path or "wdvhberja" in path:
            continue
        name = proxy_for(path)
        per_proxy[name] = per_proxy.get(name, 0.0) + sp["DensityPerHectare"] * FAR_CANOPY_PER_TREE
    total = sum(per_proxy.values())
    if total / FAR_CANOPY_PER_TREE < FAR_CANOPY_MIN_TREES:
        return []   # a sparse grove (meadow, heath, scree) is a handful of single trees: stand-in trees cover it, and a 20 m wide clump on an open plain reads as a boulder
    k = min(1.0, FAR_CANOPY_CAP / total) if total > 0 else 1.0
    out = []
    for name in sorted(per_proxy):
        d = round(per_proxy[name] * k, 2)
        if d < 0.3:
            continue
        out.append(species(PROXY % (name, name), d, res="None", tag="MOFar_Canopy", cluster=0.0, group=-1, align=False, tilt=0.0, sweep=True, smin=CANOPY_SMIN, smax=CANOPY_SMAX, collision="Passable"))
    return out


def thin_and_far(species_list):
    for sp in species_list:
        if sp["HISMTag"] == "MOBiomeCover_Grass":
            sp["DensityPerHectare"] = round(sp["DensityPerHectare"] * GRASS_THIN, 1)
    return species_list + far_grass(species_list) + far_trees(species_list) + far_canopy(species_list)


for _name in list(PALETTES):
    PALETTES[_name] = thin_and_far(PALETTES[_name])
ALPINE["species"] = thin_and_far(ALPINE["species"])
ALPINE_ROCK["species"] = thin_and_far(ALPINE_ROCK["species"])
# the shaded floor (#6): big fronds -- the near tier capped at ~2000 candidates/ha then (~500 ferns/ha realised; 6000/ha since Oct 14 eve), so cover comes from size, not count
RAINFOREST["species"] += [fern("01", 380, 1500.0, "MOBiomeCover_Fern", 1.3, 2.2), fern("02", 280, 1500.0, "MOBiomeCover_Fern", 1.3, 2.2),
                          fern("03", 200, 1800.0, "MOBiomeCover_Fern", 1.2, 2.0), fern("04", 140, 1800.0, "MOBiomeCover_Fern", 1.2, 2.0),
                          leaves("01", 70, group=4), leaves("05", 40, group=4), leaves("08", 30, group=4)]   # litter under the canopy stands (clump group 4)
RAINFOREST["species"] = thin_and_far(RAINFOREST["species"])


def merge_same_mesh(species_list):
    """Entries with the same mesh, tag and clump group become one (densities add): the hornbeam -> BlackAlder fallback maps several hornbeam shapes onto one BlackAlder mesh."""
    merged = {}
    for sp in species_list:
        key = (sp["Mesh"], sp["HISMTag"], sp["ClusterGroup"])
        if key in merged:
            merged[key]["DensityPerHectare"] = round(merged[key]["DensityPerHectare"] + sp["DensityPerHectare"], 2)
        else:
            merged[key] = dict(sp)
    return list(merged.values())


ELECTRIC_FOREST["species"] = thin_and_far(merge_same_mesh(ELECTRIC_FOREST["species"]))
NORDIC_STRAND["species"] = thin_and_far(NORDIC_STRAND["species"])


# --- COVERAGE (2026-10-14 eve, Wes: "more ground coverage in general too, more trees, more bushes") ------------------------------------------------------------------------------------------
# Per-biome, per-tier factors on top of everything above (and wider tree clumps), in a plan file so the numbers stay reviewable: Docs/Archive/Ground_Coverage_2026-10-14.md has the census and the
# model behind them. The factors are applied by Tools/pcg_stocking.py's apply_plan -- the same rule its model checks for clipping -- so this script and the live table cannot drift apart.
COVERAGE_PLANS = ["D:/UEProjects/MO57/Docs/Ground_Coverage_2026-10-14.plan.json"]


def apply_coverage_plans(out):
    import importlib.util
    spec = importlib.util.spec_from_file_location("pcg_stocking", "D:/UEProjects/MO57/Tools/pcg_stocking.py")
    stocking = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(stocking)
    for path in COVERAGE_PLANS:
        with open(path, encoding="utf-8") as f:
            plan = json.load(f)
        stocking.apply_plan([{"Name": name, "Species": row["species"]} for name, row in out.items() if "species" in row], plan)   # in place


def write_plan():
    out = {name: {"species": sp} for name, sp in PALETTES.items()}
    for name, edit in ROW_EDITS.items():
        out.setdefault(name, {}).update(edit)
    out["AlpineHeath"] = ALPINE
    out["AlpineRock"] = ALPINE_ROCK
    out["Rainforest"] = RAINFOREST
    out["ElectricForest"] = ELECTRIC_FOREST
    out["NordicStrand"] = NORDIC_STRAND
    apply_coverage_plans(out)
    with open("D:/UEProjects/MO57/Saved/Logs/biome_palettes.json", "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
    for name, sp in list(PALETTES.items()) + [("AlpineHeath", ALPINE["species"]), ("AlpineRock", ALPINE_ROCK["species"]), ("Rainforest", RAINFOREST["species"]), ("ElectricForest", ELECTRIC_FOREST["species"]), ("NordicStrand", NORDIC_STRAND["species"])]:
        total = sum(s["DensityPerHectare"] for s in sp)
        trees = sum(s["DensityPerHectare"] for s in sp if s["HISMTag"] == "MOResource_Tree")
        print("PALETTE %-16s %2d species, %5.0f instances/ha (%4.0f trees)" % (name, len(sp), total, trees))


if __name__ == "__main__" or True:
    write_plan()
