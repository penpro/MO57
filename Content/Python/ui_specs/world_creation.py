"""The New Game panel's world creation (Wes, Oct 22 2026): its two row widgets and the panel, alone (kit_mainmenu.py holds them with the rest of the main menu).

Apply (the panel's root changed shape, so remove it first):
    python Tools/ue.py ui remove /MOFramework/UI/MainMenu_UI/WBP_NewGamePanel SizeBox_96
    python Tools/ue.py ui build Content/Python/ui_specs/world_creation.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kit_mainmenu import KNOB_ROW, BIOME_CARD, NEW_GAME  # noqa: E402

SPECS = [KNOB_ROW, BIOME_CARD, NEW_GAME]
