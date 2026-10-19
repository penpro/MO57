"""Pivot Painter data textures are sampled as data: no mips, never streamed, linear (not sRGB), nearest filter (BACKLOG N-167, Docs/PCG.md 9).

    python Tools/ue.py py --file Content/Python/pivot_painter_textures.py      (editor, no PIE / instance holding the files; DRY_RUN = True only reports)

A Pivot Painter texture is DATA, not an image: each texel holds one branch's pivot (position + index) or its X axis and extent, read by the wind
graph (MF_AdvancedWind_Level, PN_WindAnimation) at the branch's own texel. A mip averages neighbouring branches into nonsense, and a streamed
texture can be resident without its top mip; the wind graph would then rotate branches about the wrong pivots. Found Oct 18 2026: of the 124
textures under the vendors' PivotPainter folders, the 6 of T_BlackAlder_Forest_08 / Sapling_01 / Seedling_01 had mips (from the texture group)
and streaming where every other one has neither.

Not touched, on purpose: the compression differs BY DESIGN (Pivot Painter 2 writes positions as 16-bit HDR and axis vectors / extents as 8-bit,
TC_VECTOR_DISPLACEMENTMAP), and the LOD group does not matter once mips and filtering are fixed. Both are reported. Idempotent. Output lines
start with PIVOTTEX. The folders are gitignored vendor content (BlackAlder, CommonHazel, Megascans, PN_WildBerries): rerun after importing or
re-migrating them.
"""
import collections

import unreal

DRY_RUN = False
REQUIRED = {
    "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    "never_stream": True,
    "srgb": False,
    "filter": unreal.TextureFilter.TF_NEAREST,
}
REPORTED = ["compression_settings", "lod_group"]
TAG = "PIVOTTEX "


def log(msg):
    unreal.log(TAG + msg)


def pivot_painter_textures():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_class_path.asset_name) == "Texture2D" and "/PivotPainter/" in str(data.package_path) + "/":
            found.append(str(data.package_name))
    return sorted(found)


def main():
    textures = [(p, unreal.load_asset(p)) for p in pivot_painter_textures()]
    textures = [(p, t) for p, t in textures if t]
    if not textures:
        log("no textures under a PivotPainter folder (vendor packs not installed?)")
        return
    for name in REPORTED:
        log("%s: %s" % (name, dict(collections.Counter(str(t.get_editor_property(name)) for _, t in textures))))
    changed = 0
    for path, tex in textures:
        diffs = [n for n, v in REQUIRED.items() if tex.get_editor_property(n) != v]
        if not diffs:
            continue
        log("%s %dx%d: %s" % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y(),
                              ", ".join("%s %s -> %s" % (n, tex.get_editor_property(n), REQUIRED[n]) for n in diffs)))
        changed += 1
        if DRY_RUN:
            continue
        for n in diffs:
            tex.set_editor_property(n, REQUIRED[n])
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        bad = [n for n, v in REQUIRED.items() if tex.get_editor_property(n) != v]
        log("%s: %s" % (path, "saved" if not bad else "!! still differs after the edit: %s" % bad))
    log("DONE %d Pivot Painter texture(s), %d %s" % (len(textures), changed, "would change (dry run)" if DRY_RUN else "changed and saved"))


main()
