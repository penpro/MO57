"""A5: graybox stand-in meshes for building recipes missing PreviewMesh.

Runs INSIDE the editor (GeometryScripting python API, plugin enabled in
MO57.uproject). For a list of recipe ids, generates a footprint-derived
UDynamicMesh — floor slab / wall panel / roof prism / box+roof hut
(+ chimney for smelters) — and bakes each to a StaticMesh at
/Game/Penumbra/Graybox/SM_GB_<RecipeId>.

Shape sizing is keyword-heuristic (recipe rows carry no footprint data).
Dimensions in cm. The ue.py driver then `asset assign`s each mesh onto
the recipe's PlacementData.PreviewMesh and re-runs ValidateArt.

API notes (probed live 2026-07-07):
- append_box(..., origin=BASE): base sits at the transform's Z.
- append_simple_extrude_polygon extrudes the local-XY polygon along world
  +Z at identity; roll=-90 stands the triangle upright and extrudes along
  world -Y (probed via get_mesh_bounding_box, don't re-guess).
- create_new_static_mesh_asset_from_mesh returns (StaticMesh, outcome).
"""
import unreal

GRAYBOX_PATH = "/Game/Penumbra/Graybox"


def _shape_for(recipe_id):
    """(kind, x, y, z) heuristics from the recipe name."""
    rid = recipe_id.lower()
    if "floor" in rid:
        return ("slab", 300.0, 300.0, 15.0)
    if "wallhalf" in rid:
        return ("slab", 300.0, 20.0, 100.0)
    if "wall" in rid:
        return ("slab", 300.0, 20.0, 250.0)
    if "roof" in rid:
        return ("prism", 300.0, 300.0, 120.0)
    if "forge" in rid or "bloomery" in rid:
        return ("hut_chimney", 200.0, 200.0, 220.0)
    if "leanto" in rid:
        return ("prism", 250.0, 200.0, 170.0)
    if "workbench" in rid or "loom" in rid:
        return ("slab", 200.0, 90.0, 100.0)
    return ("hut", 400.0, 300.0, 300.0)   # default: house-ish (school etc.)


def _xf(x=0.0, y=0.0, z=0.0, roll=0.0):
    return unreal.Transform(unreal.Vector(x, y, z),
                            unreal.Rotator(roll=roll, pitch=0.0, yaw=0.0),
                            unreal.Vector(1.0, 1.0, 1.0))


def _box(dyn, opts, x, y, z, sx, sy, sz):
    unreal.GeometryScript_Primitives.append_box(
        dyn, opts, _xf(x, y, z), sx, sy, sz,
        origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)


def _roof_prism(dyn, opts, base_z, sx, sy, rise):
    """Triangular prism: ridge along Y, base at base_z, spanning sy."""
    tri = [unreal.Vector2D(-sx / 2.0, 0.0),
           unreal.Vector2D(sx / 2.0, 0.0),
           unreal.Vector2D(0.0, rise)]
    # Probed (2026-07-07): identity extrudes the local-XY polygon along
    # world +Z; roll=-90 stands the triangle upright (height -> +Z) and
    # sends the extrusion along world -Y — so start at +sy/2 to center.
    unreal.GeometryScript_Primitives.append_simple_extrude_polygon(
        dyn, opts, _xf(0.0, sy / 2.0, base_z, roll=-90.0), tri, sy,
        capped=True, origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)


def make_graybox(recipe_id):
    kind, sx, sy, sz = _shape_for(recipe_id)
    dyn = unreal.DynamicMesh()
    opts = unreal.GeometryScriptPrimitiveOptions()
    if kind == "slab":
        _box(dyn, opts, 0.0, 0.0, 0.0, sx, sy, sz)
    elif kind == "prism":
        _roof_prism(dyn, opts, 0.0, sx, sy, sz)
    else:   # hut / hut_chimney
        wall_h = sz * 0.7
        _box(dyn, opts, 0.0, 0.0, 0.0, sx, sy, wall_h)
        _roof_prism(dyn, opts, wall_h, sx, sy, sz * 0.3)
        if kind == "hut_chimney":
            _box(dyn, opts, sx * 0.3, sy * 0.3, 0.0, 40.0, 40.0, sz * 1.15)
    asset_path = "%s/SM_GB_%s" % (GRAYBOX_PATH, recipe_id)
    opts_new = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    result = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        dyn, asset_path, opts_new)
    outcome = result[1] if isinstance(result, tuple) else result
    ok = (outcome == unreal.GeometryScriptOutcomePins.SUCCESS)
    return asset_path if ok else None


# --- item debug meshes (V3-deferred stand-ins): one shape per EMOItemType ---

ITEM_SHAPES = {
    # type -> list of (kind, args) primitives; sizes in cm, debug-grade
    "MATERIAL":   [("box", (0, 0, 0, 30, 20, 10))],
    "TOOL":       [("box", (0, 0, 0, 6, 6, 50)), ("box", (0, 0, 50, 24, 10, 10))],
    "CONSUMABLE": [("sphere", (0, 0, 6, 12))],
    "WEAPON":     [("box", (0, 0, 0, 8, 3, 70)), ("box", (0, 0, 18, 20, 4, 4))],
    "MISC":       [("box", (0, 0, 0, 15, 15, 15))],
    "ARMOR":      [("box", (0, 0, 0, 30, 8, 25))],
    "AMMO":       [("box", (0, 0, 0, 2, 2, 40)), ("box", (0, 0, 40, 4, 4, 6))],
}


def make_item_debug(type_name):
    shapes = ITEM_SHAPES.get(type_name)
    if not shapes:
        return None
    dyn = unreal.DynamicMesh()
    opts = unreal.GeometryScriptPrimitiveOptions()
    for kind, a in shapes:
        if kind == "box":
            _box(dyn, opts, a[0], a[1], a[2], a[3], a[4], a[5])
        elif kind == "sphere":
            xf = _xf(a[0], a[1], a[2])
            unreal.GeometryScript_Primitives.append_sphere_box(dyn, opts, xf, a[3])
    asset_path = "%s/Items/SM_GBI_%s" % (GRAYBOX_PATH, type_name.title())
    opts_new = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    result = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, asset_path, opts_new)
    outcome = result[1] if isinstance(result, tuple) else result
    return asset_path if outcome == unreal.GeometryScriptOutcomePins.SUCCESS else None


def run_items(types):
    made = []
    for t in types:
        try:
            p = make_item_debug(t)
        except Exception as e:  # noqa: BLE001
            print("GRAYBOX ITEM %s -> ERROR %s" % (t, e))
            made.append((t, None))
            continue
        print("GRAYBOX ITEM %s -> %s" % (t, p))
        made.append((t, p))
    if any(m[1] for m in made):
        unreal.EditorAssetLibrary.save_directory(GRAYBOX_PATH + "/Items", only_if_is_dirty=False, recursive=True)
    print("GRAYBOX ITEMS DONE %d/%d (saved)" % (len([m for m in made if m[1]]), len(made)))
    return made


def run(targets):
    made = []
    for rid in targets:
        try:
            p = make_graybox(rid)
        except Exception as e:  # noqa: BLE001
            print("GRAYBOX %s -> ERROR %s" % (rid, e))
            made.append((rid, None))
            continue
        print("GRAYBOX %s -> %s" % (rid, p))
        made.append((rid, p))
    ok = [m for m in made if m[1]]
    # SAVE TO DISK — create_new_static_mesh_asset_from_mesh only creates the
    # asset in memory; an editor restart silently discards it (and ValidateArt
    # pattern-checks the PATH, so the debt number would keep lying).
    if ok:
        unreal.EditorAssetLibrary.save_directory(GRAYBOX_PATH, only_if_is_dirty=False, recursive=True)
    print("GRAYBOX DONE %d/%d (saved)" % (len(ok), len(made)))
    return made
