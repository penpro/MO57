"""The character status screen in the Field Ledger kit: WBP_MOStatusPanel (tab row Info / Vitals / Nutrition / Nutrients / Fitness / Mental / Wounds / Conditions + Back, the category switcher),
WBP_MOStatusField (a labelled value row with an optional bar) and WBP_MOCharacterInfoEntry (a label / value row with an inline Change -> edit -> Confirm / Cancel). Names are the C++ contracts
(`python Tools/ue.py ui contract MOStatusPanel` ...).

    export MSYS_NO_PATHCONV=1
    python Tools/ue.py ui remove /MOFramework/UI/WBP_MOStatusPanel SizeBox_41
    python Tools/ue.py ui remove /MOFramework/UI/WBP_MOStatusField SizeBox_46
    python Tools/ue.py ui remove /MOFramework/UI/WBP_MOCharacterInfoEntry SizeBox_71
    python Tools/ue.py ui build Content/Python/ui_specs/kit_status.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}
R = K.slot_rect

# ------------------------------------------------------------------------------------------------ a status row
FIELD = {
    "asset": f"{UI}/WBP_MOStatusField",
    "root": {"type": "SizeBox", "name": "StatusFieldSize", "height": 64, "children": [
        {"type": "Overlay", "name": "StatusFieldOverlay", "children": [
            K.image9("StatusFieldFill", "slot_fill", "color.surface.2", slot={**FILL, "padding": [0, 3, 0, 3]}),
            {"type": "HorizontalBox", "name": "StatusFieldRow", "slot": {**FILL, "padding": [18, 8, 18, 8]}, "children": [
                K.icon_node("IconImage", "ic_sev_info", "color.text.secondary", 28, var=True, slot={**VC, "padding": [0, 0, 14, 0]}),
                {"type": "SizeBox", "name": "StatusTitleBox", "width": 260, "slot": VC, "children": [K.text("TitleText", "body.strong", "Title", "color.text.primary", var=True)]},
                {"type": "SizeBox", "name": "StatusValueBox", "width": 200, "slot": VC, "children": [K.text("ValueText", "numeric", "0 / 0", "color.text.primary", var=True)]},
                {"type": "SizeBox", "name": "StatusBarBox", "height": 8, "slot": {**VC, **FILL_SIZE, "padding": [12, 0, 0, 0]}, "children": [K.progress_bar("ValueBar")]}]}]}]},
}

# ------------------------------------------------------------------------------------------------ an info row (label, value, inline edit)
INFO = {
    "asset": f"{UI}/WBP_MOCharacterInfoEntry",
    "root": {"type": "SizeBox", "name": "InfoEntrySize", "height": 64, "children": [
        {"type": "Overlay", "name": "InfoEntryOverlay", "children": [
            K.image9("InfoEntryFill", "slot_fill", "color.surface.2", slot={**FILL, "padding": [0, 3, 0, 3]}),
            {"type": "HorizontalBox", "name": "InfoEntryRow", "slot": {**FILL, "padding": [18, 8, 18, 8]}, "children": [
                {"type": "SizeBox", "name": "InfoLabelBox", "width": 220, "slot": VC, "children": [K.text("LabelText", "caption", "Label", "color.text.secondary", var=True)]},
                {"type": "Overlay", "name": "InfoValueStack", "slot": {**VC, **FILL_SIZE}, "children": [
                    K.text("ValueText", "body", "Value", "color.text.primary", var=True, slot=VC),
                    {"type": "SizeBox", "name": "InfoEditBox", "height": 44, "slot": {"vertical_alignment": "V_ALIGN_CENTER", "horizontal_alignment": "H_ALIGN_FILL"},
                     "children": [K.text_input("EditTextBox")]}]},
                {"type": "SizeBox", "name": "InfoChangeBox", "width": 120, "height": 40, "slot": {**VC, "padding": [12, 0, 0, 0]}, "children": [K.button("ChangeButton", "Change", "Secondary")]},
                {"type": "SizeBox", "name": "InfoConfirmBox", "width": 120, "height": 40, "slot": {**VC, "padding": [12, 0, 0, 0]}, "children": [K.button("ConfirmButton", "Confirm", "Primary")]},
                {"type": "SizeBox", "name": "InfoCancelBox", "width": 110, "height": 40, "slot": {**VC, "padding": [8, 0, 0, 0]}, "children": [K.button("CancelButton", "Cancel", "Tertiary")]}]}]}]},
}


def tab(name, label, x, w=128):
    return K.button(name, label, "Tab", slot=R(x, 26, w, 44))


# ------------------------------------------------------------------------------------------------ the panel
SCROLLS = ("Info", "Vitals", "Nutrition", "Nutrients", "Fitness", "Mental", "Wounds", "Conditions")
PANEL = {
    "asset": f"{UI}/WBP_MOStatusPanel",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        K.panel("StatusPanel", 1, [{"type": "CanvasPanel", "name": "StatusCanvas", "slot": FILL, "children": [
            tab("InfoTabButton", "Info", 32, 104), tab("VitalsTabButton", "Vitals", 146, 112), tab("NutritionTabButton", "Nutrition", 268, 132), tab("NutrientsTabButton", "Nutrients", 410, 132),
            tab("FitnessTabButton", "Fitness", 552, 116), tab("MentalTabButton", "Mental", 678, 116), tab("WoundsTabButton", "Wounds", 804, 116), tab("ConditionsTabButton", "Conditions", 930, 144),
            K.button("BackButton", "Back", "Secondary", slot=R(1328, 26, 120, 44)),
            K.divider("StatusRule", slot=R(32, 88, 1416, 1)),
            {"type": "WidgetSwitcher", "name": "CategorySwitcher", "var": True, "slot": R(32, 108, 1416, 700), "children": [
                K.scroll_box(s + "ScrollBox", slot=None) for s in SCROLLS]}]}],
            w=1480, h=840, slot=K.slot_anchored((0.5, 0.5), (0.5, 0.5), left=-740, top=-420, right=1480, bottom=840))]},
}

SPECS = [FIELD, INFO, PANEL]
