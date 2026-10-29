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
  4. MO_NormalMod: patchy relief strength, snow flattening, and (Oct 13 evening) the FAR detail-normal fade: past ~250 m the few-metre detail normal maps light every
     tile alike and a far slope reads as a checker; they fade toward the mesh normal by FarNormalFlatten (0.85; cliff / quarry rock 0.15, because a coarse LOD's flat-shaded
     rock triangles show self-shadow stripes without them).
  5. MO_Weather (BR18-16, Oct 19 2026): the ground's weather state, MPC_MOGroundWeather's GroundWetness / GroundSnow (UMOWeatherIntegrationSubsystem advances them on the
     game clock at real rates -- a rain soaks the ground in ~10 min, the sun dries it in ~2 h, snow covers it in ~45 min and melts above freezing -- and replicates
     them; Ultra Dynamic Weather's own material state soaked it in 20 s and dried it in 90 s) -> wet ground (darker by WetDarken, glossier toward WetRoughness after the RoughnessMin floor: MO_WetRoughness), puddles on near-flat ground once it
     is soaked (PuddleAmount; sand drains), weather snow (patchy, then whole; on faces up to ~45 deg) merged with the altitude snow. Masked by the voxel metadata
     VFM_MOWeatherShelter, which the cave stamp writes (rock overhead: no rain or snow in a cave; UMOWorldSeedSubsystem sets the override).
NO AXIS-ALIGNED SINGLE READS of the noise at mid scales: one read repeats exactly at its period (a 180 m read printed rows of identical snow patches on every mountain).
Each mid-scale term is two reads on rotated grids at unrelated scales; the snow line wanders on the 1.1 km / 4.7 km reads.
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
# the ground's weather state (UMOWeatherIntegrationSubsystem::GroundWeatherCollection writes these two scalars on every machine)
GROUND_DIR = "/MOFramework/WorldSystems"
GROUND_MPC = GROUND_DIR + "/MPC_MOGroundWeather"
GROUND_PARAMS = ("GroundWetness", "GroundSnow")
# the cave stamp writes 1 here (UMOWorldGenSettings::CaveWeatherShelterMetadata)
SHELTER_DIR = "/Game/Penumbra/Maps"
SHELTER = SHELTER_DIR + "/VFM_MOWeatherShelter"


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


def ensure_ground_collection():
    if EAL.does_asset_exist(GROUND_MPC):
        return unreal.load_asset(GROUND_MPC)
    mpc = TOOLS.create_asset("MPC_MOGroundWeather", GROUND_DIR, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name in GROUND_PARAMS:
        sp = unreal.CollectionScalarParameter()
        sp.set_editor_property("parameter_name", name)
        sp.set_editor_property("default_value", 0.0)   # dry, bare: what a world without the subsystem shows
        params.append(sp)
    mpc.set_editor_property("scalar_parameters", params)
    EAL.save_loaded_asset(mpc)
    log("created " + GROUND_MPC)
    return mpc


def collection_param(mat, mpc, name, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, x, y)
    n.set_editor_property("collection", mpc)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("desc", "MO_" + name)
    return n


def ensure_shelter_metadata():
    md = unreal.load_asset(SHELTER) if EAL.does_asset_exist(SHELTER) else None
    if md is None:
        md = TOOLS.create_asset("VFM_MOWeatherShelter", SHELTER_DIR, unreal.VoxelFloatMetadata, None)
        md.set_editor_property("default_value", 0.0)                                          # the open ground: under the sky
        md.set_editor_property("gpu_packing", unreal.VoxelFloatMetadataPacking.ONE_BYTE)      # 0..1 is all it holds
        log("created " + SHELTER)
    # A metadata is known to the voxel runtime by its Guid (asset registry tag). Created without the plugin's factory it has none (an ensure in
    # FVoxelMetadataSingleton::RegisterFromAssetData, Oct 19 2026): give it one.
    if not unreal.GuidLibrary.is_valid_guid(md.get_editor_property("guid")):
        md.set_editor_property("guid", unreal.GuidLibrary.new_guid())
        log("gave %s a Guid" % SHELTER)
    EAL.save_loaded_asset(md)
    return md


WARP_CODE = """
float lod = max(0.0, log2(max(D, 1.0) / WarpScale));
float2 uv = P.xy / WarpScale;
float4 a = NoiseTex.SampleLevel(NoiseTexSampler, uv, lod);
float4 b = NoiseTex.SampleLevel(NoiseTexSampler, uv * 2.71 + float2(0.37, 0.11), lod);
return float3(a.r - 0.5, a.g - 0.5, b.r - 0.5) * 2.0 * WarpAmp;
"""

FINISH_CODE = """
float d = max(D, 1.0);
// One texture read on one axis-aligned grid repeats EXACTLY at its period: the 180 m read printed rows of identical snow patches across a mountainside
// (Oct 13 2026). Each mid-scale term is now two reads on grids rotated against each other, at scales with no common multiple: their sum never lines up.
float2 r1 = float2(0.80 * P.x - 0.60 * P.y, 0.60 * P.x + 0.80 * P.y);   // grid rotated 36.9 deg
float2 r2 = float2(0.28 * P.x + 0.96 * P.y, -0.96 * P.x + 0.28 * P.y);  // grid rotated 73.7 deg
float m1 = NoiseTex.SampleLevel(NoiseTexSampler, P.xy / 2500.0, max(0.0, log2(d / 2500.0))).r;
float m2a = NoiseTex.SampleLevel(NoiseTexSampler, r1 / 18000.0 + float2(0.37, 0.11), max(0.0, log2(d / 18000.0))).g;
float m2b = NoiseTex.SampleLevel(NoiseTexSampler, r2 / 29300.0 + float2(0.13, 0.61), max(0.0, log2(d / 29300.0))).r;
float m2 = 0.5 + (m2a + m2b - 1.0) * 0.7071;   // the sum of two keeps one read's contrast
float m3 = NoiseTex.SampleLevel(NoiseTexSampler, P.xy / 110000.0 + float2(0.71, 0.53), max(0.0, log2(d / 110000.0))).b;
float m4 = NoiseTex.SampleLevel(NoiseTexSampler, r1 / 470000.0 + float2(0.29, 0.83), max(0.0, log2(d / 470000.0))).g;   // 4.7 km: the snow line's own wander
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

// WEATHER (Ultra Dynamic Weather, BR18-16; Weather = MO_Weather's wet, puddle, snow): water in the pores darkens the ground -- less light scatters back
// out of it (soil and sand most, rock least: WetDarken per surface); standing water darker still, the bottom seen through it
c *= lerp(1.0, WetDarken, Weather.x);
c *= lerp(1.0, 0.72, Weather.y);

// snow: altitude line broken up by noise, flat-ish faces only, steeper faces hold snow higher up
// the snow line wanders on the LARGE scales (a mountain's snow is not polka dots); the 180 m term only frays its edge
float n = ((m4 - 0.5) * 0.45 + (m3 - 0.5) * 0.35 + (m2 - 0.5) * 0.30) * 2.0;
// ASPECT (Wes, Oct 13 2026: "less snow on the southern facing slopes, as though it's in the northern hemisphere"). The noon sun stands at -X (Ultra Dynamic Sky
// at 12:00 shines along +X, measured Oct 15), so a face that leans south has a normal pointing to -X. Sun melts the snow on it: the snow line rises on south
// faces and drops on north faces by AspectLift x the face's horizontal lean (+-300 m on a 30 deg face, +-420 m on 45 deg -- the order of the real
// north / south snow-line gap in the Alps). Flat ground is untouched. Faded to a third at range like the slope term (each coarse far triangle carries its own
// normal and would checker), so the far ranges keep a hint of north-face snow.
const float AspectLift = 60000.0;
float south = -VN.x;
float aspectW = lerp(1.0, 0.35, smoothstep(150000.0, 600000.0, d));
float h = P.z + n * SnowNoise + (m1 - 0.5) * SnowNoise * 0.6 * w1 - south * AspectLift * aspectW;
float snowH = smoothstep(SnowLine - SnowBand, SnowLine + SnowBand, h);
float up = saturate(VN.z);
float slopeMin = lerp(0.80, 0.42, saturate((h - SnowLine) / (SnowBand * 5.0)));
// far voxel LODs are coarse and each triangle has its own normal: a narrow slope window flips snow on and off triangle by triangle (a checker on the rock faces);
// the window widens with distance so the far snow follows the face's general slope
float soft = smoothstep(30000.0, 250000.0, d);
float slopeMask = smoothstep(slopeMin - 0.10 - 0.22 * soft, slopeMin + 0.06 + 0.12 * soft, up + n * 0.04);
// far voxel LODs are coarse: their per-triangle normals checker the snow into blocks, so the slope term fades out and the far snow cap follows altitude alone
slopeMask = lerp(slopeMask, 1.0, smoothstep(150000.0, 600000.0, d));
float snow = max(snowH * slopeMask * SnowAmount, Weather.z);   // the snow line's snow, or the snow that fell (MO_Weather)
c = lerp(c, SnowColor * (0.93 + 0.07 * (v * 0.5 + 0.5)), snow);
return float4(c, snow);
"""

WEATHER_CODE = """
// The weather on the ground at this pixel (BR18-16, Oct 19 2026). Wet / SnowCov = the open ground's wetness and snow cover (MPC_MOGroundWeather: the host
// advances them on the game clock at real rates and replicates them). Shelter = the voxel metadata the cave stamp writes (rock overhead: 1 in a cave,
// 0 under the sky, a fade at the mouth). Returns (wet, puddle, snow, 0).
float d = max(D, 1.0);
float open = saturate(1.0 - Shelter) * Amount;
float up = saturate(VN.z);
// where the hollows are: two reads on grids rotated against each other at unrelated scales (one axis-aligned read repeats on a grid)
float2 r1 = float2(0.80 * P.x - 0.60 * P.y, 0.60 * P.x + 0.80 * P.y);
float2 r2 = float2(0.28 * P.x + 0.96 * P.y, -0.96 * P.x + 0.28 * P.y);
float fa = NoiseTex.SampleLevel(NoiseTexSampler, r1 / 1370.0 + float2(0.41, 0.17), max(0.0, log2(d / 1370.0))).b;
float fb = NoiseTex.SampleLevel(NoiseTexSampler, r2 / 530.0 + float2(0.07, 0.66), max(0.0, log2(d / 530.0))).g;
float field = saturate(0.5 + (fa - 0.5) * 0.8 + (fb - 0.5) * 0.45);
// WET: the rain reaches every open face; a steep face sheds its water and reads a little drier
float wet = saturate(Wet) * open * lerp(0.7, 1.0, up);
// PUDDLES: standing water once the ground is soaked (wetness past half), on near-flat ground, in the hollows first; a surface that drains holds none (PuddleAmount)
float soak = smoothstep(0.5, 1.0, saturate(Wet)) * open * PuddleAmount;
float t = lerp(1.05, 0.68, soak);
float puddle = smoothstep(t, t + 0.035, field) * smoothstep(0.965, 0.992, up);
// SNOW that fell: patchy at first (the high field), whole at full coverage; it lies on faces up to ~45 deg and slides off steeper ones
float cov = saturate(SnowCov) * open;
float snow = smoothstep(1.0 - cov * 1.35, 1.18 - cov * 1.35, field) * smoothstep(0.62, 0.80, up);
return float4(wet, puddle * (1.0 - snow), snow, 0.0);
"""

WETROUGH_CODE = """
// Wet ground is glossier than the dry floor (R arrives after RoughnessMin, which Wes set to 2.0 = fully rough on every surface, Oct 14): water fills the
// micro-relief. A puddle is a near mirror; snow (Snow = all of it, the snow line's and the weather's) keeps its own roughness.
float r = saturate(R);
r = lerp(r, WetRoughness, Weather.x * (1.0 - Snow));
return lerp(r, 0.06, Weather.y);
"""

NORMAL_CODE = """
float d = max(D, 1.0);
float2 rq = float2(0.28 * P.x + 0.96 * P.y, -0.96 * P.x + 0.28 * P.y);   // rotated 73.7 deg
float a = NoiseTex.SampleLevel(NoiseTexSampler, rq / 3170.0 + float2(0.19, 0.83), max(0.0, log2(d / 3170.0))).r;
float2 rr = float2(0.80 * P.x - 0.60 * P.y, 0.60 * P.x + 0.80 * P.y);   // rotated against the first read: no shared grid
float b = NoiseTex.SampleLevel(NoiseTexSampler, rr / 21000.0 + float2(0.52, 0.27), max(0.0, log2(d / 21000.0))).g;
float t = smoothstep(0.30, 0.70, a * 0.6 + b * 0.4);
// the surface's relief is stronger in some patches and softer in others: ripples / grain stop being one repeating field. A NEAR-ground effect: from afar
// the 32 m read only printed its own period as a lattice of light / dark scales on the far cliffs (Oct 13 2026), so it fades out by ~600 m.
float vw = saturate(1.0 - d / 60000.0);
float k = lerp(1.0, lerp(1.0 - Var, 1.0 + 0.20 * Var, t), vw);
float3 nn = normalize(float3(N.xy * k, N.z));
// The detail normal maps repeat every few metres: past a few hundred metres the sun lights every tile the same way and the slope reads as a checker of
// shading (the colour fade cannot touch it, Oct 13 2026). The relief they carry is unresolvable there anyway: fade them toward the mesh normal with distance,
// by FarFlatten per surface (FarNormalFlatten): soil / grass / heath fully, cliff rock barely -- a coarse LOD's rock faces are flat-shaded triangles whose self-shadow
// stripes the detail normals hide. (Screen derivatives of the position are zero in this Nanite pass, so the steepness cannot be measured here.)
float farFlat = smoothstep(25000.0, 220000.0, d) * FarFlatten;
// snow and standing water (Puddle, MO_Weather) are flat surfaces over the relief
return normalize(lerp(nn, float3(0.0, 0.0, 1.0), max(max(Snow, Puddle), farFlat)));
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
        if "MO_Weather" not in custom_by_desc:
            # FINISH_CODE reads the Weather input: writing it into a graph without MO_Weather would break the shader
            log("!! this graph predates MO_Weather (BR18-16): `git checkout f78553e3 -- <M_QuixelBase.uasset>` with the editor closed, then rerun; nothing changed")
            return
        custom_by_desc["MO_Finish"].set_editor_property("code", FINISH_CODE)
        custom_by_desc["MO_WarpOffset"].set_editor_property("code", WARP_CODE)
        custom_by_desc["MO_NormalMod"].set_editor_property("code", NORMAL_CODE)
        custom_by_desc["MO_Weather"].set_editor_property("code", WEATHER_CODE)
        custom_by_desc["MO_WetRoughness"].set_editor_property("code", WETROUGH_CODE)
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
    p["FarNormalFlatten"] = scalar(m, "FarNormalFlatten", 0.85, X0, Y0 + 975)
    p["FarColor"] = vector(m, "FarColor", (0.030, 0.045, 0.026), X0, Y0 + 1000)
    p["SnowColor"] = vector(m, "SnowColor", (0.78, 0.82, 0.88), X0, Y0 + 1100)
    p["RoughnessMin"] = scalar(m, "RoughnessMin", 0.65, X0, Y0 + 1200)
    p["WetDarken"] = scalar(m, "WetDarken", 0.55, X0, Y0 + 1350, group="MO Weather")
    p["PuddleAmount"] = scalar(m, "PuddleAmount", 1.0, X0, Y0 + 1450, group="MO Weather")
    p["WetRoughness"] = scalar(m, "WetRoughness", 0.40, X0, Y0 + 1550, group="MO Weather")
    p["WeatherAmount"] = scalar(m, "WeatherAmount", 1.0, X0, Y0 + 1650, group="MO Weather")
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

    # 5. the weather (BR18-16): the ground's state + the cave stamp's shelter metadata -> MO_Weather (wet, puddle, snow)
    ground = ensure_ground_collection()
    ground_wet = collection_param(m, ground, "GroundWetness", X0 + 500, Y0 + 1400)
    ground_snow = collection_param(m, ground, "GroundSnow", X0 + 500, Y0 + 1550)
    shelter = MEL.create_material_expression(m, unreal.MaterialExpressionGetVoxelMetadata, X0 + 500, Y0 + 1700)
    shelter.set_editor_property("metadata", ensure_shelter_metadata())
    shelter.set_editor_property("desc", "MO_WeatherShelter")
    weather = custom(m, "MO_Weather", WEATHER_CODE, ["Wet", "SnowCov", "Shelter", "P", "D", "VN", "NoiseTex", "PuddleAmount", "Amount"],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT4, X0 + 1100, Y0 + 1400)
    connect(ground_wet, "", weather, "Wet")
    connect(ground_snow, "", weather, "SnowCov")
    connect(shelter, "", weather, "Shelter")
    connect(wp, "", weather, "P")
    connect(depth, "", weather, "D")
    connect(vn, "", weather, "VN")
    connect(tex, "", weather, "NoiseTex")
    connect(p["PuddleAmount"], "", weather, "PuddleAmount")
    connect(p["WeatherAmount"], "", weather, "Amount")

    # 6. the finish: Break(blend) -> MO_Finish -> Make
    brk = MEL.create_material_expression(m, unreal.MaterialExpressionBreakMaterialAttributes, X0 + 1500, Y0 + 0)
    brk.set_editor_property("desc", "MO_Break")
    in_names = [str(n) for n in MEL.get_material_expression_input_names(brk)]
    connect(blend, "", brk, in_names[0])
    fin = custom(m, "MO_Finish", FINISH_CODE,
                 ["Color", "P", "D", "VN", "NoiseTex", "MacroAmount", "FarAmount", "FarStart", "FarEnd", "FarColor", "SnowLine", "SnowBand", "SnowNoise", "SnowAmount", "SnowColor",
                  "Weather", "WetDarken"],
                 unreal.CustomMaterialOutputType.CMOT_FLOAT4, X0 + 1900, Y0 + 0)
    connect(weather, "", fin, "Weather")
    connect(p["WetDarken"], "", fin, "WetDarken")
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
    # Roughness floor (Oct 14 2026, playtest #8 "very shiny sand" / #22 "terrain very reflective"): the scans' roughness maps keep glossy texels that, under a
    # low sun, glint like wet glass across dry sand and soil. Dry ground is rough; per surface by RoughnessMin (sand highest, rock lowest).
    rough_floor = MEL.create_material_expression(m, unreal.MaterialExpressionMax, X0 + 2750, Y0 + 250)
    rough_floor.set_editor_property("desc", "MO_RoughnessFloor")
    connect(rough, "", rough_floor, "A")
    connect(p["RoughnessMin"], "", rough_floor, "B")

    # wet ground glossier than the floor, puddles near mirrors (after the floor: RoughnessMin 2.0 would erase it)
    wet_rough = custom(m, "MO_WetRoughness", WETROUGH_CODE, ["R", "Weather", "Snow", "WetRoughness"], unreal.CustomMaterialOutputType.CMOT_FLOAT1, X0 + 2900, Y0 + 250)
    connect(rough_floor, "", wet_rough, "R")
    connect(weather, "", wet_rough, "Weather")
    connect(snow, "", wet_rough, "Snow")
    connect(p["WetRoughness"], "", wet_rough, "WetRoughness")

    puddle = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, X0 + 2300, Y0 + 550)
    puddle.set_editor_property("r", False); puddle.set_editor_property("g", True); puddle.set_editor_property("b", False); puddle.set_editor_property("a", False)
    puddle.set_editor_property("desc", "MO_WeatherPuddle")
    connect(weather, "", puddle, "")

    nrm = custom(m, "MO_NormalMod", NORMAL_CODE, ["N", "P", "D", "NoiseTex", "Var", "Snow", "FarFlatten", "Puddle"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, X0 + 2600, Y0 + 400)
    connect(brk, "Normal", nrm, "N")
    connect(wp, "", nrm, "P")
    connect(depth, "", nrm, "D")
    connect(tex, "", nrm, "NoiseTex")
    connect(p["NormalVariation"], "", nrm, "Var")
    connect(snow, "", nrm, "Snow")
    connect(p["FarNormalFlatten"], "", nrm, "FarFlatten")
    connect(puddle, "", nrm, "Puddle")

    # pass every attribute through; replace BaseColor / Roughness / Normal
    # (MF_BaseLayer sets exactly BaseColor / Roughness / Normal / Displacement and MF_BlendLayers blends those; everything else is the default, which an unconnected Make pin also is.
    #  ShadingModel is deliberately NOT passed: the voxel generator handles the surface's shading model itself.)
    connect(brk, "Displacement", mk, "Displacement")
    connect(rgb, "", mk, "BaseColor")
    connect(wet_rough, "", mk, "Roughness")
    connect(nrm, "", mk, "Normal")
    MEL.connect_material_property(mk, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)

    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("M_QuixelBase upgraded (%d expressions) and saved" % len(list(MEL.get_material_expressions(m))))
    apply_surface_overrides()


# What covers the ground seen from far away (linear colours), per surface material instance: FarTintAmount, FarColor, + optional per-surface knobs.
# Forest floors become canopy (dark, slightly different greens), grass stays lush, rock / sand / gravel barely shift.
# Weather (BR18-16): wet = WetDarken (the colour a soaked surface keeps: soil / litter 0.55, grass 0.62, rock 0.72, sand 0.50 -- wet sand darkens most), puddle = PuddleAmount
# (how readily standing water forms on its flat ground: sand drains, gravel and pebbles mostly do, grass and rock hold some, soil and litter the most). Defaults 0.55 / 1.0.
# Far tint (Oct 13 evening): the open-ground and rock surfaces now fade toward their mean colour from 400-500 m (was 1.5-2 km, 0.30-0.55): their few-metre texture tiles
# repeat hundreds of times across a far slope and read as a checker; the far colour keeps the (non-periodic) macro variation, so the distance does not go flat.
SURFACES = {
    "MuddyLeaves":          dict(far=0.95, color=(0.020, 0.034, 0.016), start=90000.0,  end=500000.0),
    "ForestLitter":         dict(far=0.95, color=(0.022, 0.036, 0.018), start=90000.0,  end=500000.0),
    "JungleFloor":          dict(far=0.95, color=(0.014, 0.032, 0.013), start=90000.0,  end=450000.0),
    "MossyGrass":           dict(far=0.70, color=(0.060, 0.085, 0.030), start=40000.0,  end=300000.0, wet=0.62, puddle=0.6),
    "DriedGrassOnDampSoil": dict(far=0.65, color=(0.110, 0.090, 0.050), start=40000.0,  end=300000.0, wet=0.62, puddle=0.6),
    "AlpineHeath":          dict(far=0.65, color=(0.070, 0.074, 0.040), start=40000.0,  end=300000.0, wet=0.62, puddle=0.6),
    "NordicMoss":           dict(far=0.65, color=(0.052, 0.066, 0.036), start=40000.0,  end=300000.0, wet=0.62, puddle=0.6),
    "MossyRockyGround":     dict(far=0.65, color=(0.058, 0.062, 0.048), start=40000.0,  end=300000.0, wet=0.60, puddle=0.7),
    "IcelandicQuarryRock":  dict(far=0.55, color=(0.040, 0.040, 0.045), start=50000.0,  end=350000.0, far_normal=0.15, wet=0.72, puddle=0.5),
    "Cliff":                dict(far=0.55, color=(0.075, 0.068, 0.060), start=50000.0,  end=350000.0, far_normal=0.15, wet=0.72, puddle=0.5),
    "Gravel":               dict(far=0.55, color=(0.090, 0.090, 0.090), start=50000.0,  end=350000.0, wet=0.60, puddle=0.35),
    "DarkPebbles":          dict(far=0.55, color=(0.060, 0.060, 0.062), start=50000.0,  end=350000.0, wet=0.60, puddle=0.35),
    "Sand":                 dict(far=0.25, color=(0.300, 0.240, 0.150), start=200000.0, end=700000.0, macro=0.50, normal_var=0.45, warp=70.0, warp_scale=1500.0, wet=0.50, puddle=0.0),
}


# Roughness floor (RoughnessMin) for every surface: Wes set 2.0 on all of them in the editor (Oct 14 2026, "it cut down the glare on everything") -- anything >= 1
# means fully rough ground, no sheen at all. This script re-applies it, so a rebuild keeps his setting (the per-surface 0.6-0.85 floors it replaced still glinted).
ROUGHNESS_MIN_ALL = 2.0
ROUGHNESS_MIN = {}   # per-surface exceptions (none)


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
        MEL.set_material_instance_scalar_parameter_value(mi, "FarNormalFlatten", v.get("far_normal", 0.85))
        MEL.set_material_instance_scalar_parameter_value(mi, "RoughnessMin", ROUGHNESS_MIN.get(name, ROUGHNESS_MIN_ALL))
        MEL.set_material_instance_scalar_parameter_value(mi, "WetDarken", v.get("wet", 0.55))
        MEL.set_material_instance_scalar_parameter_value(mi, "PuddleAmount", v.get("puddle", 1.0))
        if "normal_var" in v:
            MEL.set_material_instance_scalar_parameter_value(mi, "NormalVariation", v["normal_var"])
        if "warp" in v:
            MEL.set_material_instance_scalar_parameter_value(mi, "WarpAmountCm", v["warp"])
            MEL.set_material_instance_scalar_parameter_value(mi, "WarpScaleCm", v["warp_scale"])
        EAL.save_loaded_asset(mi)
    log("surface overrides applied to %d material instances" % len(SURFACES))
    # Then what was tuned in the World look lab (MO_WorldLab, Tools/apply_worldlab_patch.py): last, so a rebuild keeps it over the table above.
    import importlib
    import terrain_surface_overrides
    importlib.reload(terrain_surface_overrides).apply(log)


main()
