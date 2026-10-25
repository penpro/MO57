"""Near-camera foliage fade: leaves and fronds within a metre or so of the camera lens dither away, so the third-person camera never
looks through a wall of leaves (a new player's first view was a sapling's leaves, then a rainforest fern -- first-ten-minutes pass, Oct 15 2026).

    python Tools/ue.py py --file Content/Python/foliage_camera_fade.py      (editor open, no PIE, no `inst` instance: they lock the .uasset files)

What each FOLIAGE MASTER material (MASTERS below: the parents of every scattered tree / shrub / fern / grass / flower instance, found by walking
every scattered instance's material to its base) gets -- or, for a master that works in material attributes (MA_Foliage, the trees), the
function that sets its leaves' OpacityMask (FUNCTIONS):
  OpacityMask = Shadow Pass Switch (MaterialExpressionShadowReplace)( Default: MO_CameraFade(Mask, PixelDepth), Shadow: Mask )
  MO_CameraFade keeps the mask where PixelDepth >= CameraFadeEndCm, drops it where PixelDepth <= CameraFadeStartCm, and dithers between (an
  interleaved-gradient pattern on the pixel position, offset every frame, so TSR resolves it to a soft fade). Shadow passes keep the original mask: a leaf beside the
  lens still casts its shadow, the player only stops seeing the leaf itself.
Parameters CameraFadeStartCm / CameraFadeEndCm (group "MO Camera Fade") are overridable per instance.
GROUND COVER (grass, flowers) gets a narrower band, GROUND_COVER_BAND (BR23-10, Oct 25 2026): it stands right under the low third-person
camera, so the 40-160 cm band covered whole tufts at once, and while the camera MOVES TSR cannot average the screen door: the blades read
dotted and broken ("blurry, some of them"; measured in PIE on Wes's view, D:/MOAssets/previews/grass/flowers_moving_fade_vs_nofade.png).
15-45 cm still clears a blade touching the lens. MA_Flowers (flowers only) takes it as its defaults; the grass masters are shared with
shrubs and ferns, so the grass INSTANCES override it. FOLIAGEFADE_GROUND_ONLY=1 runs only that step (no master is touched).
Only MASKED masters with an OpacityMask input are changed; others are reported and skipped. Idempotent: a master that already has MO_CameraFade
only gets its shader code updated. Output lines start with FOLIAGEFADE.
"""
import os

import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

MASTERS = [
    # (MA_Foliage -- trees + shrubs: Black Alder, Common Hazel, ... -- works in material ATTRIBUTES; its mask is set inside FUNCTIONS below)
    "/Game/MSPresets/MS_Foliage_Material_LATEST/MasterMaterials/M_Foliage",              # Megascans plants
    "/Game/MSPresets/MS_Foliage_Material_LATEST/MasterMaterials/M_Foliage_TwoSided",     # Megascans plants (ferns)
    "/Game/Custom/CustomMaterials/Materials/Master_Materials/Custom_MS_Master/M_MS_Foliage_Overwrite/M_MS_CustomFoliage",
    "/Game/Custom/CustomMaterials/Materials/Master_Materials/Custom_MS_Master/M_MS_Foliage_Overwrite/M_MS_CustomFoliageNoWPO",
    "/Game/Materials/FabOverrides/M_MS_Foliage",                                          # Fab grasses / plants
    "/Game/PN_MeadowFlowers/Materials/flowerMaterials/MA_Flowers",
]

# Material functions whose final Set/MakeMaterialAttributes takes the OpacityMask: MA_Foliage's leaf and decoration cards. The trunk
# function stays solid (a trunk has collision: the camera arm already keeps out of it).
FUNCTIONS = [
    "/Game/MSPresets/MS_Foliage_Material/MaterialFunctions/Foliage/MF_TwoSided_Leaves",
    "/Game/MSPresets/MS_Foliage_Material/MaterialFunctions/Foliage/MF_TwoSided_Decoration",
]

GROUND_COVER_BAND = (15.0, 45.0)
GROUND_COVER_MASTERS = ["/Game/PN_MeadowFlowers/Materials/flowerMaterials/MA_Flowers"]
FAB = "/Game/Fab/Megascans/Plants/"
GROUND_COVER_INSTANCES = [
    FAB + "Grass_smdrailr/Medium/smdrailr_tier_2/Materials/MI_smdrailr",
    FAB + "Wild_Grass_vlkhcbxia/Medium/vlkhcbxia_tier_2/Materials/MI_vlkhcbxia",
    FAB + "Thatching_Grass_uddmcgbia/Medium/uddmcgbia_tier_2/Materials/MI_uddmcgbia",
]

FADE_CODE = """// MO_CameraFade (Content/Python/foliage_camera_fade.py): drop the mask near the camera lens, dithered.
float f = saturate((Depth - StartCm) / max(EndCm - StartCm, 1.0));
// Interleaved gradient noise, offset every frame so TSR averages the screen door into a soft fade (a static pattern stays a visible mesh).
float2 p = Parameters.SvPosition.xy + 5.588238 * float(View.StateFrameIndexMod8);
float n = frac(52.9829189 * frac(dot(p, float2(0.06711056, 0.00583715))));
return f > n ? Mask : 0.0;
"""


def log(msg):
    unreal.log("FOLIAGEFADE " + msg)


def connect(a, a_out, b, b_in):
    ok = MEL.connect_material_expressions(a, a_out, b, b_in)
    if not ok:
        log("!! connect failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))
    return ok


def build_fade(create, x, y):
    """The MO_CameraFade custom node, its PixelDepth and parameters, and the Shadow Pass Switch; returns (fade, switch)."""
    depth = create(unreal.MaterialExpressionPixelDepth, x, y + 160)
    start = create(unreal.MaterialExpressionScalarParameter, x, y + 240)
    end = create(unreal.MaterialExpressionScalarParameter, x, y + 320)
    for node, name, default in ((start, "CameraFadeStartCm", 40.0), (end, "CameraFadeEndCm", 160.0)):
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        node.set_editor_property("group", "MO Camera Fade")
    fade = create(unreal.MaterialExpressionCustom, x + 300, y)
    inputs = []
    for name in ("Mask", "Depth", "StartCm", "EndCm"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    fade.set_editor_property("inputs", inputs)
    fade.set_editor_property("code", FADE_CODE)
    fade.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    fade.set_editor_property("description", "MO_CameraFade")
    switch = create(unreal.MaterialExpressionShadowReplace, x + 600, y)
    ok = connect(depth, "", fade, "Depth") and connect(start, "", fade, "StartCm") and connect(end, "", fade, "EndCm") and connect(fade, "", switch, "Default")
    return fade, switch, ok


def existing_fade(exprs):
    for e in exprs:
        if isinstance(e, unreal.MaterialExpressionCustom) and str(e.get_editor_property("description")) == "MO_CameraFade":
            return e
    return None


def upgrade(path):
    m = unreal.load_asset(path)
    if not m:
        log("%s: not found" % path)
        return False
    e = existing_fade(MEL.get_material_expressions(m))
    if e:
        e.set_editor_property("code", FADE_CODE)
        MEL.recompile_material(m)
        EAL.save_loaded_asset(m)
        log("%s: updated MO_CameraFade in place" % path)
        return True
    if m.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_MASKED:
        log("%s: SKIP, blend mode %s (not masked)" % (path, m.get_editor_property("blend_mode")))
        return False
    if m.get_editor_property("use_material_attributes"):
        log("%s: SKIP, uses material attributes (splice its functions: FUNCTIONS)" % path)
        return False
    src = MEL.get_material_property_input_node(m, unreal.MaterialProperty.MP_OPACITY_MASK)
    if not src:
        log("%s: SKIP, nothing drives OpacityMask" % path)
        return False
    src_out = MEL.get_material_property_input_node_output_name(m, unreal.MaterialProperty.MP_OPACITY_MASK)
    x, y = src.get_editor_property("material_expression_editor_x") + 300, src.get_editor_property("material_expression_editor_y")
    fade, switch, ok = build_fade(lambda cls, nx, ny: MEL.create_material_expression(m, cls, nx, ny), x, y)
    ok = ok and connect(src, src_out, fade, "Mask") and connect(src, src_out, switch, "Shadow")
    ok = ok and MEL.connect_material_property(switch, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    if not ok:
        log("%s: !! wiring failed -- NOT saved (revert the asset from git if the editor shows it dirty)" % path)
        return False
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("%s: MO_CameraFade spliced into OpacityMask (was %s.%s)" % (path, src.get_name(), src_out or "out"))
    return True


def upgrade_function(path):
    f = unreal.load_asset(path)
    if not f:
        log("%s: not found" % path)
        return False
    exprs = MEL.get_material_function_expressions(f)
    e = existing_fade(exprs)
    if e:
        e.set_editor_property("code", FADE_CODE)
        MEL.update_material_function(f)
        EAL.save_loaded_asset(f)
        log("%s: updated MO_CameraFade in place" % path)
        return True
    targets = []
    for node in exprs:
        if node.get_class().get_name() in ("MaterialExpressionSetMaterialAttributes", "MaterialExpressionMakeMaterialAttributes"):
            for name, src in zip(MEL.get_material_expression_input_names(node), MEL.get_inputs_for_material_function_expression(f, node)):
                if name == "OpacityMask" and src:
                    targets.append((node, src))
    if len(targets) != 1:
        log("%s: SKIP, %d attribute nodes take an OpacityMask (expected 1)" % (path, len(targets)))
        return False
    node, src = targets[0]
    src_out = MEL.get_input_node_output_name_for_material_expression(node, src)
    x, y = node.get_editor_property("material_expression_editor_x") - 900, node.get_editor_property("material_expression_editor_y") + 600
    fade, switch, ok = build_fade(lambda cls, nx, ny: MEL.create_material_expression_in_function(f, cls, nx, ny), x, y)
    ok = ok and connect(src, src_out, fade, "Mask") and connect(src, src_out, switch, "Shadow") and connect(switch, "", node, "OpacityMask")
    if not ok:
        log("%s: !! wiring failed -- NOT saved (revert the asset from git if the editor shows it dirty)" % path)
        return False
    MEL.update_material_function(f)
    EAL.save_loaded_asset(f)
    log("%s: MO_CameraFade spliced into %s.OpacityMask (was %s.%s)" % (path, node.get_name(), src.get_name(), src_out or "out"))
    return True


def ground_cover():
    """GROUND_COVER_BAND on the flower master's defaults and as an override on the grass instances (see the module doc)."""
    start, end = GROUND_COVER_BAND
    for path in GROUND_COVER_MASTERS:
        m = unreal.load_asset(path)
        n = 0
        for e in MEL.get_material_expressions(m):
            if isinstance(e, unreal.MaterialExpressionScalarParameter):
                name = str(e.get_editor_property("parameter_name"))
                want = start if name == "CameraFadeStartCm" else end if name == "CameraFadeEndCm" else None
                if want is not None and abs(e.get_editor_property("default_value") - want) > 1e-3:
                    e.set_editor_property("default_value", want)
                    n += 1
        if n:
            MEL.recompile_material(m)
            EAL.save_loaded_asset(m)
        log("%s: ground-cover band %.0f-%.0f cm (%d default(s) changed)" % (path, start, end, n))
    for path in GROUND_COVER_INSTANCES:
        mi = unreal.load_asset(path)
        # both set unconditionally and read back: set_material_instance_scalar_parameter_value can return False when it DID set the value
        MEL.set_material_instance_scalar_parameter_value(mi, "CameraFadeStartCm", start)
        MEL.set_material_instance_scalar_parameter_value(mi, "CameraFadeEndCm", end)
        MEL.update_material_instance(mi)
        got = (MEL.get_material_instance_scalar_parameter_value(mi, "CameraFadeStartCm"), MEL.get_material_instance_scalar_parameter_value(mi, "CameraFadeEndCm"))
        ok = abs(got[0] - start) < 1e-3 and abs(got[1] - end) < 1e-3
        if ok:
            EAL.save_loaded_asset(mi)
        log("%s: ground-cover band %.0f-%.0f cm %s" % (path, start, end, "set" if ok else "!! NOT SET (read back %s), not saved" % (got,)))


def main():
    if os.environ.pop("FOLIAGEFADE_GROUND_ONLY", "") != "1":
        done = [p for p in MASTERS if upgrade(p)] + [p for p in FUNCTIONS if upgrade_function(p)]
        log("DONE %d/%d graphs" % (len(done), len(MASTERS) + len(FUNCTIONS)))
    ground_cover()


main()
