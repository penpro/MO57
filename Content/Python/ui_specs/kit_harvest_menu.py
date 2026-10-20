"""The resource menu (UMOKeepOnHarvestContextMenu: right-click on a tree, a bush, a rock) in the kit look: a plate that grows with its buttons.

It was the one context menu never rebuilt in the kit: a fixed box whose title ("Dead Shrub", "Black Alder Tree") sat washed out over the plate's top
edge, with ~80 px of empty plate under the buttons (N-078 first-ten-minutes pass, Oct 19 2026). Same shape as kit_ground_menu.py / kit_station_menu.py.
The runtime buttons (Inspect, one per harvest action, Chop Down) are appended to ButtonContainer after StyleButton, which is collapsed at runtime and
only lends them its style and slot padding.

Contract: MOKeepOnHarvestContextMenu (ButtonContainer required; TargetNameText, StyleButton optional).
Over the old tree: `python Tools/ue.py ui dump /MOFramework/UI/WBP_HarvestContextMenu --no-props` for its root, `ui remove` that root, then
    python Tools/ue.py ui build Content/Python/ui_specs/kit_harvest_menu.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
BTN = {"padding": [0, 6, 0, 0]}

COLUMN = {"type": "VerticalBox", "name": "ButtonContainer", "var": True, "slot": K.fill_slot(), "children": [
    K.text("TargetNameText", "heading", "Black Alder Tree", "color.text.primary", wrap=True, var=True),
    K.divider("HarvestRule", slot={"padding": [0, 8, 0, 2]}),
    K.button("StyleButton", "Action", "Secondary", slot=BTN),
]}

HARVEST = {
    "asset": f"{UI}/WBP_HarvestContextMenu",
    "parent": "/Script/MOFramework.MOKeepOnHarvestContextMenu",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        K.panel("HarvestPanel", 2, [COLUMN], w=260, padding=[14, 12, 14, 14], slot=K.slot_anchored((0.0, 0.0), (0.0, 0.0), right=260.0, bottom=200.0, auto_size=True)),
    ]},
}

SPECS = [HARVEST]
