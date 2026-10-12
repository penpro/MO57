"""The kit gallery: every token, type style, button variant, panel piece and icon on one 1920x1080 sheet. It is the in-game component sheet, the thing to LOOK at after any kit change.

    python Tools/ue.py ui build Content/Python/ui_specs/kit_gallery.py
    (in a game)  MO.UI.Show /MOFramework/UI/Kit/WBP_KitGallery.WBP_KitGallery_C        MO.UI.Hide

Left third sits on a sky-coloured band (HUD text must read over bright sky and snow), the rest on the darkest surface.
"""
import kit_lib as K

C = []   # canvas children
FILL_ALL = K.fill_slot()
RECT = K.slot_rect


def add(node):
    C.append(node)


def swatch(name, token, x, y, w=116, h=44, label=None):
    light = token in ("color.text.primary", "color.text.secondary", "color.accent", "color.accent.hover", "color.focus.ring", "color.paper.surface", "color.severity.ok", "color.severity.notice",
                      "color.severity.warning", "color.severity.critical", "color.severity.info")
    add({"type": "Border", "name": name, "props": {"brush_color": K.color(token), "padding": [6, 4]}, "slot": RECT(x, y, w, h), "children": [
        K.text(name + "_T", "caption", label or token.replace("color.", ""), "color.text.inverse" if light else "color.text.primary")]})


# ---- backdrops: sky band (left) + dark surface
add({"type": "Border", "name": "BgDark", "props": {"brush_color": K.color("color.surface.0")}, "slot": RECT(0, 0, 1920, 1080)})
add({"type": "Border", "name": "BgSky", "props": {"brush_color": K.color("#9DBFD9")}, "slot": RECT(0, 0, 640, 360)})

# ---- section: buttons
add(K.text("HdrButtons", "label", "BUTTONS  (44 / 56 high, normal | disabled)", "color.text.secondary", slot=RECT(40, 20, 560, 22)))
VARS = [("Primary", "BS_Primary"), ("Secondary", "BS_Secondary"), ("Tertiary", "BS_Tertiary"), ("Danger", "BS_Danger")]
for i, (label, style) in enumerate(VARS):
    add({"type": "MOButton", "name": "Btn_%s" % label, "label": label, "props": {"style": "%s/Styles/%s.%s_C" % (K.KIT_UI, style, style)}, "slot": RECT(40 + i * 150, 52, 140, 44)})
    add({"type": "MOButton", "name": "BtnLg_%s" % label, "label": label + " Lg", "props": {"style": "%s/Styles/%sLg.%sLg_C" % (K.KIT_UI, style, style)}, "slot": RECT(40 + i * 150, 108, 140, 56)})
    add({"type": "MOButton", "name": "BtnOff_%s" % label, "label": "Disabled", "props": {"style": "%s/Styles/%s.%s_C" % (K.KIT_UI, style, style), "is_enabled": False}, "slot": RECT(40 + i * 150, 176, 140, 44)})

# ---- section: type scale
add(K.text("HdrType", "label", "TYPE SCALE  (sky band: HUD text over bright sky)", "color.text.secondary", slot=RECT(40, 250, 560, 22)))
y = 282
for role, sample, tint in (("display", "Display  Field Ledger", "color.text.primary"), ("title", "Title  Hand Crafting", "color.text.primary"), ("heading", "Heading  Required Materials", "color.text.primary"),
                           ("label", "LABEL  BACKPACK 12 / 30", "color.text.secondary"), ("body", "Body  A fire needs dry tinder, kindling and fuel.", "color.text.primary"),
                           ("body.strong", "Body strong  Make Splint", "color.text.primary"), ("caption", "Caption  2 of 3 gathered", "color.text.secondary"),
                           ("hud", "HUD  [E] Pick up Flint", "color.text.primary"), ("numeric", "Numeric  1:42:10  72 bpm  98%", "color.text.primary"),
                           ("numeric.large", "17:42", "color.text.primary"), ("numeric.small", "Numeric small  0.82 kg", "color.text.secondary")):
    h = int(K.px(role))
    add(K.text("Type_" + role.replace(".", "_"), role, sample, tint, slot=RECT(40, y, 900, h)))
    y += h + 6

# ---- section: colour swatches
add(K.text("HdrColor", "label", "COLOUR TOKENS", "color.text.secondary", slot=RECT(700, 20, 400, 22)))
sx, sy = 700, 52
names = ["color.surface.0", "color.surface.1", "color.surface.2", "color.surface.3", "color.surface.4", "color.accent", "color.accent.hover", "color.accent.pressed",
         "color.text.primary", "color.text.secondary", "color.text.disabled", "color.focus.ring", "color.border.subtle", "color.border.strong", "color.paper.surface",
         "color.severity.ok", "color.severity.notice", "color.severity.warning", "color.severity.critical", "color.severity.info", "color.severity.critical.fill", "color.severity.warning.fill"]
for i, n in enumerate(names):
    swatch("Sw%d" % i, n, sx + (i % 4) * 124, sy + (i // 4) * 52)

# ---- section: panels and pieces
add(K.text("HdrPieces", "label", "9-SLICE PIECES  (panel 0/1/2, modal, input, slot, HUD plate, key-cap, progress)", "color.text.secondary", slot=RECT(700, 360, 700, 22)))
px_, py_ = 700, 392
for i, (n, elev) in enumerate((("P0", 0), ("P1", 1), ("P2", 2), ("PM", "modal"))):
    add(K.panel(n, elev, [K.text(n + "_T", "caption", "panel %s" % elev, "color.text.secondary", slot=K.fill_slot())], w=150, h=100, slot=RECT(px_ + i * 174, py_ + 8, 150, 100)))
add({"type": "Overlay", "name": "InputSample", "slot": RECT(px_, py_ + 130, 300, 44), "children": [
    K.image9("InputFill", "input_fill", "color.surface.0", slot=K.fill_slot()), K.image9("InputStroke", "input_stroke", "color.border.strong", slot=K.fill_slot()),
    K.text("InputText", "body", "Type here", "color.text.disabled", slot={**K.fill_slot(), "padding": [12, 8]})]})
add({"type": "Overlay", "name": "InputFocused", "slot": RECT(px_ + 320, py_ + 130, 300, 44), "children": [
    K.image9("InputFill2", "input_fill", "color.surface.0", slot=K.fill_slot()), K.image9("InputStroke2", "input_stroke", "color.accent", slot=K.fill_slot()),
    K.text("InputText2", "body", "Focused", "color.text.primary", slot={**K.fill_slot(), "padding": [12, 8]})]})
for i, (fill, ticks) in enumerate((("color.surface.2", "color.border.strong"), ("#26302C", "color.accent"), ("color.surface.2", "color.text.secondary"))):
    add({"type": "Overlay", "name": "Slot%d" % i, "slot": RECT(px_ + i * 76, py_ + 196, 64, 64), "children": [
        K.image9("SlotFill%d" % i, "slot_fill", fill, slot=K.fill_slot()), K.image9("SlotTicks%d" % i, "slot_ticks", ticks, slot=K.fill_slot())]})
add({"type": "Overlay", "name": "Plate", "slot": RECT(px_ + 240, py_ + 204, 260, 48), "children": [
    K.image9("PlateFill", "hud_plate", "color.hud.plate", slot=K.fill_slot()), K.text("PlateText", "hud", "[E] Pick up Flint", "color.text.primary", slot={"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"})]})
add({"type": "Overlay", "name": "Keycap", "slot": RECT(px_ + 520, py_ + 212, 40, 32), "children": [
    K.image9("KeyFill", "keycap_fill", "color.text.primary", slot=K.fill_slot()), K.text("KeyText", "label", "E", "color.text.inverse", slot={"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"})]})
add({"type": "Overlay", "name": "Bar", "slot": RECT(px_, py_ + 280, 320, 8), "children": [
    K.image9("BarTrack", "progress_track", "color.data.track", slot=K.fill_slot())]})
add({"type": "Overlay", "name": "BarFilled", "slot": RECT(px_, py_ + 280, 200, 8), "children": [K.image9("BarFill", "progress_fill", "color.data.fill", slot=K.fill_slot())]})

# ---- section: icons (moodles tinted by severity, ui glyphs, item glyphs)
add(K.text("HdrIcons", "label", "ICONS  (white glyphs, tinted by tokens)", "color.text.secondary", slot=RECT(1300, 20, 560, 22)))
icons = ["moodle_cold", "moodle_very_cold", "moodle_hot", "moodle_very_hot", "moodle_damp", "moodle_wet", "moodle_soaked", "moodle_hungry", "moodle_starving", "moodle_thirsty",
         "moodle_dehydrated", "moodle_tired", "moodle_exhausted", "moodle_bleeding", "moodle_infection", "moodle_pain", "moodle_fever", "moodle_injured_limb", "moodle_unconscious",
         "sev_ok", "sev_notice", "sev_warning", "sev_critical", "sev_info",
         "item_tool", "item_weapon", "item_food", "item_drink", "item_medical", "item_material", "item_clothing", "item_container", "item_book", "item_unknown",
         "ui_close", "ui_settings", "ui_save", "ui_load", "ui_search", "ui_filter", "ui_sort", "ui_lock", "ui_clock", "ui_heart", "ui_skill", "ui_quest", "ui_person", "ui_weight", "wind_arrow", "wind_ring"]
for i, n in enumerate(icons):
    tint = "color.severity.warning" if "moodle" in n and i % 3 == 0 else "color.severity.critical" if "moodle" in n and i % 3 == 1 else "color.text.primary"
    if n.startswith("sev_"):
        tint = {"sev_ok": "color.severity.ok", "sev_notice": "color.severity.notice", "sev_warning": "color.severity.warning", "sev_critical": "color.severity.critical", "sev_info": "color.severity.info"}[n]
    add(K.icon_node("Ic_%s" % n, "ic_" + n, tint, 40, slot=RECT(1300 + (i % 12) * 52, 52 + (i // 12) * 52, 40, 40)))

# ---- section: tabs, rows, native controls
add(K.text("HdrCtl", "label", "TABS / ROWS / CONTROLS", "color.text.secondary", slot=RECT(40, 720, 560, 22)))
for i, (lbl, sel) in enumerate((("Skills", True), ("Knowledge", False), ("Quests", False))):
    add({"type": "MOButton", "name": "Tab%d" % i, "label": lbl, "props": {"style": K.btn_style("Tab"), "selectable": True}, "slot": RECT(40 + i * 150, 752, 140, 48)})
for i, lbl in enumerate(("Row  Make Splint", "Row  Make Bandage")):
    add({"type": "MOButton", "name": "RowBtn%d" % i, "label": lbl, "props": {"style": K.btn_style("Row")}, "slot": RECT(40, 816 + i * 52, 420, 48)})
add(K.checkbox("ChkOff", slot=RECT(500, 820, 24, 24)))
add(K.checkbox("ChkOn", slot=RECT(540, 820, 24, 24), checked_state="Checked"))
add(K.checkbox("TglOff", toggle=True, slot=RECT(500, 870, 52, 28)))
add(K.checkbox("TglOn", toggle=True, slot=RECT(570, 870, 52, 28), checked_state="Checked"))
add(K.slider("Sld", slot=RECT(500, 920, 260, 28), value=0.6))
add(K.text_input("InputBox", "Type here", slot=RECT(500, 960, 300, 48)))
add(K.text_input("InputMulti", "Details", multiline=True, slot=RECT(40, 930, 420, 90)))

# ---- diagnosis: how a baked 9-slice behaves as an Image / Border background
add({"type": "Image", "name": "DiagImage", "props": {"brush": K.baked_brush("T_Row_normal")}, "slot": RECT(1000, 720, 300, 90)})
add({"type": "Border", "name": "DiagBorder", "props": {"background": K.baked_brush("T_Row_normal")}, "slot": RECT(1330, 720, 300, 90)})
add({"type": "SizeBox", "name": "DiagBox", "height": 90, "slot": RECT(1000, 830, 300, 90), "children": [{"type": "Overlay", "name": "DiagOv", "children": [
    {"type": "Image", "name": "DiagImage2", "props": {"brush": K.baked_brush("T_Row_normal")}, "slot": FILL_ALL}]}]})
add({"type": "/MOFramework/UI/WBP_MOSaveSlot", "name": "DiagSlot", "slot": RECT(1000, 940, 800, 120)})

SPECS = [{
    "asset": "/MOFramework/UI/Kit/WBP_KitGallery",
    "parent": "/Script/UMG.UserWidget",
    "root": {"type": "CanvasPanel", "name": "GalleryRoot", "children": C},
}]
