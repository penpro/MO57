"""The startup splash cards (UMOSplashWidget): a black field and one centred card (the mark + a tracked caption). The card's content comes from Project Settings > MO Splash Screens;
the widget only fades it.

Contract: UMOSplashWidget (LogoImage; optional CaptionText, CardRoot).

Apply (new asset, nothing to remove first):
    python Tools/ue.py ui build Content/Python/ui_specs/kit_splash.py
"""
import kit_lib as K

UI = "/MOFramework/UI"

CAPTION = K.text("CaptionText", "heading", "PENUMBRA", "color.text.primary", justification="Center", var=True, slot={"horizontal_alignment": "H_ALIGN_CENTER", "padding": [0, 28, 0, 0]})
CAPTION["props"]["font"]["letter_spacing"] = 400   # a studio card is tracked wide: 0.4 em

SPLASH = {
    "asset": f"{UI}/MainMenu_UI/WBP_SplashSequence",
    "parent": "/Script/MOFramework.MOSplashWidget",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "Image", "name": "SplashBackground", "props": {"color_and_opacity": K.color("color.surface.0")}, "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        {"type": "VerticalBox", "name": "CardRoot", "var": True, "slot": K.place(610, 290, 700, 500, "Center"), "children": [
            {"type": "Image", "name": "LogoImage", "var": True, "slot": {"horizontal_alignment": "H_ALIGN_CENTER"}},
            CAPTION]},
    ]},
}

SPECS = [SPLASH]
