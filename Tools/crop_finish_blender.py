"""Blender (headless): finish a prepped TRELLIS crop plant for the Fab foliage master (M_MS_Foliage) -- after glb_item_prep_blender.py.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Tools/crop_finish_blender.py -- <prepped.glb> <out.glb> <out_ORT.png> [rough_lo rough_hi translucency [reference_photo]]

Why (Nov 3 2026, DECISIONS "Crop plants are made with TRELLIS"): crops use the same foliage master as the world's grass, so they sway with
the world's wind and light through their leaves. That master reads two things a TRELLIS mesh does not have:
  - Vertex colours, the Megascans plant convention (measured Nov 3 on the Dry / Wild Grass meshes exported from the project):
      A = height from the base, 0..1 (corr. with z 1.00): the primary bend weight (QMF_WindAnimation);
      R = a gradient along each blade, base 0 -> tip 1 (corr. with z ~0.5 and with distance from the axis ~0.45): the secondary flutter;
      G, B = a random value per blade (dozens of distinct levels): the phase. A TRELLIS plant is one fused shell, so G and B here are
      SMOOTH low-frequency noise over position (a per-vertex random would tear the mesh apart when the wind moves it).
  - A "Mask" texture, ORT: R opacity (1: the shell is solid), G roughness, B translucency. TRELLIS's own ORM is glTF (R occlusion,
    G roughness, B metallic 0) and its roughness is glossy for a plant (mean ~0.43): it is remapped into [rough_lo, rough_hi]
    (default 0.6..0.9; Megascans grass ~0.8). Translucency is constant (default 0.5, the Megascans grass mean), lowered where TRELLIS's
    occlusion is dark (the inside of a clump passes less light).
The occlusion also darkens the base colour a little (the foliage master has no AO input): base colour x (0.55 + 0.45 x AO).
With the reference photo given, the base colour is then matched to it: TRELLIS's bake is paler and flatter than its photo, so the
texels the mesh uses get the per-channel mean and spread of the photo's plant pixels (those differing from its grey backdrop).
First it cuts the bottom 2 % of the height (a soil disc the image model drew and TRELLIS kept) and sets the plant back on its pivot.
Output: the GLB with COLOR_0 (vertex colours) and the AO-darkened base colour, and the ORT PNG next to it (import as Masks, sRGB off);
<out>_novc.glb, the same without vertex colours, for previews (glTF viewers multiply the base colour by COLOR_0).
"""
import math
import sys

import bpy
import numpy as np

a = sys.argv[sys.argv.index("--") + 1:]
src, dst, ort_path = a[0], a[1], a[2]
rough_lo = float(a[3]) if len(a) > 3 else 0.6
rough_hi = float(a[4]) if len(a) > 4 else 0.9
transl = float(a[5]) if len(a) > 5 else 0.5
ref_path = a[6] if len(a) > 6 else ""

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
ob = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
me = ob.data
# The image models often stand the plant in a disc of soil (Qwen-Image did whatever the prompt said, Nov 3), and TRELLIS keeps it: cut the
# bottom 2 % of the height (the disc and the stubble under it) and lower the plant onto its pivot again, so the stems start at the ground.
import bmesh  # noqa: E402
bm = bmesh.new()
bm.from_mesh(me)
zs = [v.co.z for v in bm.verts]
cut = min(zs) + 0.02 * (max(zs) - min(zs))
bmesh.ops.delete(bm, geom=[f for f in bm.faces if max(v.co.z for v in f.verts) < cut], context="FACES")
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
low = min(v.co.z for v in bm.verts)
for v in bm.verts:
    v.co.z -= low
bm.to_mesh(me)
bm.free()
co = np.array([v.co[:] for v in me.vertices], dtype=np.float64)   # the prep left the pivot at the bottom centre, z up
z0, z1 = co[:, 2].min(), co[:, 2].max()
h = np.clip((co[:, 2] - z0) / max(z1 - z0, 1e-6), 0.0, 1.0)
r = np.hypot(co[:, 0], co[:, 1])
rn = r / max(r.max(), 1e-6)
blade = np.clip(0.6 * h + 0.4 * rn, 0.0, 1.0)


def smooth_noise(p, scale, seed):
    # Sum of a few sines over position: smooth, so neighbouring vertices move together; scale in metres.
    rng = np.random.default_rng(seed)
    acc = np.zeros(len(p))
    for _ in range(4):
        d = rng.normal(size=3)
        d /= np.linalg.norm(d)
        acc += np.sin(p @ d / scale * 2.0 * math.pi + rng.uniform(0, 2 * math.pi))
    return np.clip(0.5 + acc / 8.0, 0.0, 1.0)


g = smooth_noise(co, 0.25, 1)
b = smooth_noise(co, 0.18, 2)
for attr in list(me.color_attributes):
    me.color_attributes.remove(attr)
# FLOAT_COLOR: stored linear and exported as is. A BYTE_COLOR attribute is sRGB-encoded and the glTF export linearizes it, so Unreal got
# value^2.2 (Nov 3: R mean 0.435 -> 0.174); the alpha (the height weight) was never affected.
col = me.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="CORNER")
loop_v = np.empty(len(me.loops), dtype=np.int64)
me.loops.foreach_get("vertex_index", loop_v)
rgba = np.stack([blade, g, b, h], axis=1)[loop_v].astype(np.float32)
col.data.foreach_set("color", rgba.ravel())
me.color_attributes.active_color = col

# Textures: find the base colour and ORM images the prep wrote (named T_<Id>_BaseColor / _ORM).
imgs = {i.name: i for i in bpy.data.images if i.size[0] > 0}
base = next(i for n, i in imgs.items() if n.endswith("_BaseColor"))
orm = next(i for n, i in imgs.items() if n.endswith("_ORM"))
w, hgt = orm.size[:]
o = np.array(orm.pixels[:], dtype=np.float32).reshape(hgt, w, orm.channels)
ao, rough = o[..., 0], o[..., 1]
ort = np.ones((hgt, w, 4), dtype=np.float32)
ort[..., 1] = rough_lo + (rough_hi - rough_lo) * np.clip(rough, 0.0, 1.0)
ort[..., 2] = np.clip(transl * (0.6 + 0.4 * ao), 0.0, 1.0)
img = bpy.data.images.new("ORT", w, hgt, alpha=True)
img.colorspace_settings.name = "Non-Color"
img.pixels.foreach_set(ort.ravel())
img.filepath_raw = ort_path
img.file_format = "PNG"
img.save()

bw, bh = base.size[:]
bc = np.array(base.pixels[:], dtype=np.float32).reshape(bh, bw, base.channels)
if (bw, bh) == (w, hgt):
    bc[..., :3] *= (0.55 + 0.45 * ao)[..., None]
match_note = ""
if ref_path and ref_path != "-":
    # TRELLIS's bake comes out paler and flatter than its photo (Nov 3: the barley heads near white against the photo's gold). Match the
    # texels the mesh uses to the photo's plant pixels, per channel (mean and spread, in the stored sRGB values).
    ref = bpy.data.images.load(ref_path)
    rp = np.array(ref.pixels[:], dtype=np.float32).reshape(ref.size[1], ref.size[0], ref.channels)[..., :3]
    border = np.concatenate([rp[:8].reshape(-1, 3), rp[-8:].reshape(-1, 3), rp[:, :8].reshape(-1, 3), rp[:, -8:].reshape(-1, 3)])
    bg = np.median(border, axis=0)
    fg = rp[np.abs(rp - bg).max(axis=2) > 0.10]
    uv = np.empty(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    px = np.clip((uv[:, 0] * bw).astype(int), 0, bw - 1)
    py = np.clip((uv[:, 1] * bh).astype(int), 0, bh - 1)
    used = bc[py, px, :3]
    if len(fg) > 1000 and len(used) > 1000:
        # One gain per channel (tint and brightness) and one saturation scale. Matching each channel's SPREAD as well (tried first, Nov 3)
        # blew the bake's small chroma noise up into rainbow heads.
        def sat(c):
            return (c.max(axis=-1) - c.min(axis=-1)).mean()
        mt, mr = used.mean(0) + 1e-4, fg.mean(0)
        gain = np.clip(mr / mt, 0.5, 2.0)
        rgb = np.clip(bc[..., :3] * gain, 0.0, 1.0)
        s_scale = float(np.clip(sat(fg) / max(sat(np.clip(used * gain, 0, 1)), 1e-4), 1.0, 1.6))
        grey = rgb.mean(axis=-1, keepdims=True)
        bc[..., :3] = np.clip(grey + (rgb - grey) * s_scale, 0.0, 1.0)
        match_note = ", colour matched to %s (gain %s, saturation x%.2f)" % (ref_path.rsplit("/", 1)[-1], np.round(gain, 2).tolist(), s_scale)
base.pixels.foreach_set(bc.ravel())
base.pack()

bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB", export_vertex_color="ACTIVE", export_all_vertex_colors=False,
                          export_image_format="AUTO")
# glTF makes COLOR_0 a MULTIPLIER of the base colour, so any glTF viewer (the preview) tints the plant by the wind data -- rainbow heads
# (Nov 3). Unreal does not: the foliage master reads the colours as wind / mask data only. A copy without them is for previews.
me.color_attributes.remove(col)
bpy.ops.export_scene.gltf(filepath=dst[:-4] + "_novc.glb", export_format="GLB", export_vertex_color="NONE", export_image_format="AUTO")
print("CROPFINISH %s: %d verts, height %.1f cm, ORT %s rough %.2f..%.2f (TRELLIS mean %.2f) transl %.2f, AO mean %.2f%s"
      % (dst, len(me.vertices), (z1 - z0) * 100.0, ort_path, rough_lo, rough_hi, float(rough.mean()), transl, float(ao.mean()), match_note))
