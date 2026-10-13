"""Import the FAR CANOPY proxies (four low-poly tree stand-ins) + build their material, for the third far-tree tier (`MOFar_Canopy`, 3-9 km).

    python Tools/gen_far_canopy_meshes.py                                   (once: Saved/GeneratedMeshes/farcanopy/SM_FarCanopy_*.obj)
    python Tools/ue.py py --file Content/Python/make_far_canopy.py          (editor open, PIE NOT running, no `inst` instance)

Why (2026-10-13, Wes: "far trees: performant AND good looking"): beyond ~3 km a real tree is 2-6 pixels tall. Rendered as the real Nanite foliage (masked leaf cards) it is sub-pixel aliasing: a white-speckled
noise on the distant forest, on top of costing masked-material shading for nothing. A proxy is a solid crown (an egg-shaped blob of ~130 triangles) on a short trunk with a plain dark-green material: it reads as a soft
dark mass lit from the sun side, it is stable under TSR, and it is a tiny mesh that Nanite draws by the hundred thousand. Each biome tree species maps to the proxy of the nearest size (see biome_palettes.py).

(NOT built through unreal.StaticMeshDescription: creating one from Python and calling create_polygon_group dereferences a null mesh description and crashes the editor, 2026-10-13. The OBJ import is the safe route.)

Output: /Game/Penumbra/FarCanopy/{SM_FarCanopy_Tall,Mid,Round,Small, M_FarCanopy}. Idempotent. Lines start with FARCANOPY.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Penumbra/FarCanopy"
OBJ_DIR = "D:/UEProjects/MO57/Saved/GeneratedMeshes/farcanopy/"
NAMES = ("Tall", "Mid", "Round", "Small")


def log(msg):
    unreal.log("FARCANOPY " + msg)


def set_usage(m):
    """The clumps are drawn by instanced (HISM) components: a cooked game cannot compile the ISM permutation on demand, so without the flag it renders them with the DEFAULT material
    ("missing usage flag InstancedStaticMeshes! Default Material will be used in game" -- the packaged build of 2026-10-13 shipped grey canopy). The editor compiles it on the fly and hides it."""
    m.set_editor_property("used_with_instanced_static_meshes", True)


def build_material():
    path = DIR + "/M_FarCanopy"
    if EAL.does_asset_exist(path):
        # exists: only the colour / surface PARAMETERS are refreshed in place (deleting the asset or its expressions from editor Python crashed the editor twice, 2026-10-13)
        m = unreal.load_asset(path)
        set_usage(m)
        want = {"CanopyDark": (0.006, 0.014, 0.006), "CanopyLight": (0.017, 0.030, 0.010), "TrunkColor": (0.030, 0.022, 0.016)}
        for e in MEL.get_material_expressions(m):
            if isinstance(e, unreal.MaterialExpressionVectorParameter) and str(e.get_editor_property("parameter_name")) in want:
                c = want[str(e.get_editor_property("parameter_name"))]
                e.set_editor_property("default_value", unreal.LinearColor(c[0], c[1], c[2], 1.0))
            if isinstance(e, unreal.MaterialExpressionScalarParameter):
                n = str(e.get_editor_property("parameter_name"))
                if n == "Roughness":
                    e.set_editor_property("default_value", 1.0)
                if n == "Specular":
                    e.set_editor_property("default_value", 0.04)
        MEL.recompile_material(m)
        EAL.save_loaded_asset(m)
        log("refreshed the parameters of " + path)
        return m
    m = TOOLS.create_asset("M_FarCanopy", DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    set_usage(m)

    def vec(name, rgb, x, y):
        n = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return n

    def scal(name, v, x, y):
        n = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", v)
        return n

    deep = vec("CanopyDark", (0.006, 0.014, 0.006), -900, -200)       # conifer-dark green (far haze lightens it a lot)
    lush = vec("CanopyLight", (0.017, 0.030, 0.010), -900, -50)       # sunlit olive green
    trunk = vec("TrunkColor", (0.030, 0.022, 0.016), -900, 300)
    rnd = MEL.create_material_expression(m, unreal.MaterialExpressionPerInstanceRandom, -900, 100)
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1200, 450)
    vmask = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1000, 450)
    vmask.set_editor_property("r", False); vmask.set_editor_property("g", True); vmask.set_editor_property("b", False); vmask.set_editor_property("a", False)
    MEL.connect_material_expressions(uv, "", vmask, "")
    neg = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 450)
    neg.set_editor_property("const_b", -1.0)
    MEL.connect_material_expressions(vmask, "", neg, "A")
    sat = MEL.create_material_expression(m, unreal.MaterialExpressionSaturate, -650, 450)   # trunk: V = -1 -> 1; crown: V in 0..1 -> 0
    MEL.connect_material_expressions(neg, "", sat, "")
    lerp1 = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -600, -100)
    MEL.connect_material_expressions(deep, "", lerp1, "A")
    MEL.connect_material_expressions(lush, "", lerp1, "B")
    MEL.connect_material_expressions(rnd, "", lerp1, "Alpha")
    lerp2 = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -300, 0)
    MEL.connect_material_expressions(lerp1, "", lerp2, "A")
    MEL.connect_material_expressions(trunk, "", lerp2, "B")
    MEL.connect_material_expressions(sat, "", lerp2, "Alpha")
    MEL.connect_material_property(lerp2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(scal("Roughness", 1.0, -300, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(scal("Specular", 0.04, -300, 350), "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("built " + path)
    return m


def import_mesh(name, mat):
    path = "%s/SM_FarCanopy_%s" % (DIR, name)
    task = unreal.AssetImportTask()
    task.filename = OBJ_DIR + "SM_FarCanopy_%s.obj" % name
    task.destination_path = DIR
    task.destination_name = "SM_FarCanopy_" + name
    task.replace_existing = True
    task.replace_existing_settings = True
    task.automated = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    sm = unreal.load_asset(path)
    if not sm or not isinstance(sm, unreal.StaticMesh):
        log("!! import failed: " + path + " -> " + str(sm))
        return None
    sm.set_material(0, mat)
    ns = sm.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", True)
    sm.set_editor_property("nanite_settings", ns)
    EAL.save_loaded_asset(sm)
    b = sm.get_bounds()
    log("imported %s: size %d x %d x %d cm, origin z %d, %d material slot(s)" % (path, round(b.box_extent.x * 2), round(b.box_extent.y * 2), round(b.box_extent.z * 2), round(b.origin.z), len(sm.get_editor_property("static_materials"))))
    return sm


def main():
    mat = build_material()
    for n in NAMES:
        import_mesh(n, mat)


main()
