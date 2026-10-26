"""The character creator (UMOCharacterCreatorPanel) in the kit look: a new game's first survivor -- name, skin tone, hair colour, eye colour, hair style,
beard, mustache -- made IN THE WORLD while it finishes loading (the panel frames the survivor with its own camera; see MOCharacterCreatorPanel.h). The
styles are the MetaHuman groom library (Oct 25 2026); face and body are "coming soon" rows (Wes, Oct 15 2026). The body type is Femme, the only one
there is; Masc is a coming-soon row (Wes, Oct 26 2026: "make the existing character 'femme' and mark 'masc' as a customization coming soon").
Pushed on Layer_Menu by UMOSystemMenuUIController::ShowCharacterCreator.

Contract: MOCharacterCreatorPanel (OrbitArea, NameInputBox, SkinToneSlider, HairShadeSlider, HairRednessSlider, EyeColorText, EyePrevButton, EyeNextButton,
HairStyleText / HairStylePrevButton / HairStyleNextButton, BeardText / ..., MustacheText / ..., RandomizeButton, BeginButton, WorldStatusText).

Oct 25: the style rows went in after Eyes (build appends; the rows below them were removed first so they come back in order):
    for w in SoonSection HairStyleRowBox FaceRowBox BodyRowBox ButtonRow; do python Tools/ue.py ui remove /MOFramework/UI/MainMenu_UI/WBP_CharacterCreatorPanel $w; done

    python Tools/ue.py ui build Content/Python/ui_specs/kit_character_creator.py

Oct 26: the Body type row went in first under Appearance, and Masc under Coming soon (the same way):
    for w in SkinToneRowBox HairShadeRowBox HairRedRowBox EyesRowBox HairStyleRowBox BeardRowBox MustacheRowBox SoonSection FaceRowBox BodyRowBox ButtonRow; do
        python Tools/ue.py ui remove /MOFramework/UI/MainMenu_UI/WBP_CharacterCreatorPanel $w; done
"""
import kit_lib as K

UI = "/MOFramework/UI"
MM = UI + "/MainMenu_UI"
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}
LABEL_W = 170


def slider_row(name, label, slider):
    return K.row(name, label, {"type": "SizeBox", "name": name + "SliderBox", "width": 380, "slot": VC, "children": [K.slider(slider)]}, label_w=LABEL_W, h=52)


def picker_row(name, label, value, text_w=280):
    """'<  value  >': a prev / next pair around the current choice (<name>PrevButton, <name>Text, <name>NextButton)."""
    return K.row(name, label, {"type": "HorizontalBox", "name": name + "Picker", "slot": VC, "children": [
        K.button(name + "PrevButton", "<", "Secondary", slot=VC),
        {"type": "SizeBox", "name": name + "NameBox", "width": text_w, "slot": VC, "children": [
            K.text(name + "Text", "body.strong", value, "color.text.primary", justification="Center", var=True)]},
        K.button(name + "NextButton", ">", "Secondary", slot=VC)]}, label_w=LABEL_W, h=56)


def soon_row(name, label):
    return K.row(name, label, K.text(name + "Soon", "body", "Coming soon", "color.text.secondary", slot=VC), label_w=LABEL_W, h=40)


COLUMN = {"type": "VerticalBox", "name": "CreatorColumn", "children": [
    K.text("CreatorTitle", "title", "Your survivor", "color.text.primary"),
    K.text("WorldStatusText", "caption", "The world is taking shape around you", "color.text.secondary", var=True, slot={"padding": [0, 6, 0, 0]}),
    K.divider("CreatorRule", slot={"padding": [0, 12, 0, 0]}),
    {"type": "VerticalBox", "name": "NameField", "slot": {"padding": [0, 16, 0, 0]}, "children": [
        K.text("NameLabel", "caption", "Name", "color.text.secondary", slot={"padding": [0, 0, 0, 6]}),
        {"type": "SizeBox", "name": "NameInputSize", "height": 48, "children": [K.text_input("NameInputBox", "Your survivor's name")]}]},
    K.section_label("LookSection", "Appearance", slot={"padding": [0, 20, 0, 4]}),
    K.row("BodyType", "Body type", K.text("BodyTypeText", "body.strong", "Femme", "color.text.primary", slot=VC), label_w=LABEL_W, h=44),
    slider_row("SkinTone", "Skin tone", "SkinToneSlider"),
    slider_row("HairShade", "Hair colour", "HairShadeSlider"),
    slider_row("HairRed", "Red in the hair", "HairRednessSlider"),
    K.row("Eyes", "Eyes", {"type": "HorizontalBox", "name": "EyePicker", "slot": VC, "children": [
        K.button("EyePrevButton", "<", "Secondary", slot=VC),
        {"type": "SizeBox", "name": "EyeNameBox", "width": 280, "slot": VC, "children": [
            K.text("EyeColorText", "body.strong", "Brown", "color.text.primary", justification="Center", var=True)]},
        K.button("EyeNextButton", ">", "Secondary", slot=VC)]}, label_w=LABEL_W, h=56),
    picker_row("HairStyle", "Hair style", "Updo braids (short)"),
    picker_row("Beard", "Beard", "None"),
    picker_row("Mustache", "Mustache", "None"),
    K.section_label("SoonSection", "Coming soon", slot={"padding": [0, 16, 0, 2]}),
    soon_row("Face", "Face"),
    soon_row("Body", "Body"),
    soon_row("Masc", "Masc body type"),
    {"type": "HorizontalBox", "name": "ButtonRow", "slot": {"padding": [0, 24, 0, 0]}, "children": [
        K.button("RandomizeButton", "Randomize", "Tertiary"),
        {"type": "Spacer", "name": "RowSpacer", "slot": FILL_SIZE},
        K.button("BeginButton", "Begin", "PrimaryLg")]}]}

CREATOR = {
    "asset": f"{MM}/WBP_CharacterCreatorPanel",
    "parent": "/Script/MOFramework.MOCharacterCreatorPanel",
    # The creator's depth of field is OFF (bPreviewDepthOfField, C++ default; Wes, Oct 16 2026): UE 5.8 drops groom hair from the scene depth whenever
    # Single Layer Water is in view, so the blur took the buns with the background (Docs/Engine_Patches.md #1). This aperture applies when it is turned
    # back on: f/3.2 kept the face, fringe and crown sharp with the world behind soft (about 12 px of blur 5 m back at 1440p).
    "class_defaults": {"preview_f_stop": 3.2, "closes_on_outside_click": False},
    # left of the survivor the panel's camera frames at ~72 % across the screen
    # OrbitArea: the whole screen behind the panel, invisible but hit-testable -- a drag on the world orbits the camera (UMOCharacterCreatorPanel::
    # HandleOrbitAreaMouseDown) instead of the click reaching the world. FIRST, so the panel draws and takes clicks over it (Oct 25: it went in
    # with `ui remove ... CreatorPanel_Size` then this build, which re-creates the panel after it).
    "root": {"type": "CanvasPanel", "name": "CreatorCanvas", "children": [
        {"type": "Border", "name": "OrbitArea", "var": True, "props": {"brush_color": [0, 0, 0, 0], "visibility": "Visible"},
         "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        K.panel("CreatorPanel", 2, [COLUMN], w=600, padding=[28, 24, 28, 24], slot=K.place(120, 120, 600, 760, "Left", auto_height=True))]},
}

SPECS = [CREATOR]
