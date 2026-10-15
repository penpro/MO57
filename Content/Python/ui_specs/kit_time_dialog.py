"""The dev `time` dialog (UMOTimeOfDayDialog) in the kit look: a 24-hour dial whose hand is dragged (the sky follows it), Dawn / Dusk for golden hour,
Apply moves the server's clock forward to the chosen time. Wes, Oct 15 2026: "add a dusk and dawn time to the time menu for golden hour type screen shots as
well as just a clock where you can grab the hour hand and adjust the time".

No dimmed backdrop and the panel sits at the left: the point is to watch the sky change behind it.

Contract: MOTimeOfDayDialog (Dial, TimeText, ApplyButton, CancelButton; optional TitleText, MessageText, DawnButton, DuskButton). The dial is the native
UMOClockDial (a UWidget over its own Slate leaf, so a real node with its own hit-testing -- not the native-UserWidget spacer trap).

    python Tools/ue.py ui build Content/Python/ui_specs/kit_time_dialog.py
Look at it: `time` in the console popup (dev mode).
"""
import kit_lib as K

UI = "/MOFramework/UI"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
HC = {"horizontal_alignment": "H_ALIGN_CENTER"}
FILL = {"size": {"size_rule": "Fill", "value": 1.0}}
DIAL = 340

CONTENT = {"type": "VerticalBox", "name": "TimeContent", "slot": K.fill_slot(), "children": [
    K.text("TitleText", "title", "Time of day", "color.text.primary", var=True),
    K.text("MessageText", "caption", "Drag the hand: the sky follows. Apply moves the clock forward to it.", "color.text.secondary", wrap=True, var=True,
           slot={"padding": [0, 6, 0, 0]}),
    K.divider("TimeRule", slot={"padding": [0, 14, 0, 0]}),
    {"type": "SizeBox", "name": "DialBox", "width": DIAL, "height": DIAL, "slot": {**HC, "padding": [0, 16, 0, 0]}, "children": [
        {"type": "/Script/MOFramework.MOClockDial", "name": "Dial", "var": True, "props": {
            "diameter": float(DIAL), "snap_minutes": 5,
            "face_color": K.color("color.surface.0", 0.92),
            "day_color": K.color("color.severity.warning"),
            "night_color": K.color("color.severity.info", 0.55),
            "tick_color": K.color("color.border.strong"),
            "hand_color": K.color("color.text.primary"),
            "label_color": K.color("color.text.secondary"),
            "label_font": K.font("caption")}}]},
    K.text("TimeText", "numeric.large", "12:00", "color.text.primary", justification="Center", var=True, slot={**HC, "padding": [0, 10, 0, 0]}),
    {"type": "SizeBox", "name": "GoldenRowBox", "height": 56, "slot": {"padding": [0, 14, 0, 0]}, "children": [
        {"type": "HorizontalBox", "name": "GoldenRow", "children": [
            K.button("DawnButton", "Dawn", "Secondary", slot={**VC, **FILL, "padding": [0, 0, 6, 0]}),
            K.button("DuskButton", "Dusk", "Secondary", slot={**VC, **FILL, "padding": [6, 0, 0, 0]})]}]},
    K.text("GoldenHint", "caption", "Dawn and Dusk put the hand on golden hour.", "color.text.secondary", justification="Center", slot={**HC, "padding": [0, 6, 0, 0]}),
    {"type": "SizeBox", "name": "TimeButtonBox", "height": 64, "slot": {"padding": [0, 16, 0, 0]}, "children": [
        {"type": "HorizontalBox", "name": "TimeButtonRow", "children": [
            K.button("CancelButton", "Cancel", "Tertiary", slot=VC),
            {"type": "Spacer", "name": "TimeButtonSpacer", "slot": FILL},
            K.button("ApplyButton", "Apply", "Primary", slot=VC)]}]}]}

DIALOG = {
    "asset": f"{UI}/WBP_TimeOfDayDialog",
    "parent": "/Script/MOFramework.MOTimeOfDayDialog",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        K.panel("TimePanel", 2, [CONTENT], w=420, padding=[28, 24, 28, 24], slot=K.place(80, 140, 420, 800, "Left", auto_height=True))]},
}

SPECS = [DIALOG]
