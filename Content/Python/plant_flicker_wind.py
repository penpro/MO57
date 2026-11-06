"""One wind for every small plant: non-uniform flickers here and there, not a constant slow sway. Trees keep their sway.

    python Tools/ue.py py --file Content/Python/plant_flicker_wind.py      (editor open, NOT in PIE: it saves)

Why (Wes, Nov 4 2026: "all the crops move WAY too much, it looks like the grass and shit is also moving a lot ... little ripples"; then "small
plants don't move like that, it's all too much and too slow, it should be non-uniform flickers here and there, not a constant slow swaying
movement"; "Yes, do the grass" after the crops' clip). The small plants came with four different vendor winds (Fab QMF_WindAnimation, PN_WindAnimation,
MF_SimpleWind, MF_AdvancedWind), whose speed is the world's and whose per-material knobs only scale a slow sway (motion_probe, Nov 4: under 3 % of the
motion above 0.5 Hz whatever the strengths). So they all now take /Game/Materials/Wind/MF_MO_PlantFlicker, one function, the same on every mesh:

    offset.xy = dir * Strength * (min(z, HeightCap)^1.5 / 10) * (Wind Strength Plants / 2) * gate * fade * flicker,  offset.z = 0
    flicker   = 0.6 sin(2 pi (2.0 t + 7 r + 0.6 part)) + 0.4 sin(2 pi (3.3 t + 11 r + part))
    dir       = (cos 2 pi (r + 0.25 part), sin 2 pi (r + 0.25 part))
    part      = 0.077 x + 0.059 y          (cycles: parts of a plant ~10 cm apart move differently, without tearing a blade)
    z         = the vertex's height above the plant's pivot, cm (instance-local position: no vertex-colour convention, every pack paints them differently; no Bounds
                node either, primitive data that falls back to the whole component's bounds on a proxy built with no instances. 291 of the 311 small meshes have the
                pivot at the base; the other 20 are a few cm off, and z below the pivot is held at 0)
    gate      = saturate((noise(fmod(plant origin, 4000) / GustSize + drift t) + 0.25 (r - 0.5) - GustThreshold) x GustContrast), one value per plant
    fade      = 1 up to FadeStart from the camera, 0 at FadeEnd (under a pixel out there, and it would only shimmer)

  - a plant's reach follows its size and the base stays still, a thin stem's bend: 2.7 cm at the tip of a 85 cm barley, 0.6 cm on a 30 cm tuft (Strength 0.035), capped at
    3.5 cm (1 m up) so a 4 m sapling flutters and does not wave;
    broad leaves and fronds (ferns, elderberry, seedlings, saplings) take 0.006: the same centimetres on a whole frond a metre from the camera read as shaking
    (Nov 4: the rainforest's fern was nearly still under its old wind, 0.40 levels / frame; at 0.015 it moved 1.34);
  - the gate is a noise field whose patches are about a tuft's neighbourhood (1.5 m) and last 2-3 s, broken up plant by plant: at any moment only some plants
    flicker, and a plant's neighbour may be still ("here and there"). The crops' first wind used 7 m patches, which fill the view at a camera's distance;
  - Wind Strength Plants (the world's MPC, default 2) scales it: a stronger wind moves them more;
  - precision (Docs/PCG.md section 9): the world position is reduced (fmod 4000) BEFORE time is added, and Time wraps every 600 s, a whole number of
    cycles of both waves (1200 and 1980), so the flutter never jumps; the gust field reshuffles once in 10 minutes.

Which plants:
  - masters with only small plants under them take it outright: M_MS_Foliage (grass, wild grass, dry and thatching grass, dead shrubs, raspberry,
    knee-high locust, the crops), MA_Flowers, M_MS_CustomFoliage and ...NoOpacity (ferns, elderberry). The vendor graph stays in the asset, unconnected.
  - masters shared with trees get the static switch "Small Plant Flicker" (default off: M_Foliage, M_Foliage_TwoSided, MA_Foliage), set on the
    instances of the seedling and sapling meshes only. An instance shared with a tree mesh gets a _SmallPlant child for the small meshes.
  - M_MS_Billboard is left alone: its slot is on the far LODs only, which a Nanite mesh never draws; its offset also faces the card to the camera.
Rerun after reimporting a vendor master or adding small plants, or to apply new DEFAULTS. Idempotent. Output lines start with PLANTFLICKER. Test: MOFramework.Rendering.SmallPlantsFlicker.
Parameters, on any instance: Flicker Strength, Flicker Height Cap, Flicker Gust Size, Flicker Gust Drift, Flicker Gust Threshold (higher = fewer plants moving), Flicker Gust Contrast, Flicker Fade Start / End.
"""
import unreal

FUNC_DIR, FUNC_NAME = "/Game/Materials/Wind", "MF_MO_PlantFlicker"
FUNC = FUNC_DIR + "/" + FUNC_NAME
MPC = "/Fab/MaterialParameterCollection/QMPC_GlobalFoliageActor"
TAG = "MO_PlantFlicker"
SWITCH = "Small Plant Flicker"
SMALL_ONLY = (
    "/Game/Materials/FabOverrides/M_MS_Foliage",
    "/Game/PN_MeadowFlowers/Materials/flowerMaterials/MA_Flowers",
    "/Game/Custom/CustomMaterials/Materials/Master_Materials/Custom_MS_Master/M_MS_Foliage_Overwrite/M_MS_CustomFoliage",
    "/Game/Custom/CustomMaterials/Materials/Master_Materials/Custom_MS_Master/M_MS_Foliage_Overwrite/M_MS_CustomFoliageNoOpacity",
)
WITH_TREES = (
    "/Game/MSPresets/MS_Foliage_Material_LATEST/MasterMaterials/M_Foliage",
    "/Game/MSPresets/MS_Foliage_Material_LATEST/MasterMaterials/M_Foliage_TwoSided",
    "/Game/MSPresets/MS_Foliage_Material/MasterMaterials/MA_Foliage",
)
WOODY_FOLDERS = ("/Game/BlackAlder/Geometry", "/Game/CommonHazel/Geometry", "/Game/Megascans/3D_Plants/EuropeanHornbeam/Geometry")
SMALL_WORDS = ("Seedling", "Sapling")
GRAPH_VERSION = 3
DEFAULTS = {"Flicker Strength": 0.035, "Flicker Height Cap": 100.0, "Flicker Gust Size": 150.0, "Flicker Gust Threshold": 0.5, "Flicker Gust Contrast": 8.0,
            "Flicker Fade Start": 2000.0, "Flicker Fade End": 4000.0, "Flicker Gust Drift": unreal.LinearColor(0.35, 0.22, 0.15, 0.0)}
BROAD_LEAF_STRENGTH = 0.006  # on instances, over the function's 0.035: M_MS_CustomFoliage* and the seedlings and saplings
BROAD_MASTERS = ("M_MS_CustomFoliage", "M_MS_CustomFoliageNoOpacity")
# Wes, Nov 5 (the knee-high locust in the promo's co-op shot): "the wind animation on the bush looks spazzy and jank so dial down the bush wind like we did for the grass".
# Whole leafy branches a metre from the camera read like the ferns did, so the shrubs on M_MS_Foliage take the broad leaves' value (0.6 cm at the cap, not 3.5); the grass and the crops keep 0.035.
BUSH_FOLDERS = ("/Game/Fab/Megascans/Plants/Black_Locust_wdvhberja", "/Game/Fab/Megascans/Plants/Raspberry_wf0oefeja", "/Game/Fab/Megascans/Plants/Dead_Shrubs_vdknafgha")
MAX_DISPLACEMENT = 12.0   # Nanite bounds: 3.5 cm at the cap x Wind Strength Plants up to ~6 / 2 (M_MS_CustomFoliage's 3 would clip it)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
PROP = unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET


def build_function():
    """Built once per GRAPH_VERSION (TRAPS: deleting nodes of a material loaded from disk takes the editor down, delete_all leaves half of them behind).
    A new graph: bump GRAPH_VERSION, delete Content/Materials/Wind/MF_MO_PlantFlicker.uasset on disk with the editor closed, rerun. New defaults need neither."""
    if eal.does_asset_exist(FUNC):
        f = unreal.load_asset(FUNC)
        have = eal.get_metadata_tag(f, "MOPlantFlickerVersion")
        if have != str(GRAPH_VERSION):
            raise RuntimeError("MF_MO_PlantFlicker is graph version %r, this script builds %d: delete Content/Materials/Wind/MF_MO_PlantFlicker.uasset on disk with the editor closed, then rerun" % (have, GRAPH_VERSION))
        n = 0
        for e in mel.get_material_function_expressions(f):
            cls = e.get_class().get_name()
            if cls in ("MaterialExpressionScalarParameter", "MaterialExpressionVectorParameter") and str(e.get_editor_property("parameter_name")) in DEFAULTS:
                e.set_editor_property("default_value", DEFAULTS[str(e.get_editor_property("parameter_name"))])
                n += 1
        mel.update_material_function(f)
        out("PLANTFLICKER function %s: graph %d kept, %d defaults set, saved %s" % (FUNC, GRAPH_VERSION, n, eal.save_asset(FUNC, only_if_is_dirty=False)))
        return f
    f = unreal.AssetToolsHelpers.get_asset_tools().create_asset(FUNC_NAME, FUNC_DIR, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    eal.set_metadata_tag(f, "MOPlantFlickerVersion", str(GRAPH_VERSION))
    f.set_editor_property("description","Small plants' wind: non-uniform flickers here and there. Content/Python/plant_flicker_wind.py builds it; Docs/PCG.md section 9.")
    y = [0]

    def node(cls, **props):
        y[0] += 110
        e = mel.create_material_expression_in_function(f, cls, -1800, y[0])
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            raise RuntimeError("connect %s.%s -> %s.%s failed" % (a.get_class().get_name(), a_out, b.get_class().get_name(), b_in))

    def const(v):
        return node(unreal.MaterialExpressionConstant, r=v)

    def param(name):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=DEFAULTS[name], group="Flicker Wind")

    def binop(cls, a, b, a_out="", b_out=""):
        e = node(cls)
        link(a, a_out, e, "A")
        link(b, b_out, e, "B")
        return e

    def mul(*xs):
        r = xs[0]
        for x in xs[1:]:
            r = binop(unreal.MaterialExpressionMultiply, r, x)
        return r

    def add(a, b):
        return binop(unreal.MaterialExpressionAdd, a, b)

    def mask(src, out_name="", r=False, g=False, b=False):
        e = node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False)
        link(src, out_name, e, "")
        return e

    def saturate(x):
        e = node(unreal.MaterialExpressionSaturate)
        link(x, "", e, "")
        return e

    def wave(cls, x):
        e = node(cls, period=1.0)
        link(x, "", e, "")
        return e

    t = node(unreal.MaterialExpressionTime, override_period=True, period=600.0)
    r = node(unreal.MaterialExpressionPerInstanceRandom)
    lp = node(unreal.MaterialExpressionLocalPosition, included_offsets=unreal.PositionIncludedOffsets.EXCLUDE_OFFSETS, local_origin=unreal.LocalPositionOrigin.INSTANCE)

    # reach: grows with the height above the pivot (a stem's bend), capped; the world's plant wind scales it
    above = binop(unreal.MaterialExpressionMax, mask(lp, b=True), const(0.0))
    capped = binop(unreal.MaterialExpressionMin, above, param("Flicker Height Cap"))
    bend = node(unreal.MaterialExpressionPower, const_exponent=1.5)
    link(capped, "", bend, "Base")
    strength = node(unreal.MaterialExpressionCollectionParameter, collection=unreal.load_asset(MPC), parameter_name="Wind Strength Plants")
    reach = mul(param("Flicker Strength"), bend, const(0.1), mul(strength, const(0.5)))

    # the plant's own origin: one gate and one fade per plant. Instance space, not Local: Local is the component's, and a whole PCG cell then shared one gate and
    # one fade from its corner (Nov 4: DryScrub's plants read 0.14 levels / frame, 1.24 with the gate and fade forced open; Meadow's whole view flickered together)
    origin = node(unreal.MaterialExpressionTransformPosition, transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_INSTANCE,
                  transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    link(node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 0.0, 0.0)), "", origin, "")

    # gate: a drifting noise field over the reduced world position (gradient noise clusters round 0.5: threshold 0.5 x 8, Nov 4)
    fm = binop(unreal.MaterialExpressionFmod, origin, const(4000.0))
    gp = binop(unreal.MaterialExpressionDivide, fm, param("Flicker Gust Size"))
    drift = mul(t, node(unreal.MaterialExpressionVectorParameter, parameter_name="Flicker Gust Drift", default_value=DEFAULTS["Flicker Gust Drift"], group="Flicker Wind"))
    noise = node(unreal.MaterialExpressionNoise, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, scale=1.0, levels=1, output_min=0.0, output_max=1.0, turbulence=False)
    link(add(gp, drift), "", noise, "World Position")
    jitter = mul(binop(unreal.MaterialExpressionSubtract, r, const(0.5)), const(0.25))
    over = binop(unreal.MaterialExpressionSubtract, add(noise, jitter), param("Flicker Gust Threshold"))
    gate = saturate(mul(over, param("Flicker Gust Contrast")))

    # fade with the distance from the camera
    d = binop(unreal.MaterialExpressionDistance, origin, node(unreal.MaterialExpressionCameraPositionWS))
    fs, fe = param("Flicker Fade Start"), param("Flicker Fade End")
    fade = saturate(binop(unreal.MaterialExpressionDivide, binop(unreal.MaterialExpressionSubtract, fe, d), binop(unreal.MaterialExpressionSubtract, fe, fs)))

    # the flutter: two fast waves, phased per plant (r) and per part of the plant
    part = add(mul(mask(lp, r=True), const(0.077)), mul(mask(lp, g=True), const(0.059)))
    s1 = wave(unreal.MaterialExpressionSine, add(add(mul(t, const(2.0)), mul(r, const(7.0))), mul(part, const(0.6))))
    s2 = wave(unreal.MaterialExpressionSine, add(add(mul(t, const(3.3)), mul(r, const(11.0))), part))
    flick = add(mul(s1, const(0.6)), mul(s2, const(0.4)))

    # direction: horizontal, per plant, turning a little across it
    angle = add(r, mul(part, const(0.25)))
    d2 = binop(unreal.MaterialExpressionAppendVector, wave(unreal.MaterialExpressionCosine, angle), wave(unreal.MaterialExpressionSine, angle))
    off2 = mul(d2, mul(reach, gate, fade, flick))
    off3 = binop(unreal.MaterialExpressionAppendVector, off2, const(0.0))
    output = node(unreal.MaterialExpressionFunctionOutput, output_name="Offset")
    link(off3, "", output, "")
    mel.update_material_function(f)
    saved = eal.save_asset(FUNC, only_if_is_dirty=False)
    out("PLANTFLICKER function %s: %d nodes, saved %s" % (FUNC, len(mel.get_material_function_expressions(f)), saved))
    return f


def tagged(m, cls):
    """This script's node of a class in a master (nodes of a loaded material cannot be deleted: TRAPS, so a rerun reuses them)."""
    for e in mel.get_material_expressions(m):
        if e.get_class().get_name() == cls and e.get_editor_property("desc") == TAG:
            return e
    return None


def flicker_call(m, func):
    e = tagged(m, "MaterialExpressionMaterialFunctionCall")
    if not e:
        e = mel.create_material_expression(m, unreal.MaterialExpressionMaterialFunctionCall, -1800, 3000)
        e.set_editor_property("desc", TAG)
    e.set_editor_property("material_function", func)
    return e


def named_inputs(m, e):
    return dict(zip([str(n) for n in mel.get_material_expression_input_names(e)], mel.get_inputs_for_material_expression(m, e)))


def wire_small_only(path, func):
    m = unreal.load_asset(path)
    if not mel.connect_material_property(flicker_call(m, func), "", PROP):
        raise RuntimeError("connect WPO failed on " + path)
    m.set_editor_property("max_world_position_offset_displacement", MAX_DISPLACEMENT)
    mel.recompile_material(m)
    out("PLANTFLICKER %s: World Position Offset = the flicker, saved %s" % (m.get_name(), eal.save_asset(path, only_if_is_dirty=False)))


def wire_with_trees(path, func):
    """World Position Offset = switch(flicker, what it was). A rerun only refreshes the call."""
    m = unreal.load_asset(path)
    sw = tagged(m, "MaterialExpressionStaticSwitchParameter")
    if sw:
        if not mel.connect_material_expressions(flicker_call(m, func), "", sw, "True"):
            raise RuntimeError("wiring the switch failed on " + path)
        mel.recompile_material(m)
        out("PLANTFLICKER %s: switch \"%s\" kept, saved %s" % (m.get_name(), SWITCH, eal.save_asset(path, only_if_is_dirty=False)))
        return
    attrs = m.get_editor_property("use_material_attributes")
    if attrs:
        sink = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
        pin = next(n for n in (str(x) for x in mel.get_material_expression_input_names(sink)) if n.replace(" ", "").lower() == "worldpositionoffset")
        src = named_inputs(m, sink)[pin]
        src_out = mel.get_input_node_output_name_for_material_expression(sink, src) if src else ""
    else:
        sink, pin = None, None
        src = mel.get_material_property_input_node(m, PROP)
        src_out = mel.get_material_property_input_node_output_name(m, PROP) if src else ""
    if not src:
        raise RuntimeError("no wind on " + path)
    sw = mel.create_material_expression(m, unreal.MaterialExpressionStaticSwitchParameter, -1500, 3000)
    sw.set_editor_property("desc", TAG)
    sw.set_editor_property("parameter_name", SWITCH)
    sw.set_editor_property("default_value", False)
    ok = mel.connect_material_expressions(flicker_call(m, func), "", sw, "True") and mel.connect_material_expressions(src, str(src_out), sw, "False")
    ok = ok and (mel.connect_material_expressions(sw, "", sink, pin) if attrs else mel.connect_material_property(sw, "", PROP))
    if not ok:
        raise RuntimeError("wiring the switch failed on " + path)
    mel.recompile_material(m)
    out("PLANTFLICKER %s: switch \"%s\" over %s.%s (attributes %s), saved %s" % (m.get_name(), SWITCH, src.get_class().get_name().replace("MaterialExpression", ""), src_out, attrs,
                                                                               eal.save_asset(path, only_if_is_dirty=False)))


def path_of(obj):
    return obj.get_path_name().split(".")[0]


def set_small_woody():
    """Every slot of every seedling and sapling mesh: the switch on. An instance a tree mesh also uses gets a _SmallPlant child."""
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "StaticMesh")], package_paths=list(WOODY_FOLDERS), recursive_paths=True)
    small, tree_mis = [], set()
    for a in reg.get_assets(flt):
        n = str(a.asset_name)
        m = a.get_asset()
        if any(w in n for w in SMALL_WORDS):
            small.append(m)
        else:
            for i in range(len(m.get_editor_property("static_materials"))):
                mi = m.get_material(i)
                if mi:
                    tree_mis.add(path_of(mi))
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    done, children, meshes_changed = set(), 0, 0
    for m in small:
        changed = False
        for i in range(len(m.get_editor_property("static_materials"))):
            mi = m.get_material(i)
            if not mi or not isinstance(mi, unreal.MaterialInstanceConstant) or path_of(mi.get_base_material()) not in WITH_TREES:
                continue
            if path_of(mi) in tree_mis:
                folder, name = path_of(mi).rsplit("/", 1)
                child_path = folder + "/" + name + "_SmallPlant"
                child = unreal.load_asset(child_path) if eal.does_asset_exist(child_path) else tools.create_asset(name + "_SmallPlant", folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                mel.set_material_instance_parent(child, mi)
                m.set_material(i, child)
                changed = True
                children += 1
                mi = child
            p = path_of(mi)
            if p not in done:
                mel.set_material_instance_static_switch_parameter_value(mi, SWITCH, True)
                if mi.get_base_material().get_name() == "MA_Foliage":   # the Pivot Painter wind's rotated normals would keep swaying in the shading
                    mel.set_material_instance_static_switch_parameter_value(mi, "Animate Advanced Wind Normal", False)
                mel.set_material_instance_scalar_parameter_value(mi, "Flicker Strength", BROAD_LEAF_STRENGTH)
                mel.update_material_instance(mi)
                eal.save_asset(p, only_if_is_dirty=False)
                done.add(p)
        if changed:
            eal.save_asset(path_of(m), only_if_is_dirty=False)
            meshes_changed += 1
    out("PLANTFLICKER small woody: %d meshes, %d instances switched on, %d slots moved to a _SmallPlant child, %d meshes re-saved" % (len(small), len(done), children, meshes_changed))
    for p in sorted(done):
        out("PLANTFLICKER   on: " + p.rsplit("/", 1)[-1])


def set_broad_masters():
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "MaterialInstanceConstant")], package_paths=["/Game/Megascans/3D_Plants", "/Game/Custom"], recursive_paths=True)
    n = 0
    for a in reg.get_assets(flt):
        mi = a.get_asset()
        base = mi.get_base_material()
        if base and base.get_name() in BROAD_MASTERS:
            mel.set_material_instance_scalar_parameter_value(mi, "Flicker Strength", BROAD_LEAF_STRENGTH)
            mel.update_material_instance(mi)
            eal.save_asset(str(a.package_name), only_if_is_dirty=False)
            n += 1
    out("PLANTFLICKER broad leaves: %d instances at Flicker Strength %.3f" % (n, BROAD_LEAF_STRENGTH))


def set_bushes():
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "MaterialInstanceConstant")], package_paths=list(BUSH_FOLDERS), recursive_paths=True)
    n = 0
    for a in reg.get_assets(flt):
        mi = a.get_asset()
        base = mi.get_base_material()
        if base and base.get_name() == "M_MS_Foliage":
            mel.set_material_instance_scalar_parameter_value(mi, "Flicker Strength", BROAD_LEAF_STRENGTH)
            mel.update_material_instance(mi)
            eal.save_asset(str(a.package_name), only_if_is_dirty=False)
            out("PLANTFLICKER   bush: " + str(a.asset_name))
            n += 1
    out("PLANTFLICKER bushes: %d instances at Flicker Strength %.3f" % (n, BROAD_LEAF_STRENGTH))


def main():
    func = build_function()
    for p in SMALL_ONLY:
        wire_small_only(p, func)
    for p in WITH_TREES:
        wire_with_trees(p, func)
    set_small_woody()
    set_broad_masters()
    set_bushes()


main()
