"""MO57 promo kit: transitions and atmosphere as RGBA frames, to lay over any picture ("screen" them: they are light on transparency).

    blender -b --factory-startup --python Tools/promo/kit/build_overlay.py -- '{"kind": "flare", "frames_total": 16, "out": "...", "scale": 0.5}'

Kinds:
  flare    a sweep of anamorphic light across the frame: a flash for a cut on a hit. direction 1 / -1, colour warm | teal
  embers   sparks rising and drifting past the lens, soft near ones and fine far ones (loopable by overlaying two offset copies). count, seconds via frames_total
  dust     a burst of dust motes outward from the centre, slowing, lit from one side: an impact. count
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

cfg = kit.args()
KIND = cfg.get("kind", "flare")
FRAMES = int(cfg.get("frames_total", 16))
kit.reset()
kit.render_setup(cfg, FRAMES, engine="EEVEE", samples=48, transparent=True, view="Standard", look="None", blur=True, shutter=0.5, depth="8")
ASPECT = 16.0 / 9.0
D = 50.0 * ASPECT / 36.0
cam_data = bpy.data.cameras.new("Cam")
cam_data.lens = 50.0
cam_data.sensor_width = 36.0
cam = bpy.data.objects.new("Cam", cam_data)
bpy.context.collection.objects.link(cam)
bpy.context.scene.camera = cam
cam.location = (0, -D, 0)
cam.rotation_euler = (math.pi / 2, 0, 0)


def linear_keys(ob):
    for fc in kit.fcurves(ob.animation_data.action):
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"


def flare():
    """A soft horizontal bar of light that crosses the frame and a broad veil that blooms with it."""
    warm = cfg.get("colour", "warm") == "warm"
    core = (1.0, 0.86, 0.66, 1) if warm else kit.linear(kit.TEAL_HI)
    direction = int(cfg.get("direction", 1))
    bars = []
    for name, sx, sz, gain in (("veil", 2.6, 0.9, 0.85), ("bar", 2.2, 0.055, 2.5), ("line", 3.6, 0.008, 5.0)):
        m, n, out = kit.new_material("Flare_" + name)
        m.surface_render_method = "BLENDED"
        tc = n.add("TexCoord")
        gen = tc.outputs["UV"]
        sep = n.add("SeparateXYZ", Vector=gen)
        u = n.math("POWER", n.math("SUBTRACT", 1.0, n.math("ABSOLUTE", n.math("MULTIPLY", n.math("SUBTRACT", sep.outputs[0], 0.5), 2.0)), clamp=True), 2.0)
        v = n.math("POWER", n.math("SUBTRACT", 1.0, n.math("ABSOLUTE", n.math("MULTIPLY", n.math("SUBTRACT", sep.outputs[1], 0.5), 2.0)), clamp=True), 2.5)
        shape = n.math("MULTIPLY", u, v)
        env = n.value(0.0, "Env")
        alpha = n.math("MULTIPLY", shape, env.outputs[0], clamp=True)
        mix = n.add("MixShader", Fac=alpha)
        n.link(n.add("BsdfTransparent").outputs[0], mix.inputs[1])
        n.link(n.add("Emission", Color=core, Strength=gain).outputs[0], mix.inputs[2])
        n.link(mix.outputs[0], out.inputs["Surface"])
        bm = bmesh.new()
        uv = bm.loops.layers.uv.new()
        vs = [bm.verts.new((-sx, 0, -sz)), bm.verts.new((sx, 0, -sz)), bm.verts.new((sx, 0, sz)), bm.verts.new((-sx, 0, sz))]
        f = bm.faces.new(vs)
        for loop, c in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
            loop[uv].uv = c
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(m)
        ob = bpy.data.objects.new(name, me)
        bpy.context.collection.objects.link(ob)
        bars.append((ob, m))
    mid = FRAMES * 0.42
    for ob, m in bars:
        ob.location = (-direction * ASPECT * 1.1, 0, float(cfg.get("z", 0.0)))
        ob.keyframe_insert("location", frame=1)
        ob.location = (direction * ASPECT * 1.1, 0, float(cfg.get("z", 0.0)))
        ob.keyframe_insert("location", frame=FRAMES)
        linear_keys(ob)
        s = m.node_tree.nodes["Env"].outputs[0]
        for f, v in ((1, 0.0), (mid, 1.0), (FRAMES, 0.0)):
            s.default_value = v
            s.keyframe_insert("default_value", frame=f)


def motes(count, born, life, velocity, size, colour, strength, seed):
    m, n, out = kit.new_material("Mote")
    m.surface_render_method = "BLENDED"
    a = n.add("ObjectInfo").outputs["Random"]
    em = n.add("Emission", Color=colour, Strength=strength)
    n.link(em.outputs[0], out.inputs["Surface"])
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
    me = bpy.data.meshes.new("mote")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(m)
    rng = random.Random(seed)
    for i in range(count):
        ob = bpy.data.objects.new("m%d" % i, me)
        bpy.context.collection.objects.link(ob)
        f0 = born(rng)
        f1 = f0 + life(rng)
        p0, p1 = velocity(rng, f1 - f0)
        s = size(rng)
        ob.scale = (0, 0, 0)
        ob.keyframe_insert("scale", frame=f0 - 1)
        ob.scale = (s, s, s)
        ob.keyframe_insert("scale", frame=f0 + 3)
        ob.keyframe_insert("scale", frame=f1 - 8)
        ob.scale = (0, 0, 0)
        ob.keyframe_insert("scale", frame=f1)
        ob.location = p0
        ob.keyframe_insert("location", frame=f0)
        ob.location = p1
        ob.keyframe_insert("location", frame=f1)
        linear_keys(ob)


def embers():
    count = int(cfg.get("count", 90))

    def where(rng, frames):
        y = rng.uniform(-D * 0.75, 1.4)
        depth = (y + D) / D
        x0 = rng.uniform(-1.2, 1.2) * ASPECT * 0.6 * depth
        z0 = rng.uniform(-0.8, 0.5) * depth
        v = Vector((rng.uniform(-0.002, 0.004), rng.uniform(-0.001, 0.001), rng.uniform(0.002, 0.007))) * depth
        return Vector((x0, y, z0)), Vector((x0, y, z0)) + v * frames + Vector((math.sin(rng.uniform(0, 6)) * 0.05, 0, 0))

    motes(count, lambda r: r.randint(-40, FRAMES), lambda r: r.randint(40, 110), where, lambda r: r.uniform(0.0012, 0.0052), kit.linear(kit.EMBER), float(cfg.get("strength", 6.0)), 11)
    cam.data.dof.use_dof = True
    cam.data.dof.focus_distance = D
    cam.data.dof.aperture_fstop = 0.5


def dust():
    count = int(cfg.get("count", 240))

    def where(rng, frames):
        a = rng.uniform(0, 6.283)
        rad = rng.uniform(0.0, 0.25)
        p0 = Vector((math.cos(a) * rad, rng.uniform(-0.6, 0.6), math.sin(a) * rad * 0.6))
        speed = rng.uniform(0.3, 1.1)
        return p0, p0 + Vector((math.cos(a), rng.uniform(-0.2, 0.2), math.sin(a) * 0.55)) * speed

    motes(count, lambda r: r.randint(1, 4), lambda r: r.randint(FRAMES // 2, FRAMES), where, lambda r: r.uniform(0.0008, 0.0034), (0.78, 0.70, 0.60, 1), float(cfg.get("strength", 1.2)), 17)
    cam.data.dof.use_dof = True
    cam.data.dof.focus_distance = D
    cam.data.dof.aperture_fstop = 0.8


{"flare": flare, "embers": embers, "dust": dust}[KIND]()
kit.finish(cfg, KIND)
