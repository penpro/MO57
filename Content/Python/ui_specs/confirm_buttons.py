"""Confirmation dialog: real, mouse-clickable Confirm / Cancel buttons.

`ui check WBP_MOConfirmation` reported BAD: ConfirmButton and CancelButton were bare native `MOCommonButton` nodes. A native UserWidget class has no widget tree, so it renders as an empty,
hit-test-invisible spacer: real mouse clicks pass straight through it (keyboard / controller focus and delegates still worked, which is why nobody noticed). Every confirmation (exit, back to the main
menu, overwrite a save, load with unsaved progress) was unclickable with a mouse. They become `MOButton` Widget Blueprints with the same names (UMOConfirmationBase sets their text with SetButtonText).

Apply: first remove the two bare nodes (the tool cannot swap a node in place),
    python Tools/ue.py ui remove /MOFramework/UI/CommonUI/WBP_MOConfirmation ConfirmButton
    python Tools/ue.py ui remove /MOFramework/UI/CommonUI/WBP_MOConfirmation CancelButton
then   python Tools/ue.py ui build Content/Python/ui_specs/confirm_buttons.py
"""
UI = "/MOFramework/UI/CommonUI"


def button(name, label):
    return {"type": "SizeBox", "name": name + "Size", "width": 220, "height": 64, "slot": {"padding": [6, 12, 6, 0]},
            "children": [{"type": "MOButton", "name": name, "label": label}]}


CONFIRMATION = {
    "asset": f"{UI}/WBP_MOConfirmation",
    "attach": [
        {"parent": "ButtonBox", "node": button("ConfirmButton", "Confirm")},
        {"parent": "ButtonBox", "node": button("CancelButton", "Cancel")},
    ],
}

SPECS = [CONFIRMATION]
