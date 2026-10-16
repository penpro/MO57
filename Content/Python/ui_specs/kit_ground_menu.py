"""The ground / forage menu (UMOGroundContextMenu: E on bare ground or plain cover) in the kit look: a plate that grows with its buttons.

It was a fixed 300 x 309 box with a 20 % black border and dark grey text: "Forage", the range and the skill line were unreadable over grass and sand,
and the runtime "Gather ..." button (UMOForagingSubsystem::FindGatherableCover, inserted under Dig) had no room (Oct 16 2026). Same shape as
kit_station_menu.py. ButtonContainer is the column the runtime buttons go into.

Contract: MOGroundContextMenu (ButtonContainer, SearchNearbyButton, DigForSuppliesButton required; RadiusText, SkillLevelText optional).
Over the old tree: `python Tools/ue.py ui remove /MOFramework/UI/GroundSearch/WBP_GroundContextMenu SizeBox_39` first, then
    python Tools/ue.py ui build Content/Python/ui_specs/kit_ground_menu.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
BTN = {"padding": [0, 6, 0, 0]}

COLUMN = {"type": "VerticalBox", "name": "ButtonContainer", "slot": K.fill_slot(), "children": [
    K.text("ForageTitle", "heading", "Forage", "color.text.primary"),
    K.text("RadiusText", "caption", "Range: 10 m", "color.text.secondary", var=True, slot={"padding": [0, 2, 0, 0]}),
    K.text("SkillLevelText", "caption", "Foraging: 0", "color.text.secondary", var=True),
    K.divider("ForageRule", slot={"padding": [0, 8, 0, 2]}),
    K.button("SearchNearbyButton", "Search", "Secondary", slot=BTN),
    K.button("DigForSuppliesButton", "Dig", "Secondary", slot=BTN),
]}

GROUND = {
    "asset": f"{UI}/GroundSearch/WBP_GroundContextMenu",
    "parent": "/Script/MOFramework.MOGroundContextMenu",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        K.panel("ForagePanel", 2, [COLUMN], w=240, padding=[14, 12, 14, 14], slot=K.slot_anchored((0.0, 0.0), (0.0, 0.0), right=240.0, bottom=200.0, auto_size=True)),
    ]},
}

SPECS = [GROUND]
