"""The station menu (UMOStationContextMenu: E on a campfire, a kiln ...) in the kit look: a plate that grows with its buttons, the station's name and fuel on it.

It was a fixed 200 x 252 transparent box: its header floated unreadable over the scene, and the fire's runtime action buttons (FMOFireActions: add a fuel, put it
out -- inserted under LightButton) overflowed it and ran off the bottom of the screen (Oct 15 2026). The panel auto-sizes, so the base class's on-screen clamp
measures the real menu.

Contract: MOStationContextMenu (all optional: StationNameText, FuelTimeText, OpenButton, CraftButton, LightButton).
Over the old tree: `python Tools/ue.py ui remove /MOFramework/UI/CraftingSystem/WBP_StationMenu Border_0` first, then
    python Tools/ue.py ui build Content/Python/ui_specs/kit_station_menu.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
BTN = {"padding": [0, 6, 0, 0]}

COLUMN = {"type": "VerticalBox", "name": "StationColumn", "slot": K.fill_slot(), "children": [
    K.text("StationNameText", "heading", "Campfire", "color.text.primary", var=True),
    K.text("FuelTimeText", "caption", "No fuel", "color.text.secondary", var=True, slot={"padding": [0, 2, 0, 0]}),
    K.divider("StationRule", slot={"padding": [0, 8, 0, 2]}),
    K.button("OpenButton", "Open", "Secondary", slot=BTN),
    K.button("CraftButton", "Craft", "Secondary", slot=BTN),
    K.button("LightButton", "Light", "Primary", slot=BTN),
]}

STATION = {
    "asset": f"{UI}/CraftingSystem/WBP_StationMenu",
    "parent": "/Script/MOFramework.MOStationContextMenu",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        K.panel("StationPanel", 2, [COLUMN], w=260, padding=[14, 12, 14, 14], slot=K.slot_anchored((0.0, 0.0), (0.0, 0.0), right=260.0, bottom=200.0, auto_size=True)),
    ]},
}

SPECS = [STATION]
