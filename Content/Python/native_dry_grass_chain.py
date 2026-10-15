"""A dense DRY bunchgrass layer for the biomes off the lawn chain (Oct 14 2026, Wes: "more ground coverage in general").

The scatter graph's native lawn chain (VoxelSampler_4 -> MO Elevation Bands_3 -> ... -> StaticMeshSpawner_2/4) is what covers open ground
in the lawn biomes: ~1 point/m2, tufts scaled 2-7x wide (cover from size, not count). The biomes OFF it (native_grass_biomes.py: dry scrub,
rocky highland, nordic strand) only had their palette's ground tier -- after the Oct 14 raise ~0.3 tufts/m2 of small dry tufts on a
straw-coloured ground texture, which measured right (MO.PCG.Census: 3,100 dry tufts/ha at the DryScrub shot spot) and still read as bare.
Real semi-arid scrub has a bunchgrass every 0.5-1 m. This branches the SAME sampler (no extra sampling cost) into:

    VoxelSampler_4 -> MO Elevation Bands (AllowedBiomeIds = DRY_BIOMES, same bands/water/cave rules as the lawn) -> MO Terrain Mod Filter
                   -> Random Choice (KEEP) -> Transform Points (dry tufts 1.5-3.5x wide) -> Static Mesh Spawner (smdrailr B/D/E/F, no shadow,
                      culled 150-200 m like the lawn)

Idempotent: finds its own Elevation Bands node by the biome list and re-applies every setting. Never deletes a node (editor Python
deletion crashed the editor on Oct 13).

    python Tools/ue.py py --file Content/Python/native_dry_grass_chain.py      (editor open, not in PIE)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
DRY_BIOMES = ["DryScrub", "RockyHighland", "NordicStrand"]
KEEP = 0.8                        # of the shared 1/m2 sampler's points in the band: ~0.8 tufts/m2, on top of the palettes' own
SCALE_MIN = unreal.Vector(1.5, 1.5, 1.4)    # steppe bunchgrass stands 40-80 cm: taller than the 37 cm scan
SCALE_MAX = unreal.Vector(3.5, 3.5, 2.2)
GRASS = "/Game/Fab/Megascans/Plants/Grass_smdrailr/Medium/smdrailr_tier_2/StaticMeshes/SM_smdrailr_Var%s.SM_smdrailr_Var%s"
MESHES = [("B", 3), ("D", 2), ("E", 1), ("F", 2)]   # the dry, straw-coloured variants these palettes already use
log = lambda m: unreal.log("DRYGRASS " + m)

g = unreal.load_asset(GRAPH)
nodes = {n.get_name(): n for n in g.get_editor_property("nodes")}
sampler, lawn_bands, lawn_filter, lawn_xf, lawn_spawn = (nodes[k] for k in ("VoxelSampler_4", "MO Elevation Bands_3", "MO Terrain Mod Filter_1",
                                                                             "TransformPoints_2", "StaticMeshSpawner_2"))


def downstream(node, label=None):
    out = []
    for p in node.get_editor_property("output_pins"):
        if label and str(p.get_editor_property("properties").get_editor_property("label")) != label:
            continue
        for e in p.get_editor_property("edges"):
            out.append(e.get_editor_property("output_pin").get_outer())
    return out


def copy_props(dst, src):
    for a in dir(src):
        if a.startswith("_") or a in dir(unreal.PCGSettings):
            continue
        try:
            v = getattr(src, a)
        except Exception:
            continue
        if callable(v):
            continue
        try:
            setattr(dst, a, v)
        except Exception:
            pass


def add(cls):
    return g.add_node_of_type(cls)[0]


# ---- find or build the chain ------------------------------------------------------------------------------------------------------------
bands = next((n for n in downstream(sampler) if n.get_settings().get_class().get_name() == "MOPCGElevationBandsSettings"
              and [str(x) for x in n.get_settings().get_editor_property("allowed_biome_ids")] == DRY_BIOMES), None)
if bands is None:
    bands = add(unreal.MOPCGElevationBandsSettings)
    copy_props(bands.get_settings(), lawn_bands.get_settings())
    sampler.add_edge_to("Out", bands, "In")
    filt = add(unreal.MOPCGTerrainModificationFilterSettings)
    copy_props(filt.get_settings(), lawn_filter.get_settings())
    bands.add_edge_to("Band2", filt, "In")
    choice = add(unreal.PCGRandomChoiceSettings)
    filt.add_edge_to("Out", choice, "In")
    xf = add(unreal.PCGTransformPointsSettings)
    copy_props(xf.get_settings(), lawn_xf.get_settings())
    choice.add_edge_to("Chosen", xf, "In")
    spawn = add(unreal.PCGStaticMeshSpawnerSettings)
    copy_props(spawn.get_settings(), lawn_spawn.get_settings())
    xf.add_edge_to("Out", spawn, "In")
    log("built: %s -> %s -> %s -> %s -> %s" % (bands.get_name(), filt.get_name(), choice.get_name(), xf.get_name(), spawn.get_name()))
else:
    filt = downstream(bands, "Band2")[0]
    choice = downstream(filt)[0]
    xf = downstream(choice, "Chosen")[0]
    spawn = downstream(xf)[0]
    log("found: %s -> %s -> %s -> %s -> %s" % (bands.get_name(), filt.get_name(), choice.get_name(), xf.get_name(), spawn.get_name()))

# ---- settings (re-applied every run) -----------------------------------------------------------------------------------------------------
bands.get_settings().set_editor_property("allowed_biome_ids", [unreal.Name(b) for b in DRY_BIOMES])
cs = choice.get_settings()
cs.set_editor_property("fixed_mode", False)
cs.set_editor_property("ratio", KEEP)
cs.set_editor_property("output_discarded_entries", False)
xs = xf.get_settings()
xs.set_editor_property("scale_min", SCALE_MIN)
xs.set_editor_property("scale_max", SCALE_MAX)
sel = spawn.get_settings().get_editor_property("mesh_selector_parameters")
template = lawn_spawn.get_settings().get_editor_property("mesh_selector_parameters").get_editor_property("mesh_entries")[0]
entries = []
for var, weight in MESHES:
    e = unreal.PCGMeshSelectorWeightedEntry()
    d = template.get_editor_property("descriptor")
    d.set_editor_property("static_mesh", unreal.load_asset(GRASS % (var, var)))
    d.set_editor_property("cast_shadow", False)
    e.set_editor_property("descriptor", d)
    e.set_editor_property("weight", weight)
    entries.append(e)
sel.set_editor_property("mesh_entries", entries)
log("biomes=%s keep=%.2f scale %s..%s meshes=%s" % (DRY_BIOMES, KEEP, SCALE_MIN, SCALE_MAX,
    [(e.get_editor_property("descriptor").get_editor_property("static_mesh").get_name(), e.get_editor_property("weight")) for e in sel.get_editor_property("mesh_entries")]))
log("saved=%s" % unreal.EditorAssetLibrary.save_loaded_asset(g, False))
