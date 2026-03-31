# Fix missing buttons - use regular Button class
#
# Run: py "D:/UEProjects/MO57/Content/Python/fix_missing_buttons.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def add_widget(wbp, widget_class, widget_name: str, parent_name: str = ""):
    """Add a widget using native Python class (not loaded class)."""
    try:
        parent = unreal.Name(parent_name) if parent_name else unreal.Name("")
        widget = unreal.EditorUtilityLibrary.add_source_widget(
            wbp,
            widget_class,
            unreal.Name(widget_name),
            parent
        )
        if widget:
            log(f"  Added: {widget_name}")
            return widget
        else:
            log(f"  Failed to add: {widget_name}")
            return None
    except Exception as e:
        log(f"  Error adding {widget_name}: {e}")
        return None

def find_widget(wbp, widget_name: str):
    try:
        return unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(widget_name))
    except:
        return None

def save_wbp(wbp, path: str):
    try:
        wbp.modify()
        unreal.EditorAssetLibrary.save_asset(path)
        log(f"  Saved: {path}")
        return True
    except Exception as e:
        log(f"  Error saving: {e}")
        return False

def fix_confirmation():
    """Add missing buttons to WBP_MOConfirmation."""
    log("")
    log("Fixing WBP_MOConfirmation - adding buttons")

    path = "/MOFramework/UI/CommonUI/WBP_MOConfirmation"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return

    # Add buttons using native Button class
    if not find_widget(wbp, "ConfirmButton"):
        add_widget(wbp, unreal.Button, "ConfirmButton", "ButtonBox")
    else:
        log("  ConfirmButton already exists")

    if not find_widget(wbp, "CancelButton"):
        add_widget(wbp, unreal.Button, "CancelButton", "ButtonBox")
    else:
        log("  CancelButton already exists")

    save_wbp(wbp, path)

def fix_list_entry():
    """Add missing button to WBP_MOListEntry."""
    log("")
    log("Fixing WBP_MOListEntry - adding button")

    path = "/MOFramework/UI/CommonUI/WBP_MOListEntry"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return

    if not find_widget(wbp, "EntryButton"):
        add_widget(wbp, unreal.Button, "EntryButton", "BackgroundBorder")
    else:
        log("  EntryButton already exists")

    save_wbp(wbp, path)

def fix_detail_panel():
    """Add missing button to WBP_MODetailPanel."""
    log("")
    log("Fixing WBP_MODetailPanel - adding button")

    path = "/MOFramework/UI/CommonUI/WBP_MODetailPanel"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return

    if not find_widget(wbp, "ActionButton"):
        add_widget(wbp, unreal.Button, "ActionButton", "ContentBox")
    else:
        log("  ActionButton already exists")

    save_wbp(wbp, path)

def fix_colony_portrait():
    """Add missing button to WBP_MOColonyPortrait."""
    log("")
    log("Fixing WBP_MOColonyPortrait - adding button")

    path = "/MOFramework/UI/Colony/WBP_MOColonyPortrait"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return

    if not find_widget(wbp, "ClickButton"):
        add_widget(wbp, unreal.Button, "ClickButton", "AlertBorder")
    else:
        log("  ClickButton already exists")

    save_wbp(wbp, path)

def run():
    log("=" * 60)
    log("Fixing missing buttons (using Button class)")
    log("=" * 60)
    log("")
    log("NOTE: These use UButton. For MOCommonButton bindings,")
    log("you'll need to manually replace them in the designer.")

    fix_confirmation()
    fix_list_entry()
    fix_detail_panel()
    fix_colony_portrait()

    log("")
    log("Done! Open each WBP and:")
    log("  1. Delete the Button widgets")
    log("  2. Add MOCommonButton with the same names")
    log("  3. Compile")

if __name__ == "__main__":
    run()
