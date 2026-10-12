"""HUD status effects ("moodles"): the strip and its entry widget, placed in the HUD root.

The code already pushes moodles (thermal comfort, wetness, ...: UMOCharacterUIController::Refresh*Moodle -> UMOStatusEffectStripWidget::AddOrUpdateMoodle), but the Widget Blueprints the contract needs did not exist
(`ui check WBP_HUDRoot`: absent StatusStrip), so no status icon was ever shown. Contracts (C++): MOStatusMoodleWidget.h (IconImage required; LabelText, StackText optional), MOStatusEffectStripWidget.h
(MoodleBox required; MoodleEntryClass), MOHUDRootWidget.h (StatusStrip optional).

Each entry is a dark pill: the severity-tinted icon (a solid tinted square until icon textures are authored: the moodle sources pick them from designer arrays that are empty today) + the label. The strip hides
itself when it has no moodle.

Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/hud_moodles.py   (idempotent; PIE must not be running)
"""
UI = "/MOFramework/UI/HUDElements"
WHITE = [0.95, 0.95, 0.95, 1.0]

MOODLE_ENTRY = {
    "asset": f"{UI}/WBP_StatusMoodle",
    "parent": "/Script/MOFramework.MOStatusMoodleWidget",
    "root": {"type": "Border", "name": "MoodleBackground", "color": [0.0, 0.0, 0.0, 0.55], "props": {"padding": [10, 5]}, "children": [
        {"type": "HorizontalBox", "name": "MoodleRow", "children": [
            {"type": "SizeBox", "name": "IconSizeBox", "width": 22, "height": 22, "slot": {"vertical_alignment": "Center", "padding": [0, 0, 8, 0]}, "children": [
                {"type": "Image", "name": "IconImage"}]},
            {"type": "TextBlock", "name": "LabelText", "text": "Status", "font_size": 18, "color": WHITE, "slot": {"vertical_alignment": "Center"}},
            {"type": "TextBlock", "name": "StackText", "text": "x2", "font_size": 14, "color": [0.8, 0.8, 0.8, 1.0], "slot": {"vertical_alignment": "Center", "padding": [6, 0, 0, 0]}},
        ]}]},
}

STRIP = {
    "asset": f"{UI}/WBP_StatusEffectStrip",
    "parent": "/Script/MOFramework.MOStatusEffectStripWidget",
    "class_defaults": {"moodle_entry_class": f"{UI}/WBP_StatusMoodle"},
    "root": {"type": "HorizontalBox", "name": "MoodleBox"},
}

HUD_ROOT = {
    "asset": f"{UI}/WBP_HUDRoot",
    "attach": [
        {"parent": "CanvasPanel_58",
         "node": {"type": f"{UI}/WBP_StatusEffectStrip", "name": "StatusStrip",
                  "slot": {"layout_data": {"anchors": {"minimum": {"x": 0.0, "y": 1.0}, "maximum": {"x": 0.0, "y": 1.0}},
                                           "offsets": {"left": 28.0, "top": -28.0, "right": 0.0, "bottom": 0.0},
                                           "alignment": {"x": 0.0, "y": 1.0}},
                           "auto_size": True}}},
    ],
}

SPECS = [MOODLE_ENTRY, STRIP, HUD_ROOT]
