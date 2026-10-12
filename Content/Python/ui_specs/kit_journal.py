"""The journal screens in the Field Ledger kit: the SKILLS panel (WBP_SkillsPanel + WBP_SkillEntry) and the QUEST log (WBP_JournalPanel + WBP_QuestLogEntry).
Design: DesignKit screens/skills.md and quests.md (panel 1600 x 880, list 520 | detail 980, tabs on top). Names are the C++ contracts (`python Tools/ue.py ui contract MOSkillsPanel` ...).

    export MSYS_NO_PATHCONV=1
    python Tools/ue.py ui remove /MOFramework/UI/WBP_SkillsPanel SizeBox_91
    python Tools/ue.py ui remove /MOFramework/UI/WBP_SkillEntry SelectButtonSizeBox
    python Tools/ue.py ui remove /MOFramework/UI/Quest/WBP_JournalPanel SizeBox_45
    python Tools/ue.py ui remove /MOFramework/UI/Quest/WBP_QuestLogEntry SizeBox_45
    python Tools/ue.py ui build Content/Python/ui_specs/kit_journal.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}
R = K.slot_rect
CENTER = K.slot_anchored((0.5, 0.5), (0.5, 0.5), left=-800, top=-440, right=1600, bottom=880)
ROW_STYLE = {"normal": K.baked_brush("T_Row_normal"), "hovered": K.baked_brush("T_Row_hovered"), "pressed": K.baked_brush("T_Row_selected"), "disabled": K.baked_brush("T_Row_normal"),
             "normal_padding": [0, 0, 0, 0], "pressed_padding": [0, 0, 0, 0]}


def screen(root_name, asset, title_node, content, canvas_name="RootCanvas_1"):
    """A centred 1600 x 880 kit panel in a full-screen canvas; the content canvas fills the panel."""
    return {"asset": asset, "root": {"type": "CanvasPanel", "name": root_name, "children": [
        K.panel(root_name + "Panel", 1, [{"type": "CanvasPanel", "name": canvas_name, "slot": FILL, "children": ([title_node] if title_node else []) + content}], w=1600, h=880, slot=CENTER)]}}


# ------------------------------------------------------------------------------------------------ skills
SKILL_ENTRY = {
    "asset": f"{UI}/WBP_SkillEntry",
    "root": {"type": "Button", "name": "SelectButton", "var": True, "props": {"widget_style": ROW_STYLE}, "children": [
        {"type": "SizeBox", "name": "SkillEntrySize", "height": 76, "slot": FILL, "children": [
            {"type": "Overlay", "name": "SkillEntryOverlay", "children": [
                {"type": "HorizontalBox", "name": "SkillEntryRow", "slot": {**FILL, "padding": [14, 8, 14, 14]}, "children": [
                    K.icon_node("SkillIcon", "ic_ui_skill", "color.text.secondary", 36, var=True, slot={**VC, "padding": [0, 0, 14, 0]}),
                    {"type": "VerticalBox", "name": "SkillNameColumn", "slot": {**VC, **FILL_SIZE}, "children": [
                        K.text("SkillNameText", "body.strong", "Skill", "color.text.primary", var=True),
                        K.text("CategoryText", "caption", "Category", "color.text.secondary", var=True)]},
                    {"type": "VerticalBox", "name": "SkillLevelColumn", "slot": VC, "children": [
                        K.text("LevelText", "numeric", "Lv. 1", "color.text.primary", var=True, justification="Right"),
                        K.text("XPText", "numeric.small", "0 / 100 XP", "color.text.secondary", var=True, justification="Right")]}]},
                {"type": "SizeBox", "name": "SkillXPBox", "height": 6, "slot": {"horizontal_alignment": "H_ALIGN_FILL", "vertical_alignment": "V_ALIGN_BOTTOM", "padding": [14, 0, 14, 8]},
                 "children": [K.progress_bar("XPProgressBar")]}]}]}]},
}

SKILLS = screen("RootCanvas", f"{UI}/WBP_SkillsPanel", None, [
    K.button("SkillsTabButton", "Skills", "Tab", slot=R(40, 26, 160, 44)),
    K.button("KnowledgeTabButton", "Knowledge", "Tab", slot=R(212, 26, 170, 44)),
    K.button("CloseButton", "Close", "Secondary", slot=R(1440, 26, 120, 44)),
    K.divider("HeaderRule", slot=R(40, 88, 1520, 1)),
    K.scroll_box("SkillsContainer", slot=R(40, 108, 520, 730)),
    K.text("EmptyListText", "body", "Inspect items to gain knowledge and unlock skills.", "color.text.secondary", wrap=True, var=True, slot=R(60, 130, 480, 70)),
    {"type": "SizeBox", "name": "DetailsBox", "var": True, "width": 980, "height": 730, "slot": R(580, 108, 980, 730), "children": [
        {"type": "Overlay", "name": "DetailsOverlay", "children": [
            K.image9("DetailsShadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **FILL}),
            K.image9("DetailsFill", "panel_fill", "color.surface.2", slot=FILL),
            K.image9("DetailsStroke", "panel_stroke", "color.border.subtle", slot=FILL),
            {"type": "CanvasPanel", "name": "RootCanvas_2", "slot": FILL, "children": [
                K.icon_node("DetailIcon", "ic_ui_skill", "color.text.secondary", 64, var=True, slot=R(24, 24, 80, 80)),
                K.text("DetailNameText", "title", "Skill", "color.text.primary", var=True, slot=R(124, 20, 560, 44)),
                K.text("DetailLevelText", "numeric", "Level 1 / 100", "color.text.primary", var=True, slot=R(124, 70, 260, 28)),
                K.text("DetailXPText", "numeric.small", "0 / 100 XP", "color.text.secondary", var=True, slot=R(400, 74, 260, 24)),
                {"type": "SizeBox", "name": "DetailXPBox", "height": 8, "slot": R(24, 124, 932, 8), "children": [K.progress_bar("DetailXPBar")]},
                K.section_label("AboutLabel", "About", slot=R(24, 156, 300, 22)),
                K.text("DetailDescriptionText", "body", "", "color.text.secondary", wrap=True, var=True, slot=R(24, 184, 932, 200)),
                K.section_label("ImproveLabel", "How to improve", slot=R(24, 408, 300, 22)),
                K.text("DetailHowToIncreaseText", "body", "", "color.text.secondary", wrap=True, var=True, slot=R(24, 436, 932, 260))]}]}]},
])

# ------------------------------------------------------------------------------------------------ quests
QUEST_ENTRY = {
    "asset": f"{UI}/Quest/WBP_QuestLogEntry",
    "root": {"type": "SizeBox", "name": "QuestEntrySize", "height": 76, "children": [
        {"type": "Overlay", "name": "QuestEntryOverlay", "children": [
            {"type": "Border", "name": "BackgroundBorder", "var": True, "props": {"background": K.brush_9("slot_fill", "#FFFFFF"), "brush_color": [1, 1, 1, 1], "padding": 0}, "slot": FILL},
            {"type": "HorizontalBox", "name": "QuestEntryRow", "slot": {**FILL, "padding": [16, 10, 16, 10]}, "children": [
                {"type": "VerticalBox", "name": "QuestNameColumn", "slot": {**VC, **FILL_SIZE}, "children": [
                    K.text("QuestNameText", "body.strong", "Quest", "color.text.primary", var=True, wrap=True),
                    K.text("StatusText", "caption", "Active", "color.text.secondary", var=True)]},
                K.text("ProgressText", "numeric", "0/1", "color.text.secondary", var=True, justification="Right", slot=VC)]},
            K.button("SelectButton", "", "Bare", slot=FILL)]}]},
}

QUESTS = screen("RootCanvas", f"{UI}/Quest/WBP_JournalPanel", K.text("JournalTitle", "title", "Journal", "color.text.primary", slot=R(40, 24, 560, 48)), [
    K.button("CloseButton", "Close", "Secondary", slot=R(1440, 26, 120, 44)),
    K.divider("HeaderRule", slot=R(40, 88, 1520, 1)),
    K.scroll_box("QuestListScrollBox", slot=R(40, 108, 520, 730)),
    {"type": "SizeBox", "name": "QuestDetailBox", "width": 980, "height": 650, "slot": R(580, 108, 980, 650), "children": [
        {"type": "Overlay", "name": "QuestDetailOverlay", "children": [
            K.image9("QuestDetailShadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **FILL}),
            K.image9("QuestDetailFill", "panel_fill", "color.surface.2", slot=FILL),
            K.image9("QuestDetailStroke", "panel_stroke", "color.border.subtle", slot=FILL),
            {"type": "CanvasPanel", "name": "QuestDetailCanvas", "slot": FILL, "children": [
                K.text("SelectedQuestTitle", "title", "", "color.text.primary", var=True, slot=R(24, 22, 932, 44)),
                K.text("SelectedQuestDescription", "body", "", "color.text.secondary", wrap=True, var=True, slot=R(24, 78, 932, 150)),
                K.section_label("ObjectivesLabel", "Objectives", slot=R(24, 246, 300, 22)),
                {"type": "VerticalBox", "name": "ObjectivesContainer", "var": True, "slot": R(24, 276, 932, 350)}]}]}]},
    K.button("TrackButton", "Track", "Primary", slot=R(1400, 780, 160, 48)),
])

SPECS = [SKILL_ENTRY, SKILLS, QUEST_ENTRY, QUESTS]
