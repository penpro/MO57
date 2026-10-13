"""Upgrade the terrain ground material (M_QuixelBase, the parent of EVERY voxel surface material instance) with: anti-tiling warp, macro variation, distance tint, snow.

    python Tools/gen_macro_noise.py                                           (once: Saved/GeneratedTextures/macro/T_MOMacroNoise.png)
    python Tools/ue.py py --file Content/Python/upgrade_terrain_material.py   (editor open, PIE NOT running, no `inst` instance running: it locks the .uasset files)

Why one place: all 13 ground surfaces (MuddyLeaves, Gravel, Cliff, Sand, JungleFloor, AlpineHeath, NordicMoss, ...) are instances of M_QuixelBase, and the voxel mega material copies M_QuixelBase's graph into
its per-pixel surface switch -- so a change here is evaluated only for the surface a pixel actually shows (no cost multiplied by the surface count) and reaches the near terrain and the far LOD alike.

What the graph gets (all parameters are overridable per material instance; defaults keep every surface looking like before except the always-on macro variation / warp):
  1. MO_WarpOffset  (Custom, 2 fetches of T_MOMacroNoise): a world-position OFFSET (not a warped position: the world position is LWC, and a float32 position 24 km from the origin would quantise the texture lookups)
     added to the position the two triplanar layers sample with => the tile grid stops being a grid. Params WarpAmountCm / WarpScaleCm.
  2. MO_Finish      (Custom, 3-4 fetches): after the two layers are blended -- macro colour / brightness variation read at 25 m / 180 m / 1.1 km (MacroAmount), the DISTANCE TINT (the surface
     fades toward FarColor between FarTintStartCm and FarTintEndCm, scaled by FarTintAmount: a forest floor becomes dark canopy green, so a far forested mountain stops reading as bare yellow litter), and
     SNOW (above SnowLineCm +- noise, flat-ish faces only, the slope limit relaxing with altitude; SnowColor, flattened normal, rougher-than-rock but glossy-ish roughness).
  3. Break -> MO_Finish -> Make MaterialAttributes (every attribute passes through; only BaseColor / Roughness / Normal are replaced).
Idempotent: on a graph that already has the MO_ nodes only their shader code is updated (see main()); on the pristine graph it builds them.
Output lines start with TERRAINMAT.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

BASE = "/Game/VoxelExamples/NaniteMaterials/Materials"
MASTER = BASE + "/M_QuixelBase"
NOISE_DIR = BASE + "/Macro"
NOISE_PNG = "D:/UEProjects/MO57/Saved/GeneratedTextures/macro/T_MOMacroNoise.png"
NOISE_ASSET = NOISE_DIR + "/T_MOMacroNoise"


def log(msg):
    unreal.log("TERRAINMAT " + msg)


def import_noise():
    if EAL.does_asset_exist(NOISE_ASSET):
        return unreal.load_asset(NOISE_ASSET)
    task = unreal.AssetImportTask()
    task.filename = NOISE_PNG
    task.destination_path = NOISE_DIR
    task.destination_name = "T_MOMacroNoise"
    task.replace_existing = True
    task.automated = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(NOISE_ASSET)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)   # uncompressed RGBA8: smooth low-frequency noise must not block
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property("never_stream", True)
    EAL.save_loaded_asset(tex)
    log("imported " + NOISE_ASSET)
    return tex


WARP_CODE = """
float lod = max(0.0, log2(max(D, 1.0) / WarpScale));
float2 uv = P.xy / WarpScale;
float4 a = NoiseTex.SampleLevel(NoiseTexSampler, uv, lod);
float4 b = NoiseTex.SampleLevel(NoiseTexSampler, uv * 2.71 + float2(0.37, 0.11), lod);
return float3(a.r - 0.5, a.g - 0.5, b.r - 0.5) * 2.0 * WarpAmp;
"""

FINISH_CODE = """
float d = max(D, 1.0);
float2 uv1 = P.xy / 2500.0;
float2 uv2 = P.xy / 18000.0 + float2(0.37, 0.11);
float2 uv3 = P.xy / 110000.0 + float2(0.71, 0.53);
float m1 = NoiseTex.SampleLevel(NoiseTexSampler, uv1, max(0.0, log2(d / 2500.0))).r;
float m2 = NoiseTex.SampleLevel(NoiseTexSampler, uv2, max(0.0, log2(d / 18000.0))).g;
float m3 = NoiseTex.SampleLevel(NoiseTexSampler, uv3, max(0.0, log2(d / 110000.0))).b;
// the 25 m blotches only up close: at a grazing angle they smear into long rays pointing at the horizon
float w1 = saturate(1.0 - d / 70000.0);
float w2 = saturate(1.0 - d / 500000.0);
float v = ((m1 - 0.5) * 0.45 * w1 + (m2 - 0.5) * 0.35 * w2 + (m3 - 0.5) * 0.20) * 2.6;

// macro colour / brightness variation: the same material, a little lighter here, a little darker there, a hint warmer / cooler
float3 c = Color * (1.0 + v * MacroAmount);
c *= 1.0 + float3(0.05, 0.0, -0.05) * v * MacroAmount * 2.0;

// distance tint: the ground fades toward the colour of what actually covers it (canopy, grass, rock)
float far = smoothstep(FarStart, max(FarEnd, FarStart + 1.0), d) * FarAmount;
c = lerp(c, FarColor * (1.0 + v * 0.55), far);

// snow: altitude line broken up by noise, flat-ish faces only, steeper faces hold snow higher up
float n = (m2 - 0.5) * 2.0;
float h = P.z + n * SnowNoise + (m1 - 0.5) * SnowNoise * 0.6;
float snowH = smoothstep(SnowLine - SnowBand, SnowLine + SnowBand, h);
float up = saturate(VN.z);
float slopeMin = lerp(0.80, 0.42, saturate((h - SnowLine) / (SnowBand * 5.0)));
float slopeMask = smoothstep(slopeMin - 0.10, slopeMin + 0.06, up + n * 0.04);
// far voxel LODs are coarse: their per-triangle normals checker the snow into blocks, so the slope term fades out and the far snow cap follows altitude alone
slopeMask = lerp(slopeMask, 1.0, smoothstep(150000.0, 600000.0, d));
float snow = snowH * slopeMask * SnowAmount;
c = lerp(c, SnowColor * (0.93 + 0.07 * (v * 0.5 + 0.5)), snow);
return float4(c, snow);
"""

NORMAL_CODE = """
float d = max(D, 1.0);
float a = NoiseTex.SampleLevel(NoiseTexSampler, P.xy / 3200.0 + float2(0.19, 0.83), max(0.0, log2(d / 3200.0))).r;
float b = NoiseTex.SampleLevel(NoiseTexSampler, P.xy / 21000.0 + float2(0.52, 0.27), max(0.0, log2(d / 21000.0))).g;
float t = smoothstep(0.30, 0.70, a * 0.6 + b * 0.4);
float k = lerp(1.0 - Var, 1.0 + 0.20 * Var, t);      // the surface's relief is stronger in some patches and softer in others: ripples / grain stop being one repeating field
float3 nn = normalize(float3(N.xy * k, N.z));
return normalize(lerp(nn, float3(0.0, 0.0, 1.0), Snow));
"""


def custom(mat, desc, code, inputs, out_type, x, y):
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    cis = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        cis.append(ci)
    node.set_editor_property("inputs", cis)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", out_type)
    node.set_editor_property("description", desc)
    return node


def scalar(mat, name, default, x, y, group="MO Terrain"):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    n.set_editor_property("group", group)
    n.set_editor_property("desc", "MO_" + name)
    return n


def vector(mat, name, rgb, x, y, group="MO Terrain"):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    n.set_editor_property("group", group)
    n.set_editor_property("desc", "MO_" + name)
    return n


def connect(a, a_out, b, b_in):
    ok = MEL.connect_material_expressions(a, a_out, b, b_in)
    if not ok:
        log("!! connect failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))
    return ok


def main():
    noise = import_noise()
    m = unreal.load_asset(MASTER)

    # 1. a previous run's graph is UPDATED IN PLACE (the shader code), never deleted and rebuilt: MaterialEditingLibrary.delete_material_expression asserted (!IsRooted) in a fresh editor session
    #    and killed the editor (2026-10-13). To change the STRUCTURE (a new node, a new connection), revert M_QuixelBase from git first, then run this on the pristine graph.
    custom_by_desc = {}
    for e in list(MEL.get_material_expressions(m)):
        if isinstance(e, unreal.MaterialExpressionCustom):
            custom_by_desc[str(e.get_editor_property("description"))] = e
    if "MO_Finish" in custom_by_desc:
        custom_by_desc["MO_Finish"].set_editor_property("code", FINISH_CODE)
        custom_by_desc["MO_WarpOffset"].set_editor_property("code", WARP_CODE)
        custom_by_desc["MO_NormalMod"].set_editor_property("code", NORMAL_CODE)
        MEL.recompile_material(m)
        EAL.save_loaded_asset(m)
        log("updated the shader code of the existing MO_ nodes in place")
        apply_surface_overrides()
        return

    # 2. find the existing graph's landmarks
    exprs = list(MEL.get_material_expressions(m))
    calls = [e for e in exprs if isinstance(e, unreal.MaterialExpressionMaterialFunctionCall)]
    layer_calls = [e for e in calls if "MF_BaseLayer" in str(e.get_editor_property("material_function").get_name())]
    blend = [e for e in calls if "MF_BlendLayers" in str(e.get_editor_property("material_function").get_name())]
    assert len(layer_calls) == 2 and len(blend) == 1, "unexpected M_QuixelBase graph (layers=%d blend=%d)" % (len(layer_calls), len(blend))
    blend = blend[0]
    X0, Y0 = -2600, 600

    # 3. parameters
    p = {}
    p["MacroAmount"] = scalar(m, "MacroAmount", 0.34, X0, Y0 + 0)
    p["WarpAmountCm"] = scalar(m, "WarpAmountCm", 60.0, X0, Y0 + 100)
    p["WarpScaleCm"] = scalar(m, "WarpScaleCm", 2200.0, X0, Y0 + 200)
    p["FarTintAmount"] = scalar(m, "FarTintAmount", 0.0, X0, Y0 + 300)
    p["FarTintStartCm"] = scalar(m, "FarTintStartCm", 120000.0, X0, Y0 + 400)
    p["FarTintEndCm"] = scalar(m, "FarTintEndCm", 700000.0, X0, Y0 + 500)
    p["SnowLineCm"] = scalar(m, "SnowLineCm", 125000.0, X0, Y0 + 600)
    p["SnowBandCm"] = scalar(m, "SnowBandCm", 22000.0, X0, Y0 + 700)
    p["SnowNoiseCm"] = scalar(m, "SnowNoiseCm", 16000.0, X0, Y0 + 800)
    p["SnowAmount"] = scalar(m, "SnowAmount", 1.0, X0, Y0 + 900)
    p["NormalVariation"] = scalar(m, "NormalVariation", 0.30, X0, Y0 + 950)
    p["FarColor"] = vector(m, "FarColor", (0.030, 0.045, 0.026), X0, Y0 + 1000)
    p["SnowColor"] = vector(m, "SnowColor", (0.78, 0.82, 0.88), X0, Y0 + 1100)
    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, X0, Y0 - 150)
    tex.set_editor_property("parameter_name", "MacroNoise")
    tex.set_editor_property("texture", noise)
    tex.set_editor_property("group", "MO Terrain")
    tex.set_editor_property("desc", "MO_MacroNoise")
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X0, Y0 - 300)
    wp.set_editor_property("desc", "MO_WorldPosition")
    wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    depth = MEL.create_material_expression(m, unreal.MaterialExpressionPixelDepth, X0, Y0 - 450)
    depth.set_editor_property("desc", "MO_PixelDepth")
    vn = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, X0, Y0 - 600)
    vn.set_editor_property("desc", "MO_VertexNormal")

    # 4. the warp: WorldPosition + MO_WarpOffset(...) feeds both triplanar layers
    warp = custom(m, "MO_WarpOffset", WARP_CODE, ["P", "D", "NoiseTex", "WarpScale", "WarpAmp"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, X0 + 500, Y0 - 300)
    connect(wp, "", warp, "P")
    connect(depth, "", warp, "D")
    connect(tex, "", warp, "NoiseTex")
    connect(p["WarpScaleCm"], "", warp, "WarpScale")
    connect(p["WarpAmountCm"], "", warp, "WarpAmp")
    add = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, X0 + 800, Y0 - 300)
    add.set_editor_property("desc", "MO_WarpedWorldPosition")
    connect(wp, "", add, "A")
    connect(warp, "", add, "B")
    for lc in layer_calls:
        connect(add, "", lc, "WorldPosition")

    # 5. the finish: Break(blend) -> MO_Finish -> Make
    brk = MEL.create_material_expression(m, unreal.MaterialExpressionBreakMaterialAttributes, X0 + 1500, Y0 + 0)
    brk.set_editor_property("desc", "MO_Break")
    in_names = [str(n) for n in MEL.get_material_expression_input_names(brk)]
    connect(blend, "", brk, in_names[0])
    fin = custom(m, "MO_Finish", FINISH_CODE,
                 ["Color", "P", "D", "VN", "NoiseTex", "MacroAmount", "FarAmount", "FarStart", "FarEnd", "FarColor", "SnowLine", "SnowBand", "SnowNoise", "SnowAmount", "SnowColor"],
                 unreal.CustomMaterialOutputType.CMOT_FLOAT4, X0 + 1900, Y0 + 0)
    connect(brk, "BaseColor", fin, "Color")
    connect(wp, "", fin, "P")
    connect(depth, "", fin, "D")
    connect(vn, "", fin, "VN")
    connect(tex, "", fin, "NoiseTex")
    for pin, key in (("MacroAmount", "MacroAmount"), ("FarAmount", "FarTintAmount"), ("FarStart", "FarTintStartCm"), ("FarEnd", "FarTintEndCm"), ("FarColor", "FarColor"),
                     ("SnowLine", "SnowLineCm"), ("SnowBand", "SnowBandCm"), ("SnowNoise", "SnowNoiseCm"), ("SnowAmount", "SnowAmount"), ("SnowColor", "SnowColor")):
        connect(p[key], "", fin, pin)

    mk = MEL.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, X0 + 2900, Y0 + 0)
    mk.set_editor_property("desc", "MO_Make")
    rgb = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, X0 + 2300, Y0 - 100)
    rgb.set_editor_property("r", True); rgb.set_editor_property("g", True); rgb.set_editor_property("b", True); rgb.set_editor_property("a", False)
    rgb.set_editor_property("desc", "MO_FinishColor")
    connect(fin, "", rgb, "")
    snow = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, X0 + 2300, Y0 + 100)
    snow.set_editor_property("r", False); snow.set_editor_property("g", False); snow.set_editor_property("b", False); snow.set_editor_property("a", True)
    snow.set_editor_property("desc", "MO_FinishSnow")
    connect(fin, "", snow, "")

    snow_rough = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, X0 + 2300, Y0 + 250)
    snow_rough.set_editor_property("r", 0.62)
    snow_rough.set_editor_property("desc", "MO_SnowRoughness")
    rough = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, X0 + 2600, Y0 + 250)
    rough.set_editor_property("desc", "MO_RoughnessLerp")
    connect(brk, "Roughness", rough, "A")
    connect(snow_rough, "", rough, "B")
    connect(snow, "", rough, "Alpha")

    nrm = custom(m, "MO_NormalMod", NORMAL_CODE, ["N", "P", "D", "NoiseTex", "Var", "Snow"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, X0 + 2600, Y0 + 400)
    connect(brk, "Normal", nrm, "N")
    connect(wp, "", nrm, "P")
    connect(depth, "", nrm, "D")
    connect(tex, "", nrm, "NoiseTex")
    connect(p["NormalVariation"], "", nrm, "Var")
    connect(snow, "", nrm, "Snow")

    # pass every attribute through; replace BaseColor / Roughness / Normal
    # (MF_BaseLayer sets exactly BaseColor / Roughness / Normal / Displacement and MF_BlendLayers blends those; everything else is the default, which an unconnected Make pin also is.
    #  ShadingModel is deliberately NOT passed: the voxel generator handles the surface's shading model itself.)
    connect(brk, "Displacement", mk, "Displacement")
    connect(rgb, "", mk, "BaseColor")
    connect(rough, "", mk, "Roughness")
    connect(nrm, "", mk, "Normal")
    MEL.connect_material_property(mk, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)

    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("M_QuixelBase upgraded (%d expressions) and saved" % len(list(MEL.get_material_expressions(m))))
    apply_surface_overrides()


# What covers the ground seen from far away (linear colours), per surface material instance: FarTintAmount, FarColor, + optional per-surface knobs.
# Forest floors become canopy (dark, slightly different greens), grass stays lush, rock / sand / gravel barely shift.
SURFACES = {
    "MuddyLeaves":          dict(far=0.95, color=(0.020, 0.034, 0.016), start=90000.0,  end=500000.0),
    "ForestLitter":         dict(far=0.95, color=(0.022, 0.036, 0.018), start=90000.0,  end=500000.0),
    "JungleFloor":          dict(far=0.95, color=(0.014, 0.032, 0.013), start=90000.0,  end=450000.0),
    "MossyGrass":           dict(far=0.55, color=(0.060, 0.085, 0.030), start=150000.0, end=600000.0),
    "DriedGrassOnDampSoil": dict(far=0.45, color=(0.110, 0.090, 0.050), start=150000.0, end=600000.0),
    "AlpineHeath":          dict(far=0.45, color=(0.070, 0.074, 0.040), start=150000.0, end=600000.0),
    "NordicMoss":           dict(far=0.50, color=(0.052, 0.066, 0.036), start=150000.0, end=600000.0),
    "MossyRockyGround":     dict(far=0.50, color=(0.058, 0.062, 0.048), start=150000.0, end=600000.0),
    "IcelandicQuarryRock":  dict(far=0.30, color=(0.040, 0.040, 0.045), start=200000.0, end=700000.0),
    "Cliff":                dict(far=0.30, color=(0.075, 0.068, 0.060), start=200000.0, end=700000.0),
    "Gravel":               dict(far=0.30, color=(0.090, 0.090, 0.090), start=200000.0, end=700000.0),
    "DarkPebbles":          dict(far=0.30, color=(0.060, 0.060, 0.062), start=200000.0, end=700000.0),
    "Sand":                 dict(far=0.25, color=(0.300, 0.240, 0.150), start=200000.0, end=700000.0, macro=0.50, normal_var=0.45, warp=70.0, warp_scale=1500.0),
}


def apply_surface_overrides():
    for name, v in SURFACES.items():
        path = "%s/%s/MI_%s" % (BASE, name, name)
        mi = unreal.load_asset(path)
        if mi is None:
            log("!! no material instance " + path)
            continue
        MEL.set_material_instance_scalar_parameter_value(mi, "FarTintAmount", v["far"])
        MEL.set_material_instance_scalar_parameter_value(mi, "FarTintStartCm", v["start"])
        MEL.set_material_instance_scalar_parameter_value(mi, "FarTintEndCm", v["end"])
        c = v["color"]
        MEL.set_material_instance_vector_parameter_value(mi, "FarColor", unreal.LinearColor(c[0], c[1], c[2], 1.0))
        if "macro" in v:
            MEL.set_material_instance_scalar_parameter_value(mi, "MacroAmount", v["macro"])
        if "normal_var" in v:
            MEL.set_material_instance_scalar_parameter_value(mi, "NormalVariation", v["normal_var"])
        if "warp" in v:
            MEL.set_material_instance_scalar_parameter_value(mi, "WarpAmountCm", v["warp"])
            MEL.set_material_instance_scalar_parameter_value(mi, "WarpScaleCm", v["warp_scale"])
        EAL.save_loaded_asset(mi)
    log("surface overrides applied to %d material instances" % len(SURFACES))


main()
