print("=== SCRIPT LOADING ===")

# Fix Widget Bindings Complete - Add missing widgets and test IsVariable behavior
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/fix_widget_bindings_complete.py"
#
# This script:
# 1. Adds missing widgets using add_source_widget (which may auto-mark as variable)
# 2. Tests if newly created widgets are properly bound
# 3. Documents findings for manual fixes if needed
#
# KEY FINDING: The bIsVariable flag is stored in WidgetTree metadata and NOT
# exposed to Python. We can create widgets but cannot mark existing widgets
# as variables programmatically.
#
# STRATEGY:
# - For MISSING widgets: Create with add_source_widget (test if auto-variable)
# - For EXISTING widgets that need variables: Document for manual fix

print("=== IMPORTING UNREAL ===")
import unreal
print("=== UNREAL IMPORTED ===")

def log(msg: str):
    print(f"[MO57] {msg}")
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    print(f"[MO57 WARNING] {msg}")
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    print(f"[MO57 ERROR] {msg}")
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
        log_warning("Could not load MOCommonButton, falling back to Button")
        return unreal.Button

    return None

def widget_exists(wbp, widget_name: str) -> bool:
    """Check if a widget exists in the blueprint."""
    widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(widget_name))
    return widget is not None

def create_widget(wbp, widget_name: str, widget_type: str, parent_name: str = ""):
    """
    Create a widget using add_source_widget.

    Returns:
        The created widget, or None on failure
    """
    widget_class = get_widget_class(widget_type)
    if not widget_class:
        log_error(f"    Unknown widget type: {widget_type}")
        return None

    try:
        parent = unreal.Name(parent_name) if parent_name else unreal.Name("")
        new_widget = unreal.EditorUtilityLibrary.add_source_widget(
            wbp,
            widget_class,
            unreal.Name(widget_name),
            parent
        )
        if new_widget:
            log(f"    CREATED: '{widget_name}' as {widget_type}")
            return new_widget
        else:
            log_error(f"    add_source_widget returned None for '{widget_name}'")
            return None
    except Exception as e:
        log_error(f"    Exception creating '{widget_name}': {e}")
        return None

def process_widget_blueprint(wbp_path: str, required_widgets: dict):
    """
    Process a Widget Blueprint - check for missing widgets and create them.

    Args:
        wbp_path: Asset path to the widget blueprint
        required_widgets: Dict of {widget_name: (widget_type, parent_name)}

    Returns:
        Dict with 'created', 'exists_not_var', 'exists_ok' lists
    """
    log("")
    log(f"Processing: {wbp_path}")
    log("-" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"  Failed to load: {wbp_path}")
        return None

    if not isinstance(wbp, unreal.WidgetBlueprint):
        log_error(f"  Not a WidgetBlueprint")
        return None

    results = {
        'created': [],
        'exists_needs_manual_fix': [],
        'path': wbp_path
    }

    changes = False

    for widget_name, (widget_type, parent_name) in required_widgets.items():
        if widget_exists(wbp, widget_name):
            # Widget exists - it probably needs to be manually marked as variable
            log(f"  EXISTS: '{widget_name}' (may need manual 'Is Variable' check)")
            results['exists_needs_manual_fix'].append(widget_name)
        else:
            # Widget doesn't exist - create it
            log(f"  MISSING: '{widget_name}' - creating...")
            widget = create_widget(wbp, widget_name, widget_type, parent_name)
            if widget:
                results['created'].append(widget_name)
                changes = True

    # Save if we made changes
    if changes:
        try:
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            log(f"  SAVED ({len(results['created'])} widgets created)")
        except Exception as e:
            log_error(f"  Save failed: {e}")

    return results

def run():
    log("=" * 70)
    log("Fix Widget Bindings Complete")
    log("=" * 70)
    log("")
    log("This script creates MISSING widgets only.")
    log("Existing widgets that aren't variables need MANUAL fix:")
    log("  Right-click widget in Hierarchy -> 'Is Variable'")
    log("")

    # Define REQUIRED widget bindings only (BindWidget, not BindWidgetOptional)
    # Format: {widget_name: (widget_type, parent_name)}
    # parent_name = "" means add to root

    blueprints = {
        # Progress widgets - UMOProgressWidgetBase requires these
        "/MOFramework/UI/WBP_InspectionProgress": {
            "ProgressBar": ("ProgressBar", ""),
            "ActionNameText": ("TextBlock", ""),
            "TimeRemainingText": ("TextBlock", ""),
            # InstructionText and CancelButton are optional
        },
        "/MOFramework/UI/WBP_HarvestProgress": {
            "ProgressBar": ("ProgressBar", ""),
            "ActionNameText": ("TextBlock", ""),
            "TimeRemainingText": ("TextBlock", ""),
        },
        "/MOFramework/UI/CommonUI/WBP_MOProgressWidget": {
            "ProgressBar": ("ProgressBar", ""),
            "ActionNameText": ("TextBlock", ""),
            "TimeRemainingText": ("TextBlock", ""),
        },
        # Confirmation dialog - UMOConfirmationBase requires these
        "/MOFramework/UI/CommonUI/WBP_MOConfirmation": {
            "TitleText": ("TextBlock", ""),
            "MessageText": ("TextBlock", ""),
            "ConfirmButton": ("MOCommonButton", ""),
            "CancelButton": ("MOCommonButton", ""),
        },
        # The following use BindWidgetOptional, but we'll create them anyway
        # since they're needed for functionality
        "/MOFramework/UI/CommonUI/WBP_MOScrollList": {
            "ContentScrollBox": ("ScrollBox", ""),
        },
        "/MOFramework/UI/CommonUI/WBP_MOListEntry": {
            "BackgroundBorder": ("Border", ""),
            "EntryButton": ("MOCommonButton", ""),
        },
        "/MOFramework/UI/CommonUI/WBP_MODetailPanel": {
            "TitleText": ("TextBlock", ""),
            "IconImage": ("Image", ""),
            "DescriptionText": ("TextBlock", ""),
            "ActionButton": ("MOCommonButton", ""),
        },
    }

    all_results = []

    # Process each blueprint
    for wbp_path, widgets in blueprints.items():
        try:
            result = process_widget_blueprint(wbp_path, widgets)
            if result:
                all_results.append(result)
        except Exception as e:
            log_error(f"Error processing {wbp_path}: {e}")
            import traceback
            traceback.print_exc()

    # Summary
    log("")
    log("=" * 70)
    log("SUMMARY")
    log("=" * 70)

    total_created = 0
    total_needs_fix = 0

    for result in all_results:
        if result['created']:
            log(f"\nCreated in {result['path']}:")
            for name in result['created']:
                log(f"  + {name}")
                total_created += 1

        if result['exists_needs_manual_fix']:
            log(f"\nNeeds manual 'Is Variable' in {result['path']}:")
            for name in result['exists_needs_manual_fix']:
                log(f"  ! {name}")
                total_needs_fix += 1

    log("")
    log(f"Total created: {total_created}")
    log(f"Total needing manual fix: {total_needs_fix}")
    log("")

    if total_needs_fix > 0:
        log("=" * 70)
        log("MANUAL STEPS REQUIRED")
        log("=" * 70)
        log("")
        log("For each widget listed above as needing manual fix:")
        log("  1. Open the Widget Blueprint in UE Editor")
        log("  2. Find the widget in the Hierarchy panel")
        log("  3. Right-click the widget")
        log("  4. Enable 'Is Variable' checkbox")
        log("  5. Compile the Blueprint")
        log("")
        log("This is required because the Python API cannot set the")
        log("'Is Variable' flag - it's stored in WidgetTree metadata")
        log("which is not exposed to Python.")

    if total_created > 0:
        log("")
        log("IMPORTANT: Newly created widgets may or may not be auto-marked")
        log("as variables. Test by compiling after running this script.")
        log("If compilation fails, the created widgets also need manual")
        log("'Is Variable' marking.")

# UE Python doesn't set __name__ to "__main__" - just run directly
print("[MO57] Script starting...")
try:
    run()
except Exception as e:
    print(f"[MO57 FATAL ERROR] {e}")
    import traceback
    traceback.print_exc()
print("[MO57] Script finished.")
