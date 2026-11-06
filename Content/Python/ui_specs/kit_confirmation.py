"""WBP_ConfirmationWidget in the kit look: the confirmation and one-button notice dialog (the early access notice, exit, back to the main menu).

Wes, Nov 6 2026, on the early access notice: "still looks like shit". The old layout was a fixed canvas from March: a pale see-through 600 x 400 panel, the title on it, the
message in a dark box 75 px high, and the buttons at a fixed y. Any message longer than two lines ran out of its box and under the button. The panel now takes its height
from what it holds, on the kit's opaque surface, with the buttons below the message on the right; a notice's hidden Cancel collapses out of the row (UMOConfirmationBase).

Contract: MOConfirmationBase (TitleText, MessageText, ConfirmButton, CancelButton).

Apply: remove the old tree first (the tool cannot swap a node in place), then build:
    python Tools/ue.py ui remove /MOFramework/UI/WBP_ConfirmationWidget SizeBox_52
    python Tools/ue.py ui build Content/Python/ui_specs/kit_confirmation.py
Look at it: a new game shows the notice (a test instance needs notice=True).
"""
import kit_lib as K

UI = "/MOFramework/UI"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL = {"size": {"size_rule": "Fill", "value": 1.0}}

CONTENT = {"type": "VerticalBox", "name": "ConfirmContent", "slot": K.fill_slot(), "children": [
    K.text("TitleText", "title", "Title", "color.text.primary", var=True),
    K.divider("ConfirmRule", slot={"padding": [0, 14, 0, 0]}),
    K.text("MessageText", "body", "Message", "color.text.secondary", wrap=True, var=True, slot={"padding": [0, 16, 0, 0]}),
    {"type": "SizeBox", "name": "ConfirmButtonBox", "height": 56, "slot": {"padding": [0, 24, 0, 0]}, "children": [
        {"type": "HorizontalBox", "name": "ConfirmButtonRow", "children": [
            {"type": "Spacer", "name": "ConfirmButtonSpacer", "slot": FILL},
            {"type": "SizeBox", "name": "CancelButtonSize", "width": 170, "slot": {"padding": [0, 0, 12, 0]}, "children": [K.button("CancelButton", "Cancel", "Secondary")]},
            {"type": "SizeBox", "name": "ConfirmButtonSize", "width": 170, "children": [K.button("ConfirmButton", "Confirm", "Primary")]},
        ]}]},
]}

DIALOG = {
    "asset": f"{UI}/WBP_ConfirmationWidget",
    "attach": [
        {"parent": "RootCanvas", "node": K.panel("ConfirmPanel", 2, [CONTENT], w=640, padding=[32, 28, 32, 28],
                                                 slot=K.slot_anchored((0.5, 0.5), (0.5, 0.5), align=(0.5, 0.5), auto_size=True))},
    ],
}

SPECS = [DIALOG]
