"""Create / refresh the kit's CommonUI style assets (Blueprint classes of UCommonButtonStyle / UCommonTextStyle) from tokens.

    python Tools/ue.py py --file Content/Python/kit_styles.py        (editor open, not in PIE, no `inst` running; after kit_import.py and `python Tools/kit_bake.py`)

  /MOFramework/UI/Kit/Styles/TS_Btn_{Ink,Text,TextDim,Disabled}      button label text styles (type.body.strong)
  /MOFramework/UI/Kit/Styles/TS_Row_{Text,Disabled}, TS_Tab_{Dim,Text}  list-row (type.body) and tab (type.body.strong) label styles
  /MOFramework/UI/Kit/Styles/BS_{Primary,Secondary,Tertiary,Danger}[Lg]  buttons: one baked 9-slice per state (Saved/KitBake/Buttons), 44 px high (Lg = 56), padding 20 (Lg 28)
  /MOFramework/UI/Kit/Styles/BS_Icon, BS_IconTertiary                 44 x 44 icon buttons (no padding)
  /MOFramework/UI/Kit/Styles/BS_Tab                                    48 high tab: transparent, white-6 % hover, 4 px accent underline when selected
  /MOFramework/UI/Kit/Styles/BS_Row                                    48 high list row: surface.2, hover surface.3, selected = accent wash + accent edge

The existing /MOFramework/UI/MOCommonButtonStyle (every MOButton's default) is re-pointed at the secondary look, so every existing button restyles at once.
"""
import unreal

import kit_lib as K

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
STYLES = K.KIT_UI + "/Styles"


def ensure_bp(folder, name, parent):
    path = "%s/%s" % (folder, name)
    bp = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if not bp:
        fac = unreal.BlueprintFactory()
        fac.set_editor_property("parent_class", parent)
        bp = AT.create_asset(name, folder, unreal.Blueprint, fac)
    return bp


def cdo_of(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return unreal.get_default_object(bp.generated_class())


def text_style(name, tint, role="body.strong"):
    bp = ensure_bp(STYLES, name, unreal.CommonTextStyle)
    c = cdo_of(bp)
    f = c.get_editor_property("font")
    spec = K.font(role)
    f.set_editor_property("font_object", unreal.load_asset(spec["font_object"]))
    f.set_editor_property("typeface_font_name", unreal.Name(spec["typeface_font_name"]))
    f.set_editor_property("size", spec["size"])
    c.set_editor_property("font", f)
    c.set_editor_property("color", unreal.LinearColor(*K.color(tint)))
    c.set_editor_property("uses_drop_shadow", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, False)
    return bp


def brush(folder, piece):
    """A box brush on a baked texture (white tint: the colour is baked in); margins come from the bake manifest."""
    b = unreal.SlateBrush()
    m = K.BAKED[piece]
    b.set_editor_property("resource_object", unreal.load_asset("%s/%s/%s" % (K.KIT_UI, folder, piece)))
    b.set_editor_property("draw_as", unreal.SlateBrushDrawType.BOX)
    b.set_editor_property("margin", unreal.Margin(*m["margin"]))
    sz = unreal.DeprecateSlateVector2D()
    sz.set_editor_property("x", float(m["size"][0]) / 2.0)
    sz.set_editor_property("y", float(m["size"][1]) / 2.0)
    b.set_editor_property("image_size", sz)
    b.set_editor_property("tint_color", unreal.SlateColor(unreal.LinearColor(1, 1, 1, 1)))
    return b


def bare_brush():
    """A brush that draws nothing (list cards that paint their own content: the button is only the click target)."""
    b = unreal.SlateBrush()
    b.set_editor_property("draw_as", unreal.SlateBrushDrawType.NO_DRAW_TYPE)
    return b


def bare_style(name):
    """BS_Bare: every state transparent, no padding, no minimum size. The default style of a bare UCommonButtonBase draws a grey box around the content (the save cards showed it)."""
    bp = ensure_bp(STYLES, name, unreal.CommonButtonStyle)
    c = cdo_of(bp)
    for slot in ("normal_base", "normal_hovered", "normal_pressed", "selected_base", "selected_hovered", "selected_pressed", "disabled"):
        c.set_editor_property(slot, bare_brush())
    zero = unreal.Margin(0, 0, 0, 0)
    c.set_editor_property("button_padding", zero)
    c.set_editor_property("custom_padding", zero)
    for k in ("min_width", "min_height", "max_width", "max_height"):
        c.set_editor_property(k, 0)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, False)
    return bp


def button_style(name, folder, prefix, states, text_normal, text_hover, text_selected, text_disabled, height=44, pad_x=20, min_width=44, existing=None):
    """`states` maps the CommonButtonStyle brush slot to the baked piece suffix; prefix e.g. "T_Btn_primary_"."""
    bp = existing or ensure_bp(STYLES, name, unreal.CommonButtonStyle)
    c = cdo_of(bp)
    for slot, suffix in states.items():
        c.set_editor_property(slot, brush(folder, prefix + suffix))
    c.set_editor_property("button_padding", unreal.Margin(pad_x, 0, pad_x, 0))
    c.set_editor_property("custom_padding", unreal.Margin(0, 0, 0, 0))
    c.set_editor_property("min_height", height)
    c.set_editor_property("max_height", 0)
    c.set_editor_property("min_width", min_width)
    c.set_editor_property("max_width", 0)
    c.set_editor_property("normal_text_style", text_normal.generated_class())
    c.set_editor_property("normal_hovered_text_style", text_hover.generated_class())
    c.set_editor_property("selected_text_style", text_selected.generated_class())
    c.set_editor_property("selected_hovered_text_style", text_selected.generated_class())
    c.set_editor_property("disabled_text_style", text_disabled.generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, False)
    return bp


SOLID = {"normal_base": "normal", "normal_hovered": "hovered", "normal_pressed": "pressed", "selected_base": "selected", "selected_hovered": "selected_hovered", "selected_pressed": "selected_pressed",
         "disabled": "disabled"}

ink = text_style("TS_Btn_Ink", "color.accent.ink")
txt = text_style("TS_Btn_Text", "color.text.primary")
dim = text_style("TS_Btn_TextDim", "color.text.secondary")
off = text_style("TS_Btn_Disabled", "color.text.disabled")
row_txt = text_style("TS_Row_Text", "color.text.primary", "body")
row_off = text_style("TS_Row_Disabled", "color.text.disabled", "body")
tab_dim = text_style("TS_Tab_Dim", "color.text.secondary", "body.strong")
tab_txt = text_style("TS_Tab_Text", "color.text.primary", "body.strong")

made = []
for large in (False, True):
    sfx = "Lg" if large else ""
    h, px_ = (56, 28) if large else (44, 20)
    made.append(button_style("BS_Primary" + sfx, "Buttons", "T_Btn_primary_", SOLID, ink, ink, ink, off, h, px_))
    made.append(button_style("BS_Secondary" + sfx, "Buttons", "T_Btn_secondary_", SOLID, txt, txt, txt, off, h, px_))
    made.append(button_style("BS_Tertiary" + sfx, "Buttons", "T_Btn_tertiary_", SOLID, dim, txt, txt, off, h, px_))
    made.append(button_style("BS_Danger" + sfx, "Buttons", "T_Btn_danger_", SOLID, txt, txt, txt, off, h, px_))

# icon buttons: 44 x 44, no horizontal padding (the icon is the content)
made.append(button_style("BS_Icon", "Buttons", "T_Btn_secondary_", SOLID, txt, txt, txt, off, 44, 0, 44))
made.append(button_style("BS_IconTertiary", "Buttons", "T_Btn_tertiary_", SOLID, dim, txt, txt, off, 44, 0, 44))

# tabs: 48 high, padding 20; selected = the underline texture
TAB = {"normal_base": "normal", "normal_hovered": "hovered", "normal_pressed": "pressed", "selected_base": "selected", "selected_hovered": "selected_hovered", "selected_pressed": "selected_hovered", "disabled": "disabled"}
made.append(button_style("BS_Tab", "Tabs", "T_Tab_", TAB, tab_dim, tab_txt, tab_txt, off, 48, 20))

# rows: 48 high, padding 12
ROW = {"normal_base": "normal", "normal_hovered": "hovered", "normal_pressed": "hovered", "selected_base": "selected", "selected_hovered": "selected", "selected_pressed": "selected", "disabled": "disabled"}
made.append(button_style("BS_Row", "Rows", "T_Row_", ROW, row_txt, row_txt, row_txt, row_off, 48, 12, 0))

# list cards that hold their own layout (save slots, quest / skill rows): the row pieces as the state brushes, zero padding, no minimum size
made.append(button_style("BS_RowCard", "Rows", "T_Row_", ROW, row_txt, row_txt, row_txt, row_off, 0, 0, 0))
bare_style("BS_Bare")

# the project's existing default button style becomes the secondary look (every MOButton restyles)
legacy = unreal.load_asset("/MOFramework/UI/MOCommonButtonStyle")
button_style("MOCommonButtonStyle", "Buttons", "T_Btn_secondary_", SOLID, txt, txt, txt, off, 44, 20, 44, existing=legacy)

unreal.log("KITSTYLES text=8 button=%d (+legacy default re-pointed)" % len(made))
