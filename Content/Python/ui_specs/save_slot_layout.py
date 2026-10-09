"""Save slot tile layout: a bigger thumbnail, and the save name/time/play-time text moved left next to it.

Before (measured in PIE, Load panel at 1148 px wide): the tile's HorizontalBox gave its three children an equal share (Fill / Fill / Fill),
so the 80x80 thumbnail sat at the left of a 383 px column and the text started 290 px after it -- an empty gap, and only ~357 px of text.

After: the thumbnail column is Auto-width (just the picture + a small gap), the text column takes twice the share of the buttons column.
  - thumbnail 80 -> 104 px (+30%): the largest square that fits the tightest tile (the in-game Load panel, Host hidden: row pitch 112 px,
    two buttons 96 px). It is vertically centred, so in the taller main-menu tile (three buttons) it does not hang off the top.
  - text column: Fill 2 (was 1), buttons column: Fill 1 -> the name gets ~690 px instead of ~357 and starts right after the thumbnail.
  - a save with no thumbnail (older saves) keeps the same column, so every name starts at the same x (measured: the SizeBox keeps its override width).
The right margin that keeps a truncated name's ellipsis clear of the buttons is set in UMOSaveSlotEntry::NativeConstruct.

Apply with:  python Tools/ue.py ui build Content/Python/ui_specs/save_slot_layout.py   (idempotent; PIE must not be running)
"""
UI = "/MOFramework/UI"
HBOX = "HorizontalBox_46"
THUMB = 104.0  # px; keep in step with the docstring above

SAVE_SLOT_LAYOUT = {
    "asset": f"{UI}/WBP_MOSaveSlot",
    "attach": [
        {"parent": HBOX,
         "node": {"type": "SizeBox", "name": "SizeBox_0",
                  "props": {"width_override": THUMB, "height_override": THUMB},
                  "slot": {"size": {"size_rule": "AUTOMATIC"}, "padding": [0, 0, 12, 0],
                           "horizontal_alignment": "H_ALIGN_LEFT", "vertical_alignment": "V_ALIGN_CENTER"}}},
        {"parent": HBOX,
         "node": {"type": "SizeBox", "name": "SizeBox_2",
                  "slot": {"size": {"size_rule": "FILL", "value": 2.0}}}},
        {"parent": HBOX,
         "node": {"type": "VerticalBox", "name": "VerticalBox_454",
                  "slot": {"size": {"size_rule": "FILL", "value": 1.0}}}},
    ],
}

SPECS = [SAVE_SLOT_LAYOUT]
