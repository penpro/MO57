# Fix Widget Variables - Mark all binding widgets as variables
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/fix_widget_variables.py"
#
# This script finds widgets that need to be BindWidget bindings and marks them
# as variables so the C++ BindWidget meta works correctly.

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def set_widget_as_variable(wbp, widget_name: str) -> bool:
    """
    Find a widget and mark it as a variable.

    Returns True if successful.
    """
    widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(widget_name))
    if not widget:
        return False

    # Try to set b_is_variable
    try:
        if hasattr(widget, 'set_editor_property'):
            widget.set_editor_property('b_is_variable', True)
            log(f"    [OK] {widget_name} -> IsVariable=True")
            return True
        elif hasattr(widget, 'b_is_variable'):
            widget.b_is_variable = True
            log(f"    [OK] {widget_name} -> IsVariable=True")
            return True
        else:
            log_warning(f"    [??] {widget_name} - cannot set b_is_variable")
            return False
    except Exception as e:
        log_error(f"    [FAIL] {widget_name}: {e}")
        return False

def fix_widget_blueprint(wbp_path: str, widget_names: list) -> bool:
    """
    Fix a widget blueprint by marking specified widgets as variables.

    Args:
        wbp_path: Asset path to the Widget Blueprint
        widget_names: List of widget names to mark as variables

    Returns:
        True if any changes were made
    """
    log("")
    log(f"Fixing: {wbp_path}")
    log("-" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"  Failed to load: {wbp_path}")
        return False

    # Check if it's a valid WidgetBlueprint
    if not isinstance(wbp, unreal.WidgetBlueprint):
        # Try to cast it
        try:
            wbp = unreal.EditorUtilityLibrary.cast_to_widget_blueprint(wbp)
            if not wbp:
                log_error(f"  Not a WidgetBlueprint: {wbp_path}")
                return False
        except:
            log_error(f"  Could not cast to WidgetBlueprint: {wbp_path}")
            return False

    changes_made = False
    for name in widget_names:
        if set_widget_as_variable(wbp, name):
            changes_made = True

    if changes_made:
        try:
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            log(f"  SAVED: {wbp_path}")
        except Exception as e:
            log_error(f"  Failed to save: {e}")
            return False
    else:
        log(f"  No changes needed or widgets not found")

    return changes_made

def run():
    """Fix all widget blueprints."""
    log("=" * 70)
    log("Fix Widget Variables - Mark BindWidget widgets as variables")
    log("=" * 70)

    # Define which widgets need IsVariable=True for each blueprint
    blueprints_to_fix = {
        "/MOFramework/UI/WBP_InspectionProgress": [
            "ProgressBar",
            "ActionNameText",
            "TimeRemainingText",
            "InstructionText",  # Optional but good to have
            "CancelButton",     # Optional but good to have
        ],
        "/MOFramework/UI/WBP_HarvestProgress": [
            "ProgressBar",
            "ActionNameText",
            "TimeRemainingText",
            "InstructionText",
            "CancelButton",
        ],
        "/MOFramework/UI/CommonUI/WBP_MOProgressWidget": [
            "ProgressBar",
            "ActionNameText",
            "TimeRemainingText",
            "InstructionText",
            "CancelButton",
        ],
        "/MOFramework/UI/CommonUI/WBP_MOConfirmation": [
            "TitleText",
            "MessageText",
            "ConfirmButton",
            "CancelButton",
            "ContentBox",
        ],
        "/MOFramework/UI/CommonUI/WBP_MOScrollList": [
            "ContentScrollBox",
        ],
        "/MOFramework/UI/CommonUI/WBP_MOListEntry": [
            "BackgroundBorder",
            "EntryButton",
        ],
        "/MOFramework/UI/CommonUI/WBP_MODetailPanel": [
            "TitleText",
            "IconImage",
            "DescriptionText",
            "ActionButton",
            "ContentBox",
        ],
        "/MOFramework/UI/WBP_SkillsPanel": [
            "CloseButton",
            "ContentScrollBox",
            "SkillListContainer",
        ],
    }

    total_fixed = 0
    for wbp_path, widgets in blueprints_to_fix.items():
        try:
            if fix_widget_blueprint(wbp_path, widgets):
                total_fixed += 1
        except Exception as e:
            log_error(f"Error fixing {wbp_path}: {e}")

    log("")
    log("=" * 70)
    log(f"Complete! Fixed {total_fixed} blueprints.")
    log("=" * 70)
    log("")
    log("Next steps:")
    log("  1. Compile each Blueprint (right-click -> Compile)")
    log("  2. If errors persist, open Blueprint and verify widgets exist")
    log("  3. Re-run inspect script to verify: py \"D:/UEProjects/MO57/Content/Python/inspect_widget_blueprints.py\"")

if __name__ == "__main__":
    run()
