"""Options in the Field Ledger kit (screens/options.md): title, tab bar (Graphics / Audio / Gameplay / Controls), 56 px rows (body label in a 440 column, kit control after it), footer (Defaults tertiary,
Back secondary, Apply primary 56).  Hosted 1152 x 840 inside the in-game menu's shared panel shell.

Contract: MOOptionsPanel (every control name below is a BindWidget; a name that is missing or only differs in case leaves its option silently dead: `python Tools/ue.py ui check /MOFramework/UI/WBP_MOOptionsPanel`).

Apply:
    python Tools/ue.py ui remove /MOFramework/UI/WBP_MOOptionsPanel RootCanvas
    python Tools/ue.py ui build Content/Python/ui_specs/kit_options.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
KIT = K.KIT_UI
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}
CTRL = {**VC}


def toggle(name):
    return K.checkbox(name, toggle=True, slot=CTRL)


def slider_row(key, label, slider, value_text, value="100"):
    return K.row(key, label,
                 {"type": "SizeBox", "name": key + "SliderBox", "width": 420, "slot": VC, "children": [K.slider(slider)]},
                 extra=[K.text(value_text, "numeric", value, "color.text.primary", var=True, slot={**VC, "padding": [20, 0, 0, 0]})])


def combo_row(key, label, combo):
    return K.row(key, label, {"type": "SizeBox", "name": key + "ComboSize", "width": 420, "height": 44, "slot": VC, "children": [K.combo(combo)]})


def with_rules(name, rows):
    """Rows separated by 1 px rules."""
    out = []
    for i, r in enumerate(rows):
        out.append(r)
        if i < len(rows) - 1:
            out.append(K.divider("%sRule%d" % (name, i)))
    return out


def page(name, sections):
    """A scrolling page: [(section label, [rows])...]."""
    kids = []
    for j, (title, rows) in enumerate(sections):
        kids.append(K.section_label("%sSection%d" % (name, j), title, slot={"padding": [0, 12 if j else 0, 0, 6]}))
        kids.extend(with_rules("%sS%d" % (name, j), rows))
    return K.scroll_box(name + "Scroll", var=False, children=[{"type": "VerticalBox", "name": name + "Column", "slot": {"padding": [0, 0, 16, 0]}, "children": kids}])


GRAPHICS = page("Graphics", [
    ("Display", [
        combo_row("Resolution", "Resolution", "ResolutionComboBox"),
        K.row("Fullscreen", "Fullscreen", toggle("FullscreenCheckbox")),
        K.row("VSync", "Vertical sync", toggle("VSyncCheckbox")),
        K.row("MaxFps", "Maximum frame rate", {"type": "SizeBox", "name": "MaxFpsSpinSize", "width": 180, "height": 44, "slot": VC, "children": [K.spin("MaxFrameRateSpinBox")]},
              extra=[K.text("MaxFpsUnit", "caption", "FPS  (0 = unlimited)", "color.text.secondary", slot={**VC, "padding": [16, 0, 0, 0]})])]),
    ("Quality", [
        combo_row("Quality", "Graphics quality", "QualityComboBox"),
        slider_row("Fov", "Field of view", "FOVSlider", "FOVValueText", "90"),
        K.row("MotionBlur", "Motion blur", toggle("MotionBlurCheckbox"))]),
    ("Overlays", [
        K.row("ShowFps", "Show frame rate", toggle("ShowFPSCheckbox")),
        K.row("ShowFrameTime", "Show frame time", toggle("ShowFrameTimeCheckbox"))]),
])

AUDIO = page("Audio", [("Volume", [
    slider_row("Master", "Master", "MasterVolumeSlider", "MasterVolumeText"),
    slider_row("Music", "Music", "MusicSlider", "MusicVolumeText"),
    slider_row("Sfx", "Effects", "SFXVolumeSlider", "SFXVolumeText"),
    slider_row("Ambient", "Ambient", "AmbientVolumeSlider", "AmbientVolumeText"),
    slider_row("Weather", "Weather", "WeatherVolumeSlider", "WeatherVolumeText")])])

GAMEPLAY = page("Gameplay", [
    ("Camera", [
        slider_row("Sens", "Camera sensitivity", "CameraSensitivitySlider", "CameraSensitivityText", "1.0"),
        K.row("InvertY", "Invert Y axis", toggle("InvertYCheckbox")),
        K.row("Shake", "Camera shake", toggle("CameraShakeCheckbox"))]),
    # FMODevMode: the ~ console + the developer menu in a Shipping build (checked and locked in development builds, where it is always on).
    ("Developer", [
        K.row("DevMode", "Developer mode", toggle("DeveloperModeCheckbox")),
        K.text("DeveloperModeNote", "caption", "~ opens the console; Run with nothing typed opens the command menu.", "color.text.secondary", wrap=True, var=True,
               slot={"padding": [0, 4, 0, 8]})]),
])

CONTROLS = {"type": "VerticalBox", "name": "ControlsPage", "children": [
    {"type": "SizeBox", "name": "ControlsHeaderBox", "height": 48, "children": [{"type": "HorizontalBox", "name": "ControlsHeader", "children": [
        K.text("ControlsHint", "caption", "Click a binding, then press the new key. Esc cancels.", "color.text.secondary", slot={**VC, **FILL_SIZE}),
        K.button("ResetAllBindingsButton", "Reset all bindings", "Tertiary", slot=VC)]}]},
    K.scroll_box("KeyBindingsScrollBox", slot=FILL_SIZE)]}

OPTIONS = {
    "asset": f"{UI}/WBP_MOOptionsPanel",
    "root": {"type": "SizeBox", "name": "RootCanvas", "width": 1152, "height": 840, "children": [
        {"type": "VerticalBox", "name": "OptionsColumn", "children": [
            {"type": "SizeBox", "name": "OptionsTitleBox", "height": 56, "children": [K.text("OptionsTitle", "title", "Options", "color.text.primary", slot=VC)]},
            {"type": f"{KIT}/WBP_TabBar", "name": "OptionsTabs", "props": {"tab_labels": ["Graphics", "Audio", "Gameplay", "Controls"], "target_switcher_name": "OptionsPages"}},
            {"type": "WidgetSwitcher", "name": "OptionsPages", "var": True, "slot": {**FILL_SIZE, "padding": [0, 16, 0, 0]}, "children": [GRAPHICS, AUDIO, GAMEPLAY, CONTROLS]},
            K.divider("OptionsFooterRule", slot={"padding": [0, 12, 0, 0]}),
            {"type": "SizeBox", "name": "OptionsFooterBox", "height": 88, "children": [{"type": "HorizontalBox", "name": "BottomButtonsBox", "var": True, "children": [
                {"type": "Spacer", "name": "FooterSpacer", "slot": FILL_SIZE},
                K.button("ResetButton", "Defaults", "TertiaryLg", slot={**VC, "padding": [0, 0, 12, 0]}),
                K.button("BackButton", "Back", "SecondaryLg", slot={**VC, "padding": [0, 0, 12, 0]}),
                K.button("ApplyButton", "Apply", "PrimaryLg", slot=VC)]}]}]}]},
}

SPECS = [OPTIONS]
