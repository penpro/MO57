"""WBP_ItemInfoPanel (UMOItemInfoPanel): the inventory's Details tab in the kit look.

It was an old grid -- labels in one column ("Type:", "Weight:", "Description:"), values in the next, the turntable spanning a third -- so the weight
read "Weight: Weight: 1" (the grid's label plus the code's "Weight: {0}", no unit), the type was two stacked words ("Common" / "Material", the
rarity in grey on the name), and the description ran past the panel's right edge (N-180, Oct 20 2026). Now: a header (icon, name, type, weight)
beside the turntable, a rule, the description wrapping under it; the placeholder ("Click an item for details") alone when nothing is selected.
InfoGrid is the column the C++ shows / hides with the selection.

Contract: MOItemInfoPanel (all optional). Over the old tree (RootCanvas > Border_0 > GridPanel InfoGrid, PlaceholderText):
`python Tools/ue.py ui remove /MOFramework/UI/WBP_ItemInfoPanel Border_0` and `... PlaceholderText`, then
    python Tools/ue.py ui build Content/Python/ui_specs/kit_item_info.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_W = {"size": {"size_rule": "Fill", "value": 1.0}}

HEADER = {"type": "HorizontalBox", "name": "InfoHeader", "children": [
    {"type": "SizeBox", "name": "InfoIconBox", "width": 56, "height": 56, "slot": {"vertical_alignment": "V_ALIGN_TOP", "padding": [0, 4, 14, 0]}, "children": [
        {"type": "Image", "name": "ItemIconImage", "var": True}]},
    {"type": "VerticalBox", "name": "InfoTitles", "slot": FILL_W, "children": [
        K.text("ItemNameText", "title", "River Cobble", "color.text.primary", wrap=True, var=True),
        K.text("ItemTypeText", "label", "Material", "color.text.secondary", var=True, slot={"padding": [0, 4, 0, 0]}),
        {"type": "HorizontalBox", "name": "WeightRow", "slot": {"padding": [0, 12, 0, 0]}, "children": [
            K.text("WeightLabel", "label", "Weight", "color.text.secondary", slot={**VC, "padding": [0, 0, 10, 0]}),
            K.text("WeightText", "body", "1 kg", "color.text.primary", var=True, slot=VC)]}]},
    {"type": UI + "/WBP_MOItemPreview", "name": "ItemPreview", "var": True, "slot": {"vertical_alignment": "V_ALIGN_TOP", "padding": [16, 0, 0, 0]}},
]}

INFO = {"type": "VerticalBox", "name": "InfoGrid", "var": True, "children": [
    HEADER,
    K.divider("InfoRule", slot={"padding": [0, 16, 0, 12]}),
    K.text("DescriptionText", "body", "A smooth, rounded stone worn by river currents over millennia.", "color.text.secondary", wrap=True, var=True),
]}

PANEL = {
    "asset": f"{UI}/WBP_ItemInfoPanel",
    "parent": "/Script/MOFramework.MOItemInfoPanel",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "Overlay", "name": "InfoRoot", "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0)), "children": [
            {**INFO, "slot": {"horizontal_alignment": "H_ALIGN_FILL", "vertical_alignment": "V_ALIGN_TOP", "padding": [18, 16, 18, 16]}},
            K.text("PlaceholderText", "body", "Click an item for details", "color.text.secondary", justification="Center", wrap=True, var=True,
                   slot={"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"}),
        ]},
    ]},
}

SPECS = [PANEL]
