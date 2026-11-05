"""MO57 promo kit, hero prop: the clone pod, closed. A stone egg about 2.2 m long, nose scorched by entry, its hatch a fine seam with a mineral inlay.

The game's own mesh (/Game/ImageTo3D/CloneEggPod01, a TRELLIS scan of Art/References/ImageTo3D/2026-10-18-clone-egg) is the OPEN egg lying on a shore, which is
what the footage shows. This one is the same object before it opens, built from measurements so it holds up in close-up and can glow; re-use it for
anything that needs the pod in the air, in orbit, on a poster. Local axes: nose at -X (x = -1.1), rear at +X (x = +1.1), the hatch on +Z. Metres.

    import kit_pod
    pod = kit_pod.make_pod(detail=8)          # detail: icosphere subdivisions (8 = 655k verts, ~3.5 mm apart; 5 = a swarm's copy)
    kit_pod.heat_node(pod).outputs[0].default_value = 1.0   (keyframe it: the nose chars at all times, this makes it glow)
"""
import math

import bmesh
import bpy
import numpy as np

import kit_common as kit

LENGTH = 2.2
BROAD = 1.3


def _value_noise(p, seed):
    """Smooth 3D value noise over Nx3 points, about -1..1 (numpy: Python's mathutils.noise per vertex takes minutes at detail 8)."""
    ip = np.floor(p).astype(np.int64)
    f = p - ip
    f = f * f * (3 - 2 * f)

    def rnd(ix, iy, iz):
        h = (ix * 374761393 + iy * 668265263 + iz * 2147483647 + seed * 1274126177) & 0xFFFFFFFF
        h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
        return (h ^ (h >> 16)) / 2147483648.0 - 1.0

    out = 0.0
    for dx in (0, 1):
        wx = f[:, 0] if dx else 1 - f[:, 0]
        for dy in (0, 1):
            wy = f[:, 1] if dy else 1 - f[:, 1]
            for dz in (0, 1):
                wz = f[:, 2] if dz else 1 - f[:, 2]
                out = out + wx * wy * wz * rnd(ip[:, 0] + dx, ip[:, 1] + dy, ip[:, 2] + dz)
    return out


def fbm(p, octaves, seed, lac=2.03, gain=0.52):
    amp, total, a = 1.0, 0.0, 0.0
    q = p.copy()
    for i in range(octaves):
        total = total + amp * _value_noise(q, seed + i)
        a += amp
        amp *= gain
        q = q * lac + 11.3
    return total / a


def egg_points(unit):
    """Unit-sphere points -> the egg: tapered nose at -X, broad rear at +X, a little flattened underneath."""
    x, y, z = unit[:, 0], unit[:, 1], unit[:, 2]
    # the radius across the long axis follows x: thin towards the nose
    bulge = 1.0 + 0.30 * x - 0.06 * x * x
    px = x * LENGTH * 0.5
    py = y * bulge * BROAD * 0.5
    pz = z * bulge * BROAD * 0.5 * np.where(z < 0, 0.92, 1.0)
    return np.stack((px, py, pz), axis=1)


def make_pod(detail=8, name="MO_ClonePod", heat=True, seed=7):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=detail, radius=1.0)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    unit = co.reshape(-1, 3)
    unit = unit / np.linalg.norm(unit, axis=1, keepdims=True)
    p = egg_points(unit)
    normal = unit.copy()
    # the hatch: an ellipse on top, in the egg's plan view
    ax, ay = 0.74, 0.40          # half length, half width (m), centred a little back of the middle
    cx = 0.16
    planar = np.sqrt(((p[:, 0] - cx) / ax) ** 2 + (p[:, 1] / ay) ** 2)
    top = p[:, 2] > 0
    seam_d = np.abs(planar - 1.0) * min(ax, ay)     # metres from the seam, roughly
    seam = np.where(top, np.clip(1.0 - seam_d / 0.012, 0.0, 1.0), 0.0)
    lip = np.where(top, np.clip(1.0 - seam_d / 0.030, 0.0, 1.0), 0.0)
    # cracks: edges of a few big voronoi cells, hairline where two meet
    rng = np.random.default_rng(seed)
    seeds = rng.uniform(-1, 1, (12, 3)) * np.array([1.2, 0.7, 0.7])
    dist = np.linalg.norm(p[:, None, :] - seeds[None, :, :], axis=2)
    dist.sort(axis=1)
    warp = fbm(p * 5.0, 3, seed + 20)
    crack_d = dist[:, 1] - dist[:, 0] + warp * 0.02
    crack = np.clip(1.0 - np.abs(crack_d) / 0.010, 0.0, 1.0) * np.clip((fbm(p * 1.3, 2, seed + 40) + 0.15) * 4.0, 0.0, 1.0) * np.where(lip > 0, 0.0, 1.0)
    # the surface: broad swellings, weathered lumps, hammered pits; rougher towards the burnt nose
    burn = np.clip((0.1 - p[:, 0]) / 1.2, 0.0, 1.0)
    d = 0.030 * fbm(p * 1.4, 3, seed + 1)
    d += 0.012 * fbm(p * 6.0, 4, seed + 2)
    d += (0.0035 + 0.006 * burn) * fbm(p * 26.0, 4, seed + 3)
    pock = fbm(p * 13.0, 3, seed + 4)
    d -= burn * 0.022 * np.clip(pock - 0.15, 0.0, 1.0) * 3.0
    d -= 0.006 * seam + 0.002 * lip
    d -= 0.012 * crack
    out = p + normal * d[:, None]
    me.vertices.foreach_set("co", out.ravel())
    me.update()
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    a = me.attributes.new("seam", "FLOAT", "POINT")
    a.data.foreach_set("value", seam.astype(np.float32))
    a = me.attributes.new("crack", "FLOAT", "POINT")
    a.data.foreach_set("value", crack.astype(np.float32))
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    me.materials.append(pod_material(heat))
    return ob


def pod_material(heat=True):
    existing = bpy.data.materials.get("MO_PodStone")
    if existing:
        return existing
    m = kit.granite("MO_PodStone", coord_scale=0.42, heat_axis=(-1.15, 0.15) if heat else None, bump=1.3)
    t = m.node_tree
    n = kit.Nodes(t)
    out = next(x for x in t.nodes if x.bl_idname == "ShaderNodeOutputMaterial")
    surface = out.inputs["Surface"].links[0].from_socket
    seam = n.add("Attribute", p_attribute_name="seam").outputs["Fac"]
    crack = n.add("Attribute", p_attribute_name="crack").outputs["Fac"]
    dark = n.add("BsdfPrincipled", Base_Color=(0.006, 0.005, 0.005, 1), Roughness=0.9).outputs[0]
    cracked = n.add("MixShader", Fac=crack)
    n.link(surface, cracked.inputs[1])
    n.link(dark, cracked.inputs[2])
    # the seam: a thread of old metal with the mineral inlay's teal in its deepest line
    inlay_glow = n.value(0.35, "Inlay")
    em = n.add("Emission", Color=kit.linear(kit.TEAL_HI), Strength=kit.Nodes(t).math("MULTIPLY", inlay_glow.outputs[0], 3.0))
    metal = n.add("BsdfPrincipled", Base_Color=(0.32, 0.27, 0.20, 1), Metallic=1.0, Roughness=0.32).outputs[0]
    deep = n.math("GREATER_THAN", seam, 0.72)
    seam_shader = n.add("MixShader", Fac=deep)
    n.link(metal, seam_shader.inputs[1])
    n.link(em.outputs[0], seam_shader.inputs[2])
    final = n.add("MixShader", Fac=n.math("MULTIPLY", seam, 1.6, clamp=True))
    n.link(cracked.outputs[0], final.inputs[1])
    n.link(seam_shader.outputs[0], final.inputs[2])
    n.link(final.outputs[0], out.inputs["Surface"])
    return m


def heat_node(pod):
    return pod.data.materials[0].node_tree.nodes["Heat"]


def inlay_node(pod):
    return pod.data.materials[0].node_tree.nodes["Inlay"]
