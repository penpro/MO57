"""The bug report form in the Field Ledger kit (screens/modals.md BugReport: summary input, category dropdown, details multiline, attach toggles), 1152 x 840 inside the in-game menu's panel shell.

Contract: MOBugReportPanel (TitleInput, DescriptionInput, StepsInput, CategoryCombo, ContactInput, IncludeLogCheck, IncludeScreenshotCheck, PreviewButton, PreviewText, StatusText, SendButton, BackButton,
DiscordButton).

Apply:
    python Tools/ue.py ui remove /MOFramework/UI/WBP_BugReportPanel RootSizeBox
    python Tools/ue.py ui build Content/Python/ui_specs/kit_bugreport.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}


def labelled(name, label, control, pad_top=14):
    return {"type": "VerticalBox", "name": name + "Field", "slot": {"padding": [0, pad_top, 0, 0]}, "children": [K.text(name + "Label", "caption", label, "color.text.secondary", slot={"padding": [0, 0, 0, 6]}), control]}


BUG = {
    "asset": f"{UI}/WBP_BugReportPanel",
    "root": {"type": "SizeBox", "name": "RootSizeBox", "width": 1152, "height": 840, "children": [
        {"type": "VerticalBox", "name": "ContentBox", "children": [
            K.text("TitleText", "title", "Report a bug", "color.text.primary"),
            K.text("SubtitleText", "caption", "We add your game's state, the end of the log and a screenshot automatically. Preview lists everything that leaves your computer.", "color.text.secondary",
                   wrap=True, slot={"padding": [0, 4, 0, 0]}),
            K.divider("BugRule", slot={"padding": [0, 12, 0, 0]}),
            labelled("BugTitle", "Summary", {"type": "SizeBox", "name": "TitleInputSize", "height": 48, "children": [K.text_input("TitleInput", "One line: what is wrong?")]}),
            {"type": "HorizontalBox", "name": "CategoryContactRow", "slot": {"padding": [0, 14, 0, 0]}, "children": [
                {"type": "VerticalBox", "name": "CategoryColumn", "slot": {"padding": [0, 0, 16, 0]}, "children": [
                    K.text("CategoryLabel", "caption", "Category", "color.text.secondary", slot={"padding": [0, 0, 0, 6]}),
                    {"type": "SizeBox", "name": "CategorySize", "width": 360, "height": 44, "children": [K.combo("CategoryCombo")]}]},
                {"type": "VerticalBox", "name": "ContactColumn", "slot": FILL_SIZE, "children": [
                    K.text("ContactLabel", "caption", "Contact (optional): a Discord name or email, only if you want a reply", "color.text.secondary", slot={"padding": [0, 0, 0, 6]}),
                    {"type": "SizeBox", "name": "ContactSize", "height": 44, "children": [K.text_input("ContactInput", "Leave empty to stay anonymous")]}]}]},
            labelled("Description", "What happened, and what did you expect?", {"type": "SizeBox", "name": "DescriptionSizeBox", "height": 120,
                                                                                    "children": [K.text_input("DescriptionInput", "Up to 4000 characters", multiline=True)]}),
            labelled("Steps", "How can we make it happen again? (optional)", {"type": "SizeBox", "name": "StepsSizeBox", "height": 88,
                                                                                "children": [K.text_input("StepsInput", "1.  2.  3.", multiline=True)]}),
            {"type": "HorizontalBox", "name": "AttachRow", "slot": {"padding": [0, 16, 0, 0]}, "children": [
                K.checkbox("IncludeLogCheck", toggle=True, slot={**VC, "padding": [0, 0, 12, 0]}),
                K.text("LogCheckRowText", "body", "Include the end of the game log", "color.text.primary", slot={**VC, "padding": [0, 0, 40, 0]}),
                K.checkbox("IncludeScreenshotCheck", toggle=True, slot={**VC, "padding": [0, 0, 12, 0]}),
                # The game takes the screenshot when the form opens (MOBugReportPanel.h): say so, or a player looks for a way to take one (Wes, Oct 30:
                # "player is confused about screenshot toggle in bug report, have it say something about the screen shot being automatically generated").
                K.text("ScreenshotCheckRowText", "body", "Include the screenshot (taken automatically when you opened this)", "color.text.primary", slot=VC)]},
            {"type": "SizeBox", "name": "PreviewSizeBox", "height": 110, "slot": {"padding": [0, 16, 0, 0]}, "children": [
                {"type": "Overlay", "name": "PreviewOverlay", "children": [
                    K.image9("PreviewWell", "input_fill", "color.surface.0", slot=K.fill_slot()),
                    K.scroll_box("PreviewScrollBox", var=False, slot={**K.fill_slot(), "padding": 12}, children=[
                        K.text("PreviewText", "numeric.small", "", "color.text.secondary", wrap=True, var=True)])]}]},
            K.text("StatusText", "caption", "", "color.text.secondary", var=True, slot={"padding": [0, 10, 0, 0]}),
            {"type": "Spacer", "name": "BugSpacer", "slot": FILL_SIZE},
            {"type": "SizeBox", "name": "ButtonRowSizeBox", "height": 72, "children": [{"type": "HorizontalBox", "name": "ButtonRow", "children": [
                {"type": "Spacer", "name": "ButtonSpacer", "slot": FILL_SIZE},
                K.button("DiscordButton", "Open Discord", "TertiaryLg", slot={**VC, "padding": [0, 0, 12, 0]}),
                K.button("PreviewButton", "Preview", "SecondaryLg", slot={**VC, "padding": [0, 0, 12, 0]}),
                K.button("BackButton", "Back", "SecondaryLg", slot={**VC, "padding": [0, 0, 12, 0]}),
                K.button("SendButton", "Send report", "PrimaryLg", slot=VC)]}]}]}]},
}

SPECS = [BUG]
