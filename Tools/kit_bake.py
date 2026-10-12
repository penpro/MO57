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
MANIFEST = {}   # baked piece -> {"size": [w, h], "margin": [l, t, r, b] (UMG 0..1), "draw": "Box"|"Image"}; written to Content/Python/kit_baked_manifest.json for kit_lib
BOX64 = [0.125, 0.125, 0.125, 0.125]


def save(im, name, margin, draw="Box", folder=None):
    d = os.path.join(os.path.dirname(OUT), folder) if folder else OUT
    os.makedirs(d, exist_ok=True)
    im.save(os.path.join(d, name + ".png"))
    MANIFEST[name] = {"size": list(im.size), "margin": margin, "draw": draw, "folder": folder or "Buttons"}


n = 0
for variant, states in STATES.items():
    for state, (fill, stroke) in states.items():
        save(bake(fill, stroke), "T_Btn_%s_%s" % (variant, state), BOX64)
        n += 1
print("baked %d button state textures into %s" % (n, OUT))

# ---- rows (CSS .row: s2, hover s3, selected = accent muted over s2 + 1 px accent @ 55 %, disabled text only) : panel_fill + panel_stroke masks (margin 8 of 64)
PF, PS = mask("panel_fill"), mask("panel_stroke")


def bake_pf(fill, stroke=None):
    im = Image.new("RGBA", PF.size, (0, 0, 0, 0))
    im.alpha_composite(tint(PF, fill))
    if stroke is not None:
        im.alpha_composite(tint(PS, stroke))
    return im


accent_m = t("color.accent.muted")
accent55 = t("color.accent")[:3] + (int(round(0.55 * 255)),)
s2 = t("color.surface.2")
row = {"normal": (s2, None), "hovered": (t("color.surface.3"), None), "selected": (mix(s2, accent_m), accent55), "disabled": (s2, None)}
for k, (f, st) in row.items():
    save(bake_pf(f, st), "T_Row_" + k, BOX64, folder="Rows")

# ---- tabs (CSS .tab: 48 high, selected = primary text + 4 px accent underline inset 8 px, hover white 6 %) : the underline lives in the bottom margin row of a 64x64 9-slice
def tab_img(fill, underline):
    im = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    if fill is not None:
        im.alpha_composite(Image.new("RGBA", (64, 64), fill))
    if underline is not None:
        bar = Image.new("RGBA", (48, 8), underline)   # 4 px at 1x: rows 56..64, inset 8 px each side
        im.alpha_composite(bar, (8, 56))
    return im


tabs = {"normal": (None, None), "hovered": (WHITE6, None), "pressed": (BLACK20, None), "selected": (None, accent), "selected_hovered": (WHITE6, accent), "disabled": (None, None)}
for k, (f, u) in tabs.items():
    save(tab_img(f, u), "T_Tab_" + k, BOX64, folder="Tabs")

# ---- text inputs / dropdowns / spin boxes (CSS .input: s0 well, 1 px line2 + 2 px bottom rule; focus accent; error critical) : input_fill + input_stroke masks, margins 8,8,8,12 of 64
IF, IS = mask("input_fill"), mask("input_stroke")
IN_MARGIN = [0.125, 0.125, 0.125, 0.1875]


def bake_in(stroke, fill=None):
    im = Image.new("RGBA", IF.size, (0, 0, 0, 0))
    im.alpha_composite(tint(IF, fill or t("color.surface.0")))
    im.alpha_composite(tint(IS, stroke))
    return im


for k, st in {"normal": line2, "focused": accent, "error": t("color.severity.critical"), "disabled": t("color.border.subtle")}.items():
    save(bake_in(st), "T_Input_" + k, IN_MARGIN, folder="Controls")

# ---- check boxes (24 px) and toggles (52 x 28): drawn procedurally at 2x, from the kit CSS (.check / .toggle)
from PIL import ImageDraw  # noqa: E402


def rr(draw, box, r, fill=None, outline=None, width=0):
    draw.rounded_rectangle(box, radius=r, fill=fill, outline=outline, width=width)


def ss(w, h, fn):
    """supersample 4x then reduce: smooth edges."""
    big = Image.new("RGBA", (w * 4, h * 4), (0, 0, 0, 0))
    fn(ImageDraw.Draw(big), 4)
    return big.resize((w, h), Image.LANCZOS)


ink, tx2, tx = t("color.accent.ink"), t("color.text.secondary"), t("color.text.primary")
s0, s4 = t("color.surface.0"), t("color.surface.4")


def check_img(on, hover):
    W = 48   # 24 px @2x
    def f(d, k):
        if on:
            rr(d, (0, 0, W * k - 1, W * k - 1), 4 * k, fill=accent if not hover else accent_h)
            # check mark
            d.line([(12 * k, 25 * k), (20 * k, 33 * k), (36 * k, 15 * k)], fill=ink, width=5 * k, joint="curve")
        else:
            rr(d, (0, 0, W * k - 1, W * k - 1), 4 * k, fill=s0, outline=tx2 if hover else line2, width=4 * k)
    return ss(W, W, f)


def toggle_img(on, hover):
    w, h = 104, 56   # 52 x 28 @2x
    def f(d, k):
        rr(d, (0, 0, w * k - 1, h * k - 1), (h // 2) * k, fill=(accent_h if hover else accent) if on else s4)
        if not on:
            rr(d, (1 * k, 1 * k, w * k - 2 * k, h * k - 2 * k), (h // 2) * k, outline=tx2 if hover else line2, width=2 * k)
        cx = (w - 28) if on else 28
        d.ellipse(((cx - 20) * k, (h // 2 - 20) * k, (cx + 20) * k, (h // 2 + 20) * k), fill=ink if on else tx2)
    return ss(w, h, f)


for on in (False, True):
    for hover in (False, True):
        nm = ("on" if on else "off") + ("_hover" if hover else "")
        save(check_img(on, hover), "T_Chk_" + nm, [0, 0, 0, 0], draw="Image", folder="Controls")
        save(toggle_img(on, hover), "T_Tgl_" + nm, [0, 0, 0, 0], draw="Image", folder="Controls")

# ---- slider (CSS .slider: 6 px track s4, knob 20 x 28 text primary with a 2 px s1 ring)
def bar_img():
    def f(d, k):
        rr(d, (0, 0, 32 * 2 * k - 1, 12 * k - 1), 2 * k, fill=s4)
    return ss(64, 12, f)


def thumb_img(hover):
    w, h = 40, 56
    def f(d, k):
        rr(d, (0, 0, w * k - 1, h * k - 1), 4 * k, fill=t("color.surface.1"))
        rr(d, (4 * k, 4 * k, w * k - 5 * k, h * k - 5 * k), 2 * k, fill=accent_h if hover else tx)
    return ss(w, h, f)


save(bar_img(), "T_Sld_bar", [0.0469, 0.25, 0.0469, 0.25], folder="Controls")
save(thumb_img(False), "T_Sld_thumb", [0, 0, 0, 0], draw="Image", folder="Controls")
save(thumb_img(True), "T_Sld_thumb_hover", [0, 0, 0, 0], draw="Image", folder="Controls")

# ---- menu backdrop: a black-to-clear gradient behind a left-hand button rail (legibility over a bright 3D backdrop; hard-edged scrims look heavy)
def left_gradient():
    w, h = 512, 8
    im = Image.new("RGBA", (w, h))
    px = im.load()
    for x in range(w):
        a = max(0.0, 1.0 - x / (w - 1)) ** 1.6 * 0.82   # ease-out: dense behind the rail, gone by the panels
        for y in range(h):
            px[x, y] = (7, 6, 5, int(round(a * 255)))
    return im


save(left_gradient(), "T_Backdrop_left", [0, 0, 0, 0], draw="Image", folder="Backdrops")

import json as _json  # noqa: E402

with open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Content", "Python", "kit_baked_manifest.json"), "w") as f:
    _json.dump(MANIFEST, f, indent=1)
print("baked manifest: %d pieces (buttons, rows, tabs, inputs, check boxes, toggles, slider)" % len(MANIFEST))
