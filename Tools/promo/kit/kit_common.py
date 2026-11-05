"""MO57 promo kit: what every Blender build shares -- render setup, palette, node helpers, the kit's type, the granite.

Runs inside Blender 5.2 (headless). Every build script (build_*.py) starts with:

    import os, sys; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import kit_common as kit

and takes its settings as JSON after "--" (see kit.args()), so a shot re-renders with new text or timing without touching the script:

    blender -b --factory-startup --python Tools/promo/kit/build_textcard.py -- '{"id": "x", "lines": [...], "out": "D:/out/x", "scale": 0.5}'

Common settings: out (folder; frames are <out>/f_####.png), scale (of 3840x2160), samples, frames (first, last: render only these),
blend (save the built scene there instead of, or as well as, rendering), still (one frame number, written as <out>/still_####.png).
Docs/Promo_Video.md section 5 is the manual.
"""
import json
import math
import os
import random
import sys

import bpy
from mathutils import Vector

UHD = (3840, 2160)
FPS = 24

# The UI kit's Field Ledger palette (SteamArt/README.md, Tools/steam_wordmark.py): sRGB bytes
PARCHMENT = (0xEE, 0xE7, 0xD7)
TEAL = (0x6C, 0xB8, 0xA9)
TEAL_HI = (0x88, 0xCB, 0xBE)
INK = (0x0B, 0x0E, 0x0F)
EMBER = (0xFF, 0x9A, 0x4A)

FONT_DIR_ENV = "MO57_PROMO_FONTS"   # the static instances from make_fonts.py (fonts are not tracked: they are Microsoft's)


def linear(c):
    """sRGB bytes -> linear 0..1."""
    out = []
    for v in c[:3]:
        v = v / 255.0
        out.append(v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4)
    return (out[0], out[1], out[2], 1.0)


def args():
    """The JSON after "--": inline, or a path to a .json file."""
    if "--" not in sys.argv:
        return {}
    raw = " ".join(sys.argv[sys.argv.index("--") + 1:]).strip()
    if not raw:
        return {}
    if raw.endswith(".json") and os.path.isfile(raw):
        with open(raw, encoding="utf-8") as f:
            return json.load(f)
    return json.loads(raw)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if "cycles" not in bpy.context.preferences.addons:
        bpy.ops.preferences.addon_enable(module="cycles")
    return bpy.context.scene


def gpu():
    """Cycles on the GPU (OptiX): DARKTOWER's 3090 or this PC's 4090. Returns the devices used (none = CPU)."""
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("OPTIX", "CUDA"):
        try:
            prefs.compute_device_type = kind
            prefs.get_devices()
            used = []
            for d in prefs.devices:
                d.use = d.type == kind
                if d.use:
                    used.append(d.name)
            if used:
                bpy.context.scene.cycles.device = "GPU"
                return used
        except TypeError:
            continue
    bpy.context.scene.cycles.device = "CPU"
    return []


def render_setup(cfg, frames, engine="CYCLES", samples=128, transparent=False, view="AgX", look="AgX - Medium High Contrast", exposure=0.0,
                 blur=True, shutter=0.5, depth="16", bounces=None):
    """The scene's render settings from a build's config. Returns the scene."""
    s = bpy.context.scene
    s.render.engine = "CYCLES" if engine == "CYCLES" else "BLENDER_EEVEE"
    scale = float(cfg.get("scale", 1.0))
    s.render.resolution_x = int(UHD[0] * scale + 0.5) // 2 * 2
    s.render.resolution_y = int(UHD[1] * scale + 0.5) // 2 * 2
    s.render.resolution_percentage = 100
    s.render.fps = FPS
    s.frame_start, s.frame_end = 1, frames
    s.render.film_transparent = transparent
    s.render.use_persistent_data = True      # keep the BVH between frames: a heavy static scene renders its later frames far faster
    s.render.use_motion_blur = blur
    s.render.motion_blur_shutter = shutter
    s.view_settings.view_transform = view
    if look:
        s.view_settings.look = look
    s.view_settings.exposure = exposure
    s.render.image_settings.file_format = "PNG"
    s.render.image_settings.color_mode = "RGBA" if transparent else "RGB"
    s.render.image_settings.color_depth = depth
    s.render.image_settings.compression = 15
    if engine == "CYCLES":
        c = s.cycles
        c.samples = int(cfg.get("samples", samples))
        c.use_adaptive_sampling = True
        c.adaptive_threshold = 0.01
        c.use_denoising = True
        c.denoiser = "OPENIMAGEDENOISE"
        c.denoising_input_passes = "RGB_ALBEDO_NORMAL"
        c.denoising_prefilter = "ACCURATE"
        c.seed = 57
        c.use_animated_seed = True
        if bounces:
            c.max_bounces, c.diffuse_bounces, c.glossy_bounces, c.transmission_bounces, c.volume_bounces = bounces
            c.transparent_max_bounces = max(bounces[0], 24)
        c.caustics_reflective = c.caustics_refractive = False
        c.sample_clamp_indirect = 8.0
        print("KIT gpu", gpu(), flush=True)
    else:
        s.eevee.taa_render_samples = int(cfg.get("samples", samples))
    return s


def finish(cfg, name=""):
    """Save the .blend if asked, then render the frames (or the one still) into cfg["out"]."""
    s = bpy.context.scene
    bpy.context.view_layer.update()
    if cfg.get("blend"):
        os.makedirs(os.path.dirname(cfg["blend"]) or ".", exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=cfg["blend"], compress=True)
        print("KIT saved", cfg["blend"], flush=True)
    out = cfg.get("out")
    if not out or cfg.get("no_render"):
        return
    os.makedirs(out, exist_ok=True)
    still = cfg.get("still")
    if still is not None:
        stills = still if isinstance(still, list) else [still]
        for f in stills:
            s.frame_set(int(f))
            s.render.filepath = os.path.join(out, "still_%04d.png" % int(f))
            bpy.ops.render.render(write_still=True)
            print("KIT still", s.render.filepath, flush=True)
        return
    first, last = cfg.get("frames", [s.frame_start, s.frame_end])
    s.render.filepath = os.path.join(out, "f_")
    s.render.use_overwrite = bool(cfg.get("overwrite", False))   # a restarted job carries on where it stopped
    s.render.use_placeholder = False
    s.frame_start, s.frame_end = int(first), int(last)
    bpy.ops.render.render(animation=True)
    print("KIT rendered", name, first, last, flush=True)


# ---------- easing ----------

def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def key(obj, path, frame, value, index=-1, interp="BEZIER"):
    """Keyframe obj.<path> = value at frame. The fcurves are reached through keyframe_insert's return, so 5.x's layered actions need no handling."""
    target = obj
    prop = path
    if "." in path:
        head, prop = path.rsplit(".", 1)
        target = obj.path_resolve(head)
    if isinstance(value, (tuple, list, Vector)):
        setattr(target, prop, value)
    else:
        if index >= 0:
            getattr(target, prop)[index] = value
        else:
            setattr(target, prop, value)
    target.keyframe_insert(data_path=prop, frame=frame, index=index)


def set_interp(idblock, interp="LINEAR"):
    """Every keyframe on this object / node tree: linear (a constant speed) or bezier."""
    ad = idblock.animation_data
    if not ad or not ad.action:
        return
    for fc in fcurves(ad.action):
        for k in fc.keyframe_points:
            k.interpolation = interp


def fcurves(action):
    """An action's fcurves, on 5.x's layered actions or the old flat list."""
    if hasattr(action, "layers") and len(action.layers):
        out = []
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    out.extend(bag.fcurves)
        return out
    return list(action.fcurves)


def wobble(obj, path, index, scale, strength, offset=None):
    """A slow noise on one channel (a camera's drift, a held shake): an fcurve noise modifier."""
    if not obj.animation_data or not obj.animation_data.action:
        obj.keyframe_insert(data_path=path, frame=1, index=index)
    elif not [fc for fc in fcurves(obj.animation_data.action) if fc.data_path == path and fc.array_index == index]:
        obj.keyframe_insert(data_path=path, frame=1, index=index)
    for fc in fcurves(obj.animation_data.action):
        if fc.data_path == path and fc.array_index == index:
            m = fc.modifiers.new("NOISE")
            m.scale, m.strength = scale, strength
            m.phase = offset if offset is not None else random.uniform(0, 500)
            m.blend_type = "ADD"
            return m
    return None


# ---------- node helpers ----------

class Nodes:
    """A shader graph written as calls: n = Nodes(mat); noise = n.add("TexNoise", Scale=4); n.link(noise.outputs[0], other.inputs[0])."""

    def __init__(self, tree):
        self.tree = tree

    def add(self, kind, **inputs):
        name = kind if kind.startswith(("ShaderNode", "NodeGroup")) else "ShaderNode" + kind
        node = self.tree.nodes.new(name)
        for k, v in inputs.items():       # properties first: they decide which sockets a node has
            if k.startswith("p_"):        # a node property: p_operation="MULTIPLY"
                setattr(node, k[2:], v)
        for k, v in inputs.items():
            if k.startswith("p_"):
                continue
            sock = self._socket(node, k)
            if isinstance(v, bpy.types.NodeSocket):
                self.tree.links.new(v, sock)
            else:
                sock.default_value = v
        return node

    @staticmethod
    def _socket(node, key):
        """An input by name (underscores for spaces); Vector_001 = the second socket called Vector."""
        if key.endswith("_001"):
            name = key[:-4].replace("_", " ")
            return [s for s in node.inputs if s.name == name][1]
        return node.inputs[key.replace("_", " ")]

    def link(self, a, b):
        self.tree.links.new(a, b)

    def math(self, op, a, b=None, clamp=False, c=None):
        node = self.tree.nodes.new("ShaderNodeMath")
        node.operation = op
        node.use_clamp = clamp
        for i, v in enumerate((a, b, c)):
            if v is None:
                continue
            if isinstance(v, bpy.types.NodeSocket):
                self.tree.links.new(v, node.inputs[i])
            else:
                node.inputs[i].default_value = v
        return node.outputs[0]

    def mix(self, fac, a, b, blend="MIX", clamp=False):
        """Colour mix."""
        node = self.tree.nodes.new("ShaderNodeMix")
        node.data_type = "RGBA"
        node.blend_type = blend
        node.clamp_result = clamp
        if isinstance(fac, bpy.types.NodeSocket):
            self.tree.links.new(fac, node.inputs[0])
        else:
            node.inputs[0].default_value = fac
        for sock, v in ((node.inputs[6], a), (node.inputs[7], b)):
            if isinstance(v, bpy.types.NodeSocket):
                self.tree.links.new(v, sock)
            else:
                sock.default_value = v if isinstance(v, (tuple, list)) else (v, v, v, 1)
        return node.outputs[2]

    def ramp(self, fac, stops, interp="LINEAR"):
        """A colour ramp from [(pos, (r,g,b,a)) ...] or [(pos, grey) ...]. Returns the colour output."""
        node = self.tree.nodes.new("ShaderNodeValToRGB")
        cr = node.color_ramp
        cr.interpolation = interp
        while len(cr.elements) > 1:
            cr.elements.remove(cr.elements[-1])
        for i, (pos, col) in enumerate(stops):
            e = cr.elements[0] if i == 0 else cr.elements.new(pos)
            e.position = pos
            e.color = col if isinstance(col, (tuple, list)) else (col, col, col, 1)
        if isinstance(fac, bpy.types.NodeSocket):
            self.tree.links.new(fac, node.inputs[0])
        else:
            node.inputs[0].default_value = fac
        return node.outputs[0]

    def value(self, v, label=""):
        node = self.tree.nodes.new("ShaderNodeValue")
        node.outputs[0].default_value = v
        node.label = label
        node.name = label or node.name
        return node


def new_material(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    t = m.node_tree
    for n in list(t.nodes):
        t.nodes.remove(n)
    out = t.nodes.new("ShaderNodeOutputMaterial")
    return m, Nodes(t), out


# ---------- the kit's stone (the clone pod's granite: Art/References/ImageTo3D/2026-10-18-clone-egg) ----------

def granite(name="MO_Granite", coord_scale=1.0, tint=(1, 1, 1), heat_axis=None, bump=1.0):
    """Pale granite: tan ground, black mica flecks, iron-orange stains, a fine crystal grain. coord_scale: flecks per unit (scene units).
    heat_axis: (lo, hi) along local X: from hi down to lo the stone chars black and, by the material's "Heat" value node, glows.
    Returns the material; its "Heat" value node (0..1) is keyed by the shot that wants a glow."""
    m, n, out = new_material(name)
    tc = n.add("TexCoord")
    obj = tc.outputs["Object"]
    mp = n.add("Mapping", Vector=obj)
    mp.inputs["Scale"].default_value = (coord_scale, coord_scale, coord_scale)
    p = mp.outputs[0]
    warp = n.add("TexNoise", Vector=p, Scale=3.0, Detail=4.0, Roughness=0.55)
    warped = n.mix(0.06, p, warp.outputs["Color"], blend="LINEAR_LIGHT")
    # crystals: a voronoi cell per grain, its own random colour picks ground, feldspar or mica
    cells = n.add("TexVoronoi", Vector=warped, Scale=95.0, Randomness=1.0)
    grain_v = n.add("SeparateColor", Color=cells.outputs["Color"]).outputs[0]
    ground = n.ramp(grain_v, [
        (0.00, (0.012, 0.011, 0.010, 1)),   # black mica
        (0.16, (0.012, 0.011, 0.010, 1)),
        (0.19, (0.20, 0.155, 0.115, 1)),
        (0.45, (0.42, 0.34, 0.26, 1)),      # tan feldspar
        (0.75, (0.56, 0.49, 0.40, 1)),
        (1.00, (0.72, 0.67, 0.58, 1)),      # pale quartz
    ])
    # large clumps of dark mica, like the reference's blotches
    clump = n.add("TexNoise", Vector=warped, Scale=9.0, Detail=6.0, Roughness=0.6)
    clump_mask = n.ramp(clump.outputs["Fac"], [(0.0, 0.0), (0.56, 0.0), (0.66, 1.0), (1.0, 1.0)])
    dark = n.mix(n.math("MULTIPLY", clump_mask, 0.85), ground, (0.02, 0.018, 0.016, 1))
    # iron stains
    stain = n.add("TexNoise", Vector=warped, Scale=5.0, Detail=8.0, Roughness=0.62)
    stain_mask = n.ramp(stain.outputs["Fac"], [(0.0, 0.0), (0.52, 0.0), (0.68, 0.75), (1.0, 0.9)])
    colour = n.mix(stain_mask, dark, (0.36, 0.16, 0.05, 1), blend="MULTIPLY")
    colour = n.mix(n.math("MULTIPLY", stain_mask, 0.35), colour, (0.42, 0.20, 0.07, 1))
    tinted = n.mix(1.0, colour, (tint[0], tint[1], tint[2], 1), blend="MULTIPLY")
    rough = n.ramp(grain_v, [(0.0, 0.38), (0.2, 0.62), (1.0, 0.78)])
    # surface: crystal facets over pits over broad erosion
    h_cells = n.math("MULTIPLY", cells.outputs["Distance"], 0.5)
    pits = n.add("TexNoise", Vector=warped, Scale=38.0, Detail=5.0, Roughness=0.65)
    broad = n.add("TexNoise", Vector=warped, Scale=7.0, Detail=6.0, Roughness=0.6)
    height = n.math("ADD", n.math("MULTIPLY", h_cells, 0.35), n.math("ADD", n.math("MULTIPLY", pits.outputs["Fac"], 0.45), n.math("MULTIPLY", broad.outputs["Fac"], 0.9)))
    bsdf = n.add("BsdfPrincipled", Base_Color=tinted, Roughness=rough)
    emission = None
    if heat_axis:
        x = n.add("SeparateXYZ", Vector=obj).outputs[0]
        lo, hi = heat_axis
        along = n.add("MapRange", Value=x, From_Min=lo, From_Max=hi, To_Min=1.0, To_Max=0.0, p_clamp=True).outputs[0]
        edge = n.add("TexNoise", Vector=warped, Scale=4.0, Detail=7.0, Roughness=0.66)
        char = n.ramp(n.math("ADD", along, n.math("MULTIPLY", n.math("SUBTRACT", edge.outputs["Fac"], 0.5), 0.9)), [(0.0, 0.0), (0.42, 0.0), (0.72, 1.0), (1.0, 1.0)])
        burnt = n.ramp(pits.outputs["Fac"], [(0.2, (0.006, 0.005, 0.005, 1)), (0.55, (0.03, 0.022, 0.018, 1)), (0.8, (0.11, 0.055, 0.02, 1))])
        tinted2 = n.mix(char, tinted, burnt)
        n.link(tinted2, bsdf.inputs["Base Color"])
        rough2 = n.add("Math", p_operation="MAXIMUM", Value=0.5)
        n.link(rough, rough2.inputs[0])
        rough2.inputs[1].default_value = 0.6
        n.link(rough2.outputs[0], bsdf.inputs["Roughness"])
        cracks = n.add("TexVoronoi", Vector=warped, Scale=11.0, p_feature="DISTANCE_TO_EDGE", Randomness=0.9).outputs["Distance"]
        line = n.add("MapRange", Value=cracks, From_Min=0.0, From_Max=0.045, To_Min=1.0, To_Max=0.0, p_clamp=True).outputs[0]
        ember = n.add("MapRange", Value=pits.outputs["Fac"], From_Min=0.42, From_Max=0.62, To_Min=0.0, To_Max=0.22, p_clamp=True).outputs[0]
        heat_node = n.value(0.0, "Heat")
        temperature = n.math("ADD", n.math("MULTIPLY", along, 1700.0), 1000.0)
        black = n.add("Blackbody", Temperature=temperature)
        glow = n.math("MULTIPLY", n.math("POWER", along, 2.2), n.math("MULTIPLY", heat_node.outputs[0], n.math("ADD", ember, line)))
        strength = n.math("MULTIPLY", glow, 9.0)
        emission = n.add("Emission", Color=black.outputs[0], Strength=strength)
    b = n.add("Bump", Height=height, Strength=0.55 * bump, Distance=0.02 / max(coord_scale, 1e-3))
    n.link(b.outputs[0], bsdf.inputs["Normal"])
    shader = bsdf.outputs[0]
    if emission:
        shader = n.add("AddShader").outputs[0]
        add = shader.node
        n.link(bsdf.outputs[0], add.inputs[0])
        n.link(emission.outputs[0], add.inputs[1])
    n.link(shader, out.inputs["Surface"])
    return m


# ---------- type ----------

KERN = {   # optical pairs for the display caps, in em (the text object does not read GPOS)
    "AV": -0.085, "VA": -0.085, "TA": -0.065, "AT": -0.065, "LT": -0.05, "LV": -0.07, "RV": -0.03, "RT": -0.02, "AY": -0.06, "YA": -0.06, "PA": -0.05,
    "FA": -0.04, "AW": -0.05, "WA": -0.05, "TO": -0.02, "OT": -0.02, "VE": -0.02, "EV": -0.015, "RS": -0.005, "AL": 0.0, ".T": -0.03, "T.": -0.04, "Y.": -0.05,
    "V.": -0.05, "L.": -0.01, "R.": -0.01, "LL": 0.0, "AC": -0.015, "CA": -0.015,
}


def font_dir():
    d = os.environ.get(FONT_DIR_ENV) or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "Saved", "Promo", "fonts")
    return os.path.abspath(d)


def font(kind="display"):
    """display: Sitka Banner Bold (the wordmark's face); text: Sitka Subheading Semibold; label: Bahnschrift SemiCondensed. make_fonts.py makes the three."""
    path = os.path.join(font_dir(), {"display": "MOPromoDisplay.ttf", "text": "MOPromoText.ttf", "label": "MOPromoLabel.ttf"}[kind])
    if not os.path.isfile(path):
        raise FileNotFoundError(path + " (run Tools/promo/kit/make_fonts.py)")
    return bpy.data.fonts.load(path, check_existing=True)


def metrics(kind="display"):
    with open(os.path.join(font_dir(), "metrics.json"), encoding="utf-8") as f:
        return json.load(f)[kind]


def layout(text, kind, size, tracking):
    """Glyph origins for a line laid out left to right from 0: [(char, x)], width. size = em in scene units, tracking in em."""
    m = metrics(kind)
    x, out = 0.0, []
    for i, ch in enumerate(text):
        out.append((ch, x))
        adv = m["advance"].get(ch, m["advance"].get("n", 0.5))
        kern = KERN.get(text[i:i + 2], 0.0) if kind == "display" else 0.0
        x += (adv + kern + tracking) * size
    last = text[-1]
    width = x - tracking * size
    return out, width


def glyph(ch, kind, size, name=None):
    """One character as a text object at the origin, its baseline on Z = 0 (flat in XZ when turned to face -Y)."""
    cu = bpy.data.curves.new(name or ("t_" + ch), "FONT")
    cu.body = ch
    cu.font = font(kind)
    cu.size = size
    cu.align_x = "LEFT"
    cu.align_y = "BOTTOM_BASELINE"
    ob = bpy.data.objects.new(cu.name, cu)
    bpy.context.collection.objects.link(ob)
    ob.rotation_euler = (math.pi / 2, 0, 0)
    return ob
