"""Blender (headless): make a raw image-to-3D GLB (TRELLIS.2 in ComfyUI, Tripo, ...) import-ready as an MO57 item mesh.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Tools/glb_item_prep_blender.py -- <in.glb> Saved/ItemPrep/<ItemId>.glb <longest_cm> <pca|tilt> <target_tris> <tex_px>

Why (2026-10-17): TRELLIS.2 output is ~700k triangles, 4K textures, ~0.9 m whatever the object, tilted the way the reference photo
was shot (camera ~30 deg above), with generic names. Unreal names imported assets after the GLB's names and renaming assets from
editor Python hangs the editor, so the names are fixed here. This script:
  - joins the meshes and decimates to target_tris (15-40k is plenty for a hand-held item; the baked normal map keeps the detail);
  - orients it. pca: smallest extent -> up, largest -> X (flat things lie flat, long things lie along X). tilt: undoes only the
    reference camera's ~37 deg tilt, for upright objects whose opening must stay up (a container);
  - scales so the longest side is longest_cm (real size: no row scale needed) and puts the pivot at the bottom centre;
  - downsizes textures to tex_px (2048; 1024 for things under ~8 cm);
  - names the mesh SM_<ItemId>, the material M_<ItemId>, the textures T_<ItemId>_BaseColor / _Normal / _ORM.
Then: ue.py asset import (MSYS_NO_PATHCONV=1!) -> item_mesh_prepare -> ue.py rows set -> icon_studio.py. Docs/Item_Art_Pipeline.md section 8.
"""
import bpy, sys, math
import numpy as np
from mathutils import Matrix

a = sys.argv[sys.argv.index("--") + 1:]
src, dst, longest_cm, orient, target, tex_px = a[0], a[1], float(a[2]), a[3], int(a[4]), int(a[5])

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for o in bpy.context.scene.objects:
    o.select_set(o in meshes)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# drop leftover empties from the glTF hierarchy
for o in list(bpy.context.scene.objects):
    if o.type != "MESH":
        bpy.data.objects.remove(o, do_unlink=True)

tris0 = sum(len(p.vertices) - 2 for p in ob.data.polygons)
if tris0 > target:
    mod = ob.modifiers.new("Dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = target / tris0
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=mod.name)

P = np.array([v.co[:] for v in ob.data.vertices])
c = P.mean(0)
if orient == "pca":
    ev, evec = np.linalg.eigh(np.cov((P - c).T))          # ascending: evec[:,0] smallest
    small, mid, large = evec[:, 0], evec[:, 1], evec[:, 2]
    if small[2] < 0:                                        # keep the side that faced up in the reference photo on top
        small = -small
    x = large
    y = np.cross(small, x)
    R = np.array([x, y, small])                              # rows: new X, Y, Z in old coordinates
    rot = Matrix([list(R[0]) + [0], list(R[1]) + [0], list(R[2]) + [0], [0, 0, 0, 1]])
else:                                                       # tilt: the references' camera looked ~37 deg down from the front (-Y)
    rot = Matrix.Rotation(math.radians(-36.87), 4, "X")
ob.data.transform(rot)

P = np.array([v.co[:] for v in ob.data.vertices])
mn, mx = P.min(0), P.max(0)
s = (longest_cm / 100.0) / float((mx - mn).max())
ob.data.transform(Matrix.Scale(s, 4))
P = P * s
mn, mx = P.min(0), P.max(0)
ob.data.transform(Matrix.Translation((-(mn[0] + mx[0]) / 2, -(mn[1] + mx[1]) / 2, -mn[2])))  # bottom centre
ob.data.update()

for img in bpy.data.images:
    if img.size[0] > tex_px:
        img.scale(tex_px, tex_px)
        img.pack()

# Names travel into Unreal (the importer names assets after them; renaming assets from editor Python hangs the editor).
import os
item = os.path.splitext(os.path.basename(dst))[0]
ob.name = ob.data.name = "SM_" + item
for mat in ob.data.materials:
    mat.name = "M_" + item
    if not mat.use_nodes:
        continue
    for node in mat.node_tree.nodes:
        if node.type != "TEX_IMAGE" or not node.image:
            continue
        role, todo, seen = None, [node], set()
        while todo and not role:                            # follow the links to the shader input this image feeds
            n = todo.pop()
            for out in n.outputs:
                for link in out.links:
                    s = link.to_socket.name.lower()
                    if link.to_node.type == "BSDF_PRINCIPLED":
                        role = "BaseColor" if "base color" in s else "Normal" if "normal" in s else "ORM"
                    elif link.to_node.type == "NORMAL_MAP":
                        role = "Normal"
                    elif link.to_node.name not in seen:
                        seen.add(link.to_node.name); todo.append(link.to_node)
        if not role and "occlusion" in (node.label + node.name).lower():
            role = "ORM"
        node.image.name = "T_%s_%s" % (item, role or "Extra")

tris1 = sum(len(p.vertices) - 2 for p in ob.data.polygons)
ext = (mx - mn) * 100
bpy.ops.object.select_all(action="SELECT")
bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB", use_selection=True, export_yup=True, export_apply=True)
print("PREP %s tris %d -> %d  size_cm=%s  tex=%s" % (dst, tris0, tris1, [round(float(e), 1) for e in ext],
      [(i.name, tuple(i.size)) for i in bpy.data.images]))
