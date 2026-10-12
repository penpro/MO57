"""HUD status effects ("moodles") in the Field Ledger kit (screens/hud.md: Moodles 40,760 300x280 BottomLeft; pill h40: icon 24 + type.hud label + severity shape; stack upward, critical first, > 6 = "+N more").

Entries are plates (hud_plate) with the severity-tinted kit icon (COMPACT by default: the icon alone; the status panel expands them to icon + label + optional stack count + a severity SHAPE glyph (circle-check / diamond / triangle / octagon: severity never rides on colour alone));
a Critical entry also shows the kit's double border and pulses it (UMOStatusMoodleWidget). The strip sorts and folds (UMOStatusEffectStripWidget).

Apply (the old roots are replaced, so remove them first):
    python Tools/ue.py ui remove /MOFramework/UI/HUDElements/WBP_StatusMoodle MoodleBackground
    python Tools/ue.py ui remove /MOFramework/UI/HUDElements/WBP_StatusEffectStrip MoodleBox
    python Tools/ue.py ui build Content/Python/ui_specs/kit_hud_moodles.py
"""
import kit_lib as K

UI = "/MOFramework/UI/HUDElements"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL = K.fill_slot()


MOODLE_ENTRY = {
    "asset": f"{UI}/WBP_StatusMoodle",
    "parent": "/Script/MOFramework.MOStatusMoodleWidget",
    "root": {"type": "SizeBox", "name": "MoodleBackground", "height": 40, "children": [
        {"type": "Overlay", "name": "MoodleOverlay", "children": [
            K.image9("CriticalBorder", "critical_border", "color.severity.critical", slot={"padding": -6, **FILL}, var=True, visibility="Collapsed"),
            K.image9("MoodlePlate", "hud_plate", "color.hud.plate", slot=FILL, var=True),
            {"type": "HorizontalBox", "name": "MoodleRow", "slot": {**VC, "horizontal_alignment": "H_ALIGN_LEFT"}, "children": [
                {"type": "SizeBox", "name": "IconSizeBox", "width": 24, "height": 24, "slot": {**VC, "padding": [8, 0, 8, 0]}, "children": [
                    {"type": "Image", "name": "IconImage", "props": {"brush": K.brush_icon("ic_moodle_cold", "color.text.primary", 24)}}]},
                K.text("LabelText", "hud", "Status", "color.text.primary", var=True, slot={**VC, "padding": [2, 0, 0, 0]}),
                K.text("StackText", "numeric.small", "x2", "color.text.secondary", var=True, slot={**VC, "padding": [6, 0, 0, 0]}),
                {"type": "SizeBox", "name": "ShapeSizeBox", "width": 20, "height": 20, "slot": {**VC, "padding": [10, 0, 12, 0]}, "children": [
                    {"type": "Image", "name": "SeverityShapeImage", "props": {"brush": K.brush_icon("ic_sev_info", "color.severity.info", 20)}}]},
            ]},
        ]}]},
}

STRIP = {
    "asset": f"{UI}/WBP_StatusEffectStrip",
    "parent": "/Script/MOFramework.MOStatusEffectStripWidget",
    "class_defaults": {"moodle_entry_class": f"{UI}/WBP_StatusMoodle"},
    "root": {"type": "VerticalBox", "name": "MoodleBox", "children": [
        {"type": "SizeBox", "name": "OverflowPill", "height": 40, "var": True, "slot": {"horizontal_alignment": "H_ALIGN_LEFT"}, "children": [
            {"type": "Overlay", "name": "OverflowOverlay", "children": [
                K.image9("OverflowPlate", "hud_plate", "color.hud.plate", slot=FILL),
                K.text("OverflowText", "hud", "+2 more", "color.text.secondary", var=True, slot={**VC, "horizontal_alignment": "H_ALIGN_CENTER", "padding": [14, 0, 14, 0]})]}]},
    ]},
}

HUD_ROOT = {
    "asset": f"{UI}/WBP_HUDRoot",
    "attach": [
        {"parent": "CanvasPanel_58",
         "node": {"type": f"{UI}/WBP_StatusEffectStrip", "name": "StatusStrip",
                  "slot": K.slot_anchored((0.0, 1.0), (0.0, 1.0), left=40.0, top=-40.0, right=0.0, bottom=0.0, align=(0.0, 1.0), auto_size=True)}},
    ],
}

SPECS = [MOODLE_ENTRY, STRIP, HUD_ROOT]
