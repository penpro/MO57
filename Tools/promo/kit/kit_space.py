"""MO57 promo kit: the planet from orbit -- stars, a world with no lights on its night side, a thin blue atmosphere, a sun that rises over the limb. For titles and
clearly non-gameplay moments only (the honesty rule: Docs/Promo_Video.md section 1); nothing here pretends to be the game.

Scene convention: Z up, the planet a big sphere; pass sun_dir as the unit vector from the scene towards the sun.
"""
import math

import bmesh
import bpy
from mathutils import Vector

import kit_common as kit


def stars(strength=1.0, band=0.5, seed=3.0):
    """The world: three layers of pin-point stars and a faint band of dust. Pass strength=0 for pure black."""
    w = bpy.data.worlds.new("MO_Stars")
    bpy.context.scene.world = w
    w.use_nodes = True
    t = w.node_tree
    for x in list(t.nodes):
        t.nodes.remove(x)
    n = kit.Nodes(t)
    out = t.nodes.new("ShaderNodeOutputWorld")
    tc = n.add("TexCoord")
    gen = n.add("VectorMath", p_operation="NORMALIZE", Vector=tc.outputs["Generated"]).outputs[0]

    def layer(scale, power, gain, offset):
        v = n.add("TexVoronoi", Vector=gen, Scale=scale, W=seed + offset, p_voronoi_dimensions="4D", Randomness=1.0)
        d = v.outputs["Distance"]
        core = n.math("POWER", n.math("SUBTRACT", 1.0, n.math("MULTIPLY", d, 14.0), clamp=True), power)
        rand = n.add("SeparateColor", Color=v.outputs["Color"])
        bright = n.math("MULTIPLY", core, n.math("POWER", rand.outputs[0], 6.0))
        tint = n.mix(rand.outputs[1], (0.75, 0.82, 1.0, 1), (1.0, 0.86, 0.72, 1))
        return bright, tint, gain

    total = None
    for scale, power, gain, off in ((220.0, 6.0, 160.0, 0.0), (520.0, 7.0, 90.0, 1.7), (1100.0, 8.0, 44.0, 3.1)):
        bright, tint, g = layer(scale, power, gain, off)
        col = n.add("VectorMath", p_operation="SCALE", Vector=tint, Scale=n.math("MULTIPLY", bright, g)).outputs[0]
        total = col if total is None else n.add("VectorMath", p_operation="ADD", Vector=total, Vector_001=col).outputs[0]
    # a band of faint dust across the sky
    tilt = n.add("VectorMath", p_operation="DOT_PRODUCT", Vector=gen, Vector_001=(0.25, -0.55, 0.8)).outputs["Value"]
    across = n.math("POWER", n.math("SUBTRACT", 1.0, n.math("ABSOLUTE", tilt), clamp=True), 7.0)
    dust = n.add("TexNoise", Vector=gen, Scale=6.0, Detail=9.0, Roughness=0.62)
    glow = n.math("MULTIPLY", across, n.math("MULTIPLY", n.math("POWER", dust.outputs["Fac"], 3.0), 0.09 * band))
    band_col = n.add("VectorMath", p_operation="SCALE", Vector=(0.55, 0.62, 0.8), Scale=glow).outputs[0]
    total = n.add("VectorMath", p_operation="ADD", Vector=total, Vector_001=band_col).outputs[0]
    # for the camera only: as light, a sky of stars would light the planet's night side
    is_cam = n.add("LightPath").outputs["Is Camera Ray"]
    bg = n.add("Background", Color=total, Strength=n.math("MULTIPLY", is_cam, strength))
    n.link(bg.outputs[0], out.inputs["Surface"])
    return w


def _sphere(radius, name, loc, segments=512, rings=256):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=radius)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    ob = bpy.data.objects.new(name, me)
    ob.location = loc
    bpy.context.collection.objects.link(ob)
    return ob


def surface_material(sun_dir, detail=1.0):
    """Oceans, brown and green land, cloud decks, all from noise. No lights at night: nobody lives here yet."""
    m, n, out = kit.new_material("MO_PlanetSurface")
    tc = n.add("TexCoord")
    p = tc.outputs["Object"]
    unit = n.add("VectorMath", p_operation="NORMALIZE", Vector=p).outputs[0]
    land_n = n.add("TexNoise", Vector=unit, Scale=2.3, Detail=12.0, Roughness=0.56, Lacunarity=2.1)
    land = n.ramp(land_n.outputs["Fac"], [(0.0, 0.0), (0.495, 0.0), (0.515, 1.0), (1.0, 1.0)])
    hue_n = n.add("TexNoise", Vector=unit, Scale=11.0, Detail=9.0, Roughness=0.6)
    soil = n.ramp(hue_n.outputs["Fac"], [(0.28, (0.040, 0.075, 0.022, 1)), (0.5, (0.085, 0.088, 0.032, 1)), (0.68, (0.22, 0.15, 0.08, 1))])
    deep = n.ramp(land_n.outputs["Fac"], [(0.28, (0.002, 0.012, 0.035, 1)), (0.5, (0.006, 0.040, 0.075, 1))])
    ground = n.mix(land, deep, soil)
    # clouds: bands of fine fractal noise, swept by a low-frequency warp
    warp = n.add("TexNoise", Vector=unit, Scale=3.0, Detail=4.0)
    warped = n.mix(0.22, unit, warp.outputs["Color"], blend="LINEAR_LIGHT")
    cn = n.add("TexNoise", Vector=warped, Scale=14.0 * detail, Detail=15.0, Roughness=0.62, Lacunarity=2.2)
    cn2 = n.add("TexNoise", Vector=warped, Scale=4.0 * detail, Detail=8.0, Roughness=0.55)
    cloud = n.ramp(n.math("ADD", n.math("MULTIPLY", cn.outputs["Fac"], 0.75), n.math("MULTIPLY", cn2.outputs["Fac"], 0.25)), [(0.0, 0.0), (0.46, 0.0), (0.60, 0.92), (1.0, 1.0)])
    albedo = n.mix(cloud, ground, (0.86, 0.87, 0.89, 1))
    rough = n.ramp(land, [(0.0, 0.32), (1.0, 0.85)])
    rough = n.mix(cloud, rough, (0.9, 0.9, 0.9, 1))
    bsdf = n.add("BsdfPrincipled", Base_Color=albedo, Roughness=rough)
    bsdf.inputs["Specular IOR Level"].default_value = 0.25
    b = n.add("Bump", Height=cn.outputs["Fac"], Strength=0.25, Distance=1.5)
    n.link(b.outputs[0], bsdf.inputs["Normal"])
    n.link(bsdf.outputs[0], out.inputs["Surface"])
    return m


def atmosphere_material(sun_dir, radius_ratio, gain=1.0, arc_dir=(0, 0, 1)):
    """A shell seen from outside as a thin atmosphere: warm right at the planet's edge, pale then deep blue going up, nothing at the top; strongest on the part of the limb the sun is behind
    (arc_dir: from the planet's centre towards that part), fading round the disc."""
    m, n, out = kit.new_material("MO_Atmosphere")
    m.blend_method = "BLEND"
    geo = n.add("NewGeometry")
    facing = n.add("VectorMath", p_operation="DOT_PRODUCT", Vector=geo.outputs["Normal"], Vector_001=geo.outputs["Incoming"]).outputs["Value"]
    edge = n.math("SUBTRACT", 1.0, n.math("ABSOLUTE", facing), clamp=True)
    limb = 1.0 - math.sqrt(max(1.0 - (1.0 / radius_ratio) ** 2, 0.0))      # the edge value where a ray just grazes the planet
    up = n.add("MapRange", Value=edge, From_Min=limb, From_Max=1.0, To_Min=0.0, To_Max=1.0, p_clamp=True).outputs[0]      # 0 at the planet's edge .. 1 at the top of the air
    colour = n.ramp(up, [(0.0, (1.0, 0.62, 0.36, 1)), (0.10, (0.92, 0.82, 0.86, 1)), (0.26, (0.48, 0.70, 1.0, 1)), (0.55, (0.18, 0.40, 1.0, 1)), (1.0, (0.05, 0.12, 0.42, 1))])
    above = n.add("SeparateColor", Color=n.ramp(up, [(0.0, 1.0), (0.14, 0.86), (0.34, 0.42), (0.62, 0.11), (1.0, 0.0)])).outputs[0]
    # over the disc itself: only a faint veil, rising towards the edge
    over_disc = n.math("POWER", n.math("DIVIDE", edge, max(limb, 1e-4), clamp=True), 7.0)
    on_disc = n.math("LESS_THAN", edge, limb)
    radial = n.math("ADD", n.math("MULTIPLY", on_disc, n.math("MULTIPLY", over_disc, 0.22)), n.math("MULTIPLY", n.math("SUBTRACT", 1.0, on_disc), above))
    arc = n.add("VectorMath", p_operation="DOT_PRODUCT", Vector=geo.outputs["Normal"], Vector_001=arc_dir).outputs["Value"]
    along = n.add("MapRange", Value=arc, From_Min=0.05, From_Max=0.995, To_Min=0.0, To_Max=1.0, p_clamp=True, p_interpolation_type="SMOOTHSTEP").outputs[0]
    strength = n.math("MULTIPLY", radial, n.math("MULTIPLY", n.math("ADD", 0.05, n.math("POWER", along, 1.3)), 7.5 * gain))
    em = n.add("Emission", Color=colour, Strength=strength)
    add = n.add("AddShader")
    n.link(em.outputs[0], add.inputs[0])
    n.link(n.add("BsdfTransparent").outputs[0], add.inputs[1])
    n.link(add.outputs[0], out.inputs["Surface"])
    return m


def planet(center, radius, sun_dir, detail=1.0, gain=1.0, segments=512, arc_dir=(0, 0, 1)):
    """The planet and its atmosphere shell. Returns (surface object, shell object)."""
    sun = tuple(Vector(sun_dir).normalized())
    body = _sphere(radius, "MO_Planet", center, segments, segments // 2)
    body.data.materials.append(surface_material(sun, detail))
    ratio = 1.013
    shell = _sphere(radius * ratio, "MO_Atmosphere", center, segments, segments // 2)
    shell.data.materials.append(atmosphere_material(sun, ratio, gain, arc_dir))
    return body, shell


def sun(direction, distance=40000.0, angular_deg=1.1, strength=4.5, disc=120.0):
    """A sun lamp for the light, and a bright disc to rise over the limb and flare. Keyframe disc.location to make it rise."""
    d = Vector(direction).normalized()
    light = bpy.data.lights.new("MO_Sun", "SUN")
    light.energy = strength
    light.angle = math.radians(0.6)
    light.color = (1.0, 0.96, 0.9)
    ob = bpy.data.objects.new("MO_Sun", light)
    bpy.context.collection.objects.link(ob)
    ob.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    radius = math.tan(math.radians(angular_deg * 0.5)) * distance
    disc_ob = _sphere(radius, "MO_SunDisc", d * distance, 48, 24)
    m, n, out = kit.new_material("MO_SunDisc")
    em = n.add("Emission", Color=(1.0, 0.95, 0.86, 1), Strength=disc)
    n.link(em.outputs[0], out.inputs["Surface"])
    disc_ob.data.materials.append(m)
    disc_ob.visible_shadow = False
    return ob, disc_ob


def camera(lens=35.0, loc=(0, 0, 0), target=None, clip=(0.05, 1e6), name="Cam"):
    cam = bpy.data.cameras.new(name)
    cam.lens = lens
    cam.sensor_width = 36.0
    cam.clip_start, cam.clip_end = clip
    ob = bpy.data.objects.new(name, cam)
    ob.location = loc
    bpy.context.collection.objects.link(ob)
    bpy.context.scene.camera = ob
    if target is not None:
        aim(ob, target)
    return ob


def aim(ob, target):
    d = Vector(target) - ob.location
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


def glow(threshold=1.0, strength=0.9, size=7, streaks=False, streak_strength=0.25):
    """The render's highlights bloom (Fog Glow), and with streaks=True an anamorphic line through the brightest of them. In the render, so stills and frames agree."""
    s = bpy.context.scene
    tree = bpy.data.node_groups.new("MO_Comp", "CompositorNodeTree")
    s.compositing_node_group = tree
    tree.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
    rl = tree.nodes.new("CompositorNodeRLayers")
    out = tree.nodes.new("NodeGroupOutput")
    g = tree.nodes.new("CompositorNodeGlare")
    g.inputs["Type"].default_value = "Bloom" if "Type" in g.inputs else None
    _set_glare(g, threshold, strength, size)
    tree.links.new(rl.outputs["Image"], g.inputs["Image"])
    last = g.outputs["Image"]
    if streaks:
        st = tree.nodes.new("CompositorNodeGlare")
        st.inputs["Type"].default_value = "Streaks"
        _set_glare(st, threshold * 3.0, streak_strength, size)
        for k, v in (("Streaks", 2), ("Streaks Angle", 0.0), ("Iterations", 4), ("Fade", 0.92)):
            if k in st.inputs:
                st.inputs[k].default_value = v
        tree.links.new(last, st.inputs["Image"])
        last = st.outputs["Image"]
    tree.links.new(last, out.inputs[0])
    return tree


def _set_glare(g, threshold, strength, size):
    for k, v in (("Threshold", threshold), ("Strength", strength), ("Size", size / 9.0), ("Saturation", 1.0), ("Quality", "High")):
        if k in g.inputs:
            g.inputs[k].default_value = v
