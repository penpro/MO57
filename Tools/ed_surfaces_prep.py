"""Stage 1 of importing the Electric Dreams ground surfaces as voxel terrain materials: unpack the exported textures into the maps the voxel base material (M_QuixelBase) takes.

    (editor open) python Tools/ue.py py --file Content/Python/ed_surfaces_export.py     # writes Saved/EDSurf/<Surface>_<BC|N|AoRDp>.png (4K, from the migrated Megascans surface textures)
    python Tools/ed_surfaces_prep.py                                                    # writes Saved/EDSurf/out/<Surface>_{diff,nor,rough,height}.png (2048)
    (editor open) python Tools/ue.py py --file Content/Python/ed_surfaces_import.py     # stage 2: textures + material instances + surface types + the mega material

Channel layout of the Epic / Megascans "AoRDp" pack (checked from the pixel statistics): R = ambient occlusion, G = roughness, B = displacement (height, centred on 0.5). Base colour comes as BC
without AO baked in, so a little AO is multiplied back for contact darkening. Normals are Unreal-native (DirectX) maps except where a surface ships none (IcelandicQuarryRock): that one is derived
from its height as an OpenGL map, imported with the flip-green flag, the way the gravel set is.
"""
import os

import numpy as np
from PIL import Image

SRC = "D:/UEProjects/MO57/Saved/EDSurf/"
OUT = SRC + "out/"
SIZE = 2048
SURFACES = ["NordicMoss", "MossyRockyGround", "IcelandicQuarryRock", "MossyGrass", "DriedGrassOnDampSoil"]


def load(name, mode="RGB"):
    """Every map comes back at SIZE x SIZE (the packs mix 4K colour with 2K masks)."""
    p = SRC + name + ".png"
    if not os.path.exists(p):
        return None
    im = Image.open(p).convert(mode)
    if im.size != (SIZE, SIZE):
        im = im.resize((SIZE, SIZE), Image.LANCZOS)
    return np.asarray(im).astype(np.float32) / 255.0


def save(arr, name, mode="RGB"):
    a = np.clip(arr * 255.0 + 0.5, 0, 255).astype(np.uint8)
    im = Image.fromarray(a, mode)
    if im.size != (SIZE, SIZE):
        im = im.resize((SIZE, SIZE), Image.LANCZOS)
    im.save(OUT + name + ".png")


def height_to_normal_gl(h, strength):
    """OpenGL tangent-space normal from a height map (periodic so the tile stays seamless)."""
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5
    n = np.stack([-dx * strength, dy * strength, np.ones_like(h)], axis=-1)   # image y points down: GL green = up
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def main():
    os.makedirs(OUT, exist_ok=True)
    for s in SURFACES:
        bc = load(s + "_BC")[..., :3]
        pack = load(s + "_AoRDp", "RGBA")
        ao, rough, height = pack[..., 0], pack[..., 1], pack[..., 2]
        hs = (height - height.mean()) / max(height.std(), 1e-3) * 0.18 + 0.5   # contrast-stretch the (flat) displacement
        diff = bc * (0.55 + 0.45 * ao[..., None] ** 0.8)
        save(diff, s + "_diff")
        save(rough, s + "_rough", "L")
        save(hs, s + "_height", "L")
        n = load(s + "_N")
        if n is not None:
            save(n[..., :3], s + "_nor")
            kind = "native"
        else:
            save(height_to_normal_gl(hs, 6.0), s + "_nor")
            kind = "from height (GL)"
        print("%-22s done (normal: %s)" % (s, kind))


if __name__ == "__main__":
    main()
