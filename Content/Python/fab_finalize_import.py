"""Finish a Fab (Megascans) "Add to Project" import so the asset renders in game and is on disk.

    python Tools/ue.py py --file Content/Python/fab_finalize_import.py        (editor open, PIE NOT running: saving is blocked in PIE)

Why (2026-10-14): the Fab plugin imports into /Game/Fab/Megascans/3D/<Name>_<id> and parents every Material Instance on its own copy
of the Megascans masters (/Game/Fab/Materials/Standard|VT/M_MS_*). Those masters lack the Nanite and instanced-static-mesh usage
flags, so at runtime the instances silently fall back to the default gray material (the same bug fixed for the first packs on
2026-10-04 by the project-local masters in /Game/Materials/FabOverrides). The plugin also leaves the imported packages dirty: only
an autosave reaches disk. This script:
  1. reparents every MI under /Game/Fab/Megascans whose parent is a /Game/Fab/Materials master onto the FabOverrides master of the
     same name (same parameter names, so the instance keeps its textures); a master with no override is reported (MSFAB MISSING);
  2. saves the dirty packages an import creates: /Game/Fab/Megascans and the plugin's shared folders (Materials, MaterialFunctions,
     MaterialParameterCollection, Textures). Other packs under /Game/Fab are left alone (a resave of an unrelated pack is noise).
Idempotent. Output lines start with MSFAB.
"""
import unreal

ROOT = "/Game/Fab/Megascans"
FAB_MASTERS = ("/Game/Fab/Materials/Standard/", "/Game/Fab/Materials/VT/")
OVERRIDES = "/Game/Materials/FabOverrides/"
SAVE_ROOTS = ("/Game/Fab/Megascans/", "/Game/Fab/Materials/", "/Game/Fab/MaterialFunctions/", "/Game/Fab/MaterialParameterCollection/",
              "/Game/Fab/Textures/")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("MSFAB " + msg)
    out(msg) if "out" in globals() else None


def override_for(parent_path):
    for prefix in FAB_MASTERS:
        if parent_path.startswith(prefix):
            name = parent_path[len(prefix):].split(".")[0]
            name = name[:-3] if name.endswith("_VT") else name   # VT masters map onto the same override (textures are not VT here)
            candidate = OVERRIDES + name
            return candidate if eal.does_asset_exist(candidate) else ""
    return None   # not a Fab master: leave alone


def main():
    reparented = missing = 0
    for path in eal.list_assets(ROOT, recursive=True, include_folder=False):
        if eal.find_asset_data(path).asset_class_path.asset_name != "MaterialInstanceConstant":
            continue
        mi = unreal.load_asset(path)
        parent = mi.get_editor_property("parent")
        if not parent:
            continue
        target = override_for(parent.get_path_name())
        if target is None:
            continue
        if target == "":
            missing += 1
            log("MISSING override for %s (MI %s): add one to %s with Nanite + ISM usage" % (parent.get_path_name(), path, OVERRIDES))
            continue
        mi.set_editor_property("parent", unreal.load_asset(target))
        mel.update_material_instance(mi)
        reparented += 1
        log("reparented %s -> %s" % (path.split(".")[0], target))
    dirty = [p for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name().startswith(SAVE_ROOTS)]
    if dirty:
        unreal.EditorLoadingAndSavingUtils.save_packages(dirty, True)
    log("done: %d reparented, %d without an override, %d packages saved" % (reparented, missing, len(dirty)))


main()
