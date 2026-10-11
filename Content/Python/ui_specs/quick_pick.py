"""WBP_QuickPickModal: the small "pick one" popup behind the dev console's `weather` and `time` commands (and any later short list of choices).

Contract (C++): MOQuickPickDialog.h -- TitleText, OptionsGrid, CancelButton (+ optional MessageText). The option buttons are created at run time from OptionButtonClass,
which the class defaults below point at WBP_MOCommonButton (a Widget Blueprint: a native button class would be an empty, click-through spacer).

Layout: the panel sits centred at its natural size, so a 4-option picker and an 11-option picker both fit snugly. The grid cells are 190 x 56.

Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/quick_pick.py   (idempotent; PIE must not be running)
"""
UI = "/MOFramework/UI"

GREY = [0.72, 0.72, 0.72, 1.0]

MODAL = {
    "asset": f"{UI}/WBP_QuickPickModal",
    "parent": "/Script/MOFramework.MOQuickPickDialog",
    "class_defaults": {"option_button_class": f"{UI}/WBP_MOCommonButton"},
    "root": {"type": "Overlay", "name": "RootOverlay", "children": [
        {"type": "Border", "name": "ModalBorder", "color": [0.04, 0.05, 0.07, 0.94], "props": {"padding": 24},
         "slot": {"horizontal_alignment": "Center", "vertical_alignment": "Center"}, "children": [
            {"type": "VerticalBox", "name": "ContentBox", "children": [
                {"type": "TextBlock", "name": "TitleText", "text": "Pick one", "font_size": 30, "slot": {"padding": [0, 0, 0, 4], "horizontal_alignment": "Center"}},
                {"type": "TextBlock", "name": "MessageText", "text": "", "font_size": 14, "color": GREY,
                 "slot": {"padding": [0, 0, 0, 14], "horizontal_alignment": "Center"}},
                {"type": "UniformGridPanel", "name": "OptionsGrid",
                 "props": {"min_desired_slot_width": 190.0, "min_desired_slot_height": 56.0, "slot_padding": 5},
                 "slot": {"horizontal_alignment": "Center", "padding": [0, 0, 0, 10]}},
                {"type": "SizeBox", "name": "CancelSizeBox", "height": 56, "width": 190, "slot": {"horizontal_alignment": "Center"}, "children": [
                    {"type": "MOButton", "name": "CancelButton", "label": "Cancel"}]},
            ]}]},
    ]},
}

SPECS = [MODAL]
