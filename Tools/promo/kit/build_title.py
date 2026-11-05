"""MO57 promo kit: the 3D title. METAVERSE and ORIGINS cut from the pod's granite, the sun rising behind them over the planet's limb, dust in the air.

    blender -b --factory-startup --python Tools/promo/kit/build_title.py -- '{"out": "...", "scale": 0.5, "frames_total": 240}'

Settings (all optional): line1, line2 (the two lines, "METAVERSE" / "ORIGINS"), frames_total (240 = 10 s), hit (the frame of the hit: the sun clears the limb and the inlay lights, default 24),
samples, scale, dust (motes), logo_only (true: black behind the letters and film transparent, for a logo over anything). Timing and text are data: re-render, don't re-model.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit_common as kit  # noqa: E402
import kit_space as space  # noqa: E402

cfg = kit.args()
FRAMES = int(cfg.get("frames_total", 240))
HIT = int(cfg.get("hit", 24))
LINE1 = cfg.get("line1", "METAVERSE")
LINE2 = cfg.get("line2", "ORIGINS")
LOGO_ONLY = bool(cfg.get("logo_only", False))

kit.reset()
kit.render_setup(cfg, FRAMES, samples=128, transparent=LOGO_ONLY, exposure=0.0, shutter=0.5, bounces=(6, 3, 3, 0, 0))


def letter_mesh(ch, kind, size, depth, bevel, voxel, material, name):
    """One letter as a solid: extruded, bevelled, remeshed and chipped so its edges read as cut stone."""
    ob = kit.glyph(ch, kind, size, name)
    cu = ob.data
    cu.extrude = depth
    cu.bevel_depth = bevel
    cu.bevel_resolution = 3
    cu.resolution_u = 14
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target="MESH")
    ob.select_set(False)
    if voxel:
        rm = ob.modifiers.new("Remesh", "REMESH")
        rm.mode = "VOXEL"
        rm.voxel_size = voxel
        rm.use_smooth_shade = True
        tex = bpy.data.textures.get("MO_Chip") or bpy.data.textures.new("MO_Chip", "CLOUDS")
        tex.noise_scale = size * 0.02
        tex.noise_depth = 4
        d = ob.modifiers.new("Chip", "DISPLACE")
        d.texture = tex
        d.texture_coords = "GLOBAL"
        d.strength = size * 0.0045
        d.mid_level = 0.5
    ob.data.materials.append(material)
    return ob


def build_line(text, kind, size, tracking, z, y, depth, bevel, voxel, material, prefix):
    glyphs, width = kit.layout(text, kind, size, tracking)
    objs = []
    for i, (ch, x) in enumerate(glyphs):
        if ch == " ":
            continue
        ob = letter_mesh(ch, kind, size, depth, bevel, voxel, material, "%s_%02d" % (prefix, i))
        ob.location = (x - width * 0.5, y, z)
        objs.append(ob)
    return objs, width


# ---------- materials ----------
stone, n, out = kit.new_material("MO_TitleStone")
tc = n.add("TexCoord")
p = tc.outputs["Object"]
fine = n.add("TexNoise", Vector=p, Scale=60.0, Detail=8.0, Roughness=0.62)
broad = n.add("TexNoise", Vector=p, Scale=5.0, Detail=6.0, Roughness=0.55)
flecks = n.add("TexVoronoi", Vector=p, Scale=70.0, Randomness=1.0)
fleck_v = n.add("SeparateColor", Color=flecks.outputs["Color"]).outputs[0]
base_col = n.ramp(n.math("ADD", n.math("MULTIPLY", broad.outputs["Fac"], 0.5), n.math("MULTIPLY", fine.outputs["Fac"], 0.5)), [(0.3, (0.60, 0.565, 0.50, 1)), (0.5, (0.76, 0.725, 0.65, 1)), (0.7, (0.84, 0.81, 0.74, 1))])
speck = n.ramp(fleck_v, [(0.0, 1.0), (0.05, 1.0), (0.075, 0.0), (1.0, 0.0)])
col = n.mix(n.math("MULTIPLY", speck, 0.7), base_col, (0.05, 0.045, 0.04, 1))
bsdf = n.add("BsdfPrincipled", Base_Color=col, Roughness=0.66)
bump_h = n.math("ADD", n.math("MULTIPLY", fine.outputs["Fac"], 0.55), n.math("MULTIPLY", broad.outputs["Fac"], 0.8))
bmp = n.add("Bump", Height=bump_h, Strength=0.30, Distance=0.02)
n.link(bmp.outputs[0], bsdf.inputs["Normal"])
n.link(bsdf.outputs[0], out.inputs["Surface"])
inlay_m, n, out = kit.new_material("MO_TitleInlay")
tc = n.add("TexCoord")
noise = n.add("TexNoise", Vector=tc.outputs["Object"], Scale=7.0, Detail=9.0, Roughness=0.62)
vein = n.ramp(noise.outputs["Fac"], [(0.35, 0.55), (0.52, 1.0), (0.66, 0.7)])
inlay_val = n.value(0.0, "Inlay")
teal = (0.075, 0.52, 0.44, 1)
em = n.add("Emission", Color=teal, Strength=n.math("MULTIPLY", n.math("MULTIPLY", n.add("SeparateColor", Color=vein).outputs[0], inlay_val.outputs[0]), 1.9))
b = n.add("BsdfPrincipled", Base_Color=(0.11, 0.30, 0.26, 1), Roughness=0.38, Metallic=0.2)
add = n.add("AddShader")
n.link(b.outputs[0], add.inputs[0])
n.link(em.outputs[0], add.inputs[1])
n.link(add.outputs[0], out.inputs["Surface"])
for f, v in ((1, 0.0), (HIT - 2, 0.0), (HIT + 10, 1.3), (HIT + 34, 0.72), (FRAMES, 0.8)):
    inlay_val.outputs[0].default_value = v
    inlay_val.outputs[0].keyframe_insert("default_value", frame=f)

# ---------- the letters: 1 unit of scene = about a metre; the wordmark is ~9 m wide ----------
SIZE1, SIZE2 = 1.52, 0.86
cap1 = SIZE1 * kit.metrics("display")["cap_height"]
letters1, w1 = build_line(LINE1, "display", SIZE1, 0.052, 0.55, 0.0, 0.16, 0.016, 0.0052, stone, "L1")
z2 = 0.55 - SIZE2 * 0.92
letters2, w2 = build_line(LINE2, "display", SIZE2, 0.26, z2, 0.03, 0.09, 0.012, 0.0052, inlay_m, "L2")
# the rules either side of ORIGINS draw outward on the hit
rule_m, n, out = kit.new_material("MO_TitleRule")
n.link(n.add("Emission", Color=(0.075, 0.52, 0.44, 1), Strength=1.5).outputs[0], out.inputs["Surface"])
for side in (-1, 1):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.translate(bm, verts=bm.verts, vec=(side * 0.5, 0, 0))    # grows away from the letters
    me = bpy.data.meshes.new("rule")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(rule_m)
    ob = bpy.data.objects.new("rule", me)
    bpy.context.collection.objects.link(ob)
    gap = w2 * 0.5 + 0.34
    ob.location = (side * gap, 0.03, z2 + SIZE2 * 0.31)
    length = max(w1 * 0.5 - gap, 0.2)
    ob.scale = (0.001, 0.03, 0.016)
    ob.keyframe_insert("scale", frame=HIT)
    ob.scale = (length, 0.03, 0.016)
    ob.keyframe_insert("scale", frame=HIT + 30)

# ---------- behind them: the limb and the sun ----------
cam = space.camera(lens=50.0, loc=(0, -17.0, -0.15), target=(0, 0, -0.25))
cam.data.dof.use_dof = True
cam.data.dof.focus_object = None
cam.data.dof.focus_distance = 16.4
cam.data.dof.aperture_fstop = 5.6
cam.keyframe_insert("location", frame=1)
cam.location = (0, -15.2, -0.20)
cam.keyframe_insert("location", frame=FRAMES)
kit.wobble(cam, "rotation_euler", 0, 120.0, 0.0007)
kit.wobble(cam, "rotation_euler", 2, 150.0, 0.0007)
for fc in kit.fcurves(cam.animation_data.action):
    if fc.data_path == "location":
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"

if not LOGO_ONLY:
    space.stars(0.8, 0.6)
    R = 2600.0
    center = Vector((0.0, 3100.0, -R - 1.6 - 190.0))
    sun_end = Vector((0.0, 1.0, 0.004 + (-1.6 + 0.4) / 3000.0)).normalized()
    # where the planet's edge stands above the camera's level at the middle of the frame, as a slope (the shell's top is a little higher)
    to_centre = Vector((0.0, 3100.0 + 16.0, center.z + 0.15))
    down = math.atan2(-to_centre.z, to_centre.y)
    LIMB = math.tan(math.asin(R * 1.004 / to_centre.length) - down)
    sun_rise = Vector((0, 1, LIMB + 0.0050)).normalized()
    sun_start = Vector((0, 1, LIMB - 0.016)).normalized()
    space.planet(center, R, sun_rise, detail=1.0, gain=1.0, segments=int(cfg.get("segments", 1024)))
    lamp, disc = space.sun(sun_rise, strength=0.7, disc=420.0, angular_deg=0.8)
    # the disc comes over the limb on the hit, then keeps rising slowly
    for f, d in ((1, sun_start), (HIT, Vector((0, 1, LIMB - 0.0015)).normalized()), (HIT + 26, Vector((0, 1, LIMB + 0.0035)).normalized()), (FRAMES, sun_rise)):
        disc.location = d * 40000.0
        disc.keyframe_insert("location", frame=f)
else:
    bpy.context.scene.world = bpy.data.worlds.new("Black")
    bpy.context.scene.world.use_nodes = False

# the light on the letters: a raking warm key from low left, a cool fill, a rim from the sun's side that comes up on the hit
key_l = bpy.data.lights.new("Key", "AREA")
key_l.size = 7.0
key_l.color = (1.0, 0.86, 0.68)
key_ob = bpy.data.objects.new("Key", key_l)
bpy.context.collection.objects.link(key_ob)
key_ob.location = (-10.0, -7.5, -4.5)
space.aim(key_ob, (0, 0, -0.2))
for f, e in ((1, 25.0), (HIT, 60.0), (HIT + 14, 1500.0), (HIT + 50, 950.0), (FRAMES, 1050.0)):
    key_l.energy = e
    key_l.keyframe_insert("energy", frame=f)
fill = bpy.data.lights.new("Fill", "AREA")
fill.size = 14.0
fill.color = (0.55, 0.78, 0.88)
fill.energy = 240.0
fill_ob = bpy.data.objects.new("Fill", fill)
bpy.context.collection.objects.link(fill_ob)
fill_ob.location = (8.0, -9.0, 5.0)
space.aim(fill_ob, (0, 0, 0))
rim = bpy.data.lights.new("Rim", "AREA")
rim.size = 30.0
rim.color = (1.0, 0.78, 0.56)
rim_ob = bpy.data.objects.new("Rim", rim)
bpy.context.collection.objects.link(rim_ob)
rim_ob.location = (0.0, 9.0, 1.0)
space.aim(rim_ob, (0, 0, 0))
for f, e in ((1, 0.0), (HIT - 2, 40.0), (HIT + 16, 3000.0), (HIT + 60, 1800.0), (FRAMES, 2000.0)):
    rim.energy = e
    rim.keyframe_insert("energy", frame=f)

letter_set = bpy.data.collections.new("Letters")
bpy.context.scene.collection.children.link(letter_set)
for ob in letters1 + letters2 + [o for o in bpy.data.objects if o.name.startswith("rule")]:
    letter_set.objects.link(ob)
    bpy.context.collection.objects.unlink(ob)
for light_ob in (key_ob, fill_ob, rim_ob):
    light_ob.light_linking.receiver_collection = letter_set

# ---------- dust in the air: small lit motes drifting across the light ----------
mote_m, n, out = kit.new_material("MO_Dust")
n.link(n.add("Emission", Color=(1.0, 0.86, 0.68, 1), Strength=1.1).outputs[0], out.inputs["Surface"])
bm = bmesh.new()
bmesh.ops.create_icosphere(bm, subdivisions=1, radius=1.0)
me = bpy.data.meshes.new("mote")
bm.to_mesh(me)
bm.free()
me.materials.append(mote_m)
rng = random.Random(4)
for i in range(int(cfg.get("dust", 500))):
    ob = bpy.data.objects.new("mote_%d" % i, me)
    bpy.context.collection.objects.link(ob)
    s = rng.uniform(0.0025, 0.012)
    ob.scale = (s, s, s)
    p = Vector((rng.uniform(-9, 9), rng.uniform(-9.5, 2.5), rng.uniform(-3.8, 3.8)))
    v = Vector((rng.uniform(0.4, 1.6), rng.uniform(-0.3, 0.3), rng.uniform(-0.1, 0.35)))
    ob.location = p
    ob.keyframe_insert("location", frame=1)
    ob.location = p + v
    ob.keyframe_insert("location", frame=FRAMES)
    for fc in kit.fcurves(ob.animation_data.action):
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"

space.glow(threshold=1.3, strength=0.55, size=8, streaks=True, streak_strength=0.25)
kit.finish(cfg, "title")
