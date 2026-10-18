"""The survivor task menu ("what should this survivor do") in the Field Ledger kit: the same solid panel as the other menus, the tasks to queue on
the left, the survivor's job queue on the right. Wes, Oct 18 2026: "the pawn task assignment menu is very hard to read" -- it was never moved to the
kit (Oct 12): a translucent pale panel over the world, grey text, tiny descriptions and small Queue buttons.

Contracts: MOSurvivorTaskMenu (REQ CloseButton; opt SurvivorNameText, CurrentJobText, TaskListScrollBox, JobQueueScrollBox), MOSurvivorTaskEntryWidget
(opt TaskNameText, TaskDescriptionText, TaskIcon, AddButton), MOSurvivorJobEntryWidget (opt JobNameText, JobStateText, RepeatCountText, JobIcon,
ProgressBar, CancelButton).

Apply (the old hand-placed roots go first, so every bound name stays unique):
    python Tools/ue.py ui remove /MOFramework/UI/SurvivorAI/WBP_SurvivorTaskMenu SizeBox_66
    python Tools/ue.py ui remove /MOFramework/UI/SurvivorAI/WBP_TaskEntry SizeBox_41
    python Tools/ue.py ui remove /MOFramework/UI/SurvivorAI/WBP_JobQueueEntry SizeBox_35
    python Tools/ue.py ui build Content/Python/ui_specs/kit_survivor_tasks.py
"""
import kit_lib as K

UI = "/MOFramework/UI/SurvivorAI"
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}


def plate(name):
    return [K.image9(name + "Shadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **FILL}),
            K.image9(name + "Fill", "panel_fill", "color.surface.1", slot=FILL),
            K.image9(name + "Stroke", "panel_stroke", "color.border.subtle", slot=FILL)]


def column(name, label, scroll_name, weight):
    return {"type": "VerticalBox", "name": name, "slot": {"size": {"size_rule": "Fill", "value": weight}}, "children": [
        K.section_label(name + "Label", label, slot={"padding": [0, 0, 0, 10]}),
        K.scroll_box(scroll_name, slot=FILL_SIZE)]}


MENU = {
    "asset": f"{UI}/WBP_SurvivorTaskMenu",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "Border", "name": "Scrim", "props": {"brush_color": K.color("color.overlay.scrim")}, "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        {"type": "SizeBox", "name": "PanelBox", "width": 1180, "height": 760, "slot": K.place(370, 160, 1180, 760, "Center"), "children": [
            {"type": "Overlay", "name": "PanelOverlay", "children": plate("Panel") + [
                {"type": "VerticalBox", "name": "PanelColumn", "slot": {**FILL, "padding": 28}, "children": [
                    {"type": "SizeBox", "name": "HeaderBox", "height": 64, "children": [
                        {"type": "HorizontalBox", "name": "Header", "children": [
                            {"type": "VerticalBox", "name": "TitleColumn", "slot": {**VC, **FILL_SIZE}, "children": [
                                K.text("SurvivorNameText", "title", "Survivor", "color.text.primary", var=True),
                                # what the survivor is doing now ("Idle", "Gathering sticks 2/5")
                                K.text("CurrentJobText", "body", "Idle", "color.text.secondary", var=True, slot={"padding": [0, 2, 0, 0]})]},
                            K.button("CloseButton", "Close", "Secondary", slot=VC)]}]},
                    K.divider("HeaderRule", slot={"padding": [0, 14, 0, 18]}),
                    {"type": "HorizontalBox", "name": "Body", "slot": FILL_SIZE, "children": [
                        column("TasksColumn", "Tasks", "TaskListScrollBox", 1.25),
                        {"type": "Spacer", "name": "ColumnGap", "props": {"size": [28, 1]}},
                        column("QueueColumn", "Queue", "JobQueueScrollBox", 1.0)]}]}]}]},
    ]},
}

TASK_ENTRY = {
    "asset": f"{UI}/WBP_TaskEntry",
    "root": {"type": "SizeBox", "name": "EntrySize", "props": {"min_desired_height": 88}, "children": [
        {"type": "Overlay", "name": "EntryOverlay", "slot": {"padding": [0, 0, 0, 8]}, "children": [
            {"type": "Border", "name": "BackgroundBorder", "props": {"background": K.baked_brush("T_Row_normal"), "brush_color": [1, 1, 1, 1], "padding": 0}, "slot": FILL},
            {"type": "HorizontalBox", "name": "EntryRow", "slot": {**FILL, "padding": [16, 12, 16, 12]}, "children": [
                {"type": "VerticalBox", "name": "TextColumn", "slot": {**VC, **FILL_SIZE, "padding": [0, 0, 16, 0]}, "children": [
                    K.text("TaskNameText", "heading", "Task", "color.text.primary", var=True),
                    K.text("TaskDescriptionText", "body", "", "color.text.secondary", wrap=True, var=True, slot={"padding": [0, 4, 0, 0]})]},
                K.button("AddButton", "Queue", "Primary", slot=VC)]}]}]},
}

JOB_ENTRY = {
    "asset": f"{UI}/WBP_JobQueueEntry",
    "root": {"type": "SizeBox", "name": "EntrySize", "props": {"min_desired_height": 80}, "children": [
        {"type": "Overlay", "name": "EntryOverlay", "slot": {"padding": [0, 0, 0, 8]}, "children": [
            {"type": "Border", "name": "BackgroundBorder", "props": {"background": K.baked_brush("T_Row_normal"), "brush_color": [1, 1, 1, 1], "padding": 0}, "slot": FILL},
            {"type": "HorizontalBox", "name": "EntryRow", "slot": {**FILL, "padding": [16, 12, 16, 12]}, "children": [
                {"type": "VerticalBox", "name": "TextColumn", "slot": {**VC, **FILL_SIZE, "padding": [0, 0, 16, 0]}, "children": [
                    K.text("JobNameText", "heading", "Job", "color.text.primary", var=True),
                    {"type": "HorizontalBox", "name": "StateRow", "slot": {"padding": [0, 4, 0, 6]}, "children": [
                        K.text("JobStateText", "body", "", "color.text.secondary", var=True, slot={"padding": [0, 0, 12, 0]}),
                        K.text("RepeatCountText", "body", "", "color.accent", var=True)]},
                    {"type": "SizeBox", "name": "ProgressBox", "height": 6, "children": [K.progress_bar("ProgressBar")]}]},
                K.button("CancelButton", "Cancel", "Secondary", slot=VC)]}]}]},
}

SPECS = [MENU, TASK_ENTRY, JOB_ENTRY]
