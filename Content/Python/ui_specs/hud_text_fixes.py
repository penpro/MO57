"""HUD text legibility, found by looking at the packaged build:

1. Tutorial banner (WBP_TutorialHint): the title / body were dark grey (0.26) with no backing panel, so on a bright sky they read as a faint rendering fault ("Possession Menu / Press [P] ..." at the top of the
   screen for the whole start). Now white text on a dark translucent panel, top-centre.
   Apply: python Tools/ue.py ui remove /MOFramework/UI/Quest/WBP_TutorialHint VerticalBox_41   (the tool cannot swap a node in place), then build this spec.
2. Quest tracker entry (WBP_QuestTrackerEntry): fixed-position text boxes 280 px wide; an objective like "Inspect a tree to learn about its resources" ran past the panel's edge. The objective and the quest
   name now wrap, with room for two lines (fixed offsets stay: the entry is a canvas), and the progress row sits below them. The progress bar widget was named ProgressBar_0, so the C++ `ProgressBar`
   member (optional) was never bound and the bar never filled: renamed with scripts (see the commit message).

Apply: python Tools/ue.py ui build Content/Python/ui_specs/hud_text_fixes.py
"""
UI = "/MOFramework/UI/Quest"
WHITE = [1.0, 1.0, 1.0, 1.0]
SOFT = [0.92, 0.92, 0.92, 1.0]


def offsets(left, top, width, height):
    return {"layout_data": {"offsets": {"left": float(left), "top": float(top), "right": float(width), "bottom": float(height)}}}


TUTORIAL_HINT = {
    "asset": f"{UI}/WBP_TutorialHint",
    "attach": [
        {"parent": "CanvasPanel_70",
         "node": {"type": "Border", "name": "HintPanel", "color": [0.0, 0.0, 0.0, 0.62], "props": {"padding": [28, 14]},
                  "slot": {"layout_data": {"anchors": {"minimum": {"x": 0.5, "y": 0.0}, "maximum": {"x": 0.5, "y": 0.0}},
                                           "offsets": {"left": 0.0, "top": 90.0, "right": 0.0, "bottom": 0.0},
                                           "alignment": {"x": 0.5, "y": 0.0}},
                           "auto_size": True},
                  "children": [
                      {"type": "VerticalBox", "name": "HintColumn", "children": [
                          {"type": "CommonTextBlock", "name": "TitleText", "text": "Tutorial", "font_size": 30, "props": {"justification": "Center", "color_and_opacity": {"specified_color": WHITE}}},
                          {"type": "CommonTextBlock", "name": "BodyText", "text": "Hint", "font_size": 20, "props": {"justification": "Center", "color_and_opacity": {"specified_color": SOFT}}, "slot": {"padding": [0, 6, 0, 0]}},
                      ]}]}},
    ],
}

TRACKER_ENTRY = {
    "asset": f"{UI}/WBP_QuestTrackerEntry",
    "root": {"type": "SizeBox", "name": "SizeBox_61", "width": 300, "height": 108},
    "attach": [
        {"parent": "RootCanvas", "node": {"type": "TextBlock", "name": "QuestNameText", "props": {"auto_wrap_text": True}, "slot": offsets(4, 4, 288, 22)}},
        {"parent": "RootCanvas", "node": {"type": "TextBlock", "name": "ObjectiveText", "props": {"auto_wrap_text": True}, "slot": offsets(4, 30, 288, 34)}},
        {"parent": "RootCanvas", "node": {"type": "TextBlock", "name": "ProgressText", "slot": offsets(4, 68, 150, 18)}},
        {"parent": "RootCanvas", "node": {"type": "ProgressBar", "name": "ProgressBar", "props": {"fill_color_and_opacity": [0.0, 0.5, 1.0, 1.0]}, "slot": offsets(8, 90, 280, 10)}},
    ],
}

SPECS = [TUTORIAL_HINT, TRACKER_ENTRY]
