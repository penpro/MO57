"""The main menu flow in the Field Ledger kit (screens/main_menu.md): wordmark, button rail, the shared panel shell that grows to the open panel (New game, Options, Load, Host, Join), the session list and
its entries, the loading overlay and the intro hint.

Contracts: MOMainMenuWidget (ButtonsBox, 6 buttons, FocusWindowSwitcher, panels, BuildInfoLabel, PanelShell), MONewGamePanel, MOHostGamePanel, MOJoinGamePanel, MOSessionListWidget, MOSessionListEntry,
MOLoadingOverlay, MOIntroWidget. The switcher's child order is kept (blank, new game, options, load, host, join).

Apply:
    for a in "MainMenu_UI/WBP_MOInGameMenu1 RootCanvas" "MainMenu_UI/WBP_NewGamePanel SizeBox_96" "MainMenu_UI/WBP_HostGamePanel RootSizeBox" "MainMenu_UI/WBP_JoinGamePanel RootSizeBox" \
             "MainMenu_UI/WBP_SessionList ListBackground" "MainMenu_UI/WBP_SessionListEntry BackgroundBorder" "WBP_LoadingOverlay RootCanvas" "MainMenu_UI/WBP_IntroVideo RootCanvas"; do set -- $a; python Tools/ue.py ui remove /MOFramework/UI/$1 $2; done
    python Tools/ue.py ui build Content/Python/ui_specs/kit_mainmenu.py
"""
import kit_lib as K

UI = "/MOFramework/UI"
MM = UI + "/MainMenu_UI"
KIT = K.KIT_UI
FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_H = {"horizontal_alignment": "H_ALIGN_FILL"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}


def labelled(name, label, control, pad_top=16):
    return {"type": "VerticalBox", "name": name + "Field", "slot": {"padding": [0, pad_top, 0, 0]}, "children": [
        K.text(name + "Label", "caption", label, "color.text.secondary", slot={"padding": [0, 0, 0, 6]}), control]}


def input_box(name, hint, w=None, h=48):
    box = {"type": "SizeBox", "name": name + "Size", "height": h, "children": [K.text_input(name, hint)]}
    if w:
        box["width"] = w
    return box


def button_row(children):
    """Bottom-pinned button row: a fill spacer above it (the panel footprint is fixed), buttons right-aligned."""
    return {"type": "VerticalBox", "name": "ButtonRowColumn", "slot": {**FILL_SIZE, "vertical_alignment": "V_ALIGN_BOTTOM"}, "children": [
        {"type": "HorizontalBox", "name": "ButtonRow", "slot": {"padding": [0, 28, 0, 0]}, "children": [{"type": "Spacer", "name": "RowSpacer", "slot": FILL_SIZE}] + children}]}


# ---------------------------------------------------------------- main menu
WORDMARK = K.text("Wordmark", "display", "METAVERSE\nORIGINS", "color.text.primary", slot=K.place(120, 96, 480, 112, "TopLeft", auto_height=True))
WORDMARK["props"]["font"]["letter_spacing"] = 120   # kit: display wordmark tracked 0.12 em
RAIL = [("NewGameButton", "New Game", "PrimaryLg"), ("LoadGameButton", "Load Game", "SecondaryLg"), ("JoinGameButton", "Join Game", "SecondaryLg"), ("HostGameButton", "Host Game", "SecondaryLg"),
        ("OptionsButton", "Options", "SecondaryLg")]

MAIN_MENU = {
    "asset": f"{MM}/WBP_MOInGameMenu1",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "Image", "name": "RailBackdrop", "props": {"brush": K.baked_brush("T_Backdrop_left")}, "slot": K.place(0, 0, 760, 1080, "TopLeft")},
        WORDMARK,
        {"type": "SizeBox", "name": "RailBox", "width": 400, "slot": K.place(120, 300, 400, 456, "TopLeft", auto_height=True), "children": [
            {"type": "VerticalBox", "name": "ButtonsBox", "var": True, "children":
                [K.button(n, label, style, slot={**FILL_H, "padding": [0, 0, 0, 8]}) for n, label, style in RAIL]
                + [{"type": "Spacer", "name": "RailGap", "props": {"size": {"x": 1.0, "y": 24.0}}}, K.button("ExitGameButton", "Exit Game", "TertiaryLg", slot=FILL_H)]}]},
        {"type": "Overlay", "name": "PanelShell", "var": True, "slot": K.slot_anchored((0.0, 0.5), (0.0, 0.5), left=640.0, top=0.0, right=0.0, bottom=0.0, align=(0.0, 0.5), auto_size=True), "children": [
            K.image9("ShellShadow", "panel_shadow", "#000000", 0.5, slot={"padding": -16, **FILL}),
            K.image9("ShellFill", "panel_fill", "color.surface.1", slot=FILL),
            K.image9("ShellStroke", "panel_stroke", "color.border.subtle", slot=FILL),
            {"type": "WidgetSwitcher", "name": "FocusWindowSwitcher", "var": True, "slot": {"padding": 24}, "children": [
                {"type": "SizeBox", "name": "BlankPanel"},
                {"type": f"{MM}/WBP_NewGamePanel", "name": "NewGamePanel", "var": True},
                {"type": f"{UI}/WBP_MOOptionsPanel", "name": "OptionsPanel", "var": True},
                {"type": f"{UI}/WBP_LoadPanel", "name": "LoadPanel", "var": True},
                {"type": f"{MM}/WBP_HostGamePanel", "name": "HostGamePanel", "var": True},
                {"type": f"{MM}/WBP_JoinGamePanel", "name": "JoinGamePanel", "var": True}]}]},
        K.text("BuildInfoLabel", "caption", "", "color.text.secondary", var=True, slot=K.place(120, 1016, 600, 24, "BottomLeft")),
    ]},
}

# ---------------------------------------------------------------- new game (960 wide): name, seed, starting biome, the world's shape and pace
# World creation (Wes, Oct 22 2026, MOWorldCreationTypes.h): the knob rows and the biome cards are made at run time by UMONewGamePanel from
# FMOWorldCreation::GetKnobs() and DT_Biomes (rows with a PreviewImage), so a new knob or biome needs no edit here.
KNOB_ROW = {
    "asset": f"{MM}/WBP_WorldKnobRow",
    "parent": "/Script/MOFramework.MOWorldKnobRow",
    "root": {"type": "SizeBox", "name": "KnobRowSize", "height": 40, "children": [
        {"type": "HorizontalBox", "name": "KnobRow", "children": [
            {"type": "SizeBox", "name": "KnobLabelBox", "width": 260, "slot": VC, "children": [K.text("LabelText", "body", "Knob", "color.text.primary", var=True)]},
            K.slider("ValueSlider", slot={**FILL_SIZE, **VC, "padding": [0, 0, 16, 0]}),
            {"type": "SizeBox", "name": "KnobValueBox", "width": 72, "slot": VC, "children": [
                K.text("ValueText", "body.strong", "x1.00", "color.text.primary", justification="Right", var=True)]}]}]},
}

BIOME_CARD = {
    "asset": f"{MM}/WBP_BiomeCard",
    "parent": "/Script/MOFramework.MOBiomeCard",
    "root": {"type": "SizeBox", "name": "CardSize", "width": 224, "children": [
        {"type": "VerticalBox", "name": "CardColumn", "children": [
            {"type": "SizeBox", "name": "PreviewSize", "width": 224, "height": 126, "children": [
                {"type": "Overlay", "name": "PreviewOverlay", "children": [
                    K.image9("PreviewWell", "input_fill", "color.surface.0", slot=FILL),
                    {"type": "Image", "name": "PreviewImage", "var": True, "slot": FILL},
                    K.image9("SelectedFrame", "panel_stroke", "color.accent", var=True, slot=FILL)]}]},
            {"type": "SizeBox", "name": "CardButtonSize", "height": 44, "slot": {"padding": [0, 6, 0, 0]}, "children": [
                K.button("SelectButton", "Biome", "Secondary", slot=FILL_H)]}]}]},
}

NEW_GAME = {
    "asset": f"{MM}/WBP_NewGamePanel",
    "root": {"type": "SizeBox", "name": "SizeBox_96", "width": 960, "height": 860, "children": [
        {"type": "VerticalBox", "name": "NewGameColumn", "children": [
            K.text("NewGameTitle", "title", "New game", "color.text.primary"),
            K.divider("NewGameRule", slot={"padding": [0, 12, 0, 0]}),
            K.scroll_box("NewGameScroll", var=False, slot={**FILL_SIZE, "padding": [0, 4, 0, 0]}, children=[
                {"type": "VerticalBox", "name": "NewGameFields", "slot": {"padding": [0, 0, 16, 0]}, "children": [
                    labelled("WorldName", "World name", input_box("WorldNameInputBox", "Name your world")),
                    labelled("Seed", "Seed", {"type": "HorizontalBox", "name": "SeedRow", "children": [
                        {"type": "SizeBox", "name": "SeedInputSize", "height": 48, "slot": FILL_SIZE, "children": [K.text_input("SeedInputBox", "Leave empty for a random world")]},
                        K.button("RandomSeedButton", "Random", "Secondary", slot={"padding": [12, 0, 0, 0]}, )]}),
                    K.text("SeedPreviewText", "caption", "", "color.text.secondary", var=True, slot={"padding": [0, 8, 0, 0]}),
                    K.section_label("StartSection", "Starting biome", slot={"padding": [0, 24, 0, 6]}),
                    K.text("StartingBiomeText", "body", "Starting in: The coast", "color.text.secondary", var=True, slot={"padding": [0, 0, 0, 8]}),
                    K.scroll_box("BiomeCardsScroll", var=False, orientation="ORIENT_HORIZONTAL", children=[
                        {"type": "HorizontalBox", "name": "BiomeCardsBox", "var": True, "slot": {"padding": [0, 0, 0, 10]}}]),
                    K.section_label("ShapeSection", "World", slot={"padding": [0, 24, 0, 6]}),
                    {"type": "VerticalBox", "name": "ShapeKnobsBox", "var": True},
                    K.section_label("PaceSection", "Pace (1 = real time)", slot={"padding": [0, 24, 0, 6]}),
                    {"type": "VerticalBox", "name": "PaceKnobsBox", "var": True},
                    {"type": "HorizontalBox", "name": "ResetRow", "slot": {"padding": [0, 12, 0, 8]}, "children": [
                        K.button("ResetCreationButton", "Defaults", "Tertiary")]}]}]),
            button_row([K.button("BackButton", "Back", "SecondaryLg", slot={"padding": [0, 0, 12, 0]}), K.button("StartGameButton", "Start", "PrimaryLg")])]}]},
    "class_defaults": {"knob_row_class": f"{MM}/WBP_WorldKnobRow", "biome_card_class": f"{MM}/WBP_BiomeCard"},
}

# ---------------------------------------------------------------- host co-op
HOST = {
    "asset": f"{MM}/WBP_HostGamePanel",
    "root": {"type": "SizeBox", "name": "RootSizeBox", "width": 640, "height": 560, "children": [
        {"type": "VerticalBox", "name": "ContentBox", "children": [
            K.text("TitleText", "title", "Host co-op", "color.text.primary"),
            K.divider("HostRule", slot={"padding": [0, 12, 0, 0]}),
            labelled("CampName", "Camp name", input_box("SessionNameInputBox", "Your camp")),
            labelled("MaxPlayers", "Maximum players", input_box("MaxPlayersInputBox", "4", w=160)),
            K.text("HostStatusText", "caption", "", "color.text.secondary", var=True, slot={"padding": [0, 10, 0, 0]}),
            button_row([K.button("BackButton", "Back", "SecondaryLg", slot={"padding": [0, 0, 12, 0]}), K.button("HostButton", "Host", "PrimaryLg")])]}]},
}

# ---------------------------------------------------------------- join: session list in a well
JOIN = {
    "asset": f"{MM}/WBP_JoinGamePanel",
    "root": {"type": "SizeBox", "name": "RootSizeBox", "width": 640, "height": 560, "children": [
        {"type": "VerticalBox", "name": "ContentBox", "children": [
            K.text("TitleText", "title", "Join game", "color.text.primary"),
            K.divider("JoinRule", slot={"padding": [0, 12, 0, 0]}),
            K.text("JoinStatusText", "caption", "", "color.text.secondary", var=True, slot={"padding": [0, 12, 0, 8]}),
            {"type": "SizeBox", "name": "SessionListSize", "height": 316, "children": [{"type": f"{MM}/WBP_SessionList", "name": "SessionListWidget", "var": True}]},
            button_row([K.button("RefreshButton", "Refresh", "SecondaryLg", slot={"padding": [0, 0, 12, 0]}), K.button("BackButton", "Back", "SecondaryLg", slot={"padding": [0, 0, 12, 0]}),
                        K.button("JoinButton", "Join", "PrimaryLg")])]}]},
}

SESSION_LIST = {
    "asset": f"{MM}/WBP_SessionList",
    "root": {"type": "Overlay", "name": "ListBackground", "children": [
        K.image9("ListWell", "input_fill", "color.surface.0", slot=FILL),
        K.scroll_box("ContentScrollBox", slot={**FILL, "padding": 6})]},
}

SESSION_ENTRY = {
    "asset": f"{MM}/WBP_SessionListEntry",
    "root": {"type": "Border", "name": "BackgroundBorder", "var": True, "props": {"background": K.baked_brush("T_Row_normal"), "brush_color": [1, 1, 1, 1], "padding": 0}, "children": [
        {"type": "SizeBox", "name": "EntrySizeBox", "height": 56, "children": [
            {"type": "Overlay", "name": "EntryOverlay", "children": [
                {"type": "VerticalBox", "name": "TextColumn", "slot": {**VC, "padding": [14, 0, 14, 0]}, "children": [
                    K.text("SessionNameText", "body.strong", "Session", "color.text.primary", var=True),
                    K.text("SessionDetailText", "caption", "", "color.text.secondary", var=True)]},
                K.button("EntryButton", "", "Bare", slot=FILL)]}]}]},
}

# ---------------------------------------------------------------- loading overlay + intro hint
LOADING = {
    "asset": f"{UI}/WBP_LoadingOverlay",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "Image", "name": "BackgroundImage", "var": True, "props": {"color_and_opacity": K.color("color.surface.0")}, "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        K.text("LoadingText", "heading", "Loading", "color.text.primary", justification="Center", var=True, slot=K.place(560, 500, 800, 40, "Center")),
        K.text("LoadingHint", "caption", "The world keeps its own time while you wait.", "color.text.secondary", justification="Center", slot=K.place(560, 548, 800, 24, "Center")),
        # filled by the world readiness stages (UMOLoadingOverlay::SetProgress01) while the overlay is held; hidden by a negative value
        K.progress_bar("LoadingProgressBar", slot=K.place(780, 596, 360, 6, "Center"))]},
}

INTRO = {
    "asset": f"{MM}/WBP_IntroVideo",
    "root": {"type": "CanvasPanel", "name": "RootCanvas", "children": [
        {"type": "Image", "name": "VideoImage", "var": True, "slot": K.slot_anchored((0.0, 0.0), (1.0, 1.0))},
        K.text("SkipHintText", "caption", "Press any key to skip", "color.text.secondary", justification="Center", var=True, slot=K.place(660, 1000, 600, 24, "BottomCenter"))]},
}

SPECS = [MAIN_MENU, KNOB_ROW, BIOME_CARD, NEW_GAME, HOST, JOIN, SESSION_LIST, SESSION_ENTRY, LOADING, INTRO]
