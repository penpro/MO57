"""The death recap modal (UMODeathRecapWidget) in the Field Ledger kit: a dimmed field and one centred modal panel (who, how, where, when) with Continue / Return to main menu.

There was NO WBP_DeathRecap and DeathRecapWidgetClass was never assigned, so a pawn's death showed nothing at all (the controller logged "recap suppressed"). The class now falls back to this asset
(UMOSystemMenuUIController::ResolveDeathRecapClass), so no Blueprint wiring is needed.

Contract: UMODeathRecapWidget (CharacterNameText, CauseOfDeathText, ReturnToMenuButton; optional LocationText, TimeOfDeathText, ContinueButton).

Apply (new asset, nothing to remove first):
    python Tools/ue.py ui build Content/Python/ui_specs/kit_death.py
Look at it:  MO.NewGame, then `MO.Player.Kill` in the console (or ui_shoot.py after it).
"""
import kit_lib as K

UI = "/MOFramework/UI"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL = {"size": {"size_rule": "Fill", "value": 1.0}}


def fact(name, label, value_name, value="-"):
    return {"type": "HorizontalBox", "name": name + "Row", "slot": {"padding": [0, 16, 0, 0]}, "children": [
        {"type": "SizeBox", "name": name + "LabelBox", "width": 150, "slot": {**VC}, "children": [K.text(name + "Label", "caption", label, "color.text.secondary")]},
        K.text(value_name, "body", value, "color.text.primary", var=True, wrap=True, slot={**VC, **FILL})]}


CONTENT = {"type": "VerticalBox", "name": "DeathContent", "slot": K.fill_slot(), "children": [   # fill the panel (an Overlay child otherwise hugs its left edge)
    K.text("DeathTitle", "title", "You have died", "color.severity.critical"),
    K.text("DeathSubtitle", "caption", "Your story ends here. The world does not.", "color.text.secondary", slot={"padding": [0, 6, 0, 0]}),
    K.divider("DeathRule", slot={"padding": [0, 20, 0, 0]}),
    fact("Who", "Who", "CharacterNameText", "Unknown"),
    fact("How", "Cause of death", "CauseOfDeathText", "Unknown"),
    fact("Where", "Where", "LocationText", ""),
    fact("When", "When", "TimeOfDeathText", ""),
    {"type": "Spacer", "name": "DeathSpacer", "slot": FILL},
    {"type": "SizeBox", "name": "DeathButtonBox", "height": 72, "children": [{"type": "HorizontalBox", "name": "DeathButtonRow", "children": [
        {"type": "Spacer", "name": "DeathButtonSpacer", "slot": FILL},
        K.button("ContinueButton", "Continue", "SecondaryLg", slot={**VC, "padding": [0, 0, 12, 0]}),
        K.button("ReturnToMenuButton", "Return to main menu", "PrimaryLg", slot=VC)]}]}]}

DEATH = {
    "asset": f"{UI}/WBP_DeathRecap",
    "parent": "/Script/MOFramework.MODeathRecapWidget",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "Image", "name": "DeathBackdrop", "props": {"color_and_opacity": K.color("color.surface.0", 0.84)}, "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        K.panel("DeathPanel", "modal", [CONTENT], w=760, h=500, slot=K.place(580, 290, 760, 500, "Center"), padding=44)]},
}

SPECS = [DEATH]
