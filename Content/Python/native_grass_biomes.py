"""Which biomes get the scatter graph's dense NATIVE grass chain (the smdrailr / vlkhcbxia lawn grass at ~1.4 points/m2, gated by `MO Elevation Bands_3`).

The biome spawner's near ground-cover tier capped at ~2000 candidates per hectare, so a palette of tufts could not COVER open ground; the native chain does.
(Since 2026-10-14 eve the tier takes 6000/ha and the biomes below that are OFF this chain carry their own grass at 1500-2800/ha: Docs/Ground_Coverage_2026-10-14.md.)
Whether a biome gets it is a statement about its floor:
  - YES: the open / lightly wooded biomes -- TemperateForest, Meadow, LocustWoodland, ShoreStrand, and AlpineHeath (playtest 2026-10-14 #25
    "mountainside is VERY sparse": the heath had been taken off on Oct 12 when mountains read "grass only"; it now has its own ED rocks and moss ground).
  - NO: Rainforest (playtest 2026-10-14 #6 "dense forest, grass still brightly lit": a closed canopy shades a lawn out, and that grass glowed under it;
    the floor is litter, ferns and seedlings from the palette). Also the heaviest layer in the biome where the frame rate halved (#5).
  - NO: DryScrub, RockyHighland, AlpineRock, ElectricForest, NordicStrand (their own palettes carry their ground; DryScrub, RockyHighland
    and NordicStrand also get the DRY bunchgrass branch of the same sampler, native_dry_grass_chain.py, since Oct 14 2026).

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
