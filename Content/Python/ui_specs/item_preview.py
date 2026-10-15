"""WBP_MOItemPreview: the turning 3D view of an item (UMOItemPreviewWidget) -- a square dark plate with the render target in it.
Embedded by the inventory details panel (WBP_ItemInfoPanel, node ItemPreview); any panel can embed it and call SetItem(ItemId).

    python Tools/ue.py ui build Content/Python/ui_specs/item_preview.py
"""
import kit_lib as K

UI = "/MOFramework/UI"

PREVIEW = {
    "asset": UI + "/WBP_MOItemPreview",
    "parent": "/Script/MOFramework.MOItemPreviewWidget",
    "root": {"type": "SizeBox", "name": "PreviewBox", "width": 200, "height": 200, "children": [
        {"type": "Border", "name": "PreviewPlate", "props": {"background": K.brush_9("panel_fill", "color.surface.0"), "brush_color": [1, 1, 1, 1], "padding": 2},
         "children": [
             {"type": "Image", "name": "PreviewImage", "var": True,
              "slot": {"horizontal_alignment": "H_ALIGN_FILL", "vertical_alignment": "V_ALIGN_FILL"}}]}]},
}

# The inventory details panel (C++ UMOItemInfoPanel, optional BindWidget ItemPreview): the turntable to the right of the name / type / weight rows.
INFO_PANEL = {
    "asset": UI + "/WBP_ItemInfoPanel",
    "attach": [
        {"parent": "InfoGrid",
         "node": {"type": UI + "/WBP_MOItemPreview", "name": "ItemPreview", "var": True,
                  "slot": {"column": 2, "row": 0, "row_span": 6, "horizontal_alignment": "H_ALIGN_RIGHT", "vertical_alignment": "V_ALIGN_TOP"}}},
    ],
}

SPECS = [PREVIEW, INFO_PANEL]
