"""M_MO_SmokeWisp: a thin column of smoke, for a camera-facing strip (Wes, Nov 5 2026, on the campfire: "turn down the smoke effects a LOT, it's fucking massive and a giant wide cone, and make it slower").

    python Tools/ue.py py --file Content/Python/make_smoke_wisp.py      (editor open, NOT in PIE: it saves)

Why a material and not a Niagara system: the M5 pack's smokes are flipbooks of big cauliflower puffs with no size, density or speed parameter, and scaled down they read as a small ball of cotton (Nov 5, a row of real
fires). Python cannot author Niagara modules. So: one translucent strip, its opacity a column that meanders and widens as it rises, torn by two panning noises and fading out at the top. It is cheap (one quad), and every
number a note can ask about is a parameter: Wisp Speed (how fast it rises), Wisp Opacity (how much there is), Wisp Gray, and the shape: Wisp Foot and Spread (half width at the foot and how it grows, of the strip), Wander, Scale, Gain, Bias (the noises). A MaterialInstanceDynamic on a live fire tunes them without a rebuild; the defaults came from real fires at dusk, Nov 5.
u, v are the strip's own; h = 1 - v is the height up it. Built once: delete the .uasset on disk with the editor closed to change the graph (TRAPS).
Output lines start with SMOKEWISP.
"""
import unreal

PATH = "/Game/Materials/FX/M_MO_SmokeWisp"
NOISE = "/Engine/MaterialTemplates/Textures/T_Noise01"
DEFAULTS = {"Wisp Speed": 0.045, "Wisp Opacity": 0.6, "Wisp Gray": 0.7, "Wisp Foot": 0.05, "Wisp Spread": 0.3, "Wisp Wander": 0.45, "Wisp Scale": 0.3, "Wisp Gain": 2.2, "Wisp Bias": 0.5}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal


def build():
    folder, name = PATH.rsplit("/", 1)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)
    noise = unreal.load_asset(NOISE)
    pos = [0]

    def node(cls, **props):
        pos[0] += 1
        e = mel.create_material_expression(m, cls, -1500 + (pos[0] % 8) * 180, -600 + (pos[0] // 8) * 200)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            raise RuntimeError("link failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def const(v):
        return node(E.MaterialExpressionConstant, r=float(v))

    def param(n):
        return node(E.MaterialExpressionScalarParameter, parameter_name=n, default_value=float(DEFAULTS[n]))

    def bin_(cls, a, b):
        e = node(cls)
        a = const(a) if isinstance(a, (int, float)) else a
        b = const(b) if isinstance(b, (int, float)) else b
        link(a, "", e, "A")
        link(b, "", e, "B")
        return e

    def mul(a, b):
        return bin_(E.MaterialExpressionMultiply, a, b)

    def add(a, b):
        return bin_(E.MaterialExpressionAdd, a, b)

    def sub(a, b):
        return bin_(E.MaterialExpressionSubtract, a, b)

    def div(a, b):
        return bin_(E.MaterialExpressionDivide, a, b)

    def sat(a):
        e = node(E.MaterialExpressionSaturate)
        link(a, "", e, "")
        return e

    def pow_(a, p):
        e = node(E.MaterialExpressionPower)
        c = const(p)
        link(a, "", e, "Base")
        link(c, "", e, "Exp")
        return e

    def one_minus(a):
        e = node(E.MaterialExpressionOneMinus)
        link(a, "", e, "")
        return e

    def abs_(a):
        e = node(E.MaterialExpressionAbs)
        link(a, "", e, "")
        return e

    def append(a, b):
        return bin_(E.MaterialExpressionAppendVector, a, b)

    def tex(uv):
        e = node(E.MaterialExpressionTextureSample, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        e.set_editor_property("texture", noise)
        link(uv, "", e, "UVs")
        r = node(E.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
        link(e, "", r, "")
        return r

    uv = node(E.MaterialExpressionTextureCoordinate)
    u = node(E.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    link(uv, "", u, "")
    v = node(E.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
    link(uv, "", v, "")
    h = one_minus(v)
    time = node(E.MaterialExpressionTime)
    t = mul(time, param("Wisp Speed"))

    # the column's centre wanders more the higher it is; it widens as it rises
    wander = tex(append(const(0.37), sub(mul(h, 0.55), t)))
    centre = add(0.5, mul(mul(sub(wander, 0.5), param("Wisp Wander")), h))
    width = add(param("Wisp Foot"), mul(param("Wisp Spread"), pow_(h, 1.1)))
    off = sub(u, centre)
    edge = sat(sub(1.0, div(abs_(off), width)))
    # torn by two noises rising at different speeds
    scale = param("Wisp Scale")
    n1 = tex(append(add(mul(off, scale), 0.2), sub(mul(h, mul(scale, 0.7)), mul(t, 1.5))))
    n2 = tex(append(add(mul(off, mul(scale, 2.7)), 0.6), sub(mul(h, mul(scale, 1.9)), mul(t, 2.6))))
    body = sat(sub(mul(add(n1, n2), param("Wisp Gain")), param("Wisp Bias")))
    # in at the foot, thinning to nothing at the top
    fade = mul(sat(mul(h, 12.0)), pow_(one_minus(h), 1.7))
    opacity = mul(mul(mul(param("Wisp Opacity"), edge), body), fade)

    gray = param("Wisp Gray")
    if not mel.connect_material_property(gray, "", unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("base colour")
    if not mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("opacity")
    mel.recompile_material(m)
    return m


if eal.does_asset_exist(PATH):
    out("SMOKEWISP %s kept (delete it on disk with the editor closed to change the graph)" % PATH)
else:
    m = build()
    out("SMOKEWISP %s built, saved %s" % (PATH, eal.save_asset(PATH, only_if_is_dirty=False)))
