# Recreate Widget Bindings - Remove and re-add widgets to fix IsVariable
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/recreate_widget_bindings.py"
#
# This script removes widgets and re-adds them using add_source_widget,
# which should properly mark them as variables.

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def get_widget_class(widget_type: str):
    """Get the UClass for a widget type."""
    type_map = {
        "ProgressBar": unreal.ProgressBar,
        "TextBlock": unreal.TextBlock,
        "Image": unreal.Image,
        "Button": unreal.Button,
        "Border": unreal.Border,
        "CanvasPanel": unreal.CanvasPanel,
        "VerticalBox": unreal.VerticalBox,
        "HorizontalBox": unreal.HorizontalBox,
        "ScrollBox": unreal.ScrollBox,
        "Overlay": unreal.Overlay,
    }

    if widget_type in type_map:
        return type_map[widget_type]

    # Try to load MOCommonButton
    if widget_type == "MOCommonButton":
        cls = unreal.load_class(None, "/Script/MOFramework.MOCommonButton")
        if cls:
            return cls
        # Fallback to regular button
        return unreal.Button

    return None

def remove_and_readd_widget(wbp, widget_name: str, widget_type: str, parent_name: str = ""):
    """
    Remove a widget and re-add it to potentially fix IsVariable.

    Args:
        wbp: The WidgetBlueprint
        widget_name: Name of the widget
        widget_type: Type of widget (e.g., "ProgressBar", "TextBlock")
        parent_name: Parent widget name, or "" for root

    Returns:
        True if successful
    """
    # First, find the existing widget
    existing = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(widget_name))
    if not existing:
        log(f"    Widget '{widget_name}' not found, creating new...")
    else:
        log(f"    Found existing '{widget_name}' ({type(existing).__name__})")

        # Try to remove it first
        if hasattr(unreal.EditorUtilityLibrary, 'remove_source_widget'):
            try:
                unreal.EditorUtilityLibrary.remove_source_widget(wbp, existing)
                log(f"    Removed existing widget")
            except Exception as e:
                log_warning(f"    Could not remove: {e}")
                # Continue anyway - we'll try to add

    # Get the widget class
    widget_class = get_widget_class(widget_type)
    if not widget_class:
        log_error(f"    Unknown widget type: {widget_type}")
        return False

    # Add the widget
    try:
        parent = unreal.Name(parent_name) if parent_name else unreal.Name("")
        new_widget = unreal.EditorUtilityLibrary.add_source_widget(
            wbp,
            widget_class,
            unreal.Name(widget_name),
            parent
        )
        if new_widget:
            log(f"    Added '{widget_name}' as {widget_type}")
            return True
        else:
            log_error(f"    Failed to add '{widget_name}'")
            return False
    except Exception as e:
        log_error(f"    Error adding widget: {e}")
        return False

def fix_blueprint(wbp_path: str, widgets: list):
    """
    Fix a Widget Blueprint by recreating widgets.

    Args:
        wbp_path: Asset path
        widgets: List of (name, type, parent) tuples

    Returns:
        True if changes were made
    """
    log("")
    log(f"Fixing: {wbp_path}")
    log("-" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"  Failed to load: {wbp_path}")
        return False

    if not isinstance(wbp, unreal.WidgetBlueprint):
        log_error(f"  Not a WidgetBlueprint")
        return False

    changes = False
    for widget_name, widget_type, parent_name in widgets:
        if remove_and_readd_widget(wbp, widget_name, widget_type, parent_name):
            changes = True

    if changes:
        try:
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            log(f"  SAVED")
        except Exception as e:
            log_error(f"  Failed to save: {e}")
            return False

    return changes

def try_add_member_variable(wbp_path: str, var_name: str, var_type: str):
    """
    Try to add a member variable to the Blueprint.
    """
    log(f"  Trying add_member_variable for '{var_name}'...")

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        return False

    try:
        # Try to add as member variable
        # The variable type should match the widget class
        result = unreal.BlueprintEditorLibrary.add_member_variable(
            wbp,
            var_name,
            var_type  # e.g., "ProgressBar" or full path
        )
        if result:
            log(f"    Added member variable: {var_name}")
            return True
        else:
            log_warning(f"    add_member_variable returned False")
            return False
    except Exception as e:
        log_error(f"    Exception: {e}")
        return False

def run():
    log("=" * 70)
    log("Recreate Widget Bindings")
    log("=" * 70)

    # First, let's just try to add member variables for the required bindings
    # This might link existing widgets to variables

    test_path = "/MOFramework/UI/WBP_InspectionProgress"

    # Try adding member variables
    log("")
    log("Attempting to add member variables directly:")
    log("-" * 60)

    # Common widget types and their class paths
    var_attempts = [
        ("ProgressBar", "ProgressBar"),
        ("ActionNameText", "TextBlock"),
        ("TimeRemainingText", "TextBlock"),
    ]

    wbp = unreal.EditorAssetLibrary.load_asset(test_path)
    if wbp:
        for var_name, var_type in var_attempts:
            try_add_member_variable(test_path, var_name, var_type)

        try:
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(test_path)
            log("Saved changes")
        except Exception as e:
            log_error(f"Failed to save: {e}")

    log("")
    log("=" * 70)
    log("Complete - Check if blueprints compile now")
    log("=" * 70)

if __name__ == "__main__":
    run()
