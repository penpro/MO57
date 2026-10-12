"""The possession menu ("Characters") in the Field Ledger kit: a modal-style panel on the Menu layer, a scrolling list of character cards, a create button.

Contracts: MOPossessionMenu (REQ PawnListScrollBox; opt CreateCharacterButton, CloseButton, TitleText, EmptyListText), MOPawnEntryWidget (all optional: Name/Age/Gender/Status/Location/LastPlayed texts,
HealthBar, PortraitImage, PossessButton; MOListEntryBase: EntryButton, BackgroundBorder).

Apply:
    python Tools/ue.py ui remove /MOFramework/UI/WBP_MOPossessionMenu SizeBox_60
    python Tools/ue.py ui remove /MOFramework/UI/WBP_MOPawnEntryWidget SizeBox_48
    python Tools/ue.py ui build Content/Python/ui_specs/kit_possession.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}


def plate(name):
    return [K.image9(name + "Shadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **FILL}),
            K.image9(name + "Fill", "panel_fill", "color.surface.1", slot=FILL),
            K.image9(name + "Stroke", "panel_stroke", "color.border.subtle", slot=FILL)]


MENU = {
    "asset": f"{UI}/WBP_MOPossessionMenu",
    "root": {"type": "CanvasPanel", "name": "PossessionRoot", "children": [
        {"type": "Border", "name": "Scrim", "props": {"brush_color": K.color("color.overlay.scrim")}, "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        {"type": "SizeBox", "name": "PanelBox", "width": 960, "height": 700, "slot": K.place(480, 190, 960, 700, "Center"), "children": [
            {"type": "Overlay", "name": "PanelOverlay", "children": plate("Panel") + [
                {"type": "VerticalBox", "name": "PanelColumn", "slot": {**FILL, "padding": 28}, "children": [
                    {"type": "SizeBox", "name": "HeaderBox", "height": 52, "children": [
                        {"type": "HorizontalBox", "name": "Header", "children": [
                            K.text("TitleText", "title", "Characters", "color.text.primary", var=True, slot={**VC, **FILL_SIZE}),
                            K.button("CloseButton", "Close", "Secondary", slot=VC)]}]},
                    K.divider("HeaderRule", slot={"padding": [0, 12, 0, 12]}),
                    K.text("EmptyListText", "body", "No characters yet.", "color.text.secondary", var=True),
                    K.scroll_box("PawnListScrollBox", slot=FILL_SIZE),
                    {"type": "HorizontalBox", "name": "FooterRow", "slot": {"padding": [0, 16, 0, 0]}, "children": [
                        {"type": "Spacer", "name": "FooterSpacer", "slot": FILL_SIZE},
                        K.button("CreateCharacterButton", "Create character", "Primary")]}]}]}]},
    ]},
}


def label_value(name, label, value_name, w_label=84):
    return {"type": "HorizontalBox", "name": name, "slot": {"padding": [0, 2, 0, 0]}, "children": [
        {"type": "SizeBox", "name": name + "LabelBox", "width": w_label, "children": [K.text(name + "Label", "caption", label, "color.text.secondary")]},
        K.text(value_name, "body", "", "color.text.primary", var=True)]}


ENTRY = {
    "asset": f"{UI}/WBP_MOPawnEntryWidget",
    "root": {"type": "SizeBox", "name": "EntrySize", "height": 104, "children": [
        {"type": "Overlay", "name": "EntryOverlay", "children": [
            {"type": "Border", "name": "BackgroundBorder", "var": True, "props": {"background": K.baked_brush("T_Row_normal"), "brush_color": [1, 1, 1, 1], "padding": 0}, "slot": FILL},
            {"type": "HorizontalBox", "name": "EntryRow", "slot": {**FILL, "padding": [14, 10, 14, 10]}, "children": [
                {"type": "SizeBox", "name": "PortraitBox", "width": 80, "height": 80, "slot": VC, "children": [
                    {"type": "Overlay", "name": "PortraitOverlay", "children": [
                        {"type": "Border", "name": "PortraitBack", "props": {"brush_color": K.color("color.surface.0")}, "slot": FILL},
                        K.icon_node("PortraitImage", "ic_ui_person", "color.text.secondary", 44, var=True, slot={"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"}),
                        K.image9("PortraitFrame", "portrait_frame", "color.border.strong", slot=FILL)]}]},
                {"type": "VerticalBox", "name": "IdentityColumn", "slot": {**VC, **FILL_SIZE, "padding": [18, 0, 12, 0]}, "children": [
                    K.text("NameText", "heading", "Name", "color.text.primary", var=True),
                    label_value("AgeRow", "Age", "AgeText"), label_value("GenderRow", "Gender", "GenderText"),
                    {"type": "SizeBox", "name": "HealthBox", "height": 8, "slot": {"padding": [0, 8, 0, 0]}, "children": [K.progress_bar("HealthBar")]}]},
                {"type": "VerticalBox", "name": "StateColumn", "slot": {**VC, **FILL_SIZE, "padding": [0, 0, 12, 0]}, "children": [
                    label_value("StatusRow", "Status", "StatusText"), label_value("LocationRow", "Location", "LocationText"), label_value("LastPlayedRow", "Last played", "LastPlayedText")]},
                K.button("PossessButton", "Select", "Primary", slot=VC)]},
            K.button("EntryButton", "", "Bare", slot=FILL)]}]},
}

SPECS = [MENU, ENTRY]
