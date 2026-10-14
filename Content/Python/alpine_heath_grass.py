"""AlpineHeath gets the dense native grass again (playtest 2026-10-14 #25, Wes: "mountainside is VERY sparse").

The biome spawner's near ground-cover tier is capped at ~2000 candidate points per hectare (one Voxel Sampler point per cell), so a palette of
grass tufts can never COVER a hillside: at the report spot the heath realised ~510 instances/ha of 30-80 cm plants -- a bare slope with a
few rocks. Lush ground in the lowland biomes comes from the scatter graph's native grass chain (the smdrailr / vlkhcbxia grasses, the SAME
family AlpineHeath's palette uses), gated per biome by `MO Elevation Bands_3`. AlpineHeath was taken off that gate on Oct 12 when mountains
read as "grass only"; since then they carry their own ED rocks and moss ground, so the grass goes back under them.

    python Tools/ue.py py --file Content/Python/alpine_heath_grass.py      (editor open, not in PIE; idempotent)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
NODE = "MO Elevation Bands_3"
ADD = ("AlpineHeath",)

g = unreal.load_asset(GRAPH)
s = {n.get_name(): n for n in g.get_editor_property("nodes")}[NODE].get_settings()
ids = [str(x) for x in s.get_editor_property("allowed_biome_ids")]
for b in ADD:
    if b not in ids:
        ids.append(b)
s.set_editor_property("allowed_biome_ids", [unreal.Name(x) for x in ids])
print("HEATHGRASS %s allowed = %s, saved=%s" % (NODE, ids, unreal.EditorAssetLibrary.save_loaded_asset(g)))
