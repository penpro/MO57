# Fix Widget Variables V3 - Use get_object_reference_type for proper pin types
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/fix_widget_variables_v3.py"
#
# DISCOVERY: BlueprintEditorLibrary has get_object_reference_type() which creates
# proper object reference EdGraphPinType for a given class.

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def get_pin_type_for_class(widget_class):
    """
    Get the proper EdGraphPinType for an object reference to a widget class.

    Uses BlueprintEditorLibrary.get_object_reference_type() for proper typing.
    """
    try:
        # Get the UClass for the widget
        if hasattr(widget_class, 'static_class'):
            cls = widget_class.static_class()
        else:
            cls = widget_class

        # Use the proper API to create an object reference type
        pin_type = unreal.BlueprintEditorLibrary.get_object_reference_type(cls)
        log(f"    Created pin type via get_object_reference_type: {pin_type}")
        return pin_type
    except Exception as e:
        log_warning(f"    get_object_reference_type failed: {e}")
        # Fallback to empty pin type
        return unreal.EdGraphPinType()

def add_widget_variable(wbp, var_name: str, widget_class):
    """
    Add a member variable that references a widget class.

    Args:
        wbp: WidgetBlueprint
        var_name: Variable name (should match widget name for BindWidget)
        widget_class: The widget class (e.g., unreal.ProgressBar)

    Returns:
        True if successful
    """
    log(f"  Adding variable: {var_name}")

    # Get proper pin type for the widget class
    pin_type = get_pin_type_for_class(widget_class)

    # Add the variable
    try:
        result = unreal.BlueprintEditorLibrary.add_member_variable(
            wbp,
            var_name,
            pin_type
        )
        if result:
            log(f"    SUCCESS: Variable added")
            return True
        else:
            log_warning(f"    add_member_variable returned False (variable may already exist)")
            return False
    except Exception as e:
        log_error(f"    add_member_variable failed: {e}")
        return False

def fix_widget_blueprint(wbp_path: str, widgets: dict):
    """
    Fix a Widget Blueprint by adding properly-typed variables for each widget.

    Args:
        wbp_path: Asset path
        widgets: Dict of {name: class} for widgets that need variables

    Returns:
        True if any changes made
    """
    log("")
    log(f"Fixing: {wbp_path}")
    log("-" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"  Failed to load")
        return False

    if not isinstance(wbp, unreal.WidgetBlueprint):
        log_error(f"  Not a WidgetBlueprint")
        return False

    changes = False
    for var_name, widget_class in widgets.items():
        # Check if widget exists in the blueprint
        widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(var_name))
        if widget:
            log(f"  Widget '{var_name}' exists ({type(widget).__name__})")
            if add_widget_variable(wbp, var_name, widget_class):
                changes = True
        else:
            log_warning(f"  Widget '{var_name}' NOT FOUND - skipping")

    if changes:
        try:
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            log(f"  SAVED")
        except Exception as e:
            log_error(f"  Save failed: {e}")

    return changes

def run():
    log("=" * 70)
    log("Fix Widget Variables V3 - Using get_object_reference_type")
    log("=" * 70)

    # Define all blueprints and their widget bindings
    blueprints = {
        "/MOFramework/UI/WBP_InspectionProgress": {
            "ProgressBar": unreal.ProgressBar,
            "ActionNameText": unreal.TextBlock,
            "TimeRemainingText": unreal.TextBlock,
            "InstructionText": unreal.TextBlock,
            # CancelButton uses MOCommonButton - try to load it
        },
        "/MOFramework/UI/WBP_HarvestProgress": {
            "ProgressBar": unreal.ProgressBar,
            "ActionNameText": unreal.TextBlock,
            "TimeRemainingText": unreal.TextBlock,
            "InstructionText": unreal.TextBlock,
        },
        "/MOFramework/UI/CommonUI/WBP_MOProgressWidget": {
            "ProgressBar": unreal.ProgressBar,
            "TimeRemainingText": unreal.TextBlock,
        },
        "/MOFramework/UI/CommonUI/WBP_MOConfirmation": {
            "TitleText": unreal.TextBlock,
            "MessageText": unreal.TextBlock,
            "ContentBox": unreal.VerticalBox,
        },
        "/MOFramework/UI/CommonUI/WBP_MOScrollList": {
            "ContentScrollBox": unreal.ScrollBox,
        },
        "/MOFramework/UI/CommonUI/WBP_MOListEntry": {
            "BackgroundBorder": unreal.Border,
        },
        "/MOFramework/UI/CommonUI/WBP_MODetailPanel": {
            "TitleText": unreal.TextBlock,
            "IconImage": unreal.Image,
            "DescriptionText": unreal.TextBlock,
            "ContentBox": unreal.VerticalBox,
        },
    }

    # Try to load MOCommonButton class for button widgets
    mo_button_class = unreal.load_class(None, "/Script/MOFramework.MOCommonButton")
    if mo_button_class:
        log(f"Loaded MOCommonButton class: {mo_button_class}")
        # Add button widgets
        blueprints["/MOFramework/UI/WBP_InspectionProgress"]["CancelButton"] = mo_button_class
        blueprints["/MOFramework/UI/CommonUI/WBP_MOConfirmation"]["ConfirmButton"] = mo_button_class
        blueprints["/MOFramework/UI/CommonUI/WBP_MOConfirmation"]["CancelButton"] = mo_button_class
        blueprints["/MOFramework/UI/CommonUI/WBP_MOListEntry"]["EntryButton"] = mo_button_class
        blueprints["/MOFramework/UI/CommonUI/WBP_MODetailPanel"]["ActionButton"] = mo_button_class
    else:
        log_warning("Could not load MOCommonButton - button variables will be skipped")

    # Fix each blueprint
    for wbp_path, widgets in blueprints.items():
        try:
            fix_widget_blueprint(wbp_path, widgets)
        except Exception as e:
            log_error(f"Error fixing {wbp_path}: {e}")

    log("")
    log("=" * 70)
    log("Complete!")
    log("=" * 70)
    log("")
    log("Next steps:")
    log("  1. Compile each Blueprint in the editor")
    log("  2. If still failing, check if widgets are marked as variables")
    log("  3. Run inspect script to verify: py \"D:/UEProjects/MO57/Content/Python/inspect_widget_blueprints.py\"")

if __name__ == "__main__":
    run()
