"""The in-game menu flow in the Field Ledger kit (screens/ingame_menu.md): the tab bar and world-clock components, the in-game menu rail + shared panel shell, the save / load panels and their slot cards.
Options and the bug report are kit_options.py / kit_bugreport.py.

Contracts (C++ BindWidget names, `python Tools/ue.py ui contract <Class>`):  MOInGameMenu (ButtonsBox, 5 buttons, FocusWindowSwitcher, panels, PanelShell), MOSavePanel / MOLoadPanel (SaveSlotsScrollBox,
BackButton, NewSaveButton, StatusText), MOSaveSlotEntry (texts, ScreenshotImage, AutosaveIndicator, Rename/Delete/Host).  The switcher's child ORDER is the C++ panel index (0 none, 1 options, 2 save, 3 load, then bug report).

Apply:
    for a in "WBP_MOInGameMenu SizeBox_0" "WBP_SavePanel SizeBox_122" "WBP_LoadPanel SizeBox_122" "WBP_MOSaveSlot Border_0"; do set -- $a; python Tools/ue.py ui remove /MOFramework/UI/$1 $2; done
    python Tools/ue.py ui build Content/Python/ui_specs/kit_menus.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
KIT = K.KIT_UI
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_H = {"horizontal_alignment": "H_ALIGN_FILL"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}


def shell_layers(name, elev=1, shadow=True):
    """Panel plate layers (shadow, fill, stroke) as Overlay children."""
    layers = []
    if shadow:
        layers.append(K.image9(name + "Shadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **FILL}))
    layers.append(K.image9(name + "Fill", "panel_fill", "color.surface.1" if elev == 1 else "color.surface.2", slot=FILL))
    layers.append(K.image9(name + "Stroke", "panel_stroke", "color.border.subtle", slot=FILL))
    return layers


# ---------------------------------------------------------------- tab bar (kit: tab h48 + 4 px accent indicator; the C++ builds the buttons from TabLabels)
TAB_BAR = {
    "asset": f"{KIT}/WBP_TabBar",
    "parent": "/Script/MOFramework.MOTabBarWidget",
    "root": {"type": "VerticalBox", "name": "TabBarRoot", "children": [
        {"type": "SizeBox", "name": "TabRowBox", "height": 48, "children": [{"type": "HorizontalBox", "name": "TabRow", "var": True}]},
        K.divider("TabBarRule")]},
}

# ---------------------------------------------------------------- world clock (the ticking "world continues" readout: menus never pause)
WORLD_CLOCK = {
    "asset": f"{KIT}/WBP_WorldClock",
    "parent": "/Script/MOFramework.MOWorldClockWidget",
    "root": {"type": "HorizontalBox", "name": "ClockRoot", "children": [
        {"type": "SizeBox", "name": "ClockIconBox", "width": 32, "height": 32, "slot": {**VC, "padding": [0, 0, 14, 0]}, "children": [K.icon_node("TickIcon", "ic_ui_clock", "color.text.secondary", 32, var=True)]},
        {"type": "VerticalBox", "name": "ClockColumn", "slot": VC, "children": [
            K.text("TimeText", "numeric.large", "00:00", "color.text.primary", var=True),
            {"type": "HorizontalBox", "name": "ClockMeta", "children": [
                K.text("ClockLabel", "caption", "WORLD CONTINUES", "color.text.secondary"),
                K.text("DateText", "caption", "", "color.text.secondary", var=True, slot={"padding": [12, 0, 0, 0]})]}]}]},
}

# ---------------------------------------------------------------- in-game menu: scrim, world clock, rail, shared panel shell, the focus-panel switcher
RAIL = [("OptionsButton", "Options", "SecondaryLg"), ("LoadButton", "Load", "SecondaryLg"), ("SaveButton", "Save", "SecondaryLg"), ("BugReportButton", "Bug Report", "TertiaryLg")]

INGAME = {
    "asset": f"{UI}/WBP_MOInGameMenu",
    "root": {"type": "CanvasPanel", "name": "InGameRoot", "children": [
        {"type": "Border", "name": "Scrim", "props": {"brush_color": K.color("color.overlay.scrim")}, "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        {"type": f"{KIT}/WBP_WorldClock", "name": "WorldClock", "slot": K.place(120, 96, 400, 112, "TopLeft", auto_height=True)},
        {"type": "SizeBox", "name": "RailBox", "width": 400, "slot": K.place(120, 240, 400, 400, "TopLeft", auto_height=True), "children": [
            {"type": "VerticalBox", "name": "ButtonsBox", "var": True, "children":
                [K.button(n, label, style, slot={**FILL_H, "padding": [0, 0, 0, 8]}) for n, label, style in RAIL]
                + [{"type": "Spacer", "name": "RailGap", "props": {"size": {"x": 1.0, "y": 24.0}}},
                   K.button("ExitToMainMenuButton", "Main Menu", "SecondaryLg", slot={**FILL_H, "padding": [0, 0, 0, 8]}),
                   K.button("ExitGameButton", "Exit Game", "DangerLg", slot=FILL_H)]}]},
        {"type": "SizeBox", "name": "PanelShell", "var": True, "width": 1200, "height": 888, "slot": K.place(600, 96, 1200, 888, "TopLeft"),
         "children": [{"type": "Overlay", "name": "PanelShellOverlay", "children": shell_layers("Shell")}]},
        {"type": "WidgetSwitcher", "name": "FocusWindowSwitcher", "var": True, "slot": K.place(624, 120, 1152, 840, "TopLeft"), "children": [
            {"type": "SizeBox", "name": "FocusWindowSwitcherSizeBox"},
            {"type": f"{UI}/WBP_MOOptionsPanel", "name": "OptionsPanel", "var": True},
            {"type": f"{UI}/WBP_SavePanel", "name": "SavePanel", "var": True},
            {"type": f"{UI}/WBP_LoadPanel", "name": "LoadPanel", "var": True},
            {"type": f"{UI}/WBP_BugReportPanel", "name": "BugReportPanel", "var": True}]},
    ]},
}


# ---------------------------------------------------------------- save / load panels (inner 1152 x 840)
def slot_list_panel(asset, title, extra_header, tail):
    header = [K.text("PanelTitle", "title", title, "color.text.primary", slot={**VC, **FILL_SIZE})] + extra_header + [
        K.button("BackButton", "Back", "Tertiary", slot={**VC, "padding": [8, 0, 0, 0]})]
    return {
        "asset": f"{UI}/{asset}",
        "root": {"type": "SizeBox", "name": "SizeBox_122", "width": 1152, "height": 840, "children": [
            {"type": "VerticalBox", "name": "VerticalBox_0", "children": [
                {"type": "SizeBox", "name": "PanelHeaderBox", "height": 56, "children": [{"type": "HorizontalBox", "name": "PanelHeader", "children": header}]},
                K.divider("PanelRule", slot={"padding": [0, 8, 0, 12]}),
                K.scroll_box("SaveSlotsScrollBox", slot=FILL_SIZE)] + tail}]},
    }


SAVE_PANEL = slot_list_panel("WBP_SavePanel", "Save game", [K.button("NewSaveButton", "New save", "Primary", slot={**VC, "padding": [8, 0, 0, 0]})], [])
LOAD_PANEL = slot_list_panel("WBP_LoadPanel", "Load game", [], [K.text("StatusText", "caption", "", "color.text.secondary", var=True, slot={"padding": [0, 12, 0, 0]})])

# ---------------------------------------------------------------- save slot card (kit: 1152 x 120, thumbnail 192x108, name heading, meta caption + numeric small)
SAVE_SLOT = {
    "asset": f"{UI}/WBP_MOSaveSlot",
    # the entry is a UCommonButtonBase: with no style it paints the engine's grey box around the card. The row-card style paints the kit row (hover / selected too) and adds no padding.
    "class_defaults": {"style": f"{KIT}/Styles/BS_RowCard.BS_RowCard_C"},
    "root": {"type": "SizeBox", "name": "SlotRoot", "height": 120, "slot": {}, "children": [
        {"type": "Overlay", "name": "SlotOverlay", "children": [
            {"type": "HorizontalBox", "name": "SlotRow", "slot": {"padding": 6}, "children": [
                {"type": "SizeBox", "name": "ThumbBox", "width": 192, "height": 108, "slot": VC, "children": [
                    {"type": "Image", "name": "ScreenshotImage", "var": True, "props": {"color_and_opacity": K.color("color.surface.0")}}]},
                {"type": "VerticalBox", "name": "SlotText", "slot": {**VC, **FILL_SIZE, "padding": [20, 0, 12, 0]}, "children": [
                    K.text("SaveNameText", "heading", "Save name", "color.text.primary", var=True),
                    {"type": "HorizontalBox", "name": "SlotMeta", "slot": {"padding": [0, 4, 0, 0]}, "children": [
                        K.text("TimestampText", "caption", "", "color.text.secondary", var=True),
                        K.text("PlayTimeText", "numeric.small", "", "color.text.secondary", var=True, slot={"padding": [16, 0, 0, 0]})]},
                    K.text("WorldNameText", "caption", "", "color.text.secondary", var=True),
                    K.text("CharacterInfoText", "caption", "", "color.text.secondary", var=True),
                    {"type": "SizeBox", "name": "AutosaveBox", "height": 24, "var": False, "slot": {"padding": [0, 6, 0, 0], "horizontal_alignment": "H_ALIGN_LEFT"}, "children": [
                        {"type": "Overlay", "name": "AutosaveIndicator", "var": True, "children": [
                            {"type": "Border", "name": "AutosaveChip", "props": {"brush_color": K.color("color.surface.4")}, "slot": FILL},
                            K.text("AutosaveText", "label", "AUTOSAVE", "color.text.primary", slot={**VC, "padding": [8, 0, 8, 0]})]}]}]},
                {"type": "HorizontalBox", "name": "SlotButtons", "slot": {**VC, "padding": [0, 0, 12, 0]}, "children": [
                    K.button("HostButton", "Host", "Secondary", slot={"padding": [0, 0, 8, 0]}),
                    K.button("RenameButton", "Rename", "Secondary", slot={"padding": [0, 0, 8, 0]}),
                    K.button("DeleteButton", "Delete", "Danger")]}]}]}]},
}

SPECS = [TAB_BAR, WORLD_CLOCK, INGAME, SAVE_PANEL, LOAD_PANEL, SAVE_SLOT]
