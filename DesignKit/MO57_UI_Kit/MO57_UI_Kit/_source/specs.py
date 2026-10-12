"""Per-screen layout specs at 1920x1080. Neutral schema meant to be mapped onto
`python Tools/ue.py ui build <spec>`: each element = id, widget (UMG/CommonUI class or MO widget),
rect [x,y,w,h] in 1080p px, anchor (UMG anchor preset), tokens, focus (gamepad order, 0 = not focusable),
nav (explicit overrides). Menus are centred: rects are given at 1920x1080 and the panel is anchored centre."""
import json, os, sys
OUT = (sys.argv[1] if len(sys.argv) > 1 else "/mnt/user-data/outputs/MO57_UI_Kit") + "/screens"
os.makedirs(OUT, exist_ok=True)

def E(id, widget, rect, anchor="TopLeft", tokens=None, focus=0, note="", nav=None):
    d = {"id": id, "widget": widget, "rect": rect, "anchor": anchor, "tokens": tokens or {}, "focus": focus}
    if note: d["note"] = note
    if nav: d["nav"] = nav
    return d

P = {"bg": "color.surface.1", "edge": "color.border.subtle"}
SCREENS = {}

SCREENS["hud"] = {
    "title": "HUD", "layer": "Game", "input": "game (no focus)",
    "grid": "40px safe margin; corners own clusters; centre column reserved for reticle/prompt/progress",
    "elements": [
        E("ColonyBar", "WBP_ColonyBar", [40, 40, 618, 108], "TopLeft", {"card": "72x108, gap 6", "portrait": "portrait.size 64", "caption": "type.caption + outline"}, note="max 8 cards, then +N card; hidden when solo"),
        E("ModeIndicator", "WBP_ModeIndicator", [40, 164, 220, 36], "TopLeft", {"bg": "color.hud.plate", "text": "type.label", "icon": 24}),
        E("TutorialBanner", "WBP_TutorialBanner", [680, 40, 560, 112], "TopCenter", {"bg": "color.hud.plate", "title": "type.heading", "body": "type.body", "rule": "color.accent 2px top"}),
        E("Notifications", "WBP_NotificationStack", [1480, 40, 400, 312], "TopRight", {"item": "400x72 gap 8", "severity": "icon 24 + 4px leading bar", "text": "type.body"}, note="max 4 visible; info 4s, warning 8s, critical sticky until resolved"),
        E("QuestTracker", "WBP_QuestTracker", [1520, 392, 360, 152], "TopRight", {"bg": "color.hud.plate", "title": "type.body.strong", "objective": "type.body (wrap)", "bar": "progress 8px"}),
        E("Reticle", "WBP_Reticle", [956, 536, 8, 8], "Center", {"color": "color.text.primary", "outline": "1px #000 @80%"}),
        E("InteractionPrompt", "WBP_InteractionPrompt", [760, 572, 400, 44], "Center", {"keycap": "keycap 32", "text": "type.hud", "bg": "color.hud.plate"}),
        E("TimedAction", "WBP_TimedActionProgress", [760, 628, 400, 76], "Center", {"label": "type.hud", "time": "type.numeric", "bar": "progress 8px data.fill", "hint": "type.caption text.secondary"}),
        E("BreathMeter", "WBP_BreathMeter", [760, 872, 400, 40], "BottomCenter", {"segments": "10 x 36w gap 4", "fill": "color.severity.info -> warning <30% -> critical <10%"}, note="only while submerged"),
        E("Moodles", "WBP_MoodleStrip", [40, 760, 300, 280], "BottomLeft", {"pill": "h40, icon 24, type.hud", "gap": 8, "warning": "severity.warning + triangle", "critical": "severity.critical + octagon + critical_border"}, note="stack upward, critical first; >6 = '+N more' pill opens status"),
        E("Wind", "WBP_WindIndicator", [1792, 912, 88, 128], "BottomRight", {"ring": "wind_ring 64", "arrow": "wind_arrow 32 rotated to wind-relative-to-camera", "speed": "type.numeric.small"}),
        E("Debug", "WBP_FPSCounter", [1736, 8, 176, 24], "TopRight", {"text": "type.numeric.small", "bg": "color.hud.plate"}, note="dev builds only, above safe area"),
    ]}

SCREENS["inventory"] = {
    "title": "Inventory", "layer": "Menu", "input": "menu",
    "grid": "Panel 1600x880 centred (160,100); 24px padding; 3 columns 400 / 568 / 536, 24 gutters",
    "elements": [
        E("Panel", "WBP_MOPanel(elev1)", [160, 100, 1600, 880], "Center", P),
        E("Header", "WBP_PanelHeader", [184, 116, 1552, 56], "Center", {"title": "type.title", "weight": "type.numeric", "close": "icon button 44"}),
        E("PaperDoll", "WBP_EquipmentDoll", [184, 196, 400, 760], "Center", {"slot": "slot 64", "silhouette": "color.surface.3 line art"}, focus=2, note="12 slots: head, face, neck, torso-inner, torso-outer, back, hands, belt, legs, feet, L hand, R hand"),
        E("BackpackHeader", "WBP_SectionHeader", [608, 196, 568, 40], "Center", {"text": "type.label", "capacity": "type.numeric.small"}),
        E("BackpackGrid", "WBP_InventoryGrid", [608, 244, 568, 424], "Center", {"slot": "64 gap 8", "cols": 8, "rows": 6}, focus=1, note="initial focus"),
        E("NearbyHeader", "WBP_SectionHeader+Actions", [608, 692, 568, 44], "Center", {"actions": "Take all / Deposit matching (secondary, h44)"}, focus=3),
        E("NearbyList", "WBP_ItemList", [608, 740, 568, 216], "Center", {"row": "row.height 48", "count": "type.numeric"}, focus=4),
        E("ItemInfo", "WBP_ItemInfoPanel(elev2)", [1200, 196, 536, 680], "Center", {"name": "type.heading", "stats": "type.body + type.numeric", "condition": "progress segmented"}, focus=5),
        E("ItemActions", "WBP_ButtonRow", [1200, 892, 536, 56], "Center", {"primary": "Use (56)", "secondary": "Split / Drop"}, focus=6),
        E("ContextMenu", "WBP_ContextMenu", [0, 0, 280, 0], "Cursor", {"row": 44, "bg": "surface.3 + shadow"}, note="spawns at slot; gamepad: face_west"),
        E("DragVisual", "WBP_DragVisual", [0, 0, 64, 64], "Cursor", {"opacity": 0.85, "count": "type.numeric.small"}),
    ]}

SCREENS["crafting"] = {
    "title": "Crafting / Building", "layer": "Menu", "input": "menu",
    "grid": "Panel 1600x880; header 80; list 520 | detail 1008; queue strip 72",
    "elements": [
        E("Panel", "WBP_MOPanel(elev1)", [160, 100, 1600, 880], "Center", P),
        E("StationHeader", "WBP_StationHeader", [184, 116, 1552, 72], "Center", {"name": "type.title", "fuel": "progress 8 + type.numeric 'Fuel 1:42:10'"}),
        E("Search", "WBP_SearchField", [184, 204, 400, 44], "Center", {"input": "input 44"}, focus=2),
        E("Filter", "WBP_IconButton", [592, 204, 44, 44], "Center", {}, focus=3),
        E("Sort", "WBP_IconButton", [644, 204, 44, 44], "Center", {}, focus=4),
        E("RecipeList", "WBP_RecipeList", [184, 260, 520, 600], "Center", {"row": "56: icon 32, name body.strong, state caption", "locked": "text.disabled + lock"}, focus=1, note="initial focus on first craftable"),
        E("Detail", "WBP_RecipeDetail(elev2)", [728, 204, 1008, 656], "Center", {"name": "type.title", "icon": 96, "desc": "type.body", "ingredients": "row 44: have/need type.numeric coloured ok/critical + shape"}),
        E("AmountSpinner", "WBP_SpinBox", [1272, 788, 200, 56], "Center", {}, focus=5),
        E("CraftButton", "WBP_MOButton(Primary)", [1488, 788, 224, 56], "Center", {}, focus=6),
        E("WhyNot", "WBP_InlineMessage", [752, 744, 960, 32], "Center", {"icon": "sev_warning", "text": "type.body severity.warning"}),
        E("Queue", "WBP_CraftQueue", [184, 884, 1552, 72], "Center", {"item": "64 slot + radial-free linear bar under it", "time": "type.numeric.small"}, focus=7),
        E("PlacementBar", "WBP_PlacementContextBar", [560, 984, 800, 56], "BottomCenter", {"bg": "hud.plate", "keys": "keycap 32 + type.hud"}, note="Building mode only (HUD layer)"),
    ]}

SCREENS["medical"] = {
    "title": "Character status (medical)", "layer": "Menu", "input": "menu",
    "grid": "Panel 1600x880; summary strip 96; body 456 | wounds 560 | vitals 488; paper inserts",
    "elements": [
        E("Panel", "WBP_MOPanel(elev1)", [160, 100, 1600, 880], "Center", P),
        E("Summary", "WBP_StatusSummary", [184, 116, 1552, 96], "Center", {"state": "type.heading + severity shape", "chips": "consciousness / shock / blood loss / temperature"}),
        E("BodyToggle", "WBP_SegmentedTabs", [184, 228, 456, 44], "Center", {"tabs": "Front / Back"}, focus=2),
        E("BodyDiagram", "WBP_BodyDiagram", [184, 284, 456, 672], "Center", {"part": "fill by worst wound severity; hatched = bandaged; outline = selected"}, focus=1, note="~55 parts; gamepad moves between parts by spatial nav"),
        E("WoundList", "WBP_WoundList(paper)", [664, 228, 560, 728], "Center", {"bg": "color.paper.surface", "text": "color.paper.ink", "row": "56: part, wound type, state tags, time since"}, focus=3),
        E("Vitals", "WBP_VitalsPanel(paper)", [1248, 228, 488, 480], "Center", {"value": "type.numeric.large", "range": "type.caption 'normal 60-100'", "trend": "trend_up/down/flat 24"}, focus=4),
        E("Nutrition", "WBP_NutritionPanel", [1248, 724, 488, 232], "Center", {"bars": "segmented progress with threshold markers"}, focus=5),
    ]}

SCREENS["options"] = {
    "title": "Options", "layer": "Menu", "input": "menu",
    "grid": "Panel 1280x880 centred (320,100); tab bar 56; rows 56; label col 440, control col 720",
    "elements": [
        E("Panel", "WBP_MOPanel(elev1)", [320, 100, 1280, 880], "Center", P),
        E("Title", "TextBlock", [344, 116, 600, 48], "Center", {"text": "type.title"}),
        E("Tabs", "WBP_TabBar", [344, 172, 1232, 56], "Center", {"tab": "h48 + 4px indicator; LB/RB cycle"}, focus=1),
        E("Rows", "WBP_SettingsList", [344, 244, 1232, 600], "Center", {"row": "56: label body 440w | control 720w | badge", "section": "type.label"}, focus=2),
        E("Footer", "WBP_FooterBar", [344, 868, 1232, 88], "Center", {"buttons": "Defaults (tertiary) | Revert (secondary) | Apply (primary 56)"}, focus=3),
        E("KeyCapture", "WBP_KeyCaptureModal", [720, 420, 480, 240], "Center", {"text": "type.heading 'Press a key for Sprint'", "esc": "Esc / face_east cancels"}),
    ]}

SCREENS["main_menu"] = {
    "title": "Main menu", "layer": "Menu", "input": "menu",
    "grid": "Left rail 480 from x=120; content panel 760 at x=720",
    "elements": [
        E("Wordmark", "TextBlock", [120, 120, 480, 72], "Left", {"text": "type.display, letter-spacing 0.12em"}),
        E("Nav", "WBP_MenuRail", [120, 300, 400, 456], "Left", {"button": "56h primary-tertiary, gap 8"}, focus=1),
        E("ContentPanel", "WBP_MOPanel(elev1)", [720, 200, 760, 680], "Center", {"bg": "surface.1"}, focus=2),
        E("SeedInput", "WBP_TextInput+IconButton(random)", [752, 360, 696, 56], "Center", {}, focus=3),
        E("SessionList", "WBP_SessionList", [752, 360, 696, 400], "Center", {"row": "56: host, players 2/4, ping, version"}, note="Join tab"),
        E("Version", "TextBlock", [120, 1016, 400, 24], "BottomLeft", {"text": "type.caption text.secondary"}),
        E("LoadingOverlay", "WBP_LoadingOverlay", [0, 0, 1920, 1080], "Fill", {"bg": "surface.0", "bar": "progress 4px at bottom 64", "tip": "type.body"}),
    ]}

SCREENS["ingame_menu"] = {
    "title": "In-game menu + Save/Load", "layer": "Menu", "input": "menu (game keeps running)",
    "grid": "Scrim 72%; left rail 400 at x=120; world-continues readout top; save/load panel 960 at x=600",
    "elements": [
        E("Scrim", "Border", [0, 0, 1920, 1080], "Fill", {"bg": "color.overlay.scrim"}),
        E("WorldClock", "WBP_WorldContinues", [120, 96, 400, 112], "TopLeft", {"time": "type.numeric.large", "label": "type.caption", "pulse": "clock icon ticks each in-game minute"}),
        E("Nav", "WBP_MenuRail", [120, 240, 400, 400], "Left", {"button": 56}, focus=1),
        E("SavePanel", "WBP_SaveLoadPanel(elev1)", [600, 96, 1200, 888], "Center", {}, focus=2),
        E("SlotCard", "WBP_SaveSlotCard", [624, 168, 1152, 120], "Center", {"thumb": "192x108", "name": "type.heading", "meta": "type.caption + type.numeric.small"}),
    ]}

SCREENS["colony"] = {
    "title": "Colony", "layer": "Menu", "input": "menu",
    "grid": "Panel 1600x880; roster 560 | character card 968; task queue below roster",
    "elements": [
        E("Panel", "WBP_MOPanel(elev1)", [160, 100, 1600, 880], "Center", P),
        E("Tabs", "WBP_TabBar", [184, 116, 1552, 56], "Center", {"tabs": "Roster / Tasks / Relationships"}, focus=1),
        E("Roster", "WBP_PawnRoster", [184, 188, 560, 768], "Center", {"row": "72: portrait 56, name, mood, activity, health bar, alerts"}, focus=2),
        E("CharacterCard", "WBP_CharacterCard(elev2)", [768, 188, 968, 768], "Center", {}, focus=3),
    ]}

SCREENS["skills"] = {
    "title": "Journal: Skills", "layer": "Menu", "input": "menu",
    "grid": "Panel 1600x880; tabs; list 920 | knowledge 608 (paper)",
    "elements": [
        E("Panel", "WBP_MOPanel(elev1)", [160, 100, 1600, 880], "Center", P),
        E("Tabs", "WBP_TabBar", [184, 116, 1552, 56], "Center", {"tabs": "Skills / Knowledge / Quests"}, focus=1),
        E("SkillList", "WBP_SkillList", [184, 188, 920, 768], "Center", {"row": "64: icon, name, level, XP bar with decay marker, 'rusty' tag"}, focus=2),
        E("Knowledge", "WBP_KnowledgePanel(paper)", [1128, 188, 608, 768], "Center", {"bg": "paper.surface"}, focus=3),
    ]}

SCREENS["quests"] = {
    "title": "Journal: Quests", "layer": "Menu", "input": "menu",
    "grid": "Panel 1600x880; list 520 | detail 1008 (paper)",
    "elements": [
        E("Panel", "WBP_MOPanel(elev1)", [160, 100, 1600, 880], "Center", P),
        E("Tabs", "WBP_TabBar", [184, 116, 1552, 56], "Center", {}, focus=1),
        E("QuestList", "WBP_QuestList", [184, 188, 520, 768], "Center", {"row": 56, "groups": "Active / Completed / Failed"}, focus=2),
        E("QuestDetail", "WBP_QuestDetail(paper)", [728, 188, 1008, 680], "Center", {}, focus=3),
        E("Actions", "WBP_ButtonRow", [728, 884, 1008, 72], "Center", {"buttons": "Abandon (danger tertiary) | Track (primary)"}, focus=4),
    ]}

SCREENS["modals"] = {
    "title": "Modals, context menus, states", "layer": "Modal", "input": "modal (captures all)",
    "grid": "Modal widths 480 / 640; 24 padding; footer buttons right-aligned, primary rightmost",
    "elements": [
        E("Confirm", "WBP_ConfirmModal", [720, 380, 480, 0], "Center", {"title": "type.heading", "body": "type.body", "buttons": "2-3, h56"}, focus=1, note="default focus = the SAFE option"),
        E("TextInput", "WBP_TextInputModal", [640, 360, 640, 0], "Center", {}),
        E("QuickPick", "WBP_QuickPickModal", [720, 300, 480, 0], "Center", {"row": 48}),
        E("Toast", "WBP_Toast", [1480, 40, 400, 72], "TopRight", {}),
        E("BugReport", "WBP_BugReportModal", [560, 140, 800, 800], "Center", {"fields": "summary input, category dropdown, details multiline, attach-screenshot toggle, log toggle"}),
        E("ContextMenu", "WBP_ContextMenu", [0, 0, 280, 0], "Cursor", {"row": 44, "keys": "right-aligned keycap hints"}),
    ]}

idx = {}
for k, s in SCREENS.items():
    s["referenceResolution"] = [1920, 1080]
    json.dump(s, open(f"{OUT}/{k}.json", "w"), indent=2)
    idx[k] = f"screens/{k}.json"
    # markdown table too
    lines = [f"# {s['title']}", "", f"Layer: {s['layer']} · Input: {s['input']}", "", f"Grid: {s['grid']}", "",
             "| id | widget | x | y | w | h | anchor | focus | tokens | note |", "|---|---|---|---|---|---|---|---|---|---|"]
    for e in s["elements"]:
        x, y, w, h = e["rect"]
        tk = "; ".join(f"{a}: {b}" for a, b in e["tokens"].items())
        lines.append(f"| {e['id']} | {e['widget']} | {x} | {y} | {w} | {h or 'auto'} | {e['anchor']} | {e['focus'] or '-'} | {tk} | {e.get('note','')} |")
    open(f"{OUT}/{k}.md", "w").write("\n".join(lines) + "\n")
json.dump(idx, open(f"{OUT}/index.json", "w"), indent=2)
print(len(SCREENS), "screens")
