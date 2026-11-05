"""MO57 promo kit: static instances of the two Windows variable fonts the kit sets type in, plus their advance widths.

    python Tools/promo/kit/make_fonts.py [out_dir]     default Saved/Promo/fonts (not tracked: the fonts are Microsoft's, and only the rendered frames leave this machine)

display = Sitka Banner Bold (opsz 27.5, wght 700): the face of the Steam wordmark (Tools/steam_wordmark.py). text = Sitka Subheading Semibold.
label = Bahnschrift SemiCondensed SemiBold. When Wes answers fork F14 (the UI kit's OFL fonts, Bitter and IBM Plex), point these at them and re-render.
Blender's text objects load one instance of a font, not a variable axis, hence the instances; metrics.json gives kit_common.layout() its widths.
"""
import json
import os
import sys

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SPEC = {
    "display": (r"C:\Windows\Fonts\SitkaVF.ttf", {"opsz": 27.5, "wght": 700}, "MOPromoDisplay.ttf"),
    "text": (r"C:\Windows\Fonts\SitkaVF.ttf", {"opsz": 16.74, "wght": 600}, "MOPromoText.ttf"),
    "label": (r"C:\Windows\Fonts\bahnschrift.ttf", {"wght": 600, "wdth": 87.5}, "MOPromoLabel.ttf"),
}
CHARS = [chr(c) for c in range(32, 127)] + list("·—–’")


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "Promo", "fonts")
    os.makedirs(out, exist_ok=True)
    metrics = {}
    for kind, (src, axes, name) in SPEC.items():
        font = instancer.instantiateVariableFont(TTFont(src), axes)
        path = os.path.join(out, name)
        font.save(path)
        upem = font["head"].unitsPerEm
        cmap = font.getBestCmap()
        hmtx = font["hmtx"]
        metrics[kind] = {"advance": {c: hmtx[cmap[ord(c)]][0] / upem for c in CHARS if ord(c) in cmap},
                         "cap_height": getattr(font["OS/2"], "sCapHeight", 700) / upem}
        print("FONT", kind, path, "cap", metrics[kind]["cap_height"])
    with open(os.path.join(out, "metrics.json"), "w", encoding="utf-8") as f:
        json.dump(metrics, f)


if __name__ == "__main__":
    main()
