"""The "Field Ledger" UI kit as a Python library for the `ui build` specs and the asset scripts (editor-side; the C++ twin is UMOUITokens).

    from kit_lib import color, font, text, brush_9, icon_node, ...

One source of truth: DesignKit/MO57_UI_Kit/MO57_UI_Kit/tokens.json (+ nineslice/manifest.json). Specs call color("color.surface.1") / font("body") instead of writing numbers, so changing a token and
re-running the specs restyles every screen. The runtime C++ reads the SAME values from DT_UITokens (kit_import.py fills it from the same files).

Colours: the kit's hex values are sRGB; UMG colour properties are LINEAR. color() converts (alpha stays linear). `mo_ui._color` treats hex strings as already linear, so specs must pass these lists.
"""
import json
import os

KIT = r"D:\UEProjects\MO57\DesignKit\MO57_UI_Kit\MO57_UI_Kit"
KIT_UI = "/MOFramework/UI/Kit"

TOKENS = json.load(open(os.path.join(KIT, "tokens.json"), encoding="utf-8"))
PIECES = json.load(open(os.path.join(KIT, "nineslice", "manifest.json"), encoding="utf-8"))["pieces"]

# font faces: `font.face.<Family>.<Weight>` rows of DT_UITokens (kit_import.py writes them from this dict). STAND-INS until the kit fonts are imported (they need a yes to download).
FACES = {
    "Bitter.600": "/Engine/EngineFonts/Faces/RobotoBold.RobotoBold",
    "Bitter.700": "/Engine/EngineFonts/Faces/RobotoBold.RobotoBold",
    "IBMPlexSans.400": "/Engine/EngineFonts/Faces/RobotoRegular.RobotoRegular",
    "IBMPlexSans.500": "/MOFramework/UI/Kit/Fonts/FF_StandIn_RobotoMedium.FF_StandIn_RobotoMedium",
    "IBMPlexSans.600": "/Engine/EngineFonts/Faces/RobotoBold.RobotoBold",
    "IBMPlexMono.500": "/Engine/EngineFonts/Faces/DroidSansMono.DroidSansMono",
}


def _lin(c8):
    c = c8 / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def tok(name):
    return TOKENS[name]


def num(name):
    return float(TOKENS[name])


def color(token_or_hex, alpha=None):
    """[r, g, b, a] LINEAR from a token name (`color.accent`) or a hex string (`#RRGGBB[AA]`). alpha overrides the colour's own alpha."""
    hx = TOKENS[token_or_hex] if token_or_hex in TOKENS else token_or_hex
    if hx in TOKENS:   # token -> token (text.world.outline.color = color.hud.outline)
        hx = TOKENS[hx]
    h = hx.lstrip("#")
    r, g, b = (_lin(int(h[i:i + 2], 16)) for i in (0, 2, 4))
    a = int(h[6:8], 16) / 255.0 if len(h) >= 8 else 1.0
    return [r, g, b, a if alpha is None else alpha]


# One composite Font asset per family (kit_import.py builds them from FACES with UMOEditorContentUtils.MakeCompositeFont); Slate only resolves composite fonts, a bare FontFace gives tofu.
# Typefaces are named W<weight>.
FONT_ASSETS = {
    "Bitter": KIT_UI + "/Fonts/F_Bitter.F_Bitter",
    "IBMPlexSans": KIT_UI + "/Fonts/F_IBMPlexSans.F_IBMPlexSans",
    "IBMPlexMono": KIT_UI + "/Fonts/F_IBMPlexMono.F_IBMPlexMono",
}


def font_choice(family, weight):
    """(composite font asset path, typeface name) for a family and weight: the nearest weight the family has a face for (heavier on a tie), same rule as FMOUITokenSet::PickFontKey."""
    fam = family.replace(" ", "")
    best = None
    for key in FACES:
        f, w = key.rsplit(".", 1)
        if f != fam:
            continue
        w = int(w)
        d = abs(w - weight)
        if best is None or d < best[0] or (d == best[0] and w > best[1]):
            best = (d, w)
    return (FONT_ASSETS[fam], "W%d" % best[1]) if best else (None, None)


def font(role):
    """A FSlateFontInfo property dict for a type token (`body`, `title`, `numeric.large` ...), px x 0.75 sizes already in `umgSize`."""
    base = "type.%s." % role
    asset, typeface = font_choice(TOKENS[base + "font"], int(TOKENS[base + "weight"]))
    d = {"font_object": asset, "typeface_font_name": typeface, "size": int(TOKENS[base + "umgSize"])}
    ls = int(TOKENS.get(base + "letterSpacingUMG", 0))
    if ls:
        d["letter_spacing"] = ls
    return d


def px(role, what="lineHeight"):
    return float(TOKENS["type.%s.%s" % (role, what)])


def slot_rect(x, y, w, h):
    """Canvas slot offsets for a top-left anchored rect (left, top, width, height)."""
    return {"layout_data": {"offsets": {"left": float(x), "top": float(y), "right": float(w), "bottom": float(h)}}}


def slot_anchored(anchor_min, anchor_max, left=0.0, top=0.0, right=0.0, bottom=0.0, align=(0.0, 0.0), auto_size=False):
    s = {"layout_data": {"anchors": {"minimum": {"x": anchor_min[0], "y": anchor_min[1]}, "maximum": {"x": anchor_max[0], "y": anchor_max[1]}},
                         "offsets": {"left": float(left), "top": float(top), "right": float(right), "bottom": float(bottom)},
                         "alignment": {"x": align[0], "y": align[1]}}}
    if auto_size:
        s["auto_size"] = True
    return s


def brush_9(piece, tint=None, alpha=None):
    """FSlateBrush property dict for a kit 9-slice piece, tinted by a token / hex. Draw mode and margins come from the manifest."""
    p = PIECES[piece]
    m = p["umgMargin"]
    b = {"resource_object": "%s/NineSlice/T_UI_%s" % (KIT_UI, piece), "draw_as": p["drawAs"], "margin": [m["Left"], m["Top"], m["Right"], m["Bottom"]],
         "image_size": {"x": float(p["size1x"][0]), "y": float(p["size1x"][1])}}
    if tint is not None:
        b["tint_color"] = {"specified_color": color(tint, alpha)}
    return b


def brush_icon(icon, tint=None, size=24):
    """FSlateBrush for a kit icon (`ic_moodle_cold`): the white glyph, tinted by a token."""
    b = {"resource_object": "%s/Icons/T_%s" % (KIT_UI, icon), "draw_as": "Image", "image_size": {"x": float(size), "y": float(size)}}
    if tint is not None:
        b["tint_color"] = {"specified_color": color(tint)}
    return b


# ------------------------------------------------------------------------------------------------ spec node builders
def text(name, role, value, tint="color.text.primary", justification=None, wrap=False, slot=None, var=None, **props):
    """A TextBlock node set up from a type token."""
    p = {"font": font(role), "color_and_opacity": {"specified_color": color(tint)}}
    if justification:
        p["justification"] = justification
    if wrap:
        p["auto_wrap_text"] = True
    p.update(props)
    node = {"type": "TextBlock", "name": name, "text": value, "props": p}
    if slot:
        node["slot"] = slot
    if var is not None:
        node["var"] = var
    return node


def image9(name, piece, tint, alpha=None, slot=None, var=None, **props):
    p = {"brush": brush_9(piece, tint, alpha)}
    p.update(props)
    node = {"type": "Image", "name": name, "props": p}
    if slot:
        node["slot"] = slot
    if var is not None:
        node["var"] = var
    return node


def icon_node(name, icon, tint="color.text.primary", size=24, slot=None, var=None):
    node = {"type": "Image", "name": name, "props": {"brush": brush_icon(icon, tint, size)}}
    if slot:
        node["slot"] = slot
    if var is not None:
        node["var"] = var
    return node


def fill_slot():
    """Overlay / border child slot that fills its parent."""
    return {"horizontal_alignment": "H_ALIGN_FILL", "vertical_alignment": "V_ALIGN_FILL"}


def panel(name, elev, children, w=None, h=None, slot=None, padding=None):
    """The kit panel: [shadow (elev 2 / modal)] + fill + 1 px stroke stacked in an Overlay, content on top. Returns a node."""
    fill_tok = {0: "color.surface.0", 1: "color.surface.1", 2: "color.surface.2", 3: "color.surface.3"}[elev if elev != "modal" else 1]
    layers = []
    if elev == 2:
        layers.append(image9(name + "_Shadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **fill_slot()}))
    if elev == "modal":
        layers.append(image9(name + "_Shadow", "panel_shadow", "#000000", 0.7, slot={"padding": -16, **fill_slot()}))
    layers.append(image9(name + "_Fill", "panel_fill", fill_tok, slot=fill_slot()))
    layers.append(image9(name + "_Stroke", "modal_rule" if elev == "modal" else "panel_stroke", "color.border.strong" if elev == "modal" else "color.border.subtle", slot=fill_slot()))
    content = {"type": "Overlay", "name": name + "_Content", "slot": {**fill_slot(), **({"padding": padding} if padding is not None else {})}, "children": children}
    node = {"type": "Overlay", "name": name, "children": layers + [content]}
    if slot:
        node["slot"] = slot
    if w is not None or h is not None:
        sized = {"type": "SizeBox", "name": name + "_Size", "children": [node]}
        if w is not None:
            sized["width"] = w
        if h is not None:
            sized["height"] = h
        if slot:
            sized["slot"] = node.pop("slot")
        return sized
    return node
