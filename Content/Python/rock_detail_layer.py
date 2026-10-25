"""A tiling rock detail layer for large scanned / generated rocks (Wes, Oct 25 2026: "the large rocks from trellis have shitty resolution on the
texture ... they're also not very large compared to what i thought they were going to be so scaling them up would make the texture even worse").

The TRELLIS cliff formations already carry 8192^2 textures, but one UV atlas over a 30 m rock is at most ~270 texels per metre, softer up close
the larger the rock is scaled; tiling that atlas 2x2 would scramble it (the UVs are scattered charts). What large Megascans cliffs do instead:
a world-aligned (triplanar) tiling rock texture on top, its brightness modulating the bake and its normal blended into the bake's normal, so
the near detail no longer depends on the rock's size. Here:

  /Game/Materials/FabOverrides/M_MS_Base gains a static switch "Enable Rock Detail" (OFF by default: every other item on this master keeps its
  shader) and the parameters Detail Albedo / Detail Normal (texture objects), Detail Size (cm per tile), Detail Albedo Mean, Detail Albedo
  Strength, Detail Normal Strength (group "Rock Detail"). The detail is the voxel terrain's own granite, rock_05 (Poly Haven, CC0, tracked;
  the ground of the granite summits), its OpenGL normal map copied as a DirectX one (/Game/ImageTo3D/Shared/T_Rock05_Normal_DX: green flipped).
  ROCK_MIS turn the switch on (the formations' MI_*_MO, so every rock-type variant under them gets it).

    python Tools/ue.py py --file Content/Python/rock_detail_layer.py        (editor open, PIE NOT running: saves are refused in PIE)

Idempotent: an existing switch is left as it is; the MIs are set again. Output lines start with ROCKDETAIL.
"""
import unreal

MASTER = "/Game/Materials/FabOverrides/M_MS_Base"
ALBEDO = "/Game/VoxelExamples/NaniteMaterials/Materials/Cliff/rock_05_diff_2k"
NORMAL_GL = "/Game/VoxelExamples/NaniteMaterials/Materials/Cliff/rock_05_nor_gl_2k"
NORMAL_DX = "/Game/ImageTo3D/Shared/T_Rock05_Normal_DX"
SWITCH = "Enable Rock Detail"
GROUP = "Rock Detail"
ROCK_MIS = ["/Game/ImageTo3D/Cliffs/%s/%s/Materials/MI_%s_MO" % (n, n, n)
            for n in ("Cliff_BroadCliffBlock01", "Cliff_SheerBlockTower01", "Cliff_LeaningSlabStack01", "Cliff_OverhangingButtress01")]
FN_WAT = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"
FN_WAN = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal"
FN_BLEND = "/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
log = lambda m: out("ROCKDETAIL " + m)


def dx_normal():
    if not eal.does_asset_exist(NORMAL_DX):
        eal.duplicate_asset(NORMAL_GL, NORMAL_DX)
    tex = unreal.load_asset(NORMAL_DX)
    if not tex.get_editor_property("flip_green_channel"):
        tex.set_editor_property("flip_green_channel", True)
        eal.save_loaded_asset(tex)
    return tex


def node(material, cls, x, y, **props):
    e = mel.create_material_expression(material, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def fn_call(material, path, x, y):
    e = mel.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, x, y)
    e.set_editor_property("material_function", unreal.load_asset(path))
    return e


def add_detail(material, normal_tex):
    if SWITCH in [str(n) for n in mel.get_static_switch_parameter_names(material)]:
        log("M_MS_Base already has '%s'" % SWITCH)
        return False
    base_color = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    base_color_out = mel.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_BASE_COLOR)
    base_normal = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_NORMAL)
    base_normal_out = mel.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_NORMAL)
    if not base_color or not base_normal:
        raise RuntimeError("M_MS_Base: nothing feeds Base Color / Normal")
    x, y = -900, 900
    scalar = unreal.MaterialExpressionScalarParameter
    size = node(material, scalar, x - 900, y, parameter_name="Detail Size", default_value=200.0, group=GROUP)
    ones = node(material, unreal.MaterialExpressionConstant3Vector, x - 900, y + 120, constant=unreal.LinearColor(1, 1, 1, 1))
    size3 = node(material, unreal.MaterialExpressionMultiply, x - 700, y + 40)
    mel.connect_material_expressions(size, "", size3, "A")
    mel.connect_material_expressions(ones, "", size3, "B")

    # albedo: the tile's brightness over its mean, toward 1 by the strength, times the bake
    albedo = node(material, unreal.MaterialExpressionTextureObjectParameter, x - 900, y + 260, parameter_name="Detail Albedo",
                  texture=unreal.load_asset(ALBEDO), group=GROUP)
    wat = fn_call(material, FN_WAT, x - 600, y + 220)
    mel.connect_material_expressions(albedo, "", wat, "TextureObject")
    mel.connect_material_expressions(size3, "", wat, "TextureSize")
    weights = node(material, unreal.MaterialExpressionConstant3Vector, x - 600, y + 420, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1))
    luma = node(material, unreal.MaterialExpressionDotProduct, x - 350, y + 260)
    mel.connect_material_expressions(wat, "XYZ Texture", luma, "A")
    mel.connect_material_expressions(weights, "", luma, "B")
    mean = node(material, scalar, x - 350, y + 420, parameter_name="Detail Albedo Mean", default_value=0.27, group=GROUP)
    ratio = node(material, unreal.MaterialExpressionDivide, x - 150, y + 300)
    mel.connect_material_expressions(luma, "", ratio, "A")
    mel.connect_material_expressions(mean, "", ratio, "B")
    astr = node(material, scalar, x - 150, y + 460, parameter_name="Detail Albedo Strength", default_value=0.5, group=GROUP)
    alerp = node(material, unreal.MaterialExpressionLinearInterpolate, x + 50, y + 320, const_a=1.0)
    mel.connect_material_expressions(ratio, "", alerp, "B")
    mel.connect_material_expressions(astr, "", alerp, "Alpha")
    detailed_color = node(material, unreal.MaterialExpressionMultiply, x + 250, y + 200)
    mel.connect_material_expressions(base_color, base_color_out, detailed_color, "A")
    mel.connect_material_expressions(alerp, "", detailed_color, "B")
    sw_color = node(material, unreal.MaterialExpressionStaticSwitchParameter, x + 450, y + 100, parameter_name=SWITCH, default_value=False, group=GROUP)
    mel.connect_material_expressions(detailed_color, "", sw_color, "True")
    mel.connect_material_expressions(base_color, base_color_out, sw_color, "False")

    # normal: the tile's tangent-space normal, toward flat by the strength, blended angle-correctly into the bake's
    nrm = node(material, unreal.MaterialExpressionTextureObjectParameter, x - 900, y + 640, parameter_name="Detail Normal", texture=normal_tex, group=GROUP)
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wan = fn_call(material, FN_WAN, x - 600, y + 640)
    mel.connect_material_expressions(nrm, "", wan, "TextureObject")
    mel.connect_material_expressions(size3, "", wan, "TextureSize")
    flat = node(material, unreal.MaterialExpressionConstant3Vector, x - 350, y + 820, constant=unreal.LinearColor(0, 0, 1, 1))
    nstr = node(material, scalar, x - 350, y + 920, parameter_name="Detail Normal Strength", default_value=0.7, group=GROUP)
    nlerp = node(material, unreal.MaterialExpressionLinearInterpolate, x - 150, y + 700)
    mel.connect_material_expressions(flat, "", nlerp, "A")
    mel.connect_material_expressions(wan, "XYZ Texture", nlerp, "B")
    mel.connect_material_expressions(nstr, "", nlerp, "Alpha")
    blend = fn_call(material, FN_BLEND, x + 100, y + 640)
    mel.connect_material_expressions(base_normal, base_normal_out, blend, "BaseNormal")
    mel.connect_material_expressions(nlerp, "", blend, "AdditionalNormal")
    sw_normal = node(material, unreal.MaterialExpressionStaticSwitchParameter, x + 450, y + 640, parameter_name=SWITCH, default_value=False, group=GROUP)
    mel.connect_material_expressions(blend, "Result", sw_normal, "True")
    mel.connect_material_expressions(base_normal, base_normal_out, sw_normal, "False")

    mel.connect_material_property(sw_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(sw_normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(material)
    return True


material = unreal.load_asset(MASTER)
added = add_detail(material, dx_normal())
if added:
    eal.save_loaded_asset(material)
    log("M_MS_Base: '%s' added (off by default), saved" % SWITCH)
for path in ROCK_MIS:
    mi = unreal.load_asset(path)
    if not mi:
        log("missing %s" % path)
        continue
    mel.set_material_instance_static_switch_parameter_value(mi, SWITCH, True)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    log("%s: detail on" % path.split("/")[-1])
