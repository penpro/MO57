# Fix WBP_InspectionProgress - Rename ItemNameText to ActionNameText
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/fix_inspection_widget.py"
#
# This script renames the ItemNameText widget to ActionNameText to match
# the new UMOProgressWidgetBase binding requirements.

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def rename_widget_in_blueprint(wbp_path: str, old_name: str, new_name: str):
    """
    Rename a widget in a Widget Blueprint.

    Args:
        wbp_path: Asset path to the Widget Blueprint
        old_name: Current widget name
        new_name: New widget name

    Returns:
        True if successful
    """
    log(f"Loading: {wbp_path}")
    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"Failed to load: {wbp_path}")
        return False

    log(f"Loaded Widget Blueprint: {wbp.get_name()}")

    # Find the existing widget
    old_widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(old_name))
    if not old_widget:
        log_warning(f"Widget '{old_name}' not found")

        # Check if new name already exists
        new_widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(new_name))
        if new_widget:
            log(f"Widget '{new_name}' already exists - no action needed")
            return True
        else:
            log_error(f"Neither '{old_name}' nor '{new_name}' found in blueprint")
            return False

    log(f"Found widget: {old_name} -> {old_widget}")

    # Try to rename using set_editor_property
    try:
        # In UE, widget names in the hierarchy are typically controlled via the variable name
        # Try different approaches to rename

        # Approach 1: Try rename_widget if available
        if hasattr(unreal.EditorUtilityLibrary, 'rename_source_widget'):
            result = unreal.EditorUtilityLibrary.rename_source_widget(wbp, old_widget, unreal.Name(new_name))
            if result:
                log(f"Renamed via rename_source_widget: {old_name} -> {new_name}")
                wbp.modify()
                unreal.EditorAssetLibrary.save_asset(wbp_path)
                return True

        # Approach 2: Try setting the widget's name directly
        if hasattr(old_widget, 'rename'):
            old_widget.rename(new_name)
            log(f"Renamed via widget.rename(): {old_name} -> {new_name}")
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            return True

        # Approach 3: Use WidgetBlueprintLibrary if available
        if hasattr(unreal, 'WidgetBlueprintLibrary'):
            wbl = unreal.WidgetBlueprintLibrary
            if hasattr(wbl, 'rename_widget'):
                wbl.rename_widget(wbp, old_name, new_name)
                log(f"Renamed via WidgetBlueprintLibrary: {old_name} -> {new_name}")
                wbp.modify()
                unreal.EditorAssetLibrary.save_asset(wbp_path)
                return True

        # Approach 4: Access the widget tree and rename there
        widget_tree = None
        if hasattr(wbp, 'widget_tree'):
            widget_tree = wbp.widget_tree()
        elif hasattr(wbp, 'get_widget_tree'):
            widget_tree = wbp.get_widget_tree()

        if widget_tree:
            log(f"Got widget_tree: {widget_tree}")
            if hasattr(widget_tree, 'rename_widget'):
                widget_tree.rename_widget(old_widget, unreal.Name(new_name))
                log(f"Renamed via widget_tree.rename_widget: {old_name} -> {new_name}")
                wbp.modify()
                unreal.EditorAssetLibrary.save_asset(wbp_path)
                return True

        # Approach 5: Try using FName/set_name
        if hasattr(old_widget, 'set_name'):
            old_widget.set_name(new_name)
            log(f"Renamed via set_name: {old_name} -> {new_name}")
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            return True

        # List available methods for debugging
        log("Available methods on widget:")
        methods = [m for m in dir(old_widget) if not m.startswith('_') and 'name' in m.lower()]
        for m in methods:
            log(f"  {m}")

        log("Available methods on EditorUtilityLibrary:")
        methods = [m for m in dir(unreal.EditorUtilityLibrary) if not m.startswith('_') and ('widget' in m.lower() or 'rename' in m.lower())]
        for m in methods:
            log(f"  {m}")

        log_warning("Could not find a method to rename the widget programmatically")
        log_warning("You may need to rename it manually in the Widget Designer")
        return False

    except Exception as e:
        log_error(f"Exception during rename: {e}")
        return False

def run():
    """Main entry point."""
    log("=" * 60)
    log("Fix WBP_InspectionProgress - Rename ItemNameText to ActionNameText")
    log("=" * 60)

    success = rename_widget_in_blueprint(
        "/MOFramework/UI/WBP_InspectionProgress",
        "ItemNameText",
        "ActionNameText"
    )

    if success:
        log("")
        log("SUCCESS! Widget renamed. Please compile the Blueprint.")
    else:
        log("")
        log("Rename failed. Please rename manually:")
        log("  1. Open WBP_InspectionProgress in Widget Designer")
        log("  2. Find 'ItemNameText' in the Hierarchy")
        log("  3. Rename it to 'ActionNameText'")
        log("  4. Compile and Save")

    log("")
    log("Done!")

if __name__ == "__main__":
    run()
