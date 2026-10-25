"""Cap the light that Megascans foliage passes through its blades (Wes, Oct 25 2026: "for the love of god, fix the weird glowing/weirdly shaded
grass"; BR23-11 "the grasses look funny and too bright", BR24-21 "this grass still has no shadows and looks like it's glowing").

The grass is Two Sided Foliage: its Subsurface Color is the light it passes from behind. The Fab master (/Game/Materials/FabOverrides/M_MS_Foliage,
QMF_GenerateTranslucency) makes it Desaturate(BaseColor x Mask x Controls.G, 1 - Controls.R) ^ Controls.B from each "Translucency Controls*"
vector: R = saturation, G = STRENGTH, B = EXPONENT (measured live in PIE, Oct 25: G = 0 -> no transmission, R = 0 -> white). The wild grass
shipped at strength 5.0 with exponent 0.5 (a square root, which lifts every dim value), the lawn grass leaves 3.0: the transmitted light out-shone
the lit side, so the blades read pale and self-lit, and a shadow on them (a tree's, the pawn's) was drowned by the light coming through.
At strength <= 0.5 and exponent >= 1 the transmission stays below the reflected light: green, shaded blades, a sheen against the sun, darker in a
shadow (A/B shots D:/MOAssets/previews/grass/g0_spawn.png (before) / g4_s05_e1.png (after), shadow_strip.png).

Every material instance under the Megascans foliage masters (M_MS_Foliage, M_MS_Billboard) in /Game: strength = min(strength, MAX_STRENGTH),
exponent = max(exponent, MIN_EXPONENT). Idempotent; run it again after importing plants.

    python Tools/ue.py py --file Content/Python/fix_foliage_transmission.py      (editor open, NOT in PIE: it saves)
    FOLIAGE_TRANSMISSION_DRY=1 -> report only.

MOFramework.Rendering.FoliageTransmissionCapped checks the cap.
"""
import os

import unreal

MAX_STRENGTH = 0.5
MIN_EXPONENT = 1.0
MASTERS = ("M_MS_Foliage", "M_MS_Billboard")
DRY = os.environ.pop("FOLIAGE_TRANSMISSION_DRY", "") == "1"   # consumed: the editor's environment outlives the run

mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
log = lambda m: out("FOLIAGETX " + m)


def master_of(mi):
    m = mi
    while isinstance(m, unreal.MaterialInstance):
        m = m.get_editor_property("parent")
    return m.get_name() if m else None


changed_assets = 0
changed_params = 0
checked = 0
flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "MaterialInstanceConstant")], package_paths=["/Game"], recursive_paths=True)
for ad in reg.get_assets(flt):
    mi = ad.get_asset()
    if master_of(mi) not in MASTERS:
        continue
    checked += 1
    dirty = False
    for pv in mi.get_editor_property("vector_parameter_values"):
        name = str(pv.get_editor_property("parameter_info").get_editor_property("name"))
        if not name.startswith("Translucency Controls"):
            continue
        c = pv.get_editor_property("parameter_value")
        strength, exponent = min(c.g, MAX_STRENGTH), max(c.b, MIN_EXPONENT)
        if abs(strength - c.g) < 1e-4 and abs(exponent - c.b) < 1e-4:
            continue
        log("%s %s: strength %.2f -> %.2f, exponent %.2f -> %.2f" % (ad.package_name, name, c.g, strength, c.b, exponent))
        changed_params += 1
        if not DRY:
            mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(c.r, strength, exponent, c.a))
            dirty = True
    if dirty:
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi, only_if_is_dirty=False)
        changed_assets += 1
log("%s: %d foliage material instance(s) checked, %d parameter(s) over the cap, %d asset(s) saved" % ("DRY" if DRY else "done", checked, changed_params, changed_assets))
