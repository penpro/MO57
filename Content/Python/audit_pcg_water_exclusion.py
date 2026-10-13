"""H08: does every planting path of the scatter graph exclude water? For each enabled SPAWNER node (native Static Mesh Spawner, MO Biome Spawner, MO item spawner),
walk UPSTREAM and report the MO Elevation Bands nodes on the way (bExcludeWater / bExcludeCaves / WaterBankRadii). A spawner with no water-excluding Elevation
Bands upstream plants in water unless it filters by itself (the MO Biome Spawner does: its own FMOPCGWaterMask).

    python Tools/ue.py py --file Content/Python/audit_pcg_water_exclusion.py

Also prints the upland lake class's quad-tree tile size (the drawn lake plane is whole tiles around a snapped centre; the generator buries PlaneHalfSizeCm + 3 m).
Lines start with PCGWATER.
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
LAKE_CLASS = "/Oceanology_Plugin/Design/Lake/Blueprints/Lake/BP_OceanologyLake.BP_OceanologyLake_C"
log = lambda m: unreal.log("PCGWATER " + m)

g = unreal.load_asset(GRAPH)
nodes = list(g.get_editor_property("nodes"))


def cls(n):
    s = n.get_settings()
    return s.get_class().get_name() if s else "?"


def enabled(n):
    s = n.get_settings()
    try:
        return bool(s.get_editor_property("enabled")) if s else True
    except Exception:
        return True


# reverse edges: node -> upstream nodes
ups = {}
for n in nodes:
    for p in n.get_editor_property("output_pins"):
        for e in p.get_editor_property("edges"):
            m = e.get_editor_property("output_pin").get_editor_property("node")
            ups.setdefault(m.get_name(), []).append(n)

SPAWNERS = ("PCGStaticMeshSpawnerSettings", "PCGSpawnActorSettings", "MOPCGBiomeSpawnerSettings", "MOPCGMeshSpawnerSettings", "MOPCGItemSpawnerSettings",
            "MOPCGResourceSpawnerSettings", "MOPCGTreeSpawnerSettings", "MOPCGBushSpawnerSettings", "MOPCGRockSpawnerSettings", "MOPCGSpawnPointSettings")
bad = 0
for n in nodes:
    c = cls(n)
    if c not in SPAWNERS or not enabled(n):
        continue
    seen, queue, bands = {n.get_name()}, [n], []
    while queue:
        cur = queue.pop(0)
        for u in ups.get(cur.get_name(), []):
            if u.get_name() in seen:
                continue
            seen.add(u.get_name())
            if cls(u) == "MOPCGElevationBandsSettings" and enabled(u):
                s = u.get_settings()
                def prop(k):
                    try:
                        return s.get_editor_property(k)
                    except Exception:
                        return "?"
                bands.append("%s(water=%s caves=%s bank=%s)" % (u.get_name(), prop("exclude_water"), prop("exclude_caves"), prop("water_bank_radii")))
            queue.append(u)
    excludes = any("water=True" in b for b in bands)
    self_filter = c == "MOPCGBiomeSpawnerSettings"
    if not excludes and not self_filter:
        bad += 1
    log("%-28s %-30s upstream bands: %s%s" % (n.get_name(), c, ", ".join(bands) if bands else "NONE",
                                                "" if (excludes or self_filter) else "   <-- NO WATER EXCLUSION"))
log("%d enabled spawner(s) without water exclusion" % bad)

lake = unreal.load_class(None, LAKE_CLASS)
if lake:
    cdo = unreal.get_default_object(lake)
    try:
        qt = cdo.get_editor_property("quad_tree_settings")
        log("lake quad tree: tile_size=%s extent_in_tiles=%s" % (qt.get_editor_property("tile_size"), qt.get_editor_property("extent_in_tiles")))
    except Exception as ex:
        log("lake quad tree settings unreadable: %s" % ex)
else:
    log("lake class not found: " + LAKE_CLASS)
