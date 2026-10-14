"""Icon studio, step 2 of 3 (Blender, headless): render each exported item to a transparent square icon.

    blender -b --factory-startup --python Tools/icon_studio_blender.py -- <root> [Item1,Item2,...] [size]
    (Tools/icon_studio.py runs this; <root> holds one folder per item from icon_studio_export.py)

Why Blender and not the editor's thumbnail renderer: the old icons came from Content Browser thumbnails (fixed low side view,
checker burnt in, removed afterwards by icon_fix_process.py), so long items (sticks, bark, cordage) filled a sliver of the
square. Here every item gets the same studio:
  - Cycles (GPU), transparent film, sun-light key/fill/rim plus a soft gray world, so the look does not depend on item size;
  - a pole-like item modelled standing up (a sword or a bark strip authored along Z) laid down first, so it is not a vertical
    sliver: when the 3D principal axis is near-vertical and POLE_RATIO times the next one. It lies with its thinnest axis up, so
    a blade shows its flat, not its edge;
  - the item turned so its long axis runs up the diagonal (PCA of the vertices in plan view), seen from above-front: steeper for
    flat items, lower for tall ones (a jar keeps its profile);
  - the item row's non-uniform world scale applied (meta.json "scale"), so a squashed loaf renders as the flatbread it is;
  - an orthographic camera fitted to the item's projected silhouette with a fixed margin, so every icon fills the frame alike.
Materials are rebuilt from the exported textures (base colour, normal, Megascans ORM / glTF metallic-roughness, foliage mask).
Ambient occlusion maps are not used: Cycles shades occlusion itself, and glTF packs nothing in the red channel.
Writes <root>/<Item>/icon.png. Output lines start with ICONSTUDIO.
"""
import itertools
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

MARGIN = 1.10          # silhouette fills ~91% of the frame's longer side
POLE_RATIO = 5.0       # sqrt(eigenvalue ratio) above which a near-vertical item is laid down: sword 11.5 and bark strip 7.3
                       # lie down, the slender wheel-thrown vase (3.9) must stand
SAMPLES = 128


def log(msg):
    print("ICONSTUDIO " + msg, flush=True)


def reset_scene(size):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for device_type in ("OPTIX", "CUDA"):
            try:
                prefs.compute_device_type = device_type
                break
            except TypeError:
                continue
        prefs.get_devices()
        for device in prefs.devices:
            device.use = device.type != "CPU"
        scene.cycles.device = "GPU"
    except Exception as exc:   # CPU fallback still renders, just slower
        log("GPU setup failed (%s); rendering on CPU" % exc)
    scene.cycles.samples = SAMPLES
    scene.cycles.use_denoising = True
    scene.render.film_transparent = True
    scene.render.resolution_x = scene.render.resolution_y = size
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "AgX"
    world = bpy.data.worlds.new("IconWorld")
    scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes["Background"]
    background.inputs[0].default_value = (0.62, 0.64, 0.66, 1.0)
    background.inputs[1].default_value = 0.45
    return scene


def adjust_color(nodes, links, color, adjust):
    """The material's colour controls (meta slots[i].adjust: saturation, contrast, brightness, tint), applied to the base colour
    in that order. An approximation of the FabOverrides master (contrast as a power curve); identity when adjust is empty."""
    if "saturation" in adjust:
        hsv = nodes.new("ShaderNodeHueSaturation")
        hsv.inputs["Saturation"].default_value = adjust["saturation"]
        links.new(color, hsv.inputs["Color"])
        color = hsv.outputs["Color"]
    if "contrast" in adjust:
        gamma = nodes.new("ShaderNodeGamma")
        gamma.inputs["Gamma"].default_value = adjust["contrast"]
        links.new(color, gamma.inputs["Color"])
        color = gamma.outputs["Color"]
    gain = [adjust.get("brightness", 1.0) * c for c in adjust.get("tint", [1.0, 1.0, 1.0])]
    if any(abs(g - 1.0) > 1e-4 for g in gain):
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        rgba = lambda sockets, name: next(s for s in sockets if s.name == name and s.type == "RGBA")   # indices move per version
        links.new(color, rgba(mix.inputs, "A"))
        rgba(mix.inputs, "B").default_value = (gain[0], gain[1], gain[2], 1.0)
        color = rgba(mix.outputs, "Result")
    return color


def build_material(item_dir, info):
    material = bpy.data.materials.new(info.get("name") or "IconMat")
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bsdf = nodes["Principled BSDF"]

    def image(key, non_color):
        node = nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(os.path.join(item_dir, info[key]))
        if non_color:
            node.image.colorspace_settings.name = "Non-Color"
        return node

    if "base" in info:
        base = image("base", False)
        links.new(adjust_color(nodes, links, base.outputs["Color"], info.get("adjust", {})), bsdf.inputs["Base Color"])
        if info["kind"] == "foliage" and "mask" not in info:
            links.new(base.outputs["Alpha"], bsdf.inputs["Alpha"])
    if "orm" in info:
        split = nodes.new("ShaderNodeSeparateColor")
        links.new(image("orm", True).outputs["Color"], split.inputs["Color"])
        links.new(split.outputs["Green"], bsdf.inputs["Roughness"])
        links.new(split.outputs["Blue"], bsdf.inputs["Metallic"])
    if "mask" in info:
        split = nodes.new("ShaderNodeSeparateColor")
        links.new(image("mask", True).outputs["Color"], split.inputs["Color"])
        links.new(split.outputs["Red"], bsdf.inputs["Alpha"])
        links.new(split.outputs["Green"], bsdf.inputs["Roughness"])
    if "normal" in info:
        color = image("normal", True).outputs["Color"]
        if not info.get("normal_opengl", False):   # DirectX (Megascans): flip green for Blender's OpenGL convention
            split = nodes.new("ShaderNodeSeparateColor")
            invert = nodes.new("ShaderNodeMath")
            invert.operation = "SUBTRACT"
            invert.inputs[0].default_value = 1.0
            merge = nodes.new("ShaderNodeCombineColor")
            links.new(color, split.inputs["Color"])
            links.new(split.outputs["Green"], invert.inputs[1])
            links.new(split.outputs["Red"], merge.inputs["Red"])
            links.new(invert.outputs["Value"], merge.inputs["Green"])
            links.new(split.outputs["Blue"], merge.inputs["Blue"])
            color = merge.outputs["Color"]
        normal_map = nodes.new("ShaderNodeNormalMap")
        links.new(color, normal_map.inputs["Color"])
        links.new(normal_map.outputs["Normal"], bsdf.inputs["Normal"])
    return material


def apply_row_scale(obj, meta):
    """The item row's world scale (meta "scale", UE axes). Which Blender axis is which UE axis after the FBX round trip is read
    off the geometry: the permutation whose per-axis size ratios to the UE extent agree best (the unit factor is common)."""
    scale, extent = meta.get("scale"), meta.get("extent")
    if not scale or not extent or max(scale) - min(scale) < 1e-4:
        return   # uniform: the orthographic fit makes it irrelevant
    dims = list(obj.dimensions)
    ratios = lambda p: [math.log(max(dims[i], 1e-6) / max(extent[p[i]], 1e-6)) for i in range(3)]
    best = min(itertools.permutations(range(3)), key=lambda p: float(np.var(ratios(p))))
    obj.scale = tuple(scale[best[i]] for i in range(3))
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)


def import_item(item_dir, meta):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(item_dir, "mesh.fbx"))
    meshes = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    for other in [o for o in bpy.data.objects if o not in before and o.type != "MESH"]:
        bpy.data.objects.remove(other, do_unlink=True)
    if not meshes:
        return None
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    apply_row_scale(obj, meta)
    for info in meta["slots"]:
        if info["slot"] < len(obj.material_slots):
            obj.material_slots[info["slot"]].material = build_material(item_dir, info)
    return obj


def vertices(obj, limit=60000):
    co = np.empty(len(obj.data.vertices) * 3, dtype=np.float64)
    obj.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    if len(co) > limit:
        co = co[:: len(co) // limit + 1]
    return co


def pose_and_frame(scene, obj):
    pts = vertices(obj)
    center = pts.mean(0)
    local = pts - center
    lay = Matrix.Identity(3)
    evals3, evecs3 = np.linalg.eigh(local.T @ local)
    order = np.argsort(evals3)[::-1]
    major3 = Vector(evecs3[:, order[0]])
    slender = math.sqrt(evals3[order[0]] / max(evals3[order[1]], 1e-12))
    pole = slender > POLE_RATIO and abs(major3.z) > 0.7
    if pole:   # lay it down: long axis to the horizontal direction it leans toward, thinnest axis up (a blade shows its flat)
        lean = Vector((major3.x, major3.y, 0.0))
        lean = lean.normalized() if lean.length > 1e-3 else Vector((1.0, 0.0, 0.0))
        up = Vector((0.0, 0.0, 1.0))
        source = np.column_stack([evecs3[:, order[0]], evecs3[:, order[1]], evecs3[:, order[2]]])
        if np.linalg.det(source) < 0:
            source[:, 1] *= -1.0
        target = np.column_stack([np.array(lean), np.array(up.cross(lean)), np.array(up)])
        lay = Matrix((target @ source.T).tolist())
        local = local @ np.array(lay).T
    xy = local[:, :2]
    evals, evecs = np.linalg.eigh(xy.T @ xy)
    major = evecs[:, int(np.argmax(evals))]
    extent = local.max(0) - local.min(0)
    plan = max(extent[0], extent[1], 1e-6)
    elongated = math.sqrt(max(evals.max(), 1e-12) / max(evals.min(), 1e-12)) > 1.8
    flat = extent[2] < 0.6 * plan
    # long axis up the diagonal: in plan it lands at +38 deg from screen-horizontal (world X)
    turn = (math.radians(38) - math.atan2(major[1], major[0])) if elongated else math.radians(-20)
    obj.matrix_world = Matrix.Rotation(turn, 4, "Z") @ lay.to_4x4() @ Matrix.Translation(Vector((-center[0], -center[1], -center[2])))
    elevation = math.radians(52 if flat else 24)

    camera_data = bpy.data.cameras.new("IconCam")
    camera_data.type = "ORTHO"
    camera = bpy.data.objects.new("IconCam", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    distance = 10.0 * float(np.linalg.norm(extent)) + 1.0
    direction = Vector((0.0, -math.cos(elevation), math.sin(elevation)))
    camera.location = direction * distance
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    camera_data.clip_end = distance * 3.0

    # fit the ortho frame to the posed silhouette
    bpy.context.view_layer.update()
    posed = np.array([tuple(obj.matrix_world @ Vector(p)) for p in pts])
    view = np.array(camera.matrix_world.inverted())
    cam_pts = (view[:3, :3] @ posed.T).T + view[:3, 3]
    lo, hi = cam_pts[:, :2].min(0), cam_pts[:, :2].max(0)
    span = float(max(hi - lo)) * MARGIN
    camera_data.ortho_scale = span
    middle = (lo + hi) / 2.0
    camera_data.shift_x = float(middle[0] / span)
    camera_data.shift_y = float(middle[1] / span)

    def sun(name, strength, rotation, angle):
        light = bpy.data.lights.new(name, "SUN")
        light.energy = strength
        light.angle = math.radians(angle)
        node = bpy.data.objects.new(name, light)
        node.rotation_euler = [math.radians(a) for a in rotation]
        scene.collection.objects.link(node)

    sun("Key", 3.2, (40, 0, -35), 8)      # upper left front
    sun("Fill", 0.9, (60, 0, 70), 25)     # right
    sun("Rim", 2.4, (-50, 0, 160), 6)     # behind, above
    return ("laid down, " if pole else "") + ("elongated" if elongated else ("flat" if flat else "upright")) + ", slender %.1f" % slender


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    root = args[0]
    wanted = [a for a in args[1].split(",") if a] if len(args) > 1 else []
    size = int(args[2]) if len(args) > 2 else 512
    items = wanted or sorted(d for d in os.listdir(root) if os.path.isfile(os.path.join(root, d, "meta.json")))
    for item in items:
        item_dir = os.path.join(root, item)
        try:
            meta = json.load(open(os.path.join(item_dir, "meta.json")))
            scene = reset_scene(size)
            obj = import_item(item_dir, meta)
            if obj is None:
                log("FAIL %s: no mesh in mesh.fbx" % item)
                continue
            pose = pose_and_frame(scene, obj)
            scene.render.filepath = os.path.join(item_dir, "icon.png")
            bpy.ops.render.render(write_still=True)
            log("OK %s (%s)" % (item, pose))
        except Exception as exc:
            log("FAIL %s: %s" % (item, exc))


main()
