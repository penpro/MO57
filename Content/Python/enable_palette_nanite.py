"""Turn Nanite on for every mesh the biome palettes scatter (DT_Biomes species).

Why (playtest 2026-10-14 #24, Wes: "these little dry grass assets are LOD switching which means they're not nanite enabled"): the Fab tier-2 plants
(grass smdrailr / vlkhcbxia, dead shrubs vdknafgha, black locust wdvhberja, raspberry wf0oefeja), the PN meadow flowers and a few lod3 rock scans came in
with classic LOD chains while every other palette mesh is Nanite. Their LOD steps pop in view; the rest of the scatter does not.

Foliage keeps its leaf cards: shape preservation PreserveArea (Nanite would otherwise thin a masked plant to nothing at a distance), the same as the
palette's Nanite foliage. Rocks get the defaults.

Run:   python Tools/ue.py py --file Content/Python/enable_palette_nanite.py         (dry run: lists what would change)
       NANITE_APPLY = True below (or set it in a -c prelude) to change + save.
Idempotent: meshes already on Nanite are left alone.
"""
import re
import unreal

NANITE_APPLY = globals().get("NANITE_APPLY", False)
DT_PATH = "/MOFramework/Data/DT_Biomes.DT_Biomes"
FOLIAGE_HINTS = ("Plants", "Plant", "flower", "Grass", "Shrub", "Fern", "Seedling", "Sapling")


def palette_meshes():
    dt = unreal.load_asset(DT_PATH)
    col = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, "Species")
    paths = set()
    for cell in col:
        for m in re.finditer(r'Mesh="?([^",)]+)"?', cell):
            p = m.group(1).strip("'")
            if p.startswith("/"):
                paths.add(p)
    return sorted(paths)


def is_foliage(path):
    return any(h.lower() in path.lower() for h in FOLIAGE_HINTS)


def main():
    changed, already, missing = [], 0, []
    for path in palette_meshes():
        sm = unreal.load_asset(path)
        if not isinstance(sm, unreal.StaticMesh):
            missing.append(path)
            continue
        ns = sm.get_editor_property("nanite_settings")
        if ns.get_editor_property("enabled"):
            already += 1
            continue
        foliage = is_foliage(path)
        changed.append((path, foliage, sm.get_num_lods()))
        if not NANITE_APPLY:
            continue
        ns.set_editor_property("enabled", True)
        if foliage:
            try:
                ns.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.PRESERVE_AREA)
            except Exception:
                ns.set_editor_property("preserve_area", True)   # pre-5.5 name
        sm.set_editor_property("nanite_settings", ns)
        unreal.EditorAssetLibrary.save_asset(path.split(".")[0], only_if_is_dirty=False)
    for path, foliage, lods in changed:
        print("NANITE %s %s (%d classic LODs)%s" % ("ON " if NANITE_APPLY else "OFF", path, lods, "  foliage: preserve area" if foliage else ""))
    print("NANITE summary: %d %s, %d already Nanite, %d not static meshes %s" % (
        len(changed), "turned on" if NANITE_APPLY else "would be turned on", already, len(missing), missing[:5]))


main()
