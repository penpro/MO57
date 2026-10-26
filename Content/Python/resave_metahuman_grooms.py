"""Resave the imported MetaHuman groom library onto the project's paths (step 2 of the import; step 1 is Tools/import_metahuman_grooms.py).

The copied packages name their engine-plugin paths inside (/MetaHumanCharacter/Optional/Grooms/..., /MetaHumanCharacter/Materials/MI_Hair...);
with the TEMPORARY [CoreRedirects] PackageRedirects in Config/DefaultEngine.ini those load as the project's mirror (/Game/MetaHumans/Common/...),
and saving writes the redirected names. Afterwards every package's dependencies must be project or engine paths: the script checks that in the
asset registry and prints each one still naming /MetaHumanCharacter/ (none -> remove the redirect lines and restart the editor).

    python Tools/ue.py py --file Content/Python/resave_metahuman_grooms.py --timeout 3600        (editor open, NOT in PIE: it saves)

Loading a groom builds its strands / cards data for this engine (minutes for the long styles, then it is in the DDC). Idempotent. MHRESAVE lines.
"""
import unreal

import os

ROOTS = ["/Game/MetaHumans/Common/Optional/Grooms/GroomAssets/" + c for c in ("Hair", "Beards", "Mustaches")] + \
        ["/Game/MetaHumans/Common/Optional/Grooms/Thumbnails/" + c for c in ("Hair", "Beards", "Mustaches")] + \
        ["/Game/MetaHumans/Common/Optional/Grooms/GroomMesh"]
ONLY = os.environ.pop("MHRESAVE_ONLY", "")   # e.g. /Game/MetaHumans/Common/Optional/Grooms/GroomMesh: resave just that folder
ROOTS = [r for r in ROOTS if r.startswith(ONLY)]
reg = unreal.AssetRegistryHelpers.get_asset_registry()
eal = unreal.EditorAssetLibrary
log = lambda m: out("MHRESAVE " + m)

packages = []
for root in ROOTS:
    for ad in reg.get_assets_by_path(root, recursive=True):
        packages.append(str(ad.package_name))
packages = sorted(set(packages))
# Textures and meshes first, the grooms (which load them) last.
grooms = [p for p in packages if p.split("/")[-1] == p.split("/")[-2]]
others = [p for p in packages if p not in grooms]
FALLBACK_MATERIAL = "/Game/MetaHumans/Common/Materials/MI_Facial_Hair"   # for a group whose plugin material has no project copy (MI_PeachFuzz)
saved, failed = 0, []
for p in others + grooms:
    asset = eal.load_asset(p)
    if isinstance(asset, unreal.GroomAsset):
        slots = list(asset.get_editor_property("hair_groups_materials"))
        if any(m.get_editor_property("material") is None for m in slots):
            # Fresh structs: an element edited in place does not stick.
            fixed = []
            for m in slots:
                s = unreal.HairGroupsMaterial()
                s.set_editor_property("slot_name", m.get_editor_property("slot_name"))
                s.set_editor_property("material", m.get_editor_property("material") or unreal.load_asset(FALLBACK_MATERIAL))
                fixed.append(s)
            asset.modify()
            asset.set_editor_property("hair_groups_materials", fixed)
            log("groom %s: empty material slot(s) -> %s" % (p.split("/")[-1], FALLBACK_MATERIAL.split("/")[-1]))
    if asset is None or not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        failed.append(p)
        continue
    saved += 1
    if p in grooms:
        log("groom %s saved" % p.split("/")[-1])
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
stale = []
for p in packages:
    for d in reg.get_dependencies(p, opts) or []:
        if str(d).startswith("/MetaHumanCharacter/"):
            stale.append("%s -> %s" % (p, d))
for s in stale[:40]:
    log("STILL NAMES " + s)
log("done: %d package(s), %d saved, %d failed %s, %d grooms, %d dependency(ies) still on /MetaHumanCharacter/" % (
    len(packages), saved, len(failed), failed[:5], len(grooms), len(stale)))
