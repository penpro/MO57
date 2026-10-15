"""The HUD in the Field Ledger kit (DesignKit screens/hud.md): mode indicator, tutorial banner, quest tracker, notification, timed-action progress (harvest / inspection), FPS counter, wind indicator.
Moodles are kit_hud_moodles.py; reticle and breath meter are C++ (UMOReticleWidget / UMOBreathMeterWidget). Every colour / size comes from the tokens through kit_lib.

Each existing tree is replaced (names are the C++ BindWidget contracts: `python Tools/ue.py ui contract <Class>`), so remove the old roots first:

    for a in "WBP_ModeIndicator RootCanvas" "Quest/WBP_TutorialHint CanvasPanel_70" "Quest/WBP_QuestHUD RootCanvas" "Quest/WBP_QuestTrackerEntry SizeBox_61" "WBP_Notification RootCanvas" \
             "WBP_HarvestProgress RootCanvas" "WBP_InspectionProgress RootCanvas" "WBP_FPSCounter RootCanvas"; do set -- $a; python Tools/ue.py ui remove /MOFramework/UI/$1 $2; done
    python Tools/ue.py ui build Content/Python/ui_specs/kit_hud.py

The widgets keep placing themselves (full-screen canvas root, point anchor + alignment from the kit rect: K.place), exactly as before.
"""
import kit_lib as K

UI = "/MOFramework/UI"
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
PLATE = lambda n: K.image9(n, "hud_plate", "color.hud.plate", slot=FILL)


def box(name, w, h, slot, children, min_h=None, min_w=None):
    """A SizeBox holding an Overlay of the given children (the plate first): fixed width w, or -- w None -- as wide as its content,
    at least min_w (the canvas slot must then auto-size too)."""
    node = {"type": "SizeBox", "name": name, "slot": slot, "children": [{"type": "Overlay", "name": name + "Overlay", "children": children}]}
    node["width"] = w   # None clears a fixed width an existing widget may still carry
    if h is not None:
        node["height"] = h
    props = {}
    if min_h is not None:
        props["min_desired_height"] = min_h
    if min_w is not None:
        props["min_desired_width"] = min_w
    if props:
        node["props"] = props
    return node


# ---------------------------------------------------------------- mode indicator (40,164 220x36 TopLeft)
# As wide as its label, at least 220: "EXPLORING LOCUST WOODLAND" ran past a fixed 220 px plate (Wes, Oct 15 2026), and biome names vary.
MODE = {
    "asset": f"{UI}/WBP_ModeIndicator",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        box("ModeBox", None, 36, K.place(40, 164, 220, 36, "TopLeft", auto_height=True), min_w=220, children=[
            PLATE("ModePlate"),
            K.text("ModeText", "label", "EXPLORE", "color.text.primary", var=True, slot={**VC, "padding": [14, 0, 14, 0]})])]},
}

# ---------------------------------------------------------------- tutorial banner (680,40 560x112 TopCenter)
TUTORIAL = {
    "asset": f"{UI}/Quest/WBP_TutorialHint",
    "root": {"type": "CanvasPanel", "name": "CanvasPanel_70", "children": [
        box("HintBox", 560, None, K.place(680, 40, 560, 112, "TopCenter", auto_height=True), [
            PLATE("HintPlate"),
            K.solid("HintRule", "color.accent", h=2, slot={"vertical_alignment": "V_ALIGN_TOP", "horizontal_alignment": "H_ALIGN_FILL"}),
            {"type": "VerticalBox", "name": "HintColumn", "slot": {"padding": [24, 18, 24, 16]}, "children": [
                K.text("TitleText", "heading", "Tutorial", "color.text.primary", var=True, kind="CommonTextBlock"),
                K.text("BodyText", "body", "Hint", "color.text.secondary", wrap=True, var=True, kind="CommonTextBlock", slot={"padding": [0, 6, 0, 0]})]}],
            min_h=112)]},
}

# ---------------------------------------------------------------- quest tracker (1520,392 360 wide TopRight) + its entry
QUEST_HUD = {
    "asset": f"{UI}/Quest/WBP_QuestHUD",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        box("QuestBox", 360, None, K.place(1520, 392, 360, 152, "TopRight", auto_height=True), [
            PLATE("QuestPlate"),
            {"type": "VerticalBox", "name": "TrackedQuestsContainer", "var": True, "slot": {"padding": 16}}])]},
}

QUEST_ENTRY = {
    "asset": f"{UI}/Quest/WBP_QuestTrackerEntry",
    "root": {"type": "SizeBox", "name": "SizeBox_61", "width": 328, "children": [
        {"type": "VerticalBox", "name": "EntryColumn", "children": [
            K.text("QuestNameText", "body.strong", "Quest", "color.text.primary", wrap=True, var=True),
            K.text("ObjectiveText", "body", "Objective", "color.text.secondary", wrap=True, var=True, slot={"padding": [0, 2, 0, 8]}),
            {"type": "HorizontalBox", "name": "ProgressRow", "children": [
                {"type": "SizeBox", "name": "BarBox", "height": 8, "slot": {"size": {"size_rule": "Fill", "value": 1.0}, **VC}, "children": [K.progress_bar("ProgressBar")]},
                K.text("ProgressText", "numeric.small", "0/1", "color.text.secondary", var=True, slot={**VC, "padding": [10, 0, 0, 0]})]},
            {"type": "Spacer", "name": "EntryGap", "props": {"size": {"x": 1.0, "y": 12.0}}}]}]},
}

# ---------------------------------------------------------------- notification (1480,40 400 wide TopRight)
NOTIFICATION = {
    "asset": f"{UI}/WBP_Notification",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "SizeBox", "name": "NotifBox", "width": 400, "props": {"min_desired_height": 72}, "slot": K.place(1480, 40, 400, 72, "TopRight", auto_height=True), "children": [
            {"type": "Overlay", "name": "NotifOverlay", "children": [
                {"type": "Border", "name": "BackgroundBorder", "var": True, "props": {"background": K.brush_9("hud_plate", None), "brush_color": K.color("color.hud.plate"), "padding": [20, 14, 16, 14]},
                 "slot": FILL, "children": [
                     {"type": "HorizontalBox", "name": "NotifRow", "children": [
                         {"type": "SizeBox", "name": "NotifIconBox", "width": 24, "height": 24, "slot": {**VC, "padding": [0, 0, 12, 0]}, "children": [
                             {"type": "Image", "name": "IconImage", "var": True, "props": {"brush": K.brush_icon("ic_sev_info", "color.severity.info", 24)}}]},
                         K.text("MessageText", "body", "Notification", "color.text.primary", wrap=True, var=True, slot={**VC, "size": {"size_rule": "Fill", "value": 1.0}})]}]},
                {"type": "SizeBox", "name": "SeverityBarBox", "width": 4, "slot": {"horizontal_alignment": "H_ALIGN_LEFT", "vertical_alignment": "V_ALIGN_FILL"}, "children": [
                    {"type": "Border", "name": "SeverityBar", "var": True, "props": {"brush_color": K.color("color.severity.info")}}]}]}]}]},
}


# ---------------------------------------------------------------- timed action (760,628 400x76): harvest / inspection share one layout
def timed_action(asset):
    return {
        "asset": f"{UI}/{asset}",
        "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
            box("ActionBox", 400, 76, K.place(760, 628, 400, 76, "Center"), [
                PLATE("ActionPlate"),
                {"type": "VerticalBox", "name": "ActionColumn", "slot": {"padding": [16, 10, 16, 8]}, "children": [
                    {"type": "HorizontalBox", "name": "ActionTop", "children": [
                        K.text("ActionNameText", "hud", "Action", "color.text.primary", var=True, slot={**VC, "size": {"size_rule": "Fill", "value": 1.0}}),
                        K.text("TimeRemainingText", "numeric", "0.0s", "color.text.primary", var=True, slot=VC)]},
                    {"type": "SizeBox", "name": "ActionBarBox", "height": 8, "slot": {"padding": [0, 6, 0, 4]}, "children": [K.progress_bar("ProgressBar")]},
                    K.text("InstructionText", "caption", "Move to cancel", "color.text.secondary", var=True)]}])]},
    }


# ---------------------------------------------------------------- FPS counter (dev builds; 1736,8 176x24 TopRight)
FPS = {
    "asset": f"{UI}/WBP_FPSCounter",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        box("FpsBox", 176, 24, K.place(1736, 8, 176, 24, "TopRight"), [
            PLATE("FpsPlate"),
            {"type": "HorizontalBox", "name": "FpsRow", "slot": {**VC, "padding": [8, 0, 8, 0]}, "children": [
                K.text("FPSText", "numeric.small", "0", "color.text.primary", var=True, slot=VC)]}])]},
}

# ---------------------------------------------------------------- wind indicator (kit rect 1792,912 88x128 BottomRight; widened to 150 so the descriptive label wraps instead of clipping)
WIND = {
    "asset": f"{UI}/HUDElements/WBP_WindDirection",
    "parent": "/Script/MOFramework.MOWindDirectionWidget",
    "root": {"type": "SizeBox", "name": "WindRoot", "width": 150, "children": [
        {"type": "VerticalBox", "name": "WindColumn", "children": [
            {"type": "SizeBox", "name": "WindDialBox", "width": 64, "height": 64, "slot": {"horizontal_alignment": "H_ALIGN_RIGHT", "padding": [0, 0, 12, 0]}, "children": [
                {"type": "Overlay", "name": "WindDial", "children": [
                    K.icon_node("WindRing", "ic_wind_ring", "color.text.secondary", 64, slot=FILL),
                    {"type": "SizeBox", "name": "WindArrowBox", "width": 32, "height": 32, "slot": {"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"}, "children": [
                        K.icon_node("WindArrow", "ic_wind_arrow", "color.text.primary", 32, var=True)]}]}]},
            K.text("WindLabel", "caption", "Wind", "color.text.secondary", justification="Right", wrap=True, var=True, slot={"horizontal_alignment": "H_ALIGN_RIGHT", "padding": [0, 6, 0, 0]})]}]},
}

HUD_ROOT = {
    "asset": f"{UI}/HUDElements/WBP_HUDRoot",
    "attach": [{"parent": "CanvasPanel_58", "node": {"type": f"{UI}/HUDElements/WBP_WindDirection", "name": "WindIndicator", "var": True, "slot": K.place(1730, 912, 150, 128, "BottomRight", auto_height=True)}}],
}

SPECS = [MODE, TUTORIAL, QUEST_HUD, QUEST_ENTRY, NOTIFICATION, timed_action("WBP_HarvestProgress"), timed_action("WBP_InspectionProgress"), FPS, WIND, HUD_ROOT]
