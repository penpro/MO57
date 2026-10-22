"""Blender (headless): a quick look at a GLB before it goes into Unreal -- the "render it and look" step of Docs/Item_Art_Pipeline.md section 8.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Tools/glb_preview_blender.py -- <in.glb> <out.png> [size_px]

Renders the model twice side by side (3/4 view from above-front, and a plain side view) with Eevee, a gray world and one sun, the
camera fitted to the bounding box, plus its size in cm in the console (GLBPREVIEW line): what it is, which way is up, how big. For the
prepared GLB (Saved/ItemPrep/<ItemId>.glb) the size is the real one; a raw TRELLIS.2 GLB is ~0.9 m whatever the object (Oct 22, 2026).
"""
import bpy, sys, math
from mathutils import Vector

a = sys.argv[sys.argv.index("--") + 1:]
src, dst = a[0], a[1]
size = int(a[2]) if len(a) > 2 else 512

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
corners = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
lo = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
hi = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
centre, extent = (lo + hi) / 2, hi - lo
radius = max(extent.length / 2, 1e-4)
tris = sum(len(p.vertices) - 2 for o in meshes for p in o.data.polygons)
print("GLBPREVIEW %s: %.1f x %.1f x %.1f cm (x y z, z up), %d triangles" % (src, extent.x * 100, extent.y * 100, extent.z * 100, tris))

scene = bpy.context.scene
for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "BLENDER_WORKBENCH"):
    try:
        scene.render.engine = engine
        break
    except TypeError:
        continue
scene.render.resolution_x, scene.render.resolution_y = size, size
scene.render.film_transparent = False
world = bpy.data.worlds.new("Preview")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.55, 0.57, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
scene.world = world
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 3.0
sun.rotation_euler = (math.radians(50), 0, math.radians(30))
scene.collection.objects.link(sun)
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
cam.data.lens = 50
scene.collection.objects.link(cam)
scene.camera = cam

shots = []
for name, yaw, pitch in (("three_quarter", 35, 30), ("side", 0, 0)):
    d = Vector((math.cos(math.radians(pitch)) * math.sin(math.radians(yaw)), -math.cos(math.radians(pitch)) * math.cos(math.radians(yaw)),
                math.sin(math.radians(pitch))))
    dist = radius / math.tan(cam.data.angle / 2) * 1.15
    cam.location = centre + d * dist
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_start, cam.data.clip_end = dist / 100, dist * 10
    path = dst.replace(".png", "_%s.png" % name)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    shots.append(path)

# side by side into dst
imgs = [bpy.data.images.load(p) for p in shots]
out = bpy.data.images.new("Preview", size * len(imgs), size)
pixels = [0.0] * (size * len(imgs) * size * 4)
for i, img in enumerate(imgs):
    src_px = list(img.pixels)
    for y in range(size):
        row = src_px[y * size * 4:(y + 1) * size * 4]
        start = (y * size * len(imgs) + i * size) * 4
        pixels[start:start + size * 4] = row
out.pixels = pixels
out.filepath_raw = dst
out.file_format = "PNG"
out.save()
print("GLBPREVIEW wrote %s" % dst)
