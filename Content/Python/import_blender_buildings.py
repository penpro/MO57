"""Bring Tools/blender_buildings_blender.py's pieces into the project and make each one a building (Nov 2 2026; Wes: "let's flesh out a lot of
the building system and remember to have the assets 'pop in' as building goes on"). Editor open, PIE not running. Driven by
Tools/blender_buildings.py:

    python Tools/ue.py py --timeout 1800 -c "import import_blender_buildings as b; out(b.run(['WattleWall']))"

From Saved/BlenderBuildings/ into /Game/BlenderBuildings/:
  Surfaces/T_Surf_<Name>_BaseColor / _Normal / _ORM, MI_Surf_<Name>   the tiling surfaces, on M_Surf_Base (made here: it reads each primitive's tint)
  <Piece>/SM_<Piece>_<NN>_<Part>                                       each part, Nanite, its slots on MI_Surf_<surface>
  <Piece>/SM_<Support>                                                  a floor's support post (simple box collision: it is solid once built)
  <Piece>/BP_Piece_<Piece>                                              an AMOBuildPieceActor Blueprint: the parts in build order, the colliders,
                                                                        the snap box, the door, the supports, the snapping and shelter flags
  Moss01/SM_Moss01                                                      the moss item's mesh
Returns JSON: what was made, and the recipe rows (Saved/BlenderBuildings/recipes.json) the driver writes with `ue.py rows set`.

A cobble piece's stones are the scans already in the project (Tools/building_pieces.py STONES), laid here in courses -- each stone turned so
its long side runs along the face and its flat side down, sized to a building stone, bonded over the joints below -- one instanced part per
course and stone, so the wall rises course by course as the work goes on. Laid as a waller lays them (Nov 3, natural variation): each course
its own height and built of three of the eleven walling stones, short, middle and long stones mixed, each a little shy of the course and
bedded at its own height, standing proud or back, turned whichever face out, with a pinning stone knocked into a joint now and then.
"""
import importlib
import json
import math
import os
import random
import sys

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(PROJECT, "Tools"))
import building_pieces as BP   # noqa: E402
importlib.reload(BP)
import import_blender_items as IBI   # noqa: E402  (_import, _task: the legacy FBX importer that makes exactly the assets asked for)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "BlenderBuildings")
DEST = "/Game/BlenderBuildings"
WX = -50.0


def log(msg):
    unreal.log_warning("BLENDERBUILDINGS " + msg)


# ---------------------------------------------------------------------------------------------------------------------------------------
# Surfaces and meshes
# ---------------------------------------------------------------------------------------------------------------------------------------

SURF_BASE_VERSION = 2   # bump with any change to surface_master's graph (2: the weather read as 1 - V)


def surface_master():
    """M_Surf_Base: the surfaces' parent (Nov 3, natural variation). M_MS_Base's three textures (BaseColor, Normal, ORM in glTF order), and
    each primitive's tint from the mesh's second UV channel (Tools/blender_buildings_blender.py Vary): base colour x lerp(LightMin, LightMax,
    tint.x), then toward WeatherColor x its own brightness by tint.y x WeatherAmount -- one stick lighter, one silvered, a clump of moss gone
    yellow. The weather reads 1 - V: the FBX import flips V.

    Built once, tagged with SURF_BASE_VERSION. It is never rebuilt in place: delete_all_material_expressions leaves half the nodes behind, and
    deleting them one by one on a material loaded from disk asserts (!IsRooted) and takes the editor down (Nov 3). So a graph change bumps the
    version, and an older material stops the run: delete its .uasset on disk with the editor closed and run again."""
    path = DEST + "/Surfaces/M_Surf_Base"
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        have = eal.get_metadata_tag(m, "MOSurfBaseVersion")
        if have != str(SURF_BASE_VERSION):
            raise RuntimeError("M_Surf_Base is version %r, the script builds %d: delete Content/BlenderBuildings/Surfaces/M_Surf_Base.uasset "
                               "with the editor closed and run again" % (have, SURF_BASE_VERSION))
        return m
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Surf_Base", DEST + "/Surfaces", unreal.Material, unreal.MaterialFactoryNew())
    eal.set_metadata_tag(m, "MOSurfBaseVersion", str(SURF_BASE_VERSION))
    m.set_editor_property("used_with_nanite", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    def node(cls, x, y, **props):
        n = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def tex(name, sampler, default, y):
        # a default of the sampler's own kind (the bark's), or the node does not compile
        return node(unreal.MaterialExpressionTextureSampleParameter2D, -900, y, parameter_name=name, sampler_type=sampler,
                    texture=unreal.load_asset("%s/Surfaces/T_Surf_Bark_%s" % (DEST, default)))

    def scalar(name, v, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)

    col = tex("BaseColorTexture", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "BaseColor", -300)
    nrm = tex("NormalTexture", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "Normal", 400)
    orm = tex("MetallicRoughnessTexture", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "ORM", 750)   # M_MS_Base's name: the icon studio reads it
    uv1 = node(unreal.MaterialExpressionTextureCoordinate, -1300, 100, coordinate_index=1)
    light = node(unreal.MaterialExpressionComponentMask, -1100, 50, r=True, g=False, b=False, a=False)
    weather_v = node(unreal.MaterialExpressionComponentMask, -1100, 200, r=False, g=True, b=False, a=False)
    mel.connect_material_expressions(uv1, "", light, "")
    mel.connect_material_expressions(uv1, "", weather_v, "")
    # the FBX import flips V (v -> 1 - v, measured Nov 3: a tint written 0.1 read 0.9), so the weather comes back as 1 - V
    weather = node(unreal.MaterialExpressionOneMinus, -950, 200)
    mel.connect_material_expressions(weather_v, "", weather, "")
    gain = node(unreal.MaterialExpressionLinearInterpolate, -700, 0)
    mel.connect_material_expressions(scalar("LightMin", 0.75, -900, -60), "", gain, "A")
    mel.connect_material_expressions(scalar("LightMax", 1.25, -900, 10), "", gain, "B")
    mel.connect_material_expressions(light, "", gain, "Alpha")
    lit = node(unreal.MaterialExpressionMultiply, -500, -200)
    mel.connect_material_expressions(col, "RGB", lit, "A")
    mel.connect_material_expressions(gain, "", lit, "B")
    lum = node(unreal.MaterialExpressionDotProduct, -350, -50)
    mel.connect_material_expressions(lit, "", lum, "A")
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant3Vector, -500, 0, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1.0)), "", lum, "B")
    aged = node(unreal.MaterialExpressionMultiply, -200, -50)
    mel.connect_material_expressions(lum, "", aged, "A")
    mel.connect_material_expressions(node(unreal.MaterialExpressionVectorParameter, -350, 100, parameter_name="WeatherColor",
                                          default_value=unreal.LinearColor(1.08, 1.06, 1.02, 1.0)), "", aged, "B")
    amount = node(unreal.MaterialExpressionMultiply, -500, 250)
    mel.connect_material_expressions(weather, "", amount, "A")
    mel.connect_material_expressions(scalar("WeatherAmount", 0.6, -700, 300), "", amount, "B")
    base = node(unreal.MaterialExpressionLinearInterpolate, -50, -150)
    mel.connect_material_expressions(lit, "", base, "A")
    mel.connect_material_expressions(aged, "", base, "B")
    mel.connect_material_expressions(amount, "", base, "Alpha")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def surfaces():
    folder = DEST + "/Surfaces"
    master = surface_master()
    mis = {}
    for name in BP.SURFACES:
        keys = ("BaseColor", "Normal", "ORM")
        IBI._import([IBI._task(os.path.join(SRC, "Surfaces", "T_Surf_%s_%s.png" % (name, k)), folder, "T_Surf_%s_%s" % (name, k)) for k in keys])
        tex = {k: unreal.load_asset("%s/T_Surf_%s_%s" % (folder, name, k)) for k in keys}
        tex["Normal"].set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex["Normal"].set_editor_property("srgb", False)
        tex["Normal"].set_editor_property("flip_green_channel", False)   # baked DirectX
        tex["ORM"].set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex["ORM"].set_editor_property("srgb", False)
        for t in tex.values():
            eal.save_loaded_asset(t, False)
        mi_path = "%s/MI_Surf_%s" % (folder, name)
        mi = unreal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_Surf_" + name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        set_surface_params(mi, name, tex)
        eal.save_loaded_asset(mi, False)
        mis[name] = mi
    return mis


def set_surface_params(mi, name, tex):
    """An MI_Surf_<Name> on M_Surf_Base: its three textures and its tint (Tools/building_pieces.py SURFACES light / weather)."""
    spec = BP.SURFACES[name]
    mel.clear_all_material_instance_parameters(mi)   # the old M_MS_Base overrides
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", tex["BaseColor"])
    mel.set_material_instance_texture_parameter_value(mi, "NormalTexture", tex["Normal"])
    mel.set_material_instance_texture_parameter_value(mi, "MetallicRoughnessTexture", tex["ORM"])
    mel.set_material_instance_scalar_parameter_value(mi, "LightMin", spec["light"][0])
    mel.set_material_instance_scalar_parameter_value(mi, "LightMax", spec["light"][1])
    mel.set_material_instance_vector_parameter_value(mi, "WeatherColor", unreal.LinearColor(*spec["weather"], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "WeatherAmount", spec["weather_amount"])
    mel.update_material_instance(mi)


def surface_mis():
    """The surfaces as they are (textures not re-imported), each MI put on the current M_Surf_Base with its current tint."""
    master = surface_master()
    folder = DEST + "/Surfaces"
    mis = {}
    for name in BP.SURFACES:
        mi = unreal.load_asset("%s/MI_Surf_%s" % (folder, name))
        mel.set_material_instance_parent(mi, master)
        set_surface_params(mi, name, {k: unreal.load_asset("%s/T_Surf_%s_%s" % (folder, name, k)) for k in ("BaseColor", "Normal", "ORM")})
        eal.save_loaded_asset(mi, False)
        mis[name] = mi
    return mis


def import_mesh(fbx, folder, name, mis, collision=False):
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.auto_generate_collision = collision
    ui.static_mesh_import_data.convert_scene_unit = True
    IBI._import([IBI._task(fbx, folder, name, ui)])
    mesh = unreal.load_asset("%s/%s" % (folder, name))
    for i, slot in enumerate(mesh.static_materials):
        sname = str(slot.material_slot_name)
        surf = next((s for s in BP.SURFACES if sname == "Surf_" + s or sname.startswith("Surf_" + s + "_")), None)
        if surf:
            mesh.set_material(i, mis[surf])
        else:
            log("%s: slot %s has no surface" % (name, sname))
    ns = mesh.get_editor_property("nanite_settings")
    ns.enabled = True
    mesh.set_editor_property("nanite_settings", ns)
    if collision:
        # a support post: one box round it is enough (it is scaled to its length per post)
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        sub.remove_collisions(mesh)
        sub.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    eal.save_loaded_asset(mesh, False)
    return mesh


# ---------------------------------------------------------------------------------------------------------------------------------------
# Stones: scans laid in courses
# ---------------------------------------------------------------------------------------------------------------------------------------

def _rot90s():
    out = []
    for p in (0, 90, 180, 270):
        for y in (0, 90, 180, 270):
            for r in (0, 90):
                out.append(unreal.Rotator(roll=r, pitch=p, yaw=y))
    return out


_STONE_INFO = {}


def stone_info(key, lay=(2, 0, 1)):
    """A scan's native size and the quarter-turn that lands its short, middle and long sides on the world axes lay (default: short up Z,
    middle across X, long along Y -- a walling stone lying flat along the face; (1, 2, 0) stands it on edge across the wall: a coping stone)."""
    if (key, lay) in _STONE_INFO:
        return _STONE_INFO[(key, lay)]
    mesh = unreal.load_asset(BP.STONES[key])
    bb = mesh.get_bounding_box()
    native = [bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z]
    centre = unreal.Vector((bb.max.x + bb.min.x) * 0.5, (bb.max.y + bb.min.y) * 0.5, (bb.max.z + bb.min.z) * 0.5)
    want = sorted(range(3), key=lambda i: native[i])   # short, middle, long native axes
    best = None
    for rot in _rot90s():
        q = rot.quaternion()
        axes = [q.rotate_vector(unreal.Vector(*(1.0 if j == i else 0.0 for j in range(3)))) for i in range(3)]
        # which world axis each native axis lands on
        land = [max(range(3), key=lambda w: abs([a.x, a.y, a.z][w])) for a in axes]
        if land[want[0]] == lay[0] and land[want[1]] == lay[1] and land[want[2]] == lay[2]:
            best = (rot, land)
            break
    _STONE_INFO[(key, lay)] = (mesh, native, centre, best[0], best[1])
    return _STONE_INFO[(key, lay)]


# half turns about each axis: the same stone with another face out (each keeps the long side along the face, the flat side level)
FLIPS = [unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), unreal.Rotator(roll=180.0, pitch=0.0, yaw=0.0),
         unreal.Rotator(roll=0.0, pitch=180.0, yaw=0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0)]


def place_stone(key, centre, size, rng, tilt=8.0, lay=(2, 0, 1)):
    """A transform that puts the scan's centre at centre with world extents size (x, y, z), a random face out, turned a little off square."""
    mesh, native, ncentre, base, land = stone_info(key, lay)
    scale = [1.0, 1.0, 1.0]
    for i in range(3):
        scale[i] = size[land[i]] / max(native[i], 0.01)
    jitter = unreal.Rotator(roll=rng.uniform(-tilt, tilt), pitch=rng.uniform(-tilt, tilt), yaw=rng.uniform(-12.0, 12.0))
    q = jitter.quaternion() * rng.choice(FLIPS).quaternion() * base.quaternion()
    s = unreal.Vector(*scale)
    off = q.rotate_vector(unreal.Vector(ncentre.x * s.x, ncentre.y * s.y, ncentre.z * s.z))
    loc = unreal.Vector(centre[0] - off.x, centre[1] - off.y, centre[2] - off.z)
    return unreal.Transform(loc, q.rotator(), s)


def lay_face(rng, plane_x, outward, y0, y1, z0, z1, depth, courses, start_stage=0, batter=0.0):
    """Stones on one face (plane_x: the face at its foot, outward +-1 along X), in courses from z0 up to z1: (course, key) -> [transforms].
    Each course is its own height and built of three walling stones (BP.WALLING) and one pinning stone (BP.PINNING), so the parts stay a few
    per course while the wall shows them all. batter: how far the face leans in per cm of height (a dry-stone wall's 0.1)."""
    out = {}
    z = z0
    k = 0
    while z < z1 - 3.0:
        h = min(rng.uniform(*courses), z1 - z)
        if z1 - (z + h) < courses[0] * 0.6:
            h = z1 - z
        keys = rng.sample(BP.WALLING, 3)
        pin = rng.choice(BP.PINNING)
        y = y0 - (rng.uniform(0.0, 14.0) if k % 2 else rng.uniform(0.0, 5.0))
        while y < y1 - 2.0:
            L = rng.choice((rng.uniform(11.0, 17.0), rng.uniform(17.0, 25.0), rng.uniform(24.0, 34.0)))   # short, middle and long stones mixed
            ya, yb = max(y, y0), min(y + L, y1)
            if yb - ya > 6.0:
                hs = h * rng.uniform(0.72, 1.04)                        # a little shy of the course, bedded at its own height
                zc = z + hs * 0.5 + rng.uniform(0.0, max(0.0, h - hs) * 0.7)
                d = depth * rng.uniform(0.75, 1.1)
                proud = rng.uniform(-1.0, 1.6)                          # standing proud of the face or set back
                face_x = plane_x - outward * batter * (zc - z0)
                c = (face_x - outward * (d * 0.5 - 2.0 - proud), (ya + yb) * 0.5 + rng.uniform(-1.0, 1.0), zc)
                key = rng.choice(keys)
                # a scan fills ~2/3 of its box: sized up so neighbours meet and the joints stay narrow (they may touch: laid stones do)
                size = (d, (yb - ya) * rng.uniform(1.12, 1.36), hs * rng.uniform(1.25, 1.45))
                out.setdefault((start_stage + k, key), []).append(place_stone(key, c, size, rng))
                if rng.random() < 0.35 and yb < y1 - 4.0:
                    # a pinning stone knocked into the joint after it, where two rounded ends leave a hollow
                    ps = rng.uniform(4.5, 7.5)
                    pz = z + h * rng.uniform(0.25, 0.75)
                    pc = (plane_x - outward * (batter * (pz - z0) + ps * 0.4 - 1.5), yb + rng.uniform(-1.0, 1.0), pz)
                    out.setdefault((start_stage + k, pin), []).append(place_stone(pin, pc, (ps * 0.8, ps, ps * 0.75), rng, tilt=25.0))
            y += L
        z += h
        k += 1
    return out


def coping(rng, x, half, y0, y1, z, stage):
    """The coping: stones set on edge across the top of a wall from y0 to y1, close together, each its own thickness and height, leaning a
    little -> {(stage, key): [transforms]} (one course)."""
    out = {}
    keys = rng.sample(BP.WALLING, 3)
    y = y0 + rng.uniform(0.0, 2.0)
    while y < y1 - 3.0:
        t = rng.uniform(7.0, 13.0)
        h = rng.uniform(12.0, 17.0)
        across = 2.0 * half * rng.uniform(1.0, 1.15)
        c = (x + rng.uniform(-1.5, 1.5), min(y + t * 0.5, y1 - t * 0.4), z + h * 0.5 - rng.uniform(0.0, 2.0))
        key = rng.choice(keys)
        out.setdefault((stage, key), []).append(place_stone(key, c, (across * 1.15, t * 1.3, h * 1.35), rng, tilt=6.0, lay=(1, 2, 0)))
        y += t + rng.uniform(0.0, 1.0)
    return out


def interleave(bands, stones):
    """Bands of a core (built bottom up) between the stone courses ("Course<N>_<Key>" parts, in course order): band i just before the courses
    that rise past its foot, so the core never stands above the stones round it."""
    course = lambda p: int(p["name"].split("_")[0][len("Course"):])   # noqa: E731
    n = max(course(p) for p in stones) if stones else 0
    out = []
    for i, band in enumerate(bands):
        lo, hi = n * i // len(bands), n * (i + 1) // len(bands)
        out.append(band)
        out += [p for p in stones if lo < course(p) <= hi]
    return out + [p for p in stones if p not in out]


def stone_parts(piece, rng):
    lay = {}

    def add(d):
        for k, v in d.items():
            lay.setdefault(k, []).extend(v)
    if piece == "CobbleWall":
        for side in (1, -1):
            add(lay_face(rng, WX + side * 20.0, side, -50.0, 50.0, -50.0, 50.0, 19.0, (11.0, 21.0)))
    elif piece == "DryStoneWall":
        d = dict(half_foot=28.0, half_top=19.0, z0=-50.0, z_cope=38.0)   # blender_buildings_blender.DRY_WALL
        batter = (d["half_foot"] - d["half_top"]) / (d["z_cope"] - d["z0"])
        for side in (1, -1):
            add(lay_face(rng, WX + side * d["half_foot"], side, -50.0, 50.0, d["z0"], d["z_cope"], 20.0, (10.0, 19.0), batter=batter))
        # the wall heads (Y +-50): laid in their own frame (the face at X +-50, along Y across the wall's width), turned a quarter about the
        # wall's middle
        turn = unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0).quaternion()
        for side in (1, -1):
            for k, v in lay_face(rng, side * 50.0, side, -d["half_top"], d["half_top"], d["z0"], d["z_cope"], 18.0, (10.0, 19.0)).items():
                for t in v:
                    loc = turn.rotate_vector(t.translation)
                    lay.setdefault(k, []).append(unreal.Transform(unreal.Vector(loc.x + WX, loc.y, loc.z), (turn * t.rotation).rotator(), t.scale3d))
        add(coping(rng, WX, d["half_top"], -50.0, 50.0, d["z_cope"], stage=len({k[0] for k in lay})))
    elif piece == "CobbleInfill":
        add(lay_face(rng, WX + 7.0, 1, -50.0, 50.0, -50.0, 50.0, 14.0, (8.0, 14.0)))
    elif piece == "CobbleFoundation":
        # the four sides: a face along Y at X +-50 and one along X at Y +-50 (laid in its own frame, then turned a quarter)
        for side in (1, -1):
            add(lay_face(rng, side * 50.0, side, -50.0, 50.0, -35.0, 4.0, 22.0, (10.0, 18.0)))
        for side in (1, -1):
            d = lay_face(rng, side * 50.0, side, -45.0, 45.0, -35.0, 4.0, 22.0, (10.0, 18.0))
            turn = unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0).quaternion()
            for k, v in d.items():
                for t in v:
                    loc = turn.rotate_vector(t.translation)
                    lay.setdefault(k, []).append(unreal.Transform(loc, (turn * t.rotation).rotator(), t.scale3d))
    parts = []
    for (course, key) in sorted(lay):
        parts.append({"name": "Course%d_%s" % (course + 1, key), "mesh": BP.STONES[key], "instances": lay[(course, key)]})
    return parts


# ---------------------------------------------------------------------------------------------------------------------------------------
# The Blueprint
# ---------------------------------------------------------------------------------------------------------------------------------------

def xf(center, rot=(0.0, 0.0, 0.0)):
    return unreal.Transform(unreal.Vector(*center), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]), unreal.Vector(1.0, 1.0, 1.0))


def box(center, half, rot=(0.0, 0.0, 0.0), door=False):
    b = unreal.MOBuildPieceBox()
    b.set_editor_property("transform", xf(center, rot))
    b.set_editor_property("half_size", unreal.Vector(*half))
    b.set_editor_property("door_leaf", door)
    return b


def tent_colliders():
    """The cone's sides as seven tilted boards (the eighth is the doorway), from the cover's foot to its top."""
    R, H, z0, z1 = 160.0, 270.0, 8.0, 222.0
    tilt = math.degrees(math.atan2(R, H))
    zm = (z0 + z1) * 0.5
    rm = R * (1.0 - zm / H) * math.cos(math.pi / 8.0)
    half_slant = (z1 - z0) * 0.5 / math.cos(math.radians(tilt))
    out = []
    for k in range(1, 8):
        phi = 2.0 * math.pi * (k + 0.5) / 8.0
        c = (rm * math.cos(phi), rm * math.sin(phi), zm)
        out.append(box(c, (4.0, R * (1.0 - zm / H) * math.sin(math.pi / 8.0) + 6.0, half_slant), rot=(tilt, math.degrees(phi), 0.0)))
    return out


def make_blueprint(piece, spec, parts):
    folder = "%s/%s" % (DEST, piece)
    path = "%s/BP_Piece_%s" % (folder, piece)
    if eal.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        f = unreal.BlueprintFactory()
        f.set_editor_property("parent_class", unreal.MOBuildPieceActor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_Piece_" + piece, folder, unreal.Blueprint, f)
    cdo = unreal.get_default_object(bp.generated_class())
    door = spec.get("door")
    leaf = tuple(door["leaf"]) if door else ()
    plist = []
    for p in parts:
        part = unreal.MOBuildPiecePart()
        part.set_editor_property("name", p["name"])
        part.set_editor_property("mesh", unreal.load_asset(p["mesh"]))
        if p.get("instances"):
            part.set_editor_property("instances", p["instances"])
        part.set_editor_property("door_leaf", any(p["name"].startswith(x) for x in leaf))
        plist.append(part)
    cdo.set_editor_property("parts", plist)
    cols = spec.get("colliders", [])
    if cols == "tent":
        colliders = tent_colliders()
    else:
        colliders = [box(c["center"], c["half"], c.get("rot", (0.0, 0.0, 0.0)), c.get("door", False)) for c in cols]
    cdo.set_editor_property("colliders", colliders)
    snap = spec.get("snap")
    cdo.set_editor_property("snap_box", box(snap["center"], snap["half"]) if snap else unreal.MOBuildPieceBox())
    if door:
        cdo.set_editor_property("door_hinge", xf(door["hinge"]))
        cdo.set_editor_property("door_open_yaw_deg", door["open_yaw"])
    sup = spec.get("supports")
    cdo.set_editor_property("grows_supports", bool(sup))
    if sup:
        cdo.set_editor_property("support_mesh", unreal.load_asset("%s/Supports/SM_%s" % (DEST, sup["mesh"])))
        cdo.set_editor_property("support_points", [unreal.Vector(*pt) for pt in sup["points"]])
        cdo.set_editor_property("support_item", sup["item"])
        cdo.set_editor_property("support_cm_per_item", sup["cm_per_item"])
        cdo.set_editor_property("support_lashing_item", sup["lashing"])
        cdo.set_editor_property("support_seconds_per_post", float(sup["seconds"]))
    cdo.set_editor_property("snap_as_wall", bool(spec.get("snap_wall")))
    cdo.set_editor_property("snap_stack_z_offset", float(spec.get("stack_z", 50.0)))
    cdo.set_editor_property("snap_as_roof", bool(spec.get("snap_roof")))
    cdo.set_editor_property("piece_tags", spec.get("tags", []))
    cdo.set_editor_property("attach", {"Face": unreal.MOPieceAttach.FACE, "Edge": unreal.MOPieceAttach.EDGE}.get(spec.get("attach"), unreal.MOPieceAttach.NONE))
    cdo.set_editor_property("attach_host_tags", spec.get("host_tags", []))
    cdo.set_editor_property("member_axis", unreal.MOMemberAxis.HORIZONTAL if spec.get("member_axis") == "Horizontal" else unreal.MOMemberAxis.VERTICAL)
    cdo.set_editor_property("rain_pass", float(spec.get("rain_pass", 0.0)))
    cdo.set_editor_property("wind_pass", float(spec.get("wind_pass", 0.0)))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_asset(path, only_if_is_dirty=False)
    check = unreal.get_default_object(unreal.load_asset(path).generated_class())
    return path, len(check.get_editor_property("parts")), len(check.get_editor_property("colliders"))


def recipe_row(piece, spec, bp_path):
    tools = [{"ToolType": t, "MinQuality": 0.2, "DurabilityConsumed": 2, "bIsRequired": True, "MissingToolTimeMultiplier": 1,
              "MissingToolQualityMultiplier": 1} for t in spec.get("tools", [])]
    cls = "/Script/Engine.BlueprintGeneratedClass'%s.BP_Piece_%s_C'" % (bp_path, piece)
    return {
        "RecipeId": spec["recipe"], "DisplayName": spec["display"], "Description": spec["description"], "Category": "Buildings",
        "bIsBuilding": True, "CraftTime": 0, "TotalBuildTime": spec["seconds"], "BuildRange": 300,
        "RequiredSkillId": spec["skill"][0], "RequiredSkillLevel": spec["skill"][1], "SkillXPReward": max(5, int(spec["seconds"] / 120)),
        "RequiredTools": tools, "BuildParts": [dict(p, ActionId="None") for p in spec["parts"]],
        "PlacementData": {"bPrefersGroundPlacement": True, "bPrefersWallPlacement": False, "bPrefersCeilingPlacement": False, "bAllowZRotation": True,
                          "bAllowXRotation": False, "bAllowYRotation": False, "RotationIncrement": 15, "bRequiresNoCollision": True,
                          "PreviewMesh": "None", "BuildableActorClass": cls},
    }


def supports(mis):
    """The support posts floors share (Saved/BlenderBuildings/Supports): Supports/SM_<Name>, one box of simple collision each."""
    with open(os.path.join(SRC, "Supports", "Supports.json")) as f:
        names = json.load(f)["supports"]
    return [import_mesh(os.path.join(SRC, "Supports", n + ".fbx"), DEST + "/Supports", "SM_" + n, mis, collision=True).get_path_name() for n in names]


def run(pieces=None, do_surfaces=True):
    pieces = pieces or list(BP.PIECES)
    mis = surfaces() if do_surfaces else surface_mis()
    supports(mis)
    report = {"surfaces": list(mis), "pieces": {}}
    recipes = {}
    for piece in pieces:
        spec = BP.PIECES[piece]
        src = os.path.join(SRC, piece)
        with open(os.path.join(src, piece + ".json")) as f:
            built = json.load(f)
        folder = "%s/%s" % (DEST, piece)
        parts = []
        for p in built["parts"]:
            import_mesh(os.path.join(src, p["fbx"]), folder, p["mesh"], mis)
            parts.append({"name": p["name"], "mesh": "%s/%s" % (folder, p["mesh"])})
        if spec["geometry"].startswith("cobble") or spec["geometry"] == "dry_stone_wall":
            stones = stone_parts(piece, random.Random(sum(map(ord, piece))))
            if piece == "CobbleInfill":
                parts = parts[:1] + stones + parts[1:]   # the core, the stones bedded course by course, then the daub over them
            elif piece == "DryStoneWall":
                parts = interleave(parts, stones)        # each band of hearting laid with the courses round it
            else:
                parts = parts + stones
        path, n_parts, n_cols = make_blueprint(piece, spec, parts)
        recipes[spec["recipe"]] = recipe_row(piece, spec, path)
        report["pieces"][piece] = {"blueprint": path, "parts": n_parts, "colliders": n_cols,
                                   "stones": sum(len(p.get("instances", [])) for p in parts)}
        log("%s: %s, %d parts, %d colliders" % (piece, path, n_parts, n_cols))
    # merged with the rows of pieces imported before (a run for some pieces keeps the others' rows)
    path = os.path.join(SRC, "recipes.json")
    if os.path.isfile(path):
        with open(path) as f:
            recipes = dict(json.load(f), **recipes)
    with open(path, "w") as f:
        json.dump(recipes, f, indent=1)
    report["recipes"] = os.path.join(SRC, "recipes.json")
    return json.dumps(report)


def moss_item():
    """SM_Moss01: the moss item's world mesh on the moss surface."""
    mis = surface_mis()
    mesh = import_mesh(os.path.join(SRC, "Moss01", "Moss01.fbx"), DEST + "/Moss01", "SM_Moss01", mis)
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    eal.save_loaded_asset(mesh, False)
    return mesh.get_path_name()
