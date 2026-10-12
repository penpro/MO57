"""Options panel: graphics quality + VSync rows.

Contract (C++): MOOptionsPanel.h -> QualityComboBox (UComboBoxString, filled with Low / Medium / High / Epic by PopulateQualityOptions) and VSyncCheckbox (UCheckBox);
both BindWidgetOptional. Each row copies the layout of the Resolution / Full Screen rows in the Graphics section (a 300-wide label cell padded 200 from the left, then the control).
The rows are appended to the end of the Graphics section: there is no reorder / reparent API, so inserting above Resolution would mean rebuilding the later siblings.

Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/options_graphics.py   (idempotent; PIE must not be running)
"""
OPTIONS = "/MOFramework/UI/WBP_MOOptionsPanel"
LABEL_GREY = [0.2789, 0.2789, 0.2789, 1.0]


def label_cell(size_name, text_name, text):
    return {"type": "SizeBox", "name": size_name, "props": {"width_override": 300.0}, "slot": {"padding": [200, 0, 0, 0]},
            "children": [{"type": "TextBlock", "name": text_name, "text": text, "color": LABEL_GREY,
                          "slot": {"horizontal_alignment": "Left"}}]}


QUALITY_ROW = {"type": "HorizontalBox", "name": "GraphicsVertHorizontalBox_5", "children": [
    label_cell("SizeBox_QualityLabel", "TextBlock_QualityLabel", "Graphics Quality"),
    {"type": "SizeBox", "name": "SizeBox_QualityCombo", "props": {"width_override": 350.0},
     "children": [{"type": "ComboBoxString", "name": "QualityComboBox"}]},
]}

VSYNC_ROW = {"type": "HorizontalBox", "name": "GraphicsVertHorizontalBox_6", "children": [
    label_cell("SizeBox_VSyncLabel", "TextBlock_VSyncLabel", "VSync"),
    {"type": "CheckBox", "name": "VSyncCheckbox"},
]}

OPTIONS_PANEL = {
    "asset": OPTIONS,
    "attach": [
        {"parent": "GraphicsVert", "node": QUALITY_ROW},
        {"parent": "GraphicsVert", "node": VSYNC_ROW},
    ],
}

SPECS = [OPTIONS_PANEL]
