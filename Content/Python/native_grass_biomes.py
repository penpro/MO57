"""Which biomes get the scatter graph's dense NATIVE grass chain (the smdrailr / vlkhcbxia lawn grass at ~1.4 points/m2, gated by `MO Elevation Bands_3`).

The biome spawner's near ground-cover tier caps at ~2000 candidates per hectare, so a palette of tufts cannot COVER open ground; the native chain does.
Whether a biome gets it is a statement about its floor:
  - YES: the open / lightly wooded biomes -- TemperateForest, Meadow, LocustWoodland, ShoreStrand, and AlpineHeath (playtest 2026-10-14 #25
    "mountainside is VERY sparse": the heath had been taken off on Oct 12 when mountains read "grass only"; it now has its own ED rocks and moss ground).
  - NO: Rainforest (playtest 2026-10-14 #6 "dense forest, grass still brightly lit": a closed canopy shades a lawn out, and that grass glowed under it;
    the floor is litter, ferns and seedlings from the palette). Also the heaviest layer in the biome where the frame rate halved (#5).
  - NO: DryScrub, RockyHighland, AlpineRock, ElectricForest, NordicStrand (their own palettes carry their ground).

    python Tools/ue.py py --file Content/Python/native_grass_biomes.py      (editor open, not in PIE; sets the list exactly, idempotent)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
NODE = "MO Elevation Bands_3"
BIOMES = ["TemperateForest", "Meadow", "LocustWoodland", "ShoreStrand", "AlpineHeath"]

g = unreal.load_asset(GRAPH)
s = {n.get_name(): n for n in g.get_editor_property("nodes")}[NODE].get_settings()
before = [str(x) for x in s.get_editor_property("allowed_biome_ids")]
s.set_editor_property("allowed_biome_ids", [unreal.Name(x) for x in BIOMES])
print("NATIVEGRASS %s: %s -> %s, saved=%s" % (NODE, before, BIOMES, unreal.EditorAssetLibrary.save_loaded_asset(g)))
