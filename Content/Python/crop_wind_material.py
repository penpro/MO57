"""The crops' own foliage master: M_MS_Foliage's shading with a wind of non-uniform flickers instead of the slow sway.

    python Tools/ue.py py --file Content/Python/crop_wind_material.py      (editor open, NOT in PIE: it saves)

Why (Wes, Nov 4 2026: "all the crops move WAY too much ... they should move about 20% as much and a little faster, not a long swaying motion
like it's a ship at sea, little ripples"; then "small plants don't move like that, it's all too much and too slow, it should be non-uniform
flickers here and there, not a constant slow swaying movement"). The grass's master takes its wind speed from the world's MPC (Wind Speed
Plants, shared by every plant): its per-material knobs only scale the slow sway (measured with Tools/motion_probe.py, Nov 4: under 3 % of the
motion above 0.5 Hz whatever the strengths). So /Game/ImageTo3D/Crops/M_MO_CropFoliage is a copy of /Game/Materials/FabOverrides/M_MS_Foliage
(all its two-sided foliage shading and parameters) whose World Position Offset is rebuilt here:

    offset.xy = dir(PerInstanceRandom) * Amplitude * (Wind Strength Plants / 2) * A^2 * gate * flicker,  offset.z = 0
    flicker   = 0.6 sin(2 pi (2.0 t + 7 r)) + 0.4 sin(2 pi (3.3 t + 11 (r + G)))     r = PerInstanceRandom, G / A = vertex colour
    gate      = saturate((noise(fmod(world, 4000) / GustSize + drift t) - GustThreshold) x GustContrast)

  - the flutter is fast (2.0 and 3.3 Hz) and differs per plant and per part, so neighbours do not move together;
  - the gate is a slowly drifting noise field: at any moment only the plants in its patches flicker ("here and there");
  - A^2 keeps the base still and moves the heads (crop_finish_blender.py bakes A = height);
  - Wind Strength Plants (the world's, default 2) scales it: a storm moves crops more;
  - precision (Docs/PCG.md section 9): the world position is reduced (fmod 4000) BEFORE time is added, and Time wraps every 600 s, a whole
    number of cycles of both waves (1200 and 1980), so the flutter never jumps; the gust field reshuffles once in 10 minutes.
Parameters on the instances: Crop Wind Amplitude (cm, default 3), Crop Gust Size (cm, default 700), Crop Gust Threshold (0-1, default 0.5:
higher = fewer plants moving), Crop Gust Contrast (default 8: how sharply a patch switches on). Idempotent: rebuilds the copy's offset each run. Output lines start with CROPWIND.
"""
import unreal

SRC = "/Game/Materials/FabOverrides/M_MS_Foliage"
DST = "/Game/ImageTo3D/Crops/M_MO_CropFoliage"
MPC = "/Fab/MaterialParameterCollection/QMPC_GlobalFoliageActor"
TAG = "MO_CropWind"   # every node this script makes carries it (desc), so a rerun removes the old set first

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def main():
    if not eal.does_asset_exist(DST):
        eal.duplicate_asset(SRC, DST)
    m = unreal.load_asset(DST)
    for e in list(mel.get_material_expressions(m)):
        try:
            if e.get_editor_property("desc") == TAG:
                mel.delete_material_expression(m, e)
        except Exception:
            pass
    y = [3000]

    def node(cls, **props):
        y[0] += 120
        e = mel.create_material_expression(m, cls, -1800, y[0])
        e.set_editor_property("desc", TAG)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            raise RuntimeError("connect %s.%s -> %s.%s failed" % (a.get_class().get_name(), a_out, b.get_class().get_name(), b_in))

    def const(v):
        return node(unreal.MaterialExpressionConstant, r=v)

    def binop(cls, a, b, a_out="", b_out=""):
        e = node(cls)
        link(a, a_out, e, "A")
        link(b, b_out, e, "B")
        return e

    t = node(unreal.MaterialExpressionTime, override_period=True, period=600.0)
    r = node(unreal.MaterialExpressionPerInstanceRandom)
    vc = node(unreal.MaterialExpressionVertexColor)
    amp = node(unreal.MaterialExpressionScalarParameter, parameter_name="Crop Wind Amplitude", default_value=3.0)
    gsize = node(unreal.MaterialExpressionScalarParameter, parameter_name="Crop Gust Size", default_value=700.0)
    gthr = node(unreal.MaterialExpressionScalarParameter, parameter_name="Crop Gust Threshold", default_value=0.5)
    gcon = node(unreal.MaterialExpressionScalarParameter, parameter_name="Crop Gust Contrast", default_value=8.0)
    strength = node(unreal.MaterialExpressionCollectionParameter, collection=unreal.load_asset(MPC), parameter_name="Wind Strength Plants")

    # gate: a drifting noise field over the reduced world position
    wp = node(unreal.MaterialExpressionWorldPosition)
    fm = binop(unreal.MaterialExpressionFmod, wp, const(4000.0))
    gp = binop(unreal.MaterialExpressionDivide, fm, gsize)
    drift = binop(unreal.MaterialExpressionMultiply, t, node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.12, 0.07, 0.05, 0.0)))
    moving = binop(unreal.MaterialExpressionAdd, gp, drift)
    noise = node(unreal.MaterialExpressionNoise, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, scale=1.0, levels=2,
                 output_min=0.0, output_max=1.0, turbulence=False)
    link(moving, "", noise, "World Position")
    over = binop(unreal.MaterialExpressionSubtract, noise, gthr)
    gate = node(unreal.MaterialExpressionSaturate)
    # x8 about 0.5: gradient noise clusters round 0.5, so 0.55 with x4 (the first build) left the gate shut nearly everywhere (Nov 4:
    # per-pixel activity at the still floor; forced open it flickered)
    link(binop(unreal.MaterialExpressionMultiply, over, gcon), "", gate, "")

    # flicker: two fast waves, phased per plant (r) and per part (vertex G)
    s1 = node(unreal.MaterialExpressionSine, period=1.0)
    link(binop(unreal.MaterialExpressionAdd, binop(unreal.MaterialExpressionMultiply, t, const(2.0)), binop(unreal.MaterialExpressionMultiply, r, const(7.0))), "", s1, "")
    rg = binop(unreal.MaterialExpressionAdd, r, vc, b_out="G")
    s2 = node(unreal.MaterialExpressionSine, period=1.0)
    link(binop(unreal.MaterialExpressionAdd, binop(unreal.MaterialExpressionMultiply, t, const(3.3)), binop(unreal.MaterialExpressionMultiply, rg, const(11.0))), "", s2, "")
    flick = binop(unreal.MaterialExpressionAdd, binop(unreal.MaterialExpressionMultiply, s1, const(0.6)), binop(unreal.MaterialExpressionMultiply, s2, const(0.4)))

    # magnitude: amplitude x the world's plant wind (default 2 -> 1) x height^2 x gate x flicker
    h2 = binop(unreal.MaterialExpressionMultiply, vc, vc, a_out="A", b_out="A")
    world = binop(unreal.MaterialExpressionMultiply, strength, const(0.5))
    mag = binop(unreal.MaterialExpressionMultiply,
                binop(unreal.MaterialExpressionMultiply, binop(unreal.MaterialExpressionMultiply, amp, world), h2),
                binop(unreal.MaterialExpressionMultiply, gate, flick))

    # direction: per plant, in the horizontal plane
    cx = node(unreal.MaterialExpressionCosine, period=1.0)
    link(r, "", cx, "")
    sy = node(unreal.MaterialExpressionSine, period=1.0)
    link(r, "", sy, "")
    d2 = binop(unreal.MaterialExpressionAppendVector, cx, sy)
    off2 = binop(unreal.MaterialExpressionMultiply, d2, mag)
    off3 = binop(unreal.MaterialExpressionAppendVector, off2, const(0.0))
    mel.connect_material_property(off3, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    m.set_editor_property("max_world_position_offset_displacement", 12.0)   # Nanite bounds: amplitude 3 x strength up to ~4 / 2 x 1
    mel.recompile_material(m)
    saved = eal.save_asset(DST, only_if_is_dirty=False)
    out("CROPWIND %s rebuilt: %d nodes tagged %s, saved %s" % (DST, sum(1 for e in mel.get_material_expressions(m)
                                                                       if e.get_editor_property("desc") == TAG), TAG, saved))


main()
