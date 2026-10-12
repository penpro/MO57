"""Shared builders for the recipe screens in the Field Ledger kit (DesignKit screens/crafting.md): the crafting menu family (ui_specs/kit_crafting.py) and the building menu family
(ui_specs/kit_building.py) are the same screen over different Widget Blueprints and contracts (MOCraftingMenu / MOBuildingMenu, recipe list / entry / detail / queue / queue entry).

Layout (panel 1600 x 880): header 88; list 40..560 x 108..798; detail 580..1560 x 108..578; queue 580..1560 x 598..798; the crafting menu has "show all known" under the list. The ingredient /
output lines inside the detail are code (UMOUIUtils::CreateSimpleTextWidget: kit body face, ok / critical colour, strong face when short).
"""
import kit_lib as K

FILL = K.fill_slot()
VC = {"vertical_alignment": "V_ALIGN_CENTER"}
FILL_SIZE = {"size": {"size_rule": "Fill", "value": 1.0}}
R = K.slot_rect
CENTER_ICON = {"horizontal_alignment": "H_ALIGN_CENTER", "vertical_alignment": "V_ALIGN_CENTER"}


def label(name, value, x, y, w=120, tint="color.text.secondary"):
    return K.text(name, "caption", value, tint, slot=R(x, y, w, 22))


def entry(asset):
    """A recipe row: icon + name; the whole row is the (bare) button, the white fill is tinted by UMOListEntryBase with the row tokens."""
    return {"asset": asset, "root": {"type": "SizeBox", "name": "EntrySize", "height": 64, "children": [
        {"type": "Overlay", "name": "EntryOverlay", "children": [
            {"type": "Border", "name": "BackgroundBorder", "var": True, "props": {"background": K.brush_9("slot_fill", "#FFFFFF"), "brush_color": [1, 1, 1, 1], "padding": 0}, "slot": FILL},
            {"type": "HorizontalBox", "name": "EntryRow", "slot": {**FILL, "padding": [14, 8, 12, 8]}, "children": [
                K.icon_node("RecipeIcon", "ic_item_unknown", "color.text.secondary", 40, var=True, slot={**VC, "padding": [0, 0, 14, 0]}),
                K.text("RecipeNameText", "body", "Recipe", "color.text.primary", var=True, slot={**VC, **FILL_SIZE})]},
            K.button("EntryButton", "", "Bare", slot=FILL)]}]}}


def recipe_list(asset):
    return {"asset": asset, "root": {"type": "SizeBox", "name": "ListSize", "width": 520, "height": 690, "children": [
        {"type": "Overlay", "name": "ListOverlay", "children": [
            K.image9("ListFill", "panel_fill", "color.surface.0", slot=FILL),
            K.image9("ListStroke", "panel_stroke", "color.border.subtle", slot=FILL),
            K.scroll_box("RecipeScrollBox", slot={**FILL, "padding": 6})]}]}}


def detail(asset, building=False):
    """The recipe detail plate. Crafting: skill / station / time, Craft Max + amount + Begin. Building: skill / time, the three material-source check boxes + Begin."""
    kids = [
        {"type": "SizeBox", "name": "DetailIconBox", "width": 96, "height": 96, "slot": R(24, 24, 96, 96), "children": [
            K.icon_node("RecipeIcon", "ic_item_unknown", "color.text.secondary", 64, var=True, slot=CENTER_ICON)]},
        K.text("RecipeNameText", "title", "Select a recipe", "color.text.primary", var=True, slot=R(140, 22, 800, 44)),
        label("SkillLabel", "SKILL", 140, 78), K.text("SkillRequirementText", "body", "-", "color.text.primary", var=True, slot=R(260, 74, 680, 26)),
    ]
    if building:
        kids += [label("TimeLabel", "TIME", 140, 106), K.text("CraftTimeText", "numeric", "-", "color.text.primary", var=True, slot=R(260, 102, 680, 26))]
    else:
        kids += [label("StationLabel", "STATION", 140, 106), K.text("RequiredStationText", "body", "-", "color.text.primary", var=True, slot=R(260, 102, 680, 26)),
                 label("TimeLabel", "TIME", 140, 134), K.text("CraftTimeText", "numeric", "-", "color.text.primary", var=True, slot=R(260, 130, 680, 26))]
    kids += [
        K.divider("DetailRule", slot=R(24, 172, 932, 1)),
        K.text("RecipeDescriptionText", "body", "", "color.text.secondary", wrap=True, var=True, slot=R(24, 184, 932, 76)),
        K.section_label("InputLabel", "Input", slot=R(24, 266, 300, 24)),
        {"type": "VerticalBox", "name": "IngredientsContainer", "var": True, "slot": R(24, 296, 440, 120)},
        K.section_label("OutputLabel", "Output", slot=R(500, 266, 300, 24)),
        {"type": "VerticalBox", "name": "OutputsContainer", "var": True, "slot": R(500, 296, 440, 120)},
        K.text("ActionUnavailableText", "body", "", "color.severity.warning", var=True, wrap=True, slot=R(24, 420, 520, 30)),
    ]
    if building:
        kids += [K.section_label("SourcesLabel", "Take materials from", slot=R(24, 428, 240, 22)),
                 K.checkbox("InventoryCheckbox", slot=R(24, 452, 24, 24)), K.text("InventoryLabel", "body", "Inventory", "color.text.secondary", slot=R(56, 450, 140, 28)),
                 K.checkbox("ContainersCheckbox", slot=R(210, 452, 24, 24)), K.text("ContainersLabel", "body", "Nearby containers", "color.text.secondary", slot=R(242, 450, 220, 28)),
                 K.checkbox("SurroundingCheckbox", slot=R(480, 452, 24, 24)), K.text("SurroundingLabel", "body", "Surrounding area", "color.text.secondary", slot=R(512, 450, 200, 28)),
                 K.button("CraftButton", "Begin Building", "Primary", slot=R(790, 412, 166, 48))]
    else:
        kids += [K.button("CraftMaxButton", "Craft Max", "Secondary", slot=R(520, 414, 130, 44)),
                 {"type": "SizeBox", "name": "AmountBox", "width": 96, "height": 44, "slot": R(662, 414, 96, 44),
                  "children": [K.spin("CraftAmountSpinBox", min_fractional_digits=0, max_fractional_digits=0)]},
                 K.button("CraftButton", "Begin Crafting", "Primary", slot=R(772, 412, 184, 48))]
    return {"asset": asset, "root": K.panel("DetailPlate", 2, [{"type": "CanvasPanel", "name": "RootCanvas", "slot": FILL, "children": kids}], w=980, h=500 if building else 470)}


def queue_entry(asset):
    return {"asset": asset, "root": {"type": "SizeBox", "name": "QueueEntrySize", "height": 56, "children": [
        {"type": "Overlay", "name": "QueueEntryOverlay", "children": [
            K.image9("QueueEntryFill", "slot_fill", "color.surface.2", slot=FILL),
            {"type": "HorizontalBox", "name": "QueueEntryRow", "slot": {**FILL, "padding": [10, 6, 10, 6]}, "children": [
                {"type": "SizeBox", "name": "QueueIconBox", "width": 40, "height": 40, "slot": VC, "children": [
                    K.icon_node("RecipeIcon", "ic_item_unknown", "color.text.secondary", 28, var=True, slot=CENTER_ICON)]},
                {"type": "VerticalBox", "name": "QueueNameColumn", "slot": {**VC, **FILL_SIZE, "padding": [12, 0, 12, 0]}, "children": [
                    K.text("RecipeNameText", "body", "Name", "color.text.primary", var=True),
                    K.text("CountText", "caption", "x1", "color.text.secondary", var=True)]},
                {"type": "SizeBox", "name": "QueueBarBox", "width": 220, "height": 8, "slot": {**VC, "padding": [0, 0, 14, 0]}, "children": [K.progress_bar("ProgressBar")]},
                {"type": "SizeBox", "name": "QueueTimeBox", "width": 90, "slot": VC, "children": [K.text("TimeRemainingText", "numeric.small", "0:00", "color.text.secondary", var=True, justification="Right")]},
                {"type": "SizeBox", "name": "QueueCancelBox", "width": 96, "height": 36, "slot": {**VC, "padding": [12, 0, 0, 0]}, "children": [K.button("CancelButton", "Cancel", "Tertiary")]}]}]}]}}


def queue(asset, height=200):
    return {"asset": asset, "root": K.panel("QueuePlate", 1, [{"type": "CanvasPanel", "name": "RootCanvas", "slot": FILL, "children": [
        K.section_label("CurrentLabel", "Current", slot=R(20, 14, 120, 22)),
        K.text("CurrentCraftNameText", "body", "", "color.text.primary", var=True, slot=R(20, 36, 360, 26)),
        {"type": "SizeBox", "name": "CurrentBarBox", "width": 300, "height": 8, "slot": R(20, 68, 300, 8), "children": [K.progress_bar("CurrentProgressBar")]},
        K.text("ProgressText", "numeric.small", "", "color.text.secondary", var=True, slot=R(334, 62, 90, 20)),
        K.text("TimeRemainingText", "numeric.small", "", "color.text.secondary", var=True, slot=R(430, 62, 110, 20)),
        K.section_label("TotalLabel", "Total queue time", slot=R(580, 14, 200, 22)),
        K.text("TotalTimeRemainingText", "numeric", "", "color.text.primary", var=True, slot=R(580, 36, 200, 26)),
        K.button("CancelAllButton", "Cancel All", "Secondary", slot=R(824, 20, 136, 44)),
        K.divider("QueueRule", slot=R(20, 92, 940, 1)),
        K.scroll_box("QueueScrollBox", slot=R(20, 100, 940, height - 112)),
        K.text("EmptyQueueText", "body", "Queue empty", "color.text.disabled", var=True, justification="Center", slot=R(20, 100 + (height - 112) // 2 - 12, 940, 26))]}], w=980, h=height)}


def menu(asset, ui, title, title_name, building=False):
    """The menu panel: header (title, fuel for crafting, close), rule, list | detail, queue, and for crafting the "show all known" check box."""
    header = [K.text(title_name, "title", title, "color.text.primary", var=True, slot=R(40, 24, 640, 48))]
    if not building:
        header += [K.section_label("FuelLabel", "Fuel", slot=R(720, 34, 80, 24)), K.text("FuelTimeText", "numeric", "", "color.text.primary", var=True, slot=R(790, 30, 260, 28))]
    detail_h, queue_y, queue_h = (500, 628, 170) if building else (470, 598, 200)
    prefix = "WBP_Building" if building else "WBP_Crafting"
    kids = header + [
        K.button("CloseButton", "Close", "Secondary", slot=R(1440, 26, 120, 44)),
        K.divider("HeaderRule", slot=R(40, 88, 1520, 1)),
        {"type": ui + "/" + prefix + "RecipeList", "name": "RecipeList", "var": True, "slot": R(40, 108, 520, 690)},
        {"type": ui + "/" + prefix + "RecipeDetail", "name": "DetailPanel", "var": True, "slot": R(580, 108, 980, detail_h)},
        {"type": ui + "/" + prefix + "Queue", "name": "QueueWidget", "var": True, "slot": R(580, queue_y, 980, queue_h)}]
    if not building:
        kids += [K.checkbox("ShowAllKnownCheckbox", slot=R(40, 816, 24, 24)), K.text("ShowAllLabel", "body", "Show all known recipes", "color.text.secondary", slot=R(76, 814, 400, 28))]
    return {"asset": asset, "root": {"type": "CanvasPanel", "name": "MenuRoot", "children": [
        K.panel("MenuPanel", 1, [{"type": "CanvasPanel", "name": "RootCanvas", "slot": FILL, "children": kids}], w=1600, h=880,
                slot=K.slot_anchored((0.5, 0.5), (0.5, 0.5), left=-800, top=-440, right=1600, bottom=880))]}}


def crafting():
    ui = "/MOFramework/UI/CraftingSystem"
    return [entry(ui + "/WBP_CraftingRecipeEntry"), recipe_list(ui + "/WBP_CraftingRecipeList"), detail(ui + "/WBP_CraftingRecipeDetail"), queue_entry(ui + "/WBP_CraftQueueEntry"),
            queue(ui + "/WBP_CraftingQueue"), menu(ui + "/WBP_CraftingMenu", ui, "Crafting", "StationNameText")]


def building():
    ui = "/MOFramework/UI/BuildSystem"
    return [entry(ui + "/WBP_BuildingRecipeEntry"), recipe_list(ui + "/WBP_BuildingRecipeList"), detail(ui + "/WBP_BuildingRecipeDetail", True), queue_entry(ui + "/WBP_BuildQueueEntry"),
            queue(ui + "/WBP_BuildingQueue", 170), menu(ui + "/WBP_BuildingMenu", ui, "Building", "BuildingTitleText", True)]
