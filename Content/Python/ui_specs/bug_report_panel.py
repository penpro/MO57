"""In-game bug report form: WBP_BugReportPanel, and its place in the in-game menu's focus window.

Contract (C++): MOBugReportPanel.h (BindWidget names). The menu side: MOInGameMenu.h -> BugReportPanel (BindWidgetOptional); the menu's "Bug Report"
button opens it (the panel's index in FocusWindowSwitcher is looked up by widget, so its position there does not matter).

Footprint: 800 x 800, the same as every other panel in the in-game menu's focus window. Order of the form, top to bottom:
title, subtitle, Title, [Category | Contact], What happened, Steps, attachment checkboxes, preview, status, buttons.

Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/bug_report_panel.py   (idempotent; PIE must not be running)
"""
UI = "/MOFramework/UI"

GREY = [0.72, 0.72, 0.72, 1.0]
FILL = {"size_rule": "Fill", "value": 1}
# The multi-line box's default font is about half the size of the single-line box's; the form's text must read the same everywhere.
MULTILINE_STYLE = {"text_style": {"font": {"size": 16.0}}}


def label(name, text, pad=(0, 0, 0, 3)):
    return {"type": "TextBlock", "name": name, "text": text, "font_size": 14, "color": GREY, "slot": {"padding": list(pad)}}


def button(name, text):
    return {"type": "MOButton", "name": name, "label": text,
            "slot": {"padding": 6, "size": FILL, "horizontal_alignment": "Fill", "vertical_alignment": "Fill"}}


def check_row(name, box_name, text):
    return {"type": "HorizontalBox", "name": name, "slot": {"padding": [0, 0, 24, 0]}, "children": [
        {"type": "CheckBox", "name": box_name, "slot": {"vertical_alignment": "Center"}},
        {"type": "TextBlock", "name": name + "Text", "text": text, "font_size": 14, "slot": {"padding": [8, 0, 0, 0], "vertical_alignment": "Center"}},
    ]}


PANEL = {
    "asset": f"{UI}/WBP_BugReportPanel",
    "parent": "/Script/MOFramework.MOBugReportPanel",
    "root": {"type": "SizeBox", "name": "RootSizeBox", "width": 800, "height": 800, "children": [
        {"type": "Border", "name": "PanelBorder", "color": [0, 0, 0, 0.2], "props": {"padding": 24}, "children": [
            {"type": "VerticalBox", "name": "ContentBox", "children": [
                {"type": "TextBlock", "name": "TitleText", "text": "Report a Bug", "font_size": 32, "slot": {"padding": [0, 0, 0, 4]}},
                {"type": "TextBlock", "name": "SubtitleText", "font_size": 13, "color": GREY, "slot": {"padding": [0, 0, 0, 10]},
                 "text": "We add your game's state, the end of the log and a screenshot automatically. \"Preview\" lists everything that leaves your computer.",
                 "props": {"auto_wrap_text": True}},

                label("TitleLabel", "Title"),
                {"type": "EditableTextBox", "name": "TitleInput", "props": {"hint_text": "One line: what is wrong?"}, "slot": {"padding": [0, 0, 0, 10]}},

                {"type": "HorizontalBox", "name": "CategoryContactRow", "slot": {"padding": [0, 0, 0, 10]}, "children": [
                    {"type": "VerticalBox", "name": "CategoryColumn", "slot": {"size": FILL, "padding": [0, 0, 12, 0]}, "children": [
                        label("CategoryLabel", "Category"),
                        {"type": "ComboBoxString", "name": "CategoryCombo"},
                    ]},
                    {"type": "VerticalBox", "name": "ContactColumn", "slot": {"size": {"size_rule": "Fill", "value": 2}}, "children": [
                        label("ContactLabel", "Contact (optional: a Discord name or email, only if you want a reply)"),
                        {"type": "EditableTextBox", "name": "ContactInput", "props": {"hint_text": "Leave empty to stay anonymous"}},
                    ]},
                ]},

                label("DescriptionLabel", "What happened, and what did you expect?"),
                {"type": "SizeBox", "name": "DescriptionSizeBox", "height": 100, "slot": {"padding": [0, 0, 0, 10]}, "children": [
                    {"type": "MultiLineEditableTextBox", "name": "DescriptionInput", "props": {"hint_text": "Up to 4000 characters", "widget_style": MULTILINE_STYLE}}]},

                label("StepsLabel", "How can we make it happen again? (optional)"),
                {"type": "SizeBox", "name": "StepsSizeBox", "height": 70, "slot": {"padding": [0, 0, 0, 10]}, "children": [
                    {"type": "MultiLineEditableTextBox", "name": "StepsInput", "props": {"hint_text": "1. ...  2. ...  3. ...", "widget_style": MULTILINE_STYLE}}]},

                {"type": "HorizontalBox", "name": "AttachRow", "slot": {"padding": [0, 0, 0, 8]}, "children": [
                    check_row("LogCheckRow", "IncludeLogCheck", "Include the end of the game log"),
                    check_row("ScreenshotCheckRow", "IncludeScreenshotCheck", "Include a screenshot of the game"),
                ]},

                {"type": "SizeBox", "name": "PreviewSizeBox", "height": 110, "slot": {"padding": [0, 0, 0, 6]}, "children": [
                    {"type": "Border", "name": "PreviewBorder", "color": [0, 0, 0, 0.3], "props": {"padding": 6}, "children": [
                        {"type": "ScrollBox", "name": "PreviewScrollBox", "children": [
                            {"type": "TextBlock", "name": "PreviewText", "text": "", "font_size": 12, "color": GREY, "props": {"auto_wrap_text": True}}]}]}]},

                {"type": "TextBlock", "name": "StatusText", "text": "", "font_size": 15, "color": [1.0, 0.85, 0.4, 1.0],
                 "props": {"auto_wrap_text": True}, "slot": {"padding": [0, 0, 0, 4]}},

                {"type": "SizeBox", "name": "ButtonRowSizeBox", "height": 64, "children": [
                    {"type": "HorizontalBox", "name": "ButtonRow", "children": [
                        button("SendButton", "Send Report"),
                        button("PreviewButton", "Preview"),
                        button("DiscordButton", "Open Discord"),
                        button("BackButton", "Back"),
                    ]}]},
            ]}]}]},
}

IN_GAME_MENU = {
    "asset": f"{UI}/WBP_MOInGameMenu",
    "attach": [
        {"parent": "FocusWindowSwitcher", "node": {"type": f"{UI}/WBP_BugReportPanel", "name": "BugReportPanel"}},
    ],
}

SPECS = [PANEL, IN_GAME_MENU]
