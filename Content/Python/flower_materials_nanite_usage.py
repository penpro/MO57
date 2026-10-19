"""Give the PN meadow flowers' material(s) the Nanite usage flag.

Why (playtest 2026-10-19): enable_palette_nanite.py (Oct 14) turned Nanite ON for the PN meadow flower MESHES, but their material was never flagged
"Used with Nanite". The editor sets a missing usage flag on the fly (and nags "will recompile every editor launch until resaved"); a -game process
and a cooked build cannot, and the engine says so: "Material .../flower_15_03_Mat missing usage flag Nanite! Default Material will be used in game."
-- every meadow flower drew with the engine's default grey material in game (24 distinct flower materials in the instance logs, 80+ times each).

The flag lives on the BASE material (a material instance has no usage flags of its own), so this sets it on each distinct base of the flower
materials and saves it. Idempotent: a base already flagged is left alone.

Run:   python Tools/ue.py py --file Content/Python/flower_materials_nanite_usage.py              (dry run: lists the bases and their flag)
       FLOWER_APPLY = True in a -c prelude to change + save.
"""
import unreal

FLOWER_APPLY = globals().get("FLOWER_APPLY", False)
FOLDER = "/Game/PN_MeadowFlowers/Materials"


def base_of(asset):
    if isinstance(asset, unreal.MaterialInstance):
        return asset.get_base_material()
    if isinstance(asset, unreal.Material):
        return asset
    return None


def main():
    bases = {}
    for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        base = base_of(asset)
        if base is not None:
            bases.setdefault(base.get_path_name(), [base, 0])[1] += 1
    for path, (base, users) in sorted(bases.items()):
        flagged = base.get_editor_property("used_with_nanite")
        print(f"FLOWERMAT base {path} used_with_nanite={flagged} materials={users}")
        if flagged or not FLOWER_APPLY:
            continue
        base.set_editor_property("used_with_nanite", True)
        unreal.MaterialEditingLibrary.recompile_material(base)
        saved = unreal.EditorAssetLibrary.save_asset(path.split(".")[0], only_if_is_dirty=False)
        print(f"FLOWERMAT set used_with_nanite on {path}; saved={saved}")


main()
