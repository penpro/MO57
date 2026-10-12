"""The inventory slot (WBP_InventorySlot) quantity badge in the Field Ledger kit: a small dark plate with the count in the numeric face, tucked inside the slot's bottom-right corner (it used to be a white
box hanging off the corner). The slot chrome (fill + corner ticks) is code: UMOInventorySlot::ApplySlotLook. Only the nodes below change; every other widget keeps its name and settings.

    python Tools/ue.py ui build Content/Python/ui_specs/kit_slot.py
"""
import kit_lib as K

SPEC = {
    "asset": "/MOFramework/UI/InventorySystem/WBP_InventorySlot",
    "root": {"type": "SizeBox", "name": "SizeBox_412", "children": [
        {"type": "Button", "name": "SlotButton", "var": True, "children": [
            {"type": "Border", "name": "SlotBorder", "var": True, "children": [
                {"type": "Border", "name": "SlotBorderBorder", "var": True, "children": [
                    {"type": "Overlay", "name": "Overlay_0", "children": [
                        {"type": "SizeBox", "name": "QuantityBox", "var": True, "width": 30, "height": 20, "props": {"render_transform": {"translation": {"x": -5.0, "y": -5.0}}},
                         "children": [
                             {"type": "Border", "name": "QuantityBoxBorder", "var": True, "props": {"background": K.brush_9("hud_plate", "color.hud.plate"), "brush_color": [1, 1, 1, 1], "padding": 0}, "children": [
                                 {"type": "Border", "name": "Border_540", "var": True, "props": {"brush_color": [0, 0, 0, 0], "padding": 0}, "children": [
                                     K.text("QuantityText", "numeric.small", "99", "color.text.primary", var=True, justification="Center",
                                            slot={"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"})]}]}]}]}]}]}]}]},
}
SPECS = [SPEC]
