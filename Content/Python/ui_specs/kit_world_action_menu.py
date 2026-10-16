"""The world-action menu (UMOWorldActionMenu: right-click on a carcass -- anything implementing IMOWorldActionProvider) in the kit look.

New Oct 16 2026 (Steam playtest: "right clicking deer starts butchering immediately instead of giving option to click"): a heading (the target's name), a
rule, then the target's actions as runtime buttons (UMOUIUtils::RebuildActionButtons, inserted after StyleButton, which is collapsed at runtime and only lends
its look). A deer lists 13 actions, so the list scrolls past 560 px instead of running off the screen.

Contract: MOWorldActionMenu (ButtonContainer required; TitleText, StyleButton optional).
    python Tools/ue.py ui build Content/Python/ui_specs/kit_world_action_menu.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
BTN = {"padding": [0, 6, 0, 0]}

LIST = {"type": "SizeBox", "name": "ActionListSize", "props": {"max_desired_height": 560.0}, "children": [
    K.scroll_box("ButtonContainer", children=[K.button("StyleButton", "Action", "Secondary", slot=BTN)]),
]}

COLUMN = {"type": "VerticalBox", "name": "ActionColumn", "slot": K.fill_slot(), "children": [
    K.text("TitleText", "heading", "Deer Carcass", "color.text.primary", var=True),
    K.divider("ActionRule", slot={"padding": [0, 8, 0, 2]}),
    LIST,
]}

MENU = {
    "asset": f"{UI}/WorldActions/WBP_WorldActionMenu",
    "parent": "/Script/MOFramework.MOWorldActionMenu",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        K.panel("ActionPanel", 2, [COLUMN], w=300, padding=[14, 12, 14, 14], slot=K.slot_anchored((0.0, 0.0), (0.0, 0.0), right=300.0, bottom=200.0, auto_size=True)),
    ]},
}

SPECS = [MENU]
