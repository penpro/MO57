"""Which way each cliff slab's rock face points (for biome_palettes.ed_cliff's face_yaw; BR18-14 / BR18-19, Oct 19 2026): the area-weighted mean of its triangle normals, projected on the ground plane (an open-backed slab has
no faces on its back, so the mean points out of its face). Also the bounds, and how much of the area faces each horizontal quadrant.

Run:   python Tools/ue.py py --file Content/Python/cliff_face_probe.py     (prints CLIFF lines; strength ~0 = a closed rock with no face: leave it aligned)
Measured Oct 19: HugeSandstoneCliff_04 faceYaw 90 (strength 0.57), _07 90 (0.45); MassiveNordicCoastalCliff_01 none (0.00, closed)."""
import math
import unreal

MESHES = [
    "/Game/Megascans/3D_Assets/HugeSandstoneCliff/SM_HugeSandstoneCliff_04.SM_HugeSandstoneCliff_04",
    "/Game/Megascans/3D_Assets/HugeSandstoneCliff/SM_HugeSandstoneCliff_07.SM_HugeSandstoneCliff_07",
    "/Game/Megascans/3D_Assets/MassiveNordicCoastalCliff/SM_MassiveNordicCoastalCliff_01.SM_MassiveNordicCoastalCliff_01",
]

for path in MESHES:
    sm = unreal.load_asset(path)
    if not sm:
        out("CLIFF %s MISSING" % path)
        continue
    b = sm.get_bounds()
    sx = sx_area = 0.0
    sy = 0.0
    quad = {"+X": 0.0, "-X": 0.0, "+Y": 0.0, "-Y": 0.0, "up": 0.0, "down": 0.0}
    total = 0.0
    n_sections = sm.get_num_sections(0)
    for s in range(n_sections):
        verts, tris, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
        for t in range(0, len(tris), 3):
            a, bb, c = verts[tris[t]], verts[tris[t + 1]], verts[tris[t + 2]]
            e1 = unreal.Vector(bb.x - a.x, bb.y - a.y, bb.z - a.z)
            e2 = unreal.Vector(c.x - a.x, c.y - a.y, c.z - a.z)
            nx = e1.y * e2.z - e1.z * e2.y
            ny = e1.z * e2.x - e1.x * e2.z
            nz = e1.x * e2.y - e1.y * e2.x
            area = 0.5 * math.sqrt(nx * nx + ny * ny + nz * nz)
            if area <= 0.0:
                continue
            # UE winding: the geometric normal from this cross product points opposite to the rendered front face; check against the vertex normals.
            vn = normals[tris[t]]
            if nx * vn.x + ny * vn.y + nz * vn.z < 0.0:
                nx, ny, nz = -nx, -ny, -nz
            l = 2.0 * area
            nx, ny, nz = nx / l, ny / l, nz / l
            sx += nx * area
            sy += ny * area
            total += area
            if abs(nz) > 0.7:
                quad["up" if nz > 0 else "down"] += area
            elif abs(nx) >= abs(ny):
                quad["+X" if nx > 0 else "-X"] += area
            else:
                quad["+Y" if ny > 0 else "-Y"] += area
    yaw = math.degrees(math.atan2(sy, sx))
    strength = math.sqrt(sx * sx + sy * sy) / max(total, 1.0)
    out("CLIFF %s ext=(%.0f, %.0f, %.0f) origin=(%.0f, %.0f, %.0f) sections=%d faceYaw=%.1f deg strength=%.2f quads=%s" % (
        path.split(".")[-1], b.box_extent.x, b.box_extent.y, b.box_extent.z, b.origin.x, b.origin.y, b.origin.z, n_sections, yaw, strength,
        " ".join("%s:%.0f%%" % (k, 100.0 * v / max(total, 1.0)) for k, v in quad.items())))
