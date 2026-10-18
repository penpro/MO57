"""Pivot Painter wind keeps its precision far from the world origin: the trees no longer "freeze, then glitch" (Wes, Oct 18 2026).

    python Tools/ue.py py --file Content/Python/foliage_wind_precision.py      (editor open, no PIE, no `inst` instance: they lock the .uasset files)

The bug (measured Oct 18 2026; Docs/PCG.md "Foliage wind"): the Megascans Pivot Painter wind (MF_AdvancedWind_Level, called once per branch level
by MA_Foliage -- Black Alder, Common Hazel) and its copy in the PN plant packs (PN_WindAnimation -- meadow flowers, wild berries) look up a 64x64
turbulence texture at
    UV = frac(Time * Speed + dot(WorldSpacePivot, WindDir))         and         frac((Time * Speed + dot(...)) / RandomRotationChangeRate)
in 32-bit float. ms_PivotPainter2_DecodePostion returns the pivot in WORLD space, and MO57's trees stand 5-15 km from the origin, where that dot
product is ~1e5-1e6 cm: the float spacing there is 1/64-1/16 of a UV, so Time * Speed (0.01-0.04 UV a second) can only move the lookup a whole
texel at a time. Each branch level held its pose, then snapped, every (1/64) / Speed = 0.4-1.6 s. Near the origin the same trees sway smoothly.

The fix, one node per graph: the world-space offset is reduced modulo 1200 BEFORE time is added (Fmod, description MO_WindPrecision). 1200 is a
multiple of 1 (the texture's wrap) and of 1..4 (the default Random Rotation Change Rates), so both lookups read exactly the same texels as before;
only the sum stays below ~1200 + Time * Speed, where float spacing is ~1e-4 UV. Idempotent: a graph that already has the node is left alone.
Output lines start with WINDPRECISION. MSPresets LATEST, Content/Custom and PN_WildBerries are gitignored vendor folders: rerun after a reimport.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

FUNCTIONS = [
    "/Game/MSPresets/MS_Foliage_Material/MaterialFunctions/Foliage/MF_AdvancedWind_Level",   # MA_Foliage: Black Alder, Common Hazel
    "/Game/PN_MeadowFlowers/Materials/MaterialFunctions/PN_WindAnimation",                    # meadow flowers
    "/Game/PN_WildBerries/Materials/MaterialFunctions/PN_WindAnimation",                      # wild berries (gitignored vendor folder)
]
MODULUS = 1200.0
DRY_RUN = False   # True: report the sites only (safe in a -game instance; the edit itself needs the editor)
TAG = "MO_WindPrecision"
PIVOT_FUNCTION = "ms_PivotPainter2_DecodePostion"


def log(msg):
    unreal.log("WINDPRECISION " + msg)


def inputs_of(f, node):
    """[(input name, source expression or None, source output name)] of an expression in a material function."""
    names = MEL.get_material_expression_input_names(node)
    srcs = MEL.get_inputs_for_material_function_expression(f, node)
    out = []
    for name, src in zip(names, srcs):
        out.append((name, src, MEL.get_input_node_output_name_for_material_expression(node, src) if src else ""))
    return out


def reaches(f, node, test, seen=None, depth=40):
    """Does `node` (or anything upstream of it) satisfy `test`?"""
    seen = seen if seen is not None else set()
    if node is None or depth < 0 or node.get_name() in seen:
        return False
    seen.add(node.get_name())
    if test(node):
        return True
    return any(reaches(f, src, test, seen, depth - 1) for _, src, _ in inputs_of(f, node) if src)


def is_time(node):
    return isinstance(node, unreal.MaterialExpressionTime)


def is_world_pivot(node):
    """The Pivot Painter pivot: a call of ms_PivotPainter2_DecodePostion, or the "Position And Index Texture" it decodes (Python cannot
    always read a function call's target)."""
    if isinstance(node, unreal.MaterialExpressionFunctionInput):
        return "position and index" in str(node.get_editor_property("input_name")).lower()
    if isinstance(node, unreal.MaterialExpressionMaterialFunctionCall):
        try:
            fn = node.get_editor_property("material_function")
        except Exception:
            return False
        return bool(fn) and fn.get_name() == PIVOT_FUNCTION
    return False


def feeds_only_frac(f, node, consumers, depth=12):
    """Every path from `node` ends in a Frac through Reroutes and Divides only: reducing the operand modulo 1200 then changes nothing the
    Frac can see (exactly, for divisors that divide 1200; for any other divisor only each element's fixed phase). Anything else downstream
    (a Multiply, a position, a rotation) -> False."""
    outs = consumers.get(node.get_name(), [])
    if not outs or depth < 0:
        return False
    for c in outs:
        if isinstance(c, unreal.MaterialExpressionFrac):
            continue
        if isinstance(c, (unreal.MaterialExpressionReroute, unreal.MaterialExpressionDivide)) and feeds_only_frac(f, c, consumers, depth - 1):
            continue
        return False
    return True


def is_tagged(node):
    return isinstance(node, unreal.MaterialExpressionFmod) and str(node.get_editor_property("desc")) == TAG


def patch(path):
    f = unreal.load_asset(path)
    if not f:
        log("%s: not found (vendor pack not installed?)" % path)
        return None
    exprs = MEL.get_material_function_expressions(f)
    consumers = {}
    for e in exprs:
        for _, src, _ in inputs_of(f, e):
            if src:
                consumers.setdefault(src.get_name(), []).append(e)
    sites = []
    for node in exprs:
        if not isinstance(node, unreal.MaterialExpressionAdd):
            continue
        ins = inputs_of(f, node)
        if len(ins) != 2 or not all(src for _, src, _ in ins):
            continue
        (na, sa, oa), (nb, sb, ob) = ins
        for (tn, ts, _), (pn, ps, po) in (((na, sa, oa), (nb, sb, ob)), ((nb, sb, ob), (na, sa, oa))):
            if reaches(f, ts, is_time) and not reaches(f, ts, is_world_pivot) and reaches(f, ps, is_world_pivot) and not reaches(f, ps, is_time):
                if feeds_only_frac(f, node, consumers):
                    sites.append((node, pn, ps, po))
                else:
                    log("%s: %s adds time to a world-space term but feeds more than a Frac -- left alone" % (path, node.get_name()))
    if not sites:
        log("%s: SKIP, no Add of time and a world-space pivot term" % path)
        return False
    if DRY_RUN:
        for add, pin, src, src_out in sites:
            log("%s: DRY RUN site %s.%s <- %s.%s%s" % (path, add.get_name(), pin, src.get_name(), src_out or "out", " (already reduced)" if is_tagged(src) else ""))
        return True
    changed = 0
    for add, pin, src, src_out in sites:
        if is_tagged(src):
            log("%s: %s.%s already reduced (%s)" % (path, add.get_name(), pin, src.get_name()))
            continue
        x = src.get_editor_property("material_expression_editor_x") + 200
        y = src.get_editor_property("material_expression_editor_y") + 120
        fmod = MEL.create_material_expression_in_function(f, unreal.MaterialExpressionFmod, x, y)
        const = MEL.create_material_expression_in_function(f, unreal.MaterialExpressionConstant, x - 160, y + 80)
        const.set_editor_property("r", MODULUS)
        fmod.set_editor_property("desc", TAG)
        ok = (MEL.connect_material_expressions(src, src_out, fmod, "A") and MEL.connect_material_expressions(const, "", fmod, "B")
              and MEL.connect_material_expressions(fmod, "", add, pin))
        if not ok:
            log("%s: !! wiring failed at %s.%s -- NOT saved (revert the asset from git if the editor shows it dirty)" % (path, add.get_name(), pin))
            return False
        changed += 1
        log("%s: %s.%s <- Fmod(%s.%s, %.0f)" % (path, add.get_name(), pin, src.get_name(), src_out or "out", MODULUS))
    if changed:
        MEL.update_material_function(f)
        EAL.save_loaded_asset(f)
        log("%s: saved (%d site(s))" % (path, changed))
    return True


def main():
    results = [patch(p) for p in FUNCTIONS]
    log("DONE %d/%d graphs reduce their world-space wind offset" % (sum(1 for r in results if r), sum(1 for r in results if r is not None)))


main()
