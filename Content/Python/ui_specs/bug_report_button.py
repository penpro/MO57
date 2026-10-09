"""In-game menu "Bug Report" button.

Contract (C++): MOInGameMenu.h -> BugReportButton (BindWidgetOptional). The click handler opens UMOCommunitySettings::BugReportUrl in the default
browser and falls back to the clipboard (HandleBugReportClicked).

Layout: the menu's buttons live in the GridPanel `ButtonsBox`, one SizeBox (300 x 75) per row: Options 0, Load 1, Save 2, Main Menu 3, Exit Game 4.
Bug Report takes row 4 and Exit Game moves to row 5, so Exit Game stays last. (The panel switcher is in column 1 and spans the rows, so a new row
in column 0 collides with nothing.)

Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/bug_report_button.py   (idempotent; PIE must not be running)
"""
UI = "/MOFramework/UI"

IN_GAME_MENU = {
    "asset": f"{UI}/WBP_MOInGameMenu",
    "attach": [
        {"parent": "ButtonsBox",
         "node": {"type": "SizeBox", "name": "ButtonsBoxSizeBox_4", "slot": {"row": 5}}},          # Exit Game: one row down
        {"parent": "ButtonsBox",
         "node": {"type": "SizeBox", "name": "ButtonsBoxSizeBox_6",
                  "props": {"height_override": 75.0, "width_override": 300.0},
                  "slot": {"row": 4},
                  "children": [{"type": "MOButton", "name": "BugReportButton", "label": "Bug Report"}]}},
    ],
}

SPECS = [IN_GAME_MENU]
