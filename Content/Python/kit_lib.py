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
# baked composites (Tools/kit_bake.py writes the manifest): piece -> {size (2x px), margin (UMG 0..1), draw, folder}
BAKED = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "kit_baked_manifest.json"), encoding="utf-8"))

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


def baked_brush(piece, tint=None):
    """FSlateBrush property dict for a baked piece (a button / row / tab state, an input well, a check box, a toggle, a slider part)."""
    b = BAKED[piece]
    d = {"resource_object": "%s/%s/%s" % (KIT_UI, b["folder"], piece), "draw_as": b["draw"], "margin": b["margin"], "image_size": {"x": b["size"][0] / 2.0, "y": b["size"][1] / 2.0}}
    if tint is not None:
        d["tint_color"] = {"specified_color": color(tint)}
    return d


def brush_icon(icon, tint=None, size=24):
    """FSlateBrush for a kit icon (`ic_moodle_cold`): the white glyph, tinted by a token."""
    b = {"resource_object": "%s/Icons/T_%s" % (KIT_UI, icon), "draw_as": "Image", "image_size": {"x": float(size), "y": float(size)}}
    if tint is not None:
        b["tint_color"] = {"specified_color": color(tint)}
    return b


# ------------------------------------------------------------------------------------------------ spec node builders
def text(name, role, value, tint="color.text.primary", justification=None, wrap=False, slot=None, var=None, kind="TextBlock", **props):
    """A TextBlock (or kind="CommonTextBlock") node set up from a type token."""
    p = {"font": font(role), "color_and_opacity": {"specified_color": color(tint)}}
    if justification:
        p["justification"] = justification
    if wrap:
        p["auto_wrap_text"] = True
    p.update(props)
    node = {"type": kind, "name": name, "text": value, "props": p}
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


_ANCHORS = {"Left": 0.0, "Center": 0.5, "Right": 1.0, "Top": 0.0, "Bottom": 1.0}


def place(x, y, w, h, anchor="TopLeft", auto_height=False):
    """Canvas slot for a kit rect (1920x1080 coordinates) with the kit's anchor name (TopLeft, TopCenter, TopRight, Left, Center, Right, BottomLeft, BottomCenter, BottomRight).
    Point anchor + alignment, so the widget keeps its margin to that edge at any resolution (the canvas scales with DPI)."""
    v = "Top" if anchor.startswith("Top") else "Bottom" if anchor.startswith("Bottom") else "Center"
    hz = "Left" if anchor.endswith("Left") else "Right" if anchor.endswith("Right") else "Center"
    ax, ay = _ANCHORS[hz], _ANCHORS[v]
    ox = x + w * ax - 1920.0 * ax
    oy = y + h * ay - 1080.0 * ay
    return slot_anchored((ax, ay), (ax, ay), left=ox, top=oy, right=w, bottom=h, align=(ax, ay), auto_size=auto_height)


def solid(name, token, w=None, h=None, slot=None, alpha=None):
    """A flat colour rectangle (a Border with a brush colour) of a token, optionally sized."""
    node = {"type": "Border", "name": name, "props": {"brush_color": color(token, alpha)}}
    if w is not None or h is not None:
        node = {"type": "SizeBox", "name": name + "Size", **({"width": w} if w is not None else {}), **({"height": h} if h is not None else {}), "children": [node]}
    if slot:
        node["slot"] = slot
    return node


def progress_bar(name, fill_token="color.data.fill", fill_piece="progress_fill", var=True, slot=None, **props):
    """A ProgressBar styled with the kit track + fill pieces (8 px high when wrapped in a SizeBox by the caller)."""
    p = {"widget_style": {"background_image": brush_9("progress_track", "color.data.track"), "fill_image": brush_9(fill_piece, fill_token)}}
    p.update(props)
    node = {"type": "ProgressBar", "name": name, "props": p}
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
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


# ------------------------------------------------------------------------------------------------ native controls in the kit look
def btn_style(variant):
    """Path of a kit CommonButtonStyle class: Primary / Secondary / Tertiary / Danger [Lg], Icon, IconTertiary, Tab, Row."""
    return "%s/Styles/BS_%s.BS_%s_C" % (KIT_UI, variant, variant)


def button(name, label, variant="Secondary", slot=None, var=True, **props):
    """An MOButton with a kit style. Size the slot (44 / 56 high)."""
    p = {"style": btn_style(variant)}
    p.update(props)
    node = {"type": "MOButton", "name": name, "label": label, "props": p}
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
    return node


def checkbox(name, toggle=False, var=True, slot=None, **props):
    """A CheckBox (24 px box, or a 52 x 28 toggle) with the baked kit images."""
    t_ = "Tgl" if toggle else "Chk"
    sz = (52.0, 28.0) if toggle else (24.0, 24.0)

    def img(state):
        b = baked_brush("T_%s_%s" % (t_, state))
        b["image_size"] = {"x": sz[0], "y": sz[1]}
        return b
    style = {"unchecked_image": img("off"), "unchecked_hovered_image": img("off_hover"), "unchecked_pressed_image": img("off_hover"),
             "checked_image": img("on"), "checked_hovered_image": img("on_hover"), "checked_pressed_image": img("on_hover"),
             "undetermined_image": img("off"), "undetermined_hovered_image": img("off_hover"), "undetermined_pressed_image": img("off_hover"),
             "background_image": {}, "padding": [0, 0, 0, 0]}
    p = {"widget_style": style}
    p.update(props)
    node = {"type": "CheckBox", "name": name, "props": p}
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
    return node


def slider(name, var=True, slot=None, **props):
    """A Slider: 6 px track, 20 x 28 knob (accent when hovered)."""
    style = {"normal_bar_image": baked_brush("T_Sld_bar"), "hovered_bar_image": baked_brush("T_Sld_bar"), "disabled_bar_image": baked_brush("T_Sld_bar"),
             "normal_thumb_image": {**baked_brush("T_Sld_thumb"), "image_size": {"x": 20.0, "y": 28.0}}, "hovered_thumb_image": {**baked_brush("T_Sld_thumb_hover"), "image_size": {"x": 20.0, "y": 28.0}},
             "disabled_thumb_image": {**baked_brush("T_Sld_thumb"), "image_size": {"x": 20.0, "y": 28.0}}, "bar_thickness": 6.0}
    p = {"widget_style": style, "slider_bar_color": [1, 1, 1, 1], "slider_handle_color": [1, 1, 1, 1]}
    p.update(props)
    node = {"type": "Slider", "name": name, "props": p}
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
    return node


def _input_style():
    f = font("body")
    return {"background_image_normal": baked_brush("T_Input_normal"), "background_image_hovered": baked_brush("T_Input_normal"), "background_image_focused": baked_brush("T_Input_focused"),
            "background_image_read_only": baked_brush("T_Input_disabled"), "padding": [12, 8, 12, 10], "foreground_color": {"specified_color": color("color.text.primary")},
            "focused_foreground_color": {"specified_color": color("color.text.primary")}, "read_only_foreground_color": {"specified_color": color("color.text.disabled")},
            "text_style": {"font": f, "color_and_opacity": {"specified_color": color("color.text.primary")}}}


def text_input(name, hint="", multiline=False, var=True, slot=None, **props):
    """An (optionally multi-line) editable text box in the kit input look (surface.0 well, ruled bottom edge, accent when focused)."""
    p = {"widget_style": _input_style(), "hint_text": hint}
    p.update(props)
    node = {"type": "MultiLineEditableTextBox" if multiline else "EditableTextBox", "name": name, "props": p}
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
    return node


def solid_brush(tint, alpha=None):
    """A flat colour FSlateBrush (no texture)."""
    return {"draw_as": "Box", "tint_color": {"specified_color": color(tint, alpha)}}


def combo(name, var=True, slot=None, **props):
    """A ComboBoxString in the kit input look; the dropdown rows use the kit row pieces."""
    btn = {"normal": baked_brush("T_Input_normal"), "hovered": baked_brush("T_Input_normal"), "pressed": baked_brush("T_Input_focused"), "disabled": baked_brush("T_Input_disabled"),
           "normal_padding": [12, 8, 12, 10], "pressed_padding": [12, 8, 12, 10]}
    style = {"combo_button_style": {"button_style": btn, "down_arrow_image": brush_icon("ic_ui_arrow_down", "color.text.secondary", 20), "menu_border_brush": brush_9("panel_fill", "color.surface.3"),
                                    "menu_border_padding": 4, "content_padding": [0, 0, 0, 0], "down_arrow_padding": [0, 0, 12, 0]},
             "content_padding": [0, 0, 0, 0], "menu_row_padding": [0, 2, 0, 2]}
    item = {"active_brush": baked_brush("T_Row_selected"), "active_hovered_brush": baked_brush("T_Row_hovered"), "inactive_brush": baked_brush("T_Row_selected"), "inactive_hovered_brush": baked_brush("T_Row_hovered"),
            "even_row_background_brush": baked_brush("T_Row_normal"), "odd_row_background_brush": baked_brush("T_Row_normal"),
            "even_row_background_hovered_brush": baked_brush("T_Row_hovered"), "odd_row_background_hovered_brush": baked_brush("T_Row_hovered"),
            "text_color": {"specified_color": color("color.text.primary")}, "selected_text_color": {"specified_color": color("color.text.primary")}}
    p = {"widget_style": style, "item_style": item, "font": font("body"), "foreground_color": {"specified_color": color("color.text.primary")}, "content_padding": [0, 0, 0, 0]}
    p.update(props)
    node = {"type": "ComboBoxString", "name": name, "props": p}
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
    return node


def spin(name, var=True, slot=None, **props):
    """A SpinBox in the kit input look."""
    clear = {"tint_color": {"specified_color": [0, 0, 0, 0]}}
    style = {"background_brush": baked_brush("T_Input_normal"), "hovered_background_brush": baked_brush("T_Input_normal"), "active_background_brush": baked_brush("T_Input_focused"),
             "active_fill_brush": clear, "hovered_fill_brush": clear, "inactive_fill_brush": clear, "text_padding": [12, 6, 12, 8]}
    p = {"widget_style": style, "font": font("numeric"), "foreground_color": {"specified_color": color("color.text.primary")}}
    p.update(props)
    node = {"type": "SpinBox", "name": name, "props": p}
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
    return node


def scrollbar_style():
    """FScrollBarStyle properties: a 6 px thumb that brightens on hover / drag, no track."""
    return {"thickness": 6.0, "normal_thumb_image": solid_brush("color.border.strong"), "hovered_thumb_image": solid_brush("color.text.secondary"), "dragged_thumb_image": solid_brush("color.accent"),
            "horizontal_background_image": solid_brush("color.surface.0", 0.0), "vertical_background_image": solid_brush("color.surface.0", 0.0),
            "horizontal_top_slot_image": solid_brush("color.surface.0", 0.0), "horizontal_bottom_slot_image": solid_brush("color.surface.0", 0.0),
            "vertical_top_slot_image": solid_brush("color.surface.0", 0.0), "vertical_bottom_slot_image": solid_brush("color.surface.0", 0.0)}


def scroll_box(name, children=None, var=True, slot=None, **props):
    p = {"widget_bar_style": scrollbar_style()}
    p.update(props)
    node = {"type": "ScrollBox", "name": name, "props": p}
    if children:
        node["children"] = children
    if var is not None:
        node["var"] = var
    if slot:
        node["slot"] = slot
    return node


def divider(name, tint="color.border.subtle", slot=None):
    """A 1 px horizontal rule."""
    return solid(name, tint, h=1, slot=slot)


def section_label(name, value, slot=None):
    """A section heading inside a panel: type.label (caps, tracked), secondary colour."""
    return text(name, "label", value.upper(), "color.text.secondary", slot=slot)


def row(name, label, control, label_w=440, h=56, label_name=None, extra=None, var=None):
    """A settings row: body label in a fixed column, then the control (and optional extra widgets such as a value readout)."""
    kids = [{"type": "SizeBox", "name": name + "LabelBox", "width": label_w, "slot": {"vertical_alignment": "V_ALIGN_CENTER"},
             "children": [text(label_name or name + "Label", "body", label, "color.text.primary", var=var)]},
            control]
    if extra:
        kids.extend(extra)
    return {"type": "SizeBox", "name": name + "RowBox", "height": h, "children": [{"type": "HorizontalBox", "name": name + "Row", "children": kids}]}
