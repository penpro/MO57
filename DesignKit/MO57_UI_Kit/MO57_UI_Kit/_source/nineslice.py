"""MO57 9-slice kit. Every texture is WHITE (+alpha) so it is tinted by tokens at runtime.
Panels are drawn as separate layers (fill / stroke / shadow) stacked in one UMG Overlay, so a
state change is a tint change, never a texture swap. Grain lives in the fill's luminance only,
240-255 (<= 6% variation, under the 8% body-text rule).
Also writes baked/ state previews (tinted) for reference and for quick brush import."""
import os, sys, json, random
from PIL import Image, ImageDraw, ImageFilter

OUT = (sys.argv[1] if len(sys.argv) > 1 else "/mnt/user-data/outputs/MO57_UI_Kit") + "/nineslice"
TOK = json.load(open((sys.argv[1] if len(sys.argv) > 1 else "/mnt/user-data/outputs/MO57_UI_Kit") + "/tokens.json"))
SS = 4  # supersample
random.seed(57)

def canvas(w, h, scale):
    return Image.new("RGBA", (w * scale * SS, h * scale * SS), (255, 255, 255, 0))

def down(im, w, h, scale):
    return im.resize((w * scale, h * scale), Image.LANCZOS)

def rrect(d, box, r, fill=None, outline=None, width=0):
    d.rounded_rectangle(box, radius=r, fill=fill, outline=outline, width=width)

PIECES = {}

def piece(name, w, h, margin, desc):
    def deco(fn):
        PIECES[name] = (w, h, margin, desc, fn)
        return fn
    return deco

def k(v, scale):  # geometry units -> supersampled px
    return int(round(v * scale * SS))

@piece("panel_fill", 32, 32, (4, 4, 4, 4), "Panel/card/tooltip body. Tint: color.surface.N. Grain baked in luminance (<=6%).")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(2, scale), fill=(255, 255, 255, 255))
    return im

@piece("panel_stroke", 32, 32, (4, 4, 4, 4), "1px panel edge. Tint: color.border.subtle (elev 0/1) or border.strong (inputs).")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(2, scale), outline=(255, 255, 255, 255), width=k(1, scale))
    return im

@piece("panel_shadow", 64, 64, (24, 24, 24, 24), "Soft drop shadow, draw 16px outside the panel rect (padding -16). Tint black; alpha 0.5 elev 2, 0.7 modal.")
def _(scale):
    im = canvas(64, 64, scale); d = ImageDraw.Draw(im)
    rrect(d, (k(16, scale), k(18, scale), k(48, scale), k(50, scale)), k(4, scale), fill=(255, 255, 255, 255))
    return im.filter(ImageFilter.GaussianBlur(k(6, scale)))

@piece("modal_rule", 32, 32, (6, 6, 6, 6), "Ledger double rule for modals: 1px outer + 1px inner at 3px inset. Tint: border.strong.")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(2, scale), outline=(255, 255, 255, 255), width=k(1, scale))
    rrect(d, (k(3, scale), k(3, scale), k(29, scale) - 1, k(29, scale) - 1), 0, outline=(255, 255, 255, 140), width=k(1, scale))
    return im

@piece("button_fill", 32, 32, (4, 4, 4, 4), "Button body (all variants). Tint per state from tokens; no grain (clean hit area).")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(2, scale), fill=(255, 255, 255, 255))
    return im

@piece("button_stroke", 32, 32, (4, 4, 4, 4), "Button edge 1px (secondary/tertiary). Tint border.strong; selected = accent.")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(2, scale), outline=(255, 255, 255, 255), width=k(1, scale))
    return im

@piece("focus_ring", 40, 40, (8, 8, 8, 8), "Focus ring: draw at padding -4 around ANY focusable. 2px ring, 2px gap. Tint color.focus.ring. Never clipped: put it in the widget's own Overlay, scroll boxes need 4px inner padding.")
def _(scale):
    im = canvas(40, 40, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(40, scale) - 1, k(40, scale) - 1), k(4, scale), outline=(255, 255, 255, 255), width=k(2, scale))
    return im

@piece("input_fill", 32, 32, (4, 4, 4, 4), "Text input / dropdown / spin box body. Tint surface.0 (wells sit below the panel).")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(2, scale), fill=(255, 255, 255, 255))
    return im

@piece("input_stroke", 32, 32, (4, 4, 4, 6), "Input edge: 1px all round + 2px bottom rule (ruled-ledger line). Tint border.strong / accent (focus) / severity.critical (error).")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(2, scale), outline=(255, 255, 255, 255), width=k(1, scale))
    d.rectangle((0, k(30, scale), k(32, scale), k(32, scale)), fill=(255, 255, 255, 255))
    return im

@piece("slot_fill", 64, 64, (8, 8, 8, 8), "Inventory slot body. Tint surface.2; hover +hover.overlay; valid drop accent.muted; invalid critical @ 18%.")
def _(scale):
    im = canvas(64, 64, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(64, scale) - 1, k(64, scale) - 1), k(2, scale), fill=(255, 255, 255, 255))
    return im

@piece("slot_ticks", 64, 64, (12, 12, 12, 12), "Ledger corner ticks (8px L marks). Empty slot = ticks only at border.strong; selected = accent; equipped = text.secondary.")
def _(scale):
    im = canvas(64, 64, scale); d = ImageDraw.Draw(im)
    W = k(64, scale); L = k(8, scale); T = k(2, scale)
    wh = (255, 255, 255, 255)
    for x0 in (0, W - L):
        for y0 in (0, W - T):
            d.rectangle((x0, y0, x0 + L - 1, y0 + T - 1), fill=wh)  # horizontal arms
    for x0 in (0, W - T):
        for y0 in (0, W - L):
            d.rectangle((x0, y0, x0 + T - 1, y0 + L - 1), fill=wh)  # vertical arms
    d.rectangle((0, 0, W - 1, W - 1), outline=(255, 255, 255, 70), width=k(1, scale))
    return im

@piece("slot_hatch", 64, 64, (0, 0, 0, 0), "Diagonal hatch overlay (Tile, not Box). Damaged = warning @ 35%; spoiled = critical @ 35%; locked = text.disabled @ 25%. Carries state without colour.")
def _(scale):
    im = canvas(64, 64, scale); d = ImageDraw.Draw(im)
    W = k(64, scale)
    for i in range(-64, 128, 8):
        d.line([(k(i, scale), W), (k(i + 64, scale), 0)], fill=(255, 255, 255, 255), width=k(1.5, scale))
    return im

@piece("tab_indicator", 16, 4, (4, 0, 4, 0), "Selected-tab underline, 4px high under the tab label. Tint accent.")
def _(scale):
    im = canvas(16, 4, scale); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, k(16, scale), k(4, scale)), fill=(255, 255, 255, 255))
    return im

@piece("progress_track", 16, 8, (3, 3, 3, 3), "Progress / meter track. Tint data.track.")
def _(scale):
    im = canvas(16, 8, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(16, scale) - 1, k(8, scale) - 1), k(1, scale), fill=(255, 255, 255, 255))
    return im

@piece("progress_fill", 16, 8, (3, 3, 3, 3), "Progress fill (use as UMG ProgressBar FillImage, Box draw). Tint data.fill or severity.")
def _(scale):
    im = canvas(16, 8, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(16, scale) - 1, k(8, scale) - 1), k(1, scale), fill=(255, 255, 255, 255))
    return im

@piece("progress_marker", 4, 16, (0, 0, 0, 0), "Threshold marker (2px line + 1px halo) drawn 4px taller than the bar. Tint text.primary.")
def _(scale):
    im = canvas(4, 16, scale); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, k(4, scale), k(16, scale)), fill=(255, 255, 255, 60))
    d.rectangle((k(1, scale), 0, k(3, scale), k(16, scale)), fill=(255, 255, 255, 255))
    return im

@piece("critical_border", 32, 32, (8, 8, 8, 8), "Critical double border (2px, 2px gap, 2px). Draw at padding -6. Tint severity.critical; pulse alpha 1->0.55 over motion.pulse.critical, static when reduced-motion.")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(4, scale), outline=(255, 255, 255, 255), width=k(2, scale))
    rrect(d, (k(4, scale), k(4, scale), k(28, scale) - 1, k(28, scale) - 1), k(2, scale), outline=(255, 255, 255, 255), width=k(2, scale))
    return im

@piece("keycap_fill", 32, 32, (8, 6, 8, 10), "Key-cap body with 3px lower lip. Tint text.primary; label text.inverse. Min height 32, min width 32.")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(4, scale), fill=(255, 255, 255, 170))
    rrect(d, (0, 0, k(32, scale) - 1, k(29, scale) - 1), k(4, scale), fill=(255, 255, 255, 255))
    return im

@piece("hud_plate", 32, 32, (6, 6, 6, 6), "Backing plate for ALL HUD text over the world. Tint color.hud.plate (82%). No grain.")
def _(scale):
    im = canvas(32, 32, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(32, scale) - 1, k(32, scale) - 1), k(3, scale), fill=(255, 255, 255, 255))
    return im

@piece("divider", 8, 1, (0, 0, 0, 0), "1px rule. Tile horizontally. Tint border.subtle (panel) / paper.rule (paper).")
def _(scale):
    im = canvas(8, 1, scale); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, k(8, scale), k(1, scale)), fill=(255, 255, 255, 255))
    return im

@piece("portrait_frame", 64, 64, (6, 6, 6, 6), "Pawn portrait frame, 2px. Tint border.strong; selected accent; critical uses critical_border on top.")
def _(scale):
    im = canvas(64, 64, scale); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, k(64, scale) - 1, k(64, scale) - 1), k(2, scale), outline=(255, 255, 255, 255), width=k(2, scale))
    return im


def grain(im, scale):
    px = im.load(); w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a:
                v = 255 - random.randint(0, 15)  # 240..255
                px[x, y] = (v, v, v, a)
    return im

def hexrgba(hx):
    hx = hx.lstrip('#'); a = int(hx[6:8], 16) if len(hx) == 8 else 255
    return tuple(int(hx[i:i + 2], 16) for i in (0, 2, 4)) + (a,)

def tint(im, hx):
    r, g, b, a = hexrgba(hx)
    out = Image.new("RGBA", im.size)
    src = im.load(); dst = out.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            pr, pg, pb, pa = src[x, y]
            dst[x, y] = (r * pr // 255, g * pg // 255, b * pb // 255, pa * a // 255)
    return out

manifest = {"note": "All textures white; tint from tokens. UMG Brush: DrawAs=Box, Margin = px/texture size (given). Stack fill+stroke(+shadow) in an Overlay.", "pieces": {}}
made = {}
for scale in (1, 2):
    d = f"{OUT}/{scale}x"; os.makedirs(d, exist_ok=True)
    for name, (w, h, m, desc, fn) in PIECES.items():
        im = down(fn(scale), w, h, scale)
        if name == "panel_fill":
            im = grain(im, scale)
        fn_ = f"T_UI_{name}{'' if scale == 1 else '@2x'}.png"
        im.save(f"{d}/{fn_}")
        made[(name, scale)] = im
        if scale == 1:
            L, T, R, B = m
            manifest["pieces"][name] = {"file1x": f"1x/{fn_}", "file2x": f"2x/T_UI_{name}@2x.png", "size1x": [w, h],
                                        "marginPx": {"left": L, "top": T, "right": R, "bottom": B},
                                        "umgMargin": {"Left": round(L / w, 4), "Top": round(T / h, 4), "Right": round(R / w, 4), "Bottom": round(B / h, 4)},
                                        "drawAs": "Image" if name in ("slot_hatch", "progress_marker") else ("Tile" if name == "divider" else "Box"),
                                        "usage": desc}
json.dump(manifest, open(f"{OUT}/manifest.json", "w"), indent=2)

# baked previews (2x) of composite states for reference
B = f"{OUT}/baked_preview"; os.makedirs(B, exist_ok=True)
def comp(w, h, layers):
    base = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for name, color, pad in layers:
        src = made[(name, 2)]
        sw, sh = src.size
        L, T, R, Bm = [v * 2 for v in PIECES[name][2].__iter__()]
        tw, th = w - 2 * pad, h - 2 * pad
        if L + R == 0:
            tile = src.resize((tw, th))
        else:
            tile = Image.new("RGBA", (tw, th))
            xs = [(0, L, 0, L), (L, sw - R, L, tw - R), (sw - R, sw, tw - R, tw)]
            ys = [(0, T, 0, T), (T, sh - Bm, T, th - Bm), (sh - Bm, sh, th - Bm, th)]
            for sx0, sx1, dx0, dx1 in xs:
                for sy0, sy1, dy0, dy1 in ys:
                    if dx1 > dx0 and dy1 > dy0 and sx1 > sx0 and sy1 > sy0:
                        tile.paste(src.crop((sx0, sy0, sx1, sy1)).resize((dx1 - dx0, dy1 - dy0)), (dx0, dy0))
        base.alpha_composite(tint(tile, color), (pad, pad))
    return base

t = lambda key: TOK[key]
states = {
    "button_primary_normal": [("button_fill", t("color.accent"), 4)],
    "button_primary_hover": [("button_fill", t("color.accent.hover"), 4)],
    "button_primary_pressed": [("button_fill", t("color.accent.pressed"), 4)],
    "button_primary_focused": [("button_fill", t("color.accent"), 4), ("focus_ring", t("color.focus.ring"), 0)],
    "button_primary_disabled": [("button_fill", t("color.surface.3"), 4)],
    "button_secondary_normal": [("button_fill", t("color.surface.3"), 4), ("button_stroke", t("color.border.strong"), 4)],
    "button_secondary_hover": [("button_fill", t("color.surface.4"), 4), ("button_stroke", t("color.border.strong"), 4)],
    "button_secondary_selected": [("button_fill", "#24302B", 4), ("button_stroke", t("color.accent"), 4)],
    "button_danger_normal": [("button_fill", t("color.severity.critical.fill"), 4)],
    "panel_elev0": [("panel_fill", t("color.surface.0"), 0)],
    "panel_elev1": [("panel_fill", t("color.surface.1"), 0), ("panel_stroke", t("color.border.subtle"), 0)],
    "panel_elev2": [("panel_shadow", "#00000080", 0), ("panel_fill", t("color.surface.2"), 32), ("panel_stroke", t("color.border.subtle"), 32)],
    "modal": [("panel_shadow", "#000000B3", 0), ("panel_fill", t("color.surface.1"), 32), ("modal_rule", t("color.border.strong"), 32)],
    "input_normal": [("input_fill", t("color.surface.0"), 0), ("input_stroke", t("color.border.strong"), 0)],
    "input_focused": [("input_fill", t("color.surface.0"), 4), ("input_stroke", t("color.accent"), 4), ("focus_ring", t("color.focus.ring"), 0)],
    "input_error": [("input_fill", t("color.surface.0"), 0), ("input_stroke", t("color.severity.critical"), 0)],
    "slot_empty": [("slot_fill", t("color.surface.2"), 0), ("slot_ticks", t("color.border.strong"), 0)],
    "slot_selected": [("slot_fill", "#26302C", 0), ("slot_ticks", t("color.accent"), 0)],
    "slot_damaged": [("slot_fill", t("color.surface.2"), 0), ("slot_hatch", "#E7A04259", 0), ("slot_ticks", t("color.border.strong"), 0)],
    "slot_spoiled": [("slot_fill", t("color.surface.2"), 0), ("slot_hatch", "#EE655059", 0), ("slot_ticks", t("color.border.strong"), 0)],
    "tooltip": [("panel_shadow", "#00000080", 0), ("panel_fill", t("color.surface.3"), 32), ("panel_stroke", t("color.border.strong"), 32)],
    "hud_plate": [("hud_plate", t("color.hud.plate"), 0)],
    "keycap": [("keycap_fill", t("color.text.primary"), 0)],
    "critical_border": [("critical_border", t("color.severity.critical"), 0)],
}
sizes = {"button": (400, 112), "panel": (480, 320), "modal": (560, 360), "input": (400, 96), "slot": (128, 128),
         "tooltip": (440, 200), "hud": (360, 80), "keycap": (64, 64), "critical": (160, 160)}
for name, layers in states.items():
    w, h = next(v for kk, v in sizes.items() if name.startswith(kk))
    comp(w, h, layers).save(f"{B}/{name}@2x.png")
print(len(PIECES), "pieces;", len(states), "baked previews")
