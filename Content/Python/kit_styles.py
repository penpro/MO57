"""Create / refresh the kit's CommonUI style assets (Blueprint classes of UCommonButtonStyle / UCommonTextStyle) from tokens.

    python Tools/ue.py py --file Content/Python/kit_styles.py        (editor open, not in PIE; after kit_import.py and `python Tools/kit_bake.py`)

  /MOFramework/UI/Kit/Styles/TS_Btn_{Ink,Text,TextDim,Disabled}   button label text styles (type.body.strong)
  /MOFramework/UI/Kit/Styles/BS_{Primary,Secondary,Tertiary,Danger}[Lg]   CommonButtonStyles: one baked 9-slice per state (Saved/KitBake/Buttons), 44 px high (Lg = 56), padding 20 (Lg 28)

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


def text_style(name, tint):
    bp = ensure_bp(STYLES, name, unreal.CommonTextStyle)
    c = cdo_of(bp)
    f = c.get_editor_property("font")
    spec = K.font("body.strong")
    f.set_editor_property("font_object", unreal.load_asset(spec["font_object"]))
    f.set_editor_property("typeface_font_name", unreal.Name(spec["typeface_font_name"]))
    f.set_editor_property("size", spec["size"])
    c.set_editor_property("font", f)
    c.set_editor_property("color", unreal.LinearColor(*K.color(tint)))
    c.set_editor_property("uses_drop_shadow", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp)
    return bp


def brush(variant, state):
    """A box brush on the baked button texture (white tint: the colour is baked in)."""
    b = unreal.SlateBrush()
    b.set_editor_property("resource_object", unreal.load_asset("%s/Buttons/T_Btn_%s_%s" % (K.KIT_UI, variant, state)))
    b.set_editor_property("draw_as", unreal.SlateBrushDrawType.BOX)
    b.set_editor_property("margin", unreal.Margin(0.125, 0.125, 0.125, 0.125))
    sz = unreal.DeprecateSlateVector2D()
    sz.set_editor_property("x", 32.0)
    sz.set_editor_property("y", 32.0)
    b.set_editor_property("image_size", sz)
    b.set_editor_property("tint_color", unreal.SlateColor(unreal.LinearColor(1, 1, 1, 1)))
    return b


def button_style(name, variant, text_normal, text_hover, text_selected, text_disabled, large=False, existing=None):
    bp = existing or ensure_bp(STYLES, name, unreal.CommonButtonStyle)
    c = cdo_of(bp)
    c.set_editor_property("normal_base", brush(variant, "normal"))
    c.set_editor_property("normal_hovered", brush(variant, "hovered"))
    c.set_editor_property("normal_pressed", brush(variant, "pressed"))
    c.set_editor_property("selected_base", brush(variant, "selected"))
    c.set_editor_property("selected_hovered", brush(variant, "selected_hovered"))
    c.set_editor_property("selected_pressed", brush(variant, "selected_pressed"))
    c.set_editor_property("disabled", brush(variant, "disabled"))
    pad = 28 if large else 20
    c.set_editor_property("button_padding", unreal.Margin(pad, 0, pad, 0))
    c.set_editor_property("custom_padding", unreal.Margin(0, 0, 0, 0))
    h = 56 if large else 44
    c.set_editor_property("min_height", h)
    c.set_editor_property("max_height", 0)
    c.set_editor_property("min_width", 44)
    c.set_editor_property("max_width", 0)
    c.set_editor_property("normal_text_style", text_normal.generated_class())
    c.set_editor_property("normal_hovered_text_style", text_hover.generated_class())
    c.set_editor_property("selected_text_style", text_selected.generated_class())
    c.set_editor_property("selected_hovered_text_style", text_selected.generated_class())
    c.set_editor_property("disabled_text_style", text_disabled.generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp)
    return bp


ink = text_style("TS_Btn_Ink", "color.accent.ink")
txt = text_style("TS_Btn_Text", "color.text.primary")
dim = text_style("TS_Btn_TextDim", "color.text.secondary")
off = text_style("TS_Btn_Disabled", "color.text.disabled")

made = []
for large in (False, True):
    sfx = "Lg" if large else ""
    made.append(button_style("BS_Primary" + sfx, "primary", ink, ink, ink, off, large))
    made.append(button_style("BS_Secondary" + sfx, "secondary", txt, txt, txt, off, large))
    made.append(button_style("BS_Tertiary" + sfx, "tertiary", dim, txt, txt, off, large))
    made.append(button_style("BS_Danger" + sfx, "danger", txt, txt, txt, off, large))

# the project's existing default button style becomes the secondary look (every MOButton restyles)
legacy = unreal.load_asset("/MOFramework/UI/MOCommonButtonStyle")
button_style("MOCommonButtonStyle", "secondary", txt, txt, txt, off, False, existing=legacy)

unreal.log("KITSTYLES text=4 button=%d (+legacy default re-pointed)" % len(made))
