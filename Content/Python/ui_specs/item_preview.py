"""WBP_MOItemPreview: the turning 3D view of an item (UMOItemPreviewWidget) -- a square dark plate with the render target in it.
Embedded by the inventory details panel (WBP_ItemInfoPanel, node ItemPreview: kit_item_info.py); any panel can embed it and call SetItem(ItemId).

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

# The inventory details panel (WBP_ItemInfoPanel) embeds it in its own spec now: ui_specs/kit_item_info.py (Oct 20 2026).

SPECS = [PREVIEW]
