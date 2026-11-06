"""WBP_TextInputModal in the kit look: the dev console and the save rename dialog.

Wes, Nov 6 2026, on the console: "this looks like shit". The old layout was a fixed canvas from May: a see-through 500 x 300 panel, the title in its corner, the message running
out of the panel, a white input box and three buttons at fixed spots. It now takes its height from what it holds, on the kit's opaque surface, with the kit's input well.
Wes, same day: "if you open the console there's a menu button that has all of our current menus": UMOTextInputDialog::ExtraButton, on the left of the row, collapsed unless the
caller gives it a label (a rename dialog shows two buttons).

Contract: MOTextInputDialog (TitleText, MessageText, TextInput, ConfirmButton, CancelButton; optional ExtraButton). Build it after the C++ with ExtraButton: a widget of that name
made first becomes its own Blueprint variable and clashes with the property.

Apply: remove the old tree first (the tool cannot swap a node in place), then build:
    python Tools/ue.py ui remove /MOFramework/UI/WBP_TextInputModal SizeBox_80
    python Tools/ue.py ui build Content/Python/ui_specs/kit_text_input.py
Look at it: ~ in a game (dev mode).
"""
import kit_lib as K

UI = "/MOFramework/UI"
FILL = {"size": {"size_rule": "Fill", "value": 1.0}}
BUTTON = 150

CONTENT = {"type": "VerticalBox", "name": "InputContent", "slot": K.fill_slot(), "children": [
    K.text("TitleText", "title", "Title", "color.text.primary", var=True),
    K.divider("InputRule", slot={"padding": [0, 14, 0, 0]}),
    K.text("MessageText", "body", "Message", "color.text.secondary", wrap=True, var=True, slot={"padding": [0, 14, 0, 0]}),
    {"type": "SizeBox", "name": "InputBox", "height": 48, "slot": {"padding": [0, 14, 0, 0]}, "children": [K.text_input("TextInput", "command")]},
    {"type": "SizeBox", "name": "InputButtonBox", "height": 56, "slot": {"padding": [0, 20, 0, 0]}, "children": [
        {"type": "HorizontalBox", "name": "InputButtonRow", "children": [
            {"type": "SizeBox", "name": "ExtraButtonSize", "width": BUTTON, "children": [K.button("ExtraButton", "Menu", "Secondary")]},
            {"type": "Spacer", "name": "InputButtonSpacer", "slot": FILL},
            {"type": "SizeBox", "name": "CancelButtonSize", "width": BUTTON, "slot": {"padding": [0, 0, 12, 0]}, "children": [K.button("CancelButton", "Cancel", "Secondary")]},
            {"type": "SizeBox", "name": "ConfirmButtonSize", "width": BUTTON, "children": [K.button("ConfirmButton", "Confirm", "Primary")]},
        ]}]},
]}

DIALOG = {
    "asset": f"{UI}/WBP_TextInputModal",
    "attach": [
        {"parent": "CanvasPanel_38", "node": K.panel("InputPanel", 2, [CONTENT], w=640, padding=[32, 28, 32, 28],
                                                     slot=K.slot_anchored((0.5, 0.5), (0.5, 0.5), align=(0.5, 0.5), auto_size=True))},
    ],
}

SPECS = [DIALOG]
