"""WBP_SkillToast: the HUD toast for a skill or knowledge change (UMOSkillToastWidget; UMONotificationComponent's skill-popup queue shows it).

    python Tools/ue.py ui build Content/Python/ui_specs/kit_skill_toast.py

Until Oct 20 2026 the queue floated the Skills panel's row widget (WBP_SkillEntry) over the world at 20% / 80% of the screen: no plate, faint
grey sub-text, a near-empty black bar, right behind the pawn (Wes: "the ui for the skill-ups/knowledge-ups sucks and is hard to read"). This is
the text notification's look (hud plate, a leading accent bar, a glyph) in the same right-hand column: the notification sits at 40, this at 196,
which leaves room for a three-line notification above and ends well clear of the quest tracker (392). The headline says what changed
("Woodworking +2 XP", "New knowledge: Wood Properties"), the detail the standing ("Level 0 - 2 / 100 XP to level 1"), the bar the progress.
"""
import kit_lib as K

UI = "/MOFramework/UI"
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}

SKILL_TOAST = {
    "asset": f"{UI}/WBP_SkillToast",
    "parent": "/Script/MOFramework.MOSkillToastWidget",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "SizeBox", "name": "ToastBox", "width": 400, "props": {"min_desired_height": 80}, "slot": K.place(1480, 196, 400, 80, "TopRight", auto_height=True), "children": [
            {"type": "Overlay", "name": "ToastOverlay", "children": [
                {"type": "Border", "name": "ToastPlate", "props": {"background": K.brush_9("hud_plate", None), "brush_color": K.color("color.hud.plate"), "padding": [20, 12, 18, 14]},
                 "slot": FILL, "children": [
                     {"type": "HorizontalBox", "name": "ToastRow", "children": [
                         {"type": "SizeBox", "name": "ToastIconBox", "width": 28, "height": 28, "slot": {**VC, "padding": [0, 0, 14, 0]}, "children": [
                             {"type": "Image", "name": "ToastIcon", "var": True, "props": {"brush": K.brush_icon("ic_ui_skill", "color.accent", 28)}}]},
                         {"type": "VerticalBox", "name": "ToastColumn", "slot": {**VC, "size": {"size_rule": "Fill", "value": 1.0}}, "children": [
                             K.text("HeadlineText", "heading", "Woodworking +2 XP", "color.text.primary", wrap=True, var=True),
                             K.text("DetailText", "body", "Level 0 · 2 / 100 XP to level 1", "color.text.secondary", wrap=True, var=True, slot={"padding": [0, 3, 0, 0]}),
                             {"type": "SizeBox", "name": "XPBarBox", "height": 6, "slot": {"padding": [0, 9, 0, 0]}, "children": [
                                 K.progress_bar("XPProgressBar", fill_token="color.text.primary", percent=0.4)]}]}]}]},
                {"type": "SizeBox", "name": "AccentBarBox", "width": 4, "slot": {"horizontal_alignment": "H_ALIGN_LEFT", "vertical_alignment": "V_ALIGN_FILL"}, "children": [
                    {"type": "Border", "name": "AccentBar", "var": True, "props": {"brush_color": K.color("color.accent")}}]}]}]}]},
}

SPECS = [SKILL_TOAST]
