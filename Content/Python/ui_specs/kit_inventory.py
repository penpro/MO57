"""The unified inventory menu (WBP_UnifiedInventory) in the Field Ledger kit: the panel plate, a header (title, hairline, close), a labelled backpack, the transfer column, a tab row and the right panel switcher.

Layout (design: DesignKit screens/inventory.md; the C++ menu keeps its tabbed right panel, so equipment / nearby / details stay tabs for now):
    panel 1600 x 900 centred; header 88 px; left column 76..676 (backpack grid 600 x 600 + Sort); transfer column 736; right column 926..1526 (tab row, switcher 600 x 580).

Contract (parsed from the C++ header): `python Tools/ue.py ui contract MOUnifiedInventoryMenu`. The widget names are the BindWidget contract; the old tree is replaced:

    python Tools/ue.py ui remove /MOFramework/UI/InventorySystem/WBP_UnifiedInventory SizeBox_55
    python Tools/ue.py ui build Content/Python/ui_specs/kit_inventory.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
INV = UI + "/InventorySystem"
FILL = K.fill_slot()
R = K.slot_rect


def tab(name, label, x):
    return K.button(name, label, "Secondary", slot=R(x, 112, 140, 44))


def transfer(name, label, y):
    return K.button(name, label, "Secondary", slot=R(736, y, 88, 56))


CONTENT = [
    # header
    K.text("InventoryTitle", "title", "Inventory", "color.text.primary", slot=R(40, 26, 480, 48)),
    K.button("CloseButton", "Close", "Secondary", slot=R(1440, 28, 120, 44)),
    K.divider("HeaderRule", slot=R(40, 88, 1520, 1)),
    # left column
    K.section_label("BackpackLabel", "Backpack", slot=R(76, 120, 400, 28)),
    {"type": INV + "/WBP_InventoryGrid", "name": "PlayerInventoryGrid", "var": True, "slot": R(76, 160, 600, 590)},
    K.button("StackOrganizePlayerButton", "Sort", "Secondary", slot=R(76, 780, 140, 44)),
    # transfer column
    transfer("FillLeftButton", "<", 240), transfer("FillRightButton", ">", 316), transfer("TakeAllButton", "<<", 392), transfer("StoreAllButton", ">>", 468),
    # right column: tabs + switcher
    tab("OtherInventoryTabButton", "Other", 926), tab("NearbyTabButton", "Nearby", 1076), tab("EquipmentTabButton", "Gear", 1226), tab("DetailsTabButton", "Details", 1376),
    {"type": "WidgetSwitcher", "name": "RightPanelSwitcher", "var": True, "slot": R(926, 170, 600, 580), "children": [
        {"type": INV + "/WBP_InventoryGrid", "name": "SecondaryInventoryGrid", "var": True},
        {"type": UI + "/WBP_ItemInfoPanel", "name": "DetailsPanel", "var": True},
        {"type": INV + "/WBP_NearbyItems", "name": "NearbyPanel", "var": True},
        {"type": INV + "/WBP_EquipmentPanel", "name": "EquipmentPanel", "var": True}]},
]

SPEC = {
    "asset": INV + "/WBP_UnifiedInventory",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "SizeBox", "name": "SizeBox_55", "width": 1600, "height": 900, "slot": K.slot_anchored((0.5, 0.5), (0.5, 0.5), left=-800, top=-450, right=1600, bottom=900), "children": [
            {"type": "Overlay", "name": "PanelOverlay", "children": [
                K.image9("PanelShadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **FILL}),
                K.image9("PanelFill", "panel_fill", "color.surface.1", slot=FILL),
                K.image9("PanelStroke", "panel_stroke", "color.border.subtle", slot=FILL),
                {"type": "CanvasPanel", "name": "RootCanvas_1", "slot": FILL, "children": CONTENT}]}]}]},
}

SPECS = [SPEC]
