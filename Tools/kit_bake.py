"""Bake the UI-kit composites that CommonUI needs as ONE texture per state (a CommonButtonStyle takes a single brush per state).

    python Tools/kit_bake.py            ->  Saved/KitBake/Buttons/T_Btn_<variant>_<state>.png  (64x64, 9-slice margin 8 px = 0.125, identical to button_fill / button_stroke)

The recipe is the kit's own web mirror (DesignKit/.../_source/mo57_web_mirror.css, .btn-*): fill (button_fill mask) + optional 1 px stroke (button_stroke mask) tinted from tokens.json, composited
at the SAME size and margins as the masks, so the result is still a valid 9-slice. Import with Content/Python/kit_import.py (it picks up Saved/KitBake/Buttons). Colours come from tokens.json:
change a token, re-run this, re-run kit_import: every button restyles.
"""
import json
import os
import sys

from PIL import Image

KIT = r"D:\UEProjects\MO57\DesignKit\MO57_UI_Kit\MO57_UI_Kit"
OUT = r"D:\UEProjects\MO57\Saved\KitBake\Buttons"

TOK = json.load(open(os.path.join(KIT, "tokens.json"), encoding="utf-8"))


def hexrgba(hx):
    hx = hx.lstrip("#")
    a = int(hx[6:8], 16) if len(hx) == 8 else 255
    return tuple(int(hx[i:i + 2], 16) for i in (0, 2, 4)) + (a,)


def t(name):
    return hexrgba(TOK[name])


def mix(base, over):
    """`over` (r,g,b,a) laid over an opaque-ish `base` colour, returns an rgba tuple (alpha of base kept)."""
    oa = over[3] / 255.0
    return tuple(int(round(base[i] * (1 - oa) + over[i] * oa)) for i in range(3)) + (base[3],)


WHITE6 = (255, 255, 255, int(round(0.06 * 255)))     # --hover overlay
BLACK20 = (0, 0, 0, int(round(0.20 * 255)))          # --pressed overlay


def mask(name):
    return Image.open(os.path.join(KIT, "nineslice", "2x", "T_UI_%s@2x.png" % name)).convert("RGBA")


def tint(im, rgba):
    r, g, b, a = rgba
    out = Image.new("RGBA", im.size)
    src, dst = im.load(), out.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            pr, pg, pb, pa = src[x, y]
            dst[x, y] = (r * pr // 255, g * pg // 255, b * pb // 255, pa * a // 255)
    return out


FILL, STROKE = mask("button_fill"), mask("button_stroke")


def bake(fill, stroke=None):
    im = Image.new("RGBA", FILL.size, (0, 0, 0, 0))
    if fill is not None:
        im.alpha_composite(tint(FILL, fill))
    if stroke is not None:
        im.alpha_composite(tint(STROKE, stroke))
    return im


accent, accent_h, accent_p = t("color.accent"), t("color.accent.hover"), t("color.accent.pressed")
s3, s4, line2 = t("color.surface.3"), t("color.surface.4"), t("color.border.strong")
sel_fill = hexrgba("#24302B")
crit_fill = t("color.severity.critical.fill")
crit_hover, crit_pressed = hexrgba("#8F2B1E"), hexrgba("#5E1A12")
disabled_fill = s3

STATES = {
    # CSS .btn-primary / :hover / .is-pressed / .is-disabled
    "primary": {
        "normal": (accent, None), "hovered": (accent_h, None), "pressed": (accent_p, None),
        "selected": (accent, None), "selected_hovered": (accent_h, None), "selected_pressed": (accent_p, None),
        "disabled": (disabled_fill, None),
    },
    # .btn-secondary: s3 + 1 px line2; hover s4; pressed #24211C; selected #24302B + accent stroke
    "secondary": {
        "normal": (s3, line2), "hovered": (s4, line2), "pressed": (hexrgba("#24211C"), line2),
        "selected": (sel_fill, accent), "selected_hovered": (mix(sel_fill, WHITE6), accent), "selected_pressed": (mix(sel_fill, BLACK20), accent),
        "disabled": (disabled_fill, None),
    },
    # .btn-tertiary: transparent; hover white 6 %; pressed black 20 %; selected = accent muted
    "tertiary": {
        "normal": (None, None), "hovered": (WHITE6, None), "pressed": (BLACK20, None),
        "selected": (t("color.accent.muted"), None), "selected_hovered": (mix(t("color.accent.muted"), WHITE6), None), "selected_pressed": (mix(t("color.accent.muted"), BLACK20), None),
        "disabled": (None, None),
    },
    # .btn-danger: crit-fill; hover #8F2B1E; pressed #5E1A12
    "danger": {
        "normal": (crit_fill, None), "hovered": (crit_hover, None), "pressed": (crit_pressed, None),
        "selected": (crit_fill, t("color.severity.critical")), "selected_hovered": (crit_hover, t("color.severity.critical")), "selected_pressed": (crit_pressed, t("color.severity.critical")),
        "disabled": (disabled_fill, None),
    },
}

os.makedirs(OUT, exist_ok=True)
n = 0
for variant, states in STATES.items():
    for state, (fill, stroke) in states.items():
        bake(fill, stroke).save(os.path.join(OUT, "T_Btn_%s_%s.png" % (variant, state)))
        n += 1
print("baked %d button state textures into %s" % (n, OUT))
