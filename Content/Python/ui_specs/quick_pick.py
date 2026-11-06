"""WBP_QuickPickModal: the small "pick one" popup behind the dev console's menus (Menu, spawn, cinema, weather, biome, poi, and any later short list of choices).

Contract (C++): MOQuickPickDialog.h -- TitleText, OptionsGrid, CancelButton (+ optional MessageText). The option buttons are created at run time from OptionButtonClass,
which the class defaults below point at WBP_MOCommonButton (a Widget Blueprint: a native button class would be an empty, click-through spacer).

Layout: the kit's opaque panel (Nov 6 2026, to match the console and the notices), centred at its natural size, so a 4-option picker and a 20-option picker both fit snugly. The grid
cells are 190 x 56.

Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/quick_pick.py   (idempotent; PIE must not be running)
An asset from before the kit panel needs `python Tools/ue.py ui remove /MOFramework/UI/WBP_QuickPickModal ModalBorder` first.
"""
import kit_lib as K

UI = "/MOFramework/UI"

CONTENT = {"type": "VerticalBox", "name": "ContentBox", "slot": K.fill_slot(), "children": [
    K.text("TitleText", "title", "Pick one", "color.text.primary", var=True, slot={"horizontal_alignment": "H_ALIGN_CENTER"}),
    K.text("MessageText", "body", "", "color.text.secondary", var=True, slot={"padding": [0, 6, 0, 16], "horizontal_alignment": "H_ALIGN_CENTER"}),
    # Long lists (the developer menu: up to ~30 commands in a group) scroll inside a capped height; short pickers stay snug.
    {"type": "SizeBox", "name": "OptionsScrollSize", "props": {"max_desired_height": 620.0},
     "slot": {"horizontal_alignment": "H_ALIGN_CENTER", "padding": [0, 0, 0, 14]}, "children": [
        {"type": "ScrollBox", "name": "OptionsScroll", "children": [
            {"type": "UniformGridPanel", "name": "OptionsGrid", "var": True,
             "props": {"min_desired_slot_width": 190.0, "min_desired_slot_height": 56.0, "slot_padding": 5}}]}]},
    {"type": "SizeBox", "name": "CancelSizeBox", "height": 56, "width": 190, "slot": {"horizontal_alignment": "H_ALIGN_CENTER"}, "children": [
        K.button("CancelButton", "Cancel", "Secondary")]},
]}

MODAL = {
    "asset": f"{UI}/WBP_QuickPickModal",
    "parent": "/Script/MOFramework.MOQuickPickDialog",
    "class_defaults": {"option_button_class": f"{UI}/WBP_MOCommonButton"},
    "root": {"type": "Overlay", "name": "RootOverlay", "children": [
        K.panel("ModalPanel", 2, [CONTENT], padding=[28, 24, 28, 24], slot={"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"}),
    ]},
}

SPECS = [MODAL]
