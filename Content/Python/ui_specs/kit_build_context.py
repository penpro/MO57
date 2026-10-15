"""The building site's context menu (WBP_BuildContext, UMOGhostContextMenu) in the kit look -- right-click a placed ghost: Build,
Add Materials, where the materials come from, Cancel, the material list and the build timer.

It had no panel: its labels and timer were dark text drawn straight over the world (first-ten-minutes pass, Oct 15 2026), and its
third check box was named "SurroundingsCheckbox" while the C++ binds "SurroundingCheckbox" -- the box did nothing and showed
unchecked while the surrounding area was always used (renamed before this spec). Every widget keeps its name and order, so the
canvas position the menu is placed at and any graph reference survive.

    python Tools/ue.py ui build Content/Python/ui_specs/kit_build_context.py
"""
import kit_lib as K

BS = "/MOFramework/UI/BuildSystem"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_H = {"horizontal_alignment": "H_ALIGN_FILL"}


def source(row, box, label_name, label):
    return {"type": "HorizontalBox", "name": row, "slot": {**FILL_H, "padding": [0, 6, 0, 0]}, "children": [
        K.checkbox(box, slot=VC),
        K.text(label_name, "body", label, "color.text.primary", var=True, justification="Left", slot={**VC, "padding": [10, 0, 0, 0], "size": {"size_rule": "Fill", "value": 1.0}})]}


BUILD_CONTEXT = {
    "asset": f"{BS}/WBP_BuildContext",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "SizeBox", "name": "SizeBox_58", "children": [
            {"type": "Border", "name": "Border_0", "var": True,
             "props": {"background": K.brush_9("panel_fill", "color.surface.2"), "brush_color": [1, 1, 1, 1], "padding": [16, 14, 16, 14]},
             "children": [
                 {"type": "VerticalBox", "name": "VerticalBox_79", "children": [
                     K.button("BuildButton", "Build", "Primary", slot={**FILL_H, "padding": [0, 0, 0, 6]}),
                     K.button("AddMaterialsButton", "Add Materials", "Secondary", slot={**FILL_H, "padding": [0, 0, 0, 6]}),
                     source("HorizontalBox_0", "InventoryCheckbox", "TextBlock_202", "Inventory"),
                     source("HorizontalBox_1", "ContainersCheckbox", "TextBlock_341", "Nearby containers"),
                     source("HorizontalBox_2", "SurroundingCheckbox", "TextBlock_519", "Surrounding area"),
                     K.button("CancelButton", "Cancel", "Tertiary", slot={**FILL_H, "padding": [0, 10, 0, 0]}),
                     {"type": "VerticalBox", "name": "MaterialListContainer", "var": True, "slot": {"padding": [0, 10, 0, 4]}},
                     {"type": "ProgressBar", "name": "BuildProgressBar", "var": True,
                      "props": {"fill_color_and_opacity": K.color("color.accent")}, "slot": {"padding": [0, 4, 0, 4]}},
                     K.text("BuildTimeText", "numeric", "Time", "color.text.secondary", var=True, slot={"horizontal_alignment": "H_ALIGN_CENTER"}),
                 ]}]}]}]},
}

SPECS = [BUILD_CONTEXT]
