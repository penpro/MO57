"""Load-panel "Host" action: HostButton on every save slot entry + a status line on the Load panel.

Contracts live in C++ (BindWidgetOptional): MOSaveSlotEntry.h (HostButton), MOLoadPanel.h (StatusText).
Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/load_host.py
Idempotent: re-running updates what exists and creates what is missing. The slot entry hides HostButton itself unless the
main menu's Load panel enabled the Host action (UMOSaveSlotListPanel::SetHostActionEnabled), so the in-game Save/Load panels
that share this entry widget never show it.
"""
UI = "/MOFramework/UI"
GREY = [0.72, 0.72, 0.72, 1.0]

SAVE_SLOT_ENTRY = {
    "asset": f"{UI}/WBP_MOSaveSlot",
    # next to the existing Rename / Delete actions
    "attach": [
        {"parent": "VerticalBox_454",
         "node": {"type": "MOButton", "name": "HostButton", "label": "Host"}},
    ],
}

LOAD_PANEL = {
    "asset": f"{UI}/WBP_LoadPanel",
    "attach": [
        {"parent": "VerticalBox_0",
         "node": {"type": "TextBlock", "name": "StatusText", "text": "", "font_size": 16, "color": GREY,
                  "props": {"visibility": "Collapsed"}, "slot": {"padding": [0, 8, 0, 0]}}},
    ],
}

SPECS = [SAVE_SLOT_ENTRY, LOAD_PANEL]
