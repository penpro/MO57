# Fix Widget Variables Final - Uses MOWidgetEditorUtils C++ utility
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/fix_widget_variables_final.py"
#
# This script uses the MOWidgetEditorUtils C++ class to set bIsVariable
# on widgets, which is otherwise inaccessible from Python.
#
# REQUIRES: MOFramework module compiled with MOWidgetEditorUtils

print("=== SCRIPT LOADING ===")
import unreal

def log(msg):
    print(f"[MO57] {msg}")

def log_warning(msg):
    print(f"[MO57 WARNING] {msg}")

def log_error(msg):
    print(f"[MO57 ERROR] {msg}")

def fix_widget_blueprint(wbp_path: str, widget_names: list):
    """
    Fix a Widget Blueprint by setting bIsVariable on all specified widgets.

    Args:
        wbp_path: Asset path to the widget blueprint
        widget_names: List of widget names to mark as variables

    Returns:
        Number of widgets successfully modified
    """
    log(f"\nFixing: {wbp_path}")
    log("-" * 50)

    # Load the widget blueprint
    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"  Failed to load blueprint")
        return 0

    if not isinstance(wbp, unreal.WidgetBlueprint):
        log_error(f"  Not a WidgetBlueprint")
        return 0

    # Convert widget names to FName array
    name_array = unreal.Array(unreal.Name)
    for name in widget_names:
        name_array.append(unreal.Name(name))

    # Use the C++ utility to batch set variables
    try:
        modified_count = unreal.MOWidgetEditorUtils.batch_set_widgets_as_variables(wbp, name_array, True)
        log(f"  Modified {modified_count} widgets")

        if modified_count > 0:
            # Save the asset
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            log(f"  SAVED")

        return modified_count
    except Exception as e:
        log_error(f"  Exception: {e}")
        return 0

def run():
    log("=" * 60)
    log("Fix Widget Variables Final")
    log("Using MOWidgetEditorUtils C++ Utility")
    log("=" * 60)

    # Check if MOWidgetEditorUtils is available
    if not hasattr(unreal, 'MOWidgetEditorUtils'):
        log_error("")
        log_error("MOWidgetEditorUtils not found!")
        log_error("Please compile the MOFramework module first.")
        log_error("")
        log_error("Steps:")
        log_error("  1. Close Unreal Editor")
        log_error("  2. Build the project (MO57Editor target)")
        log_error("  3. Reopen Unreal Editor")
        log_error("  4. Run this script again")
        return

    log("")
    log("MOWidgetEditorUtils found - proceeding with fixes...")

    # Define all widget blueprints and their required widget variables
    blueprints = {
        "/MOFramework/UI/WBP_InspectionProgress": [
            "ProgressBar", "ActionNameText", "TimeRemainingText"
        ],
        "/MOFramework/UI/WBP_HarvestProgress": [
            "ProgressBar", "ActionNameText", "TimeRemainingText"
        ],
        "/MOFramework/UI/CommonUI/WBP_MOProgressWidget": [
            "ProgressBar", "ActionNameText", "TimeRemainingText"
        ],
        "/MOFramework/UI/CommonUI/WBP_MOConfirmation": [
            "TitleText", "MessageText", "ConfirmButton", "CancelButton"
        ],
        "/MOFramework/UI/CommonUI/WBP_MOScrollList": [
            "ContentScrollBox"
        ],
        "/MOFramework/UI/CommonUI/WBP_MOListEntry": [
            "BackgroundBorder", "EntryButton"
        ],
        "/MOFramework/UI/CommonUI/WBP_MODetailPanel": [
            "TitleText", "IconImage", "DescriptionText", "ActionButton"
        ],
    }

    total_modified = 0

    for wbp_path, widgets in blueprints.items():
        try:
            modified = fix_widget_blueprint(wbp_path, widgets)
            total_modified += modified
        except Exception as e:
            log_error(f"Error fixing {wbp_path}: {e}")
            import traceback
            traceback.print_exc()

    log("")
    log("=" * 60)
    log(f"COMPLETE - Modified {total_modified} widgets total")
    log("=" * 60)
    log("")
    log("Next steps:")
    log("  1. Open each Widget Blueprint")
    log("  2. Click Compile")
    log("  3. Verify no binding errors")

print("[MO57] Script starting...")
try:
    run()
except Exception as e:
    print(f"[MO57 FATAL ERROR] {e}")
    import traceback
    traceback.print_exc()
print("[MO57] Script finished.")
