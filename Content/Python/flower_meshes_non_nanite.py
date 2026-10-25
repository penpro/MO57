"""The PN meadow flowers render WITHOUT Nanite, their LODs cross-fading (BR23-10; Wes, Oct 23-24 2026: "a few of these little flowers are
blurry", "these little purple flowers look good here ... but they're weird and blurry in other places").

Why: enable_palette_nanite.py (Oct 14, for the palette foliage's LOD pops) turned Nanite on for the 24 PN flower meshes the biome palettes plant;
the native scatter's two lupine meshes stayed classic. Measured in PIE (BR23-10 captures, D:/MOAssets/previews/grass/): under Nanite the flower
material MA_Flowers writes wrong motion vectors -- its Pivot Painter / player-bend wind (PN_AnimationShader) -- so whatever reads velocity smears
the flowers: post-process motion blur streaks them even with the camera still, TSR ghosts them. With no AA and no motion blur the same Nanite
flowers are sharp; as classic meshes they are sharp under TSR and motion blur (the native lupines Wes found "good"). So: every static mesh whose
material is (an instance of) MA_Flowers goes back to classic rendering, and MA_Flowers gets Dithered LOD Transition so the LOD steps that made
Nanite attractive cross-fade instead of popping.

    python Tools/ue.py py --file Content/Python/flower_meshes_non_nanite.py        (editor open, NOT in PIE: it saves)
    FLOWER_NANITE_NOSAVE=1 -> change in memory only (a PIE A/B).
Idempotent. Test MOFramework.Rendering.FlowersNotNanite checks it; enable_palette_nanite.py leaves MA_Flowers meshes alone.
"""
import os

import unreal

MASTER = "/Game/PN_MeadowFlowers/Materials/flowerMaterials/MA_Flowers"
NOSAVE = os.environ.pop("FLOWER_NANITE_NOSAVE", "") == "1"   # consumed: the editor's environment outlives the run
log = lambda m: out("FLOWERNANITE " + m)

sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
eal = unreal.EditorAssetLibrary
master = unreal.load_asset(MASTER)
if not master.get_editor_property("dithered_lod_transition"):
    master.set_editor_property("dithered_lod_transition", True)
    unreal.MaterialEditingLibrary.recompile_material(master)
    log("MA_Flowers: Dithered LOD Transition on")
if not NOSAVE:
    eal.save_loaded_asset(master, only_if_is_dirty=True)   # (also a change made by an earlier in-memory run)

reg = unreal.AssetRegistryHelpers.get_asset_registry()
flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "StaticMesh")], package_paths=["/Game"], recursive_paths=True)
changed, checked = 0, 0
for ad in reg.get_assets(flt):
    if not str(ad.package_name).startswith("/Game/PN_MeadowFlowers"):
        continue   # the pack's own meshes (MA_Flowers is the pack's master)
    m = ad.get_asset()
    mats = [m.get_material(i) for i in range(len(m.get_editor_property("static_materials")))]
    if not any(x and x.get_base_material() == master for x in mats):
        continue
    checked += 1
    ns = m.get_editor_property("nanite_settings")
    if ns.get_editor_property("enabled"):
        ns.set_editor_property("enabled", False)
        sme.set_nanite_settings(m, ns, True)
        changed += 1
        log("%s: Nanite off (%d LODs)" % (m.get_name(), m.get_num_lods()))
    if not NOSAVE:
        eal.save_loaded_asset(m, only_if_is_dirty=True)   # also a change an earlier in-memory run made
log("%s: %d flower meshes checked, %d turned classic" % ("in memory" if NOSAVE else "saved", checked, changed))
