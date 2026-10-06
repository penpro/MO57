"""Co-op main-menu UI: session list row, session list, Join Game panel, Host Game panel, menu wiring.

Contracts live in C++ (BindWidget names): MOSessionListEntry.h, MOJoinGamePanel.h, MOHostGamePanel.h.
Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/coop_menu.py
The build is idempotent: re-running updates what exists and creates what is missing.
"""
UI = "/MOFramework/UI/MainMenu_UI"
BTN_STYLE = "/MOFramework/UI/MOCommonButtonStyle.MOCommonButtonStyle_C"

GREY = [0.72, 0.72, 0.72, 1.0]
FILL = {"size_rule": "Fill", "value": 1}


def button(name, label, pad=6):
    """A reusable MO button that shares row width equally with its siblings."""
    return {"type": "MOButton", "name": name, "label": label,
            "slot": {"padding": pad, "size": FILL, "horizontal_alignment": "Fill", "vertical_alignment": "Fill"}}


def button_row(name, buttons):
    return {"type": "SizeBox", "name": name + "SizeBox", "height": 72, "slot": {"padding": [0, 16, 0, 0]},
            "children": [{"type": "HorizontalBox", "name": name, "children": buttons}]}


def panel(title, children, root="RootSizeBox"):
    """800x800 panel matching the main menu's focus window (same footprint as BlankPanel)."""
    return {"type": "SizeBox", "name": root, "width": 800, "height": 800, "children": [
        {"type": "Border", "name": "PanelBorder", "color": [0, 0, 0, 0.2], "props": {"padding": 24}, "children": [
            {"type": "VerticalBox", "name": "ContentBox", "children": [
                {"type": "TextBlock", "name": "TitleText", "text": title, "font_size": 32, "slot": {"padding": [0, 0, 0, 12]}},
            ] + children}]}]}


SESSION_ENTRY = {
    "asset": f"{UI}/WBP_SessionListEntry",
    "parent": "/Script/MOFramework.MOSessionListEntry",
    "root": {"type": "Border", "name": "BackgroundBorder", "color": [0, 0, 0, 0.25], "children": [
        {"type": "SizeBox", "name": "EntrySizeBox", "height": 72, "children": [
            {"type": "Overlay", "name": "EntryOverlay", "children": [
                # the clickable surface; text sits above it and must not eat the click
                {"type": "MOCommonButton", "name": "EntryButton", "props": {"style": BTN_STYLE},
                 "slot": {"horizontal_alignment": "Fill", "vertical_alignment": "Fill"}},
                {"type": "VerticalBox", "name": "TextColumn", "props": {"visibility": "HitTestInvisible"},
                 "slot": {"padding": [16, 8], "horizontal_alignment": "Fill", "vertical_alignment": "Center"}, "children": [
                    {"type": "TextBlock", "name": "SessionNameText", "text": "Session Name", "font_size": 22},
                    {"type": "TextBlock", "name": "SessionDetailText", "text": "0/4 players - 0 ms", "font_size": 14, "color": GREY},
                ]},
            ]}]}]},
}

SESSION_LIST = {
    "asset": f"{UI}/WBP_SessionList",
    "parent": "/Script/MOFramework.MOSessionListWidget",
    "class_defaults": {"entry_widget_class": f"{UI}/WBP_SessionListEntry"},
    "root": {"type": "Border", "name": "ListBackground", "color": [0, 0, 0, 0.2], "props": {"padding": 6}, "children": [
        {"type": "ScrollBox", "name": "ContentScrollBox"}]},
}

JOIN_PANEL = {
    "asset": f"{UI}/WBP_JoinGamePanel",
    "parent": "/Script/MOFramework.MOJoinGamePanel",
    "root": panel("Join Game", [
        {"type": "TextBlock", "name": "JoinStatusText", "text": "Searching...", "font_size": 16, "color": GREY,
         "slot": {"padding": [0, 0, 0, 12]}},
        {"type": f"{UI}/WBP_SessionList", "name": "SessionListWidget", "slot": {"size": FILL}},
        button_row("ButtonRow", [button("RefreshButton", "Refresh"), button("JoinButton", "Join"), button("BackButton", "Back")]),
    ]),
}

HOST_PANEL = {
    "asset": f"{UI}/WBP_HostGamePanel",
    "parent": "/Script/MOFramework.MOHostGamePanel",
    "root": panel("Host Game", [
        {"type": "TextBlock", "name": "CampNameLabel", "text": "Camp name", "font_size": 16, "color": GREY},
        {"type": "EditableTextBox", "name": "SessionNameInputBox", "text": "Untitled Camp", "slot": {"padding": [0, 4, 0, 16]}},
        {"type": "TextBlock", "name": "MaxPlayersLabel", "text": "Max players (1-8)", "font_size": 16, "color": GREY},
        {"type": "EditableTextBox", "name": "MaxPlayersInputBox", "text": "4", "slot": {"padding": [0, 4, 0, 16]}},
        {"type": "TextBlock", "name": "HostStatusText", "text": "", "font_size": 16, "color": GREY},
        {"type": "Spacer", "name": "Filler", "slot": {"size": FILL}},
        button_row("ButtonRow", [button("HostButton", "Host"), button("BackButton", "Back")]),
    ]),
}

# Hang the two panels in the main menu's focus window (order in the switcher does not matter:
# UMOMainMenuWidget looks each panel's index up from the widget itself).
MAIN_MENU = {
    "asset": f"{UI}/WBP_MOInGameMenu1",
    "attach": [
        {"parent": "FocusWindowSwitcher", "node": {"type": f"{UI}/WBP_HostGamePanel", "name": "HostGamePanel"}},
        {"parent": "FocusWindowSwitcher", "node": {"type": f"{UI}/WBP_JoinGamePanel", "name": "JoinGamePanel"}},
    ],
}

SPECS = [SESSION_ENTRY, SESSION_LIST, JOIN_PANEL, HOST_PANEL, MAIN_MENU]
