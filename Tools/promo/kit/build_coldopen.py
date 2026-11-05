"""MO57 promo kit: the cold open, three CG shots of the pods coming down (clearly not gameplay: no terrain, no game camera).

    blender -b --factory-startup --python Tools/promo/kit/build_coldopen.py -- '{"shot": "void", "frames_total": 160, "out": "...", "scale": 0.5}'

Shots: "void" (the stream of pods past the planet's limb, the sun rising over it), "entry" (one pod, nose first, burning), "streak" (the dusk side from high up,
the pods falling as thin lines, one passing close; ends in a flash for the cut to black).
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
from mathutils import Euler, Vector  # noqa: E402

import kit_common as kit  # noqa: E402
import kit_pod  # noqa: E402
import kit_space as space  # noqa: E402

cfg = kit.args()
SHOT = cfg.get("shot", "void")
FRAMES = int(cfg.get("frames_total", {"void": 160, "entry": 160, "streak": 80}[SHOT]))
kit.reset()


def tumble(ob, rate, f0=1, f1=None, axis=None):
    """Keep turning at a constant rate (radians per frame about each axis)."""
    f1 = f1 or FRAMES
    r0 = ob.rotation_euler.copy()
    ob.rotation_mode = "XYZ"
    for f, s in ((f0, 0.0), (f1, float(f1 - f0))):
        ob.rotation_euler = (r0.x + rate[0] * s, r0.y + rate[1] * s, r0.z + rate[2] * s)
        ob.keyframe_insert("rotation_euler", frame=f)
    kit.set_interp(ob, "LINEAR")


def void():
    kit.render_setup(cfg, FRAMES, samples=96, exposure=0.0, shutter=0.5, bounces=(4, 2, 2, 0, 0))
    # the camera at the origin looks along +Y; the planet below and ahead; the sun behind its limb, to rise
    R = 3200.0
    center = Vector((0.0, 4700.0, -3050.0))
    sun_start = Vector((0.20, 1.0, -0.02)).normalized()
    sun_end = Vector((0.19, 1.0, 0.052)).normalized()
    space.stars(1.0, 1.0)
    space.planet(center, R, sun_end, detail=1.0, gain=1.1, arc_dir=(0.2, 0.15, 0.97), segments=int(cfg.get("segments", 512)))
    lamp, disc = space.sun(sun_end, strength=5.0, disc=260.0)
    for f, d in ((1, sun_start), (FRAMES, sun_end)):
        disc.location = d * 40000.0
        disc.keyframe_insert("location", frame=f)
    kit.set_interp(disc, "LINEAR")
    # a soft blue bounce from the planet's lit limb, so the near pods' undersides are not pure black
    fill = bpy.data.lights.new("Bounce", "SUN")
    fill.energy = 0.55
    fill.color = (0.45, 0.6, 1.0)
    fob = bpy.data.objects.new("Bounce", fill)
    bpy.context.collection.objects.link(fob)
    fob.rotation_euler = Vector((0.1, 0.5, -1)).to_track_quat("-Z", "Y").to_euler()
    pods = bpy.data.collections.new("Pods")
    bpy.context.scene.collection.children.link(pods)
    fob.light_linking.receiver_collection = pods
    cam = space.camera(lens=38.0, loc=(0, 0, 0), target=(120, 1000, 30))
    cam.data.dof.use_dof = True
    cam.data.dof.focus_distance = 26.0
    cam.data.dof.aperture_fstop = 1.2
    cam.keyframe_insert("location", frame=1)
    cam.location = (0.0, 6.0, -0.3)
    cam.keyframe_insert("location", frame=FRAMES)
    kit.set_interp(cam, "LINEAR")
    kit.wobble(cam, "rotation_euler", 0, 90.0, 0.0012)
    kit.wobble(cam, "rotation_euler", 2, 110.0, 0.0012)
    # the stream: copies of a small pod, far to near, all falling towards the planet
    rng = random.Random(57)
    base = kit_pod.make_pod(detail=4, name="MO_PodSwarm", heat=True)
    base.hide_render = True
    base.hide_viewport = True
    # the line they fall along, from high behind the camera's right to the planet at frame left
    start = Vector((520.0, 180.0, 210.0))
    stop = Vector((-520.0, 2200.0, -360.0))
    for i in range(int(cfg.get("swarm", 420))):
        t = rng.random()
        spread = 8.0 + 170.0 * rng.random() ** 2.0
        off = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))) * spread
        p0 = start.lerp(stop, t) + off
        ob = bpy.data.objects.new("swarm_%03d" % i, base.data)
        pods.objects.link(ob)
        ob.scale = (1.0, 1.0, 1.0)
        ob.rotation_euler = (rng.uniform(0, 6.28), rng.uniform(0, 6.28), rng.uniform(0, 6.28))
        tumble(ob, (rng.uniform(-0.01, 0.01), rng.uniform(-0.01, 0.01), rng.uniform(-0.01, 0.01)))
        drift = (stop - start).normalized() * rng.uniform(26.0, 44.0)
        ob.location = p0
        ob.keyframe_insert("location", frame=1)
        ob.location = p0 + drift
        ob.keyframe_insert("location", frame=FRAMES)
        for fc in kit.fcurves(ob.animation_data.action):
            for k in fc.keyframe_points:
                k.interpolation = "LINEAR"
    # the hero pod passes through focus, close, turning slowly
    hero = kit_pod.make_pod(detail=int(cfg.get("detail", 7)), name="MO_PodHero", heat=True)
    pods.objects.link(hero)
    bpy.context.collection.objects.unlink(hero)
    hero.rotation_euler = (0.5, 0.25, 2.5)
    tumble(hero, (0.0032, 0.0015, -0.0024))
    hero.location = (9.5, 25.0, 3.2)
    hero.keyframe_insert("location", frame=1)
    hero.location = (-6.5, 28.0, -1.0)
    hero.keyframe_insert("location", frame=FRAMES)
    for fc in kit.fcurves(hero.animation_data.action):
        if fc.data_path == "location":
            for k in fc.keyframe_points:
                k.interpolation = "LINEAR"
    space.glow(threshold=1.3, strength=0.5, size=7, streaks=True, streak_strength=0.22)


def shell_mesh(name, a, length, power=0.5, segments=96, rings=64, x0=-1.2):
    """A closed surface of revolution about X: radius a * (x - x0)^power, from the nose back along +X, capped at the back. The flame's volume lives inside it."""
    import bmesh
    bm = bmesh.new()
    verts = []
    for i in range(rings + 1):
        t = (i / rings) ** 1.6
        x = x0 + t * length
        r = a * (max(x - x0, 1e-4)) ** power
        ring = []
        for j in range(segments):
            ang = 2 * math.pi * j / segments
            ring.append(bm.verts.new((x, r * math.cos(ang), r * math.sin(ang))))
        verts.append(ring)
    for i in range(rings):
        for j in range(segments):
            bm.faces.new((verts[i][j], verts[i][(j + 1) % segments], verts[i + 1][(j + 1) % segments], verts[i + 1][j]))
    bm.faces.new(list(reversed(verts[0])))
    bm.faces.new(verts[-1])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    return ob


def flame_material(name, a=0.98, length=13.0, gain=9.0, x0=-1.16):
    """An emission volume: a sheath of fire hugging the bow shock, torn by noise that runs back along +X, cooling and thinning down the wake; a core of fire in the wake behind the pod."""
    m, n, out = kit.new_material(name)
    tc = n.add("TexCoord")
    p = tc.outputs["Object"]
    sep = n.add("SeparateXYZ", Vector=p)
    x, y, z = sep.outputs[0], sep.outputs[1], sep.outputs[2]
    r = n.math("SQRT", n.math("ADD", n.math("MULTIPLY", y, y), n.math("MULTIPLY", z, z)))
    xn = n.math("MAXIMUM", n.math("SUBTRACT", x, x0), 0.0)
    t = n.math("DIVIDE", xn, length, clamp=True)
    bow = n.math("MULTIPLY", n.math("SQRT", xn), a)
    width = n.math("ADD", 0.075, n.math("MULTIPLY", t, 0.75))
    d = n.math("DIVIDE", n.math("SUBTRACT", r, bow), width)
    sheath = n.math("POWER", 2.718, n.math("MULTIPLY", n.math("MULTIPLY", d, d), -1.0))
    core_w = n.math("ADD", 0.34, n.math("MULTIPLY", t, 1.0))
    core_d = n.math("DIVIDE", r, core_w)
    behind = n.add("MapRange", Value=x, From_Min=0.95, From_Max=2.0, To_Min=0.0, To_Max=1.0, p_clamp=True).outputs[0]
    core = n.math("MULTIPLY", n.math("POWER", 2.718, n.math("MULTIPLY", n.math("MULTIPLY", core_d, core_d), -1.0)), n.math("MULTIPLY", behind, 0.75))
    body = n.math("MAXIMUM", sheath, core)
    time_node = n.value(0.0, "Time")
    mp = n.add("CombineXYZ", X=n.math("ADD", n.math("MULTIPLY", x, 0.42), n.math("MULTIPLY", time_node.outputs[0], -0.17)), Y=n.math("MULTIPLY", y, 2.5), Z=n.math("MULTIPLY", z, 2.5))
    tear = n.add("TexNoise", Vector=mp.outputs[0], Scale=2.6, Detail=9.0, Roughness=0.6, Distortion=0.7)
    fine = n.add("TexNoise", Vector=mp.outputs[0], Scale=9.0, Detail=6.0, Roughness=0.55)
    torn = n.add("MapRange", Value=n.math("ADD", n.math("MULTIPLY", tear.outputs["Fac"], 0.75), n.math("MULTIPLY", fine.outputs["Fac"], 0.25)), From_Min=0.40, From_Max=0.70, To_Min=0.0, To_Max=1.0, p_clamp=True, p_interpolation_type="SMOOTHSTEP").outputs[0]
    fade = n.math("POWER", n.math("SUBTRACT", 1.0, t, clamp=True), 2.4)
    plasma = n.value(1.0, "Plasma")
    density = n.math("MULTIPLY", n.math("MULTIPLY", body, torn), n.math("MULTIPLY", fade, plasma.outputs[0]))
    temperature = n.math("ADD", 1700.0, n.math("MULTIPLY", n.math("POWER", n.math("SUBTRACT", 1.0, t, clamp=True), 2.5), 3600.0))
    col = n.add("Blackbody", Temperature=temperature)
    em = n.add("Emission", Color=col.outputs[0], Strength=n.math("MULTIPLY", density, gain))
    n.link(em.outputs[0], out.inputs["Volume"])
    return m


def key_material_values(mat, values):
    """values: {node name: [(frame, value), ...]}."""
    for name, keys in values.items():
        sock = mat.node_tree.nodes[name].outputs[0]
        for f, v in keys:
            sock.default_value = v
            sock.keyframe_insert("default_value", frame=f)


def entry():
    kit.render_setup(cfg, FRAMES, samples=110, exposure=-0.4, shutter=0.6, bounces=(4, 2, 2, 0, 0))
    # falling along -X (the pod's nose); the camera rides alongside, a little behind and above; the planet far below
    R = 3200.0
    center = Vector((0, 0, -R - 34.0))
    sun_dir = Vector((0.75, 0.62, 0.22)).normalized()     # out of shot, ahead and right
    space.stars(0.6, 0.4)
    space.planet(center, R, sun_dir, detail=1.6, gain=0.55, arc_dir=(0.6, 0.5, 0.62), segments=int(cfg.get("segments", 512)))
    lamp, disc = space.sun(sun_dir, strength=0.75, disc=90.0)
    pod = kit_pod.make_pod(detail=int(cfg.get("detail", 8)), name="MO_PodHero", heat=True)
    pod.rotation_euler = (0.25, math.radians(-16), 0)
    tumble(pod, (0.0035, 0.0, 0.0), axis="X")
    hn = kit_pod.heat_node(pod)
    for f, v in ((1, 0.25), (int(FRAMES * 0.45), 0.8), (FRAMES, 1.0)):
        hn.outputs[0].default_value = v
        hn.outputs[0].keyframe_insert("default_value", frame=f)
    # the fire round it: one volume, aligned with the fall
    flame = shell_mesh("flame", 2.5, 15.0, power=0.52)
    mat = flame_material("MO_Flame")
    flame.data.materials.append(mat)
    flame.rotation_euler = (0, math.radians(-10), 0)
    key_material_values(mat, {"Time": [(1, 0.0), (FRAMES, float(FRAMES))], "Plasma": [(1, 0.28), (int(FRAMES * 0.55), 0.9), (FRAMES, 1.2)]})
    kit.set_interp(mat.node_tree, "LINEAR")
    c = bpy.context.scene.cycles
    for attr, val in (("volume_step_rate", 1.0), ("volume_max_steps", 512), ("volume_bounces", 0)):
        if hasattr(c, attr):
            setattr(c, attr, val)
    # sparks
    spark_mat, n, out = kit.new_material("MO_Spark")
    em = n.add("Emission", Color=kit.linear(kit.EMBER), Strength=70.0)
    n.link(em.outputs[0], out.inputs["Surface"])
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=1.0)
    me = bpy.data.meshes.new("spark")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(spark_mat)
    rng = random.Random(9)
    for i in range(int(cfg.get("sparks", 260))):
        ob = bpy.data.objects.new("spark_%03d" % i, me)
        bpy.context.collection.objects.link(ob)
        ob.scale = [rng.uniform(0.004, 0.018)] * 3
        born = rng.randint(-40, FRAMES)
        life = rng.randint(18, 46)
        ang, rad = rng.uniform(0, 6.28), rng.uniform(0.15, 0.85)
        y, z = rad * math.cos(ang), rad * math.sin(ang)
        p0 = Vector((rng.uniform(-1.1, -0.2), y, z))
        v = Vector((rng.uniform(0.35, 0.8), y * rng.uniform(0.01, 0.05), z * rng.uniform(0.01, 0.05)))
        ob.location = p0
        ob.keyframe_insert("location", frame=born)
        ob.location = p0 + v * life
        ob.keyframe_insert("location", frame=born + life)
        ob.hide_render = True
        ob.keyframe_insert("hide_render", frame=born - 1)
        ob.hide_render = False
        ob.keyframe_insert("hide_render", frame=born)
        ob.hide_render = True
        ob.keyframe_insert("hide_render", frame=born + life)
        for fc in kit.fcurves(ob.animation_data.action):
            for k in fc.keyframe_points:
                k.interpolation = "LINEAR" if fc.data_path == "location" else "CONSTANT"
    cam = space.camera(lens=35.0, loc=(0.3, -6.6, 1.1), target=(0.15, 0, 0.0))
    cam.data.dof.use_dof = True
    cam.data.dof.focus_distance = 6.2
    cam.data.dof.aperture_fstop = 0.9
    cam.keyframe_insert("location", frame=1)
    cam.location = (-0.3, -4.8, 0.7)
    cam.keyframe_insert("location", frame=FRAMES)
    # the buffeting grows with the fire
    for idx in range(3):
        m = kit.wobble(cam, "rotation_euler", idx, 7.0, 0.006)
        if m:
            m.use_restricted_range = False
    kit.wobble(cam, "location", 2, 5.0, 0.03)
    space.glow(threshold=1.1, strength=0.7, size=8, streaks=False)


def streak():
    kit.render_setup(cfg, FRAMES, samples=96, exposure=0.0, shutter=0.55, bounces=(3, 2, 2, 0, 0))
    R = 3200.0
    center = Vector((0, 0, -R - 90.0))
    sun_dir = Vector((-0.70, 0.71, -0.012)).normalized()     # just under the horizon, ahead and to the left: the world's dusk
    space.stars(1.2, 0.8)
    space.planet(center, R, sun_dir, detail=5.0, gain=0.5, arc_dir=(-0.75, 0.2, 0.63), segments=int(cfg.get("segments", 512)))
    lamp, disc = space.sun(sun_dir, strength=1.5, disc=60.0)
    lamp.data.color = (1.0, 0.62, 0.36)
    cam = space.camera(lens=28.0, loc=(0, 0, 0), target=(-160, 600, -40))
    kit.wobble(cam, "rotation_euler", 0, 40.0, 0.001)
    mat, n, out = kit.new_material("MO_Streak")
    m_tc = n.add("TexCoord")
    z = n.add("SeparateXYZ", Vector=m_tc.outputs["Object"]).outputs[2]
    fade = n.add("MapRange", Value=z, From_Min=0.0, From_Max=1.0, To_Min=1.0, To_Max=0.0, p_clamp=True).outputs[0]
    col = n.ramp(fade, [(0.0, (0.9, 0.25, 0.05, 1)), (0.7, (1.0, 0.65, 0.3, 1)), (1.0, (1.0, 0.95, 0.85, 1))])
    em = n.add("Emission", Color=col, Strength=n.math("MULTIPLY", n.math("POWER", fade, 2.0), 90.0))
    add = n.add("AddShader")
    n.link(em.outputs[0], add.inputs[0])
    n.link(n.add("BsdfTransparent").outputs[0], add.inputs[1])
    n.link(add.outputs[0], out.inputs["Surface"])
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=10, radius1=1.0, radius2=0.0, depth=1.0)
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0, 0, 0.5))
    me = bpy.data.meshes.new("streak")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    rng = random.Random(21)

    def add_streak(p0, p1, f0, f1, width, length):
        ob = bpy.data.objects.new("streak", me)
        bpy.context.collection.objects.link(ob)
        d = (p1 - p0).normalized()
        ob.rotation_euler = (-d).to_track_quat("Z", "Y").to_euler()
        ob.scale = (width, width, length)
        ob.location = p0
        ob.keyframe_insert("location", frame=f0)
        ob.location = p1
        ob.keyframe_insert("location", frame=f1)
        for fc in kit.fcurves(ob.animation_data.action):
            for k in fc.keyframe_points:
                k.interpolation = "LINEAR"
        return ob

    # the far rain of pods: thin lines coming down all over the cloud deck
    for i in range(int(cfg.get("rain", 420))):
        far = 260.0 * (8.0 ** rng.random())    # many far, a few near
        x = rng.uniform(-1.25, 1.25) * far * 0.75
        top = -60.0 + rng.uniform(-0.05, 0.42) * far       # some already low over the deck, some high in the frame
        p0 = Vector((x, far, top))
        p1 = p0 + Vector((rng.uniform(-0.05, -0.02) * far, 0, -rng.uniform(0.09, 0.17) * far))
        f0 = rng.randint(-40, FRAMES - 10)
        add_streak(p0, p1, f0, f0 + rng.randint(30, 64), 0.0022 * far, 0.05 * far)
    # one close: it crosses the frame and burns out over the lens
    add_streak(Vector((62.0, 118.0, 46.0)), Vector((-6.0, 16.0, -7.0)), 18, FRAMES, 0.38, 26.0)
    head_mat, n, out = kit.new_material("MO_StreakHead")
    n.link(n.add("Emission", Color=(1, 0.92, 0.78, 1), Strength=900.0).outputs[0], out.inputs["Surface"])
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3, radius=0.42)
    hm = bpy.data.meshes.new("head")
    bm.to_mesh(hm)
    bm.free()
    hm.materials.append(head_mat)
    head = bpy.data.objects.new("head", hm)
    bpy.context.collection.objects.link(head)
    head.location = (62.0, 118.0, 46.0)
    head.keyframe_insert("location", frame=18)
    head.location = (-6.0, 16.0, -7.0)
    head.keyframe_insert("location", frame=FRAMES)
    for fc in kit.fcurves(head.animation_data.action):
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"
    space.glow(threshold=1.3, strength=0.7, size=8, streaks=True, streak_strength=0.3)


{"void": void, "entry": entry, "streak": streak}[SHOT]()
kit.finish(cfg, SHOT)
