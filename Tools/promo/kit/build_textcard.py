"""MO57 promo kit: text over the picture, as RGBA frames (straight alpha, parchment and teal). Every line is set in real type in a real 3D scene, so it can drift and track.

    blender -b --factory-startup --python Tools/promo/kit/build_textcard.py -- '<json>'

The json: {"style": "fade" | "slam" | "lower" | "end", "text": "...", "frames_total": N, "out": "...", "scale": 1.0, ...}

  fade   a line centred, letters fading in from the middle out, the tracking slowly opening. Options: size (em against the picture's height, 0.13), tracking, kind, y (-0.5..0.5, 0), f_in, f_out (frames)
  slam   a line that lands hard on its first frame (a beat), a flash of heat that cools, a slow push, gone in 4 frames. Options as fade
  lower  a lower third: a teal rule draws out from the left, "text" over it in label type, "sub" under it in small text type
  end    the end card's lines, centred low: "lines": [{"text": ..., "size": ..., "kind": ..., "color": "parchment" | "teal" | "dim"}, ...], fading in one after another

Timing is data: frames_total, f_in, f_out. Tools/promo/render_cards.py builds these jobs from film.py.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh  # noqa: E402
import bpy  # noqa: E402

import kit_common as kit  # noqa: E402

cfg = kit.args()
STYLE = cfg.get("style", "fade")
FRAMES = int(cfg.get("frames_total", 72))
kit.reset()
kit.render_setup(cfg, FRAMES, engine="EEVEE", samples=32, transparent=True, view="Standard", look="None", blur=False, depth="8")
if STYLE == "lower":
    bpy.context.scene.render.film_transparent = True

ASPECT = 16.0 / 9.0
LENS = 50.0
D = LENS * ASPECT / 36.0     # at y = 0 the picture is exactly 1 unit high

cam_data = bpy.data.cameras.new("Cam")
cam_data.lens = LENS
cam_data.sensor_width = 36.0
cam = bpy.data.objects.new("Cam", cam_data)
bpy.context.collection.objects.link(cam)
bpy.context.scene.camera = cam
cam.location = (0, -D, 0)
cam.rotation_euler = (math.pi / 2, 0, 0)

COLOURS = {"parchment": kit.PARCHMENT, "teal": kit.TEAL_HI, "dim": (0xB8, 0xB2, 0xA4), "white": (0xFF, 0xFB, 0xF2)}


def letter_material(name, colour, heat=False):
    """Emission with its own alpha value node ("A"); with heat, a "Heat" node that pushes the colour white-hot for the slam."""
    m, n, out = kit.new_material(name)
    m.surface_render_method = "BLENDED"
    a = n.value(0.0, "A")
    base = kit.linear(colour)
    col = base
    if heat:
        h = n.value(0.0, "Heat")
        col = n.mix(h.outputs[0], base, (1.0, 0.86, 0.62, 1))
    em = n.add("Emission", Color=col, Strength=1.0)
    mix = n.add("MixShader", Fac=a.outputs[0])
    n.link(n.add("BsdfTransparent").outputs[0], mix.inputs[1])
    n.link(em.outputs[0], mix.inputs[2])
    n.link(mix.outputs[0], out.inputs["Surface"])
    return m


def line(text, kind, size, tracking, x, z, align, colour, heat=False, prefix="L"):
    """Letters as separate objects, each with its own material. Returns [(object, material, x offset from the line's centre)], width."""
    glyphs, width = kit.layout(text, kind, size, tracking)
    left = {"center": x - width * 0.5, "left": x, "right": x - width}[align]
    out = []
    for i, (ch, gx) in enumerate(glyphs):
        if ch == " ":
            continue
        ob = kit.glyph(ch, kind, size, "%s_%02d" % (prefix, i))
        ob.data.resolution_u = 12
        m = letter_material("%s_%02d" % (prefix, i), colour, heat)
        ob.data.materials.append(m)
        ob.location = (left + gx, 0.0, z)
        out.append((ob, m, gx - width * 0.5, left + gx))
    return out, width


def alpha_key(m, f, v):
    s = m.node_tree.nodes["A"].outputs[0]
    s.default_value = v
    s.keyframe_insert("default_value", frame=f)


def heat_key(m, f, v):
    s = m.node_tree.nodes["Heat"].outputs[0]
    s.default_value = v
    s.keyframe_insert("default_value", frame=f)


def fade():
    size = float(cfg.get("size", 0.13)) * 0.62      # 0.62: cap height for the em, so "size" reads as the caps' share of the picture
    tracking = float(cfg.get("tracking", 0.10))
    f_in, f_out = int(cfg.get("f_in", 16)), int(cfg.get("f_out", 14))
    z = float(cfg.get("y", 0.0)) - size * 0.306
    kind = cfg.get("kind", "display")
    letters, width = line(cfg["text"], kind, size, tracking, 0.0, z, "center", COLOURS[cfg.get("color", "parchment")])
    half = width * 0.5 or 1.0
    grow = 1.0 + float(cfg.get("open", 0.10))
    for ob, m, dx, abs_x in letters:
        delay = abs(dx) / half * f_in * 0.6
        alpha_key(m, 1 + delay, 0.0)
        alpha_key(m, 1 + delay + f_in, 1.0)
        alpha_key(m, FRAMES - f_out, 1.0)
        alpha_key(m, FRAMES, 0.0)
        # the tracking opens: every letter moves away from the centre, at a constant speed for the whole card
        ob.keyframe_insert("location", frame=1)
        ob.location.x = abs_x + (abs_x + (-0.0)) * (grow - 1.0)
        ob.keyframe_insert("location", frame=FRAMES)
        for fc in kit.fcurves(ob.animation_data.action):
            for k in fc.keyframe_points:
                k.interpolation = "LINEAR"


def slam():
    size = float(cfg.get("size", 0.15)) * 0.62
    tracking = float(cfg.get("tracking", 0.075))
    kind = cfg.get("kind", "display")
    z = float(cfg.get("y", 0.0)) - size * 0.306
    letters, width = line(cfg["text"], kind, size, tracking, 0.0, z, "center", COLOURS["parchment"], heat=True)
    f_out = int(cfg.get("f_out", 4))
    for ob, m, dx, abs_x in letters:
        alpha_key(m, 1, 1.0)
        alpha_key(m, FRAMES - f_out, 1.0)
        alpha_key(m, FRAMES, 0.0)
        heat_key(m, 1, 1.0)
        heat_key(m, 10, 0.0)
    # lands a little large and settles in 3 frames, then a slow push for the rest
    cam.location = (0, -D * 0.93, 0)
    cam.keyframe_insert("location", frame=1)
    cam.location = (0, -D, 0)
    cam.keyframe_insert("location", frame=4)
    cam.location = (0, -D * 0.965, 0)
    cam.keyframe_insert("location", frame=FRAMES)


def lower():
    """A lower third at the picture's left: the rule draws out, then the words come up."""
    text, sub = cfg["text"], cfg.get("sub", "")
    margin_x = -ASPECT * 0.5 + float(cfg.get("margin", 0.085))
    base_z = -0.5 + float(cfg.get("margin_y", 0.105))
    f_in, f_out = int(cfg.get("f_in", 14)), int(cfg.get("f_out", 12))
    size = float(cfg.get("size", 0.042))
    sub_size = float(cfg.get("sub_size", 0.0195))
    head, w_head = line(text.upper(), "label", size, 0.13, margin_x, base_z + sub_size * 1.75, "left", COLOURS["parchment"], prefix="H")
    subs, w_sub = line(sub, "label", sub_size, 0.09, margin_x, base_z, "left", COLOURS["dim"], prefix="S") if sub else ([], 0.0)
    # the rule, under the head line
    rule_m = letter_material("Rule", kit.TEAL_HI)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0.5, 0, 0))
    me = bpy.data.meshes.new("rule")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(rule_m)
    rule = bpy.data.objects.new("rule", me)
    bpy.context.collection.objects.link(rule)
    rule.location = (margin_x, 0.0, base_z + sub_size * 1.42)
    h = 0.00165
    w_full = max(w_head, w_sub) + 0.03
    rule.scale = (0.0001, 0.001, h)
    rule.keyframe_insert("scale", frame=1)
    rule.scale = (w_full, 0.001, h)
    rule.keyframe_insert("scale", frame=1 + f_in)
    rule.scale = (w_full, 0.001, h)
    rule.keyframe_insert("scale", frame=FRAMES - f_out)
    rule.scale = (0.0001, 0.001, h)
    rule.keyframe_insert("scale", frame=FRAMES)
    alpha_key(rule_m, 1, 1.0)
    alpha_key(rule_m, FRAMES, 1.0)
    span = max(w_full, 0.01)
    for group, lag in ((head, 4), (subs, 9)):
        for ob, m, dx, abs_x in group:
            d = lag + (abs_x - margin_x) / span * f_in * 0.7
            alpha_key(m, 1 + d, 0.0)
            alpha_key(m, 1 + d + 8, 1.0)
            alpha_key(m, FRAMES - f_out, 1.0)
            alpha_key(m, FRAMES - f_out + 8, 0.0)


def end():
    """Lines centred low in the picture, fading in one after another."""
    z = float(cfg.get("y", -0.24))
    gap = float(cfg.get("gap", 1.75))
    stagger = int(cfg.get("stagger", 20))
    for i, spec in enumerate(cfg["lines"]):
        size = float(spec.get("size", 0.03))
        letters, width = line(spec["text"], spec.get("kind", "label"), size, float(spec.get("tracking", 0.12)), 0.0, z, "center", COLOURS[spec.get("color", "parchment")], prefix="E%d" % i)
        z -= size * gap
        start = 1 + i * stagger
        for ob, m, dx, abs_x in letters:
            alpha_key(m, start, 0.0)
            alpha_key(m, start + 16, 1.0)
            alpha_key(m, FRAMES - 16, 1.0)
            alpha_key(m, FRAMES, 0.0 if cfg.get("fade_out", True) else 1.0)


{"fade": fade, "slam": slam, "lower": lower, "end": end}[STYLE]()
kit.finish(cfg, cfg.get("id", STYLE))
