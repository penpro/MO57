"""The kit button (WBP_MOCommonButton, the MOButton every screen uses): label = a plain TextBlock (the WBP's UpdateButtonText graph owns it as a Text object; the C++ button pushes the style's per-state font and colour into any text block it finds), the kit focus ring in the button's own
overlay (shown only for keyboard / gamepad focus), the look = the CommonButtonStyle (Content/Python/kit_styles.py).

Apply (the label is replaced, so remove the old plain TextBlock first):
    python Tools/ue.py ui remove /MOFramework/UI/WBP_MOCommonButton ButtonOverlay
    python Tools/ue.py ui build Content/Python/ui_specs/kit_button.py
"""
import kit_lib as K

SPECS = [{
    "asset": "/MOFramework/UI/WBP_MOCommonButton",
    # the widget's class-default Color and Opacity was 0.356 (a leftover): it multiplied EVERY button's brush and label to a dim grey. Buttons must render at the colours the style / tokens say.
    "class_defaults": {"color_and_opacity": [1.0, 1.0, 1.0, 1.0]},
    "attach": [{
        "parent": "SizeBox_87",
        "node": {"type": "Overlay", "name": "ButtonOverlay", "children": [
            K.image9("FocusRing", "focus_ring", "color.focus.ring", slot={"padding": -4, **K.fill_slot()}, var=True, visibility="Collapsed"),
            {"type": "TextBlock", "name": "ButtonText", "var": True, "text": "Button",
             "slot": {"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"}},
        ]},
    }],
}]
