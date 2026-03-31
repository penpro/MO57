# Batch UI Operations - Apply changes across multiple Widget Blueprints
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/batch_ui_operations.py"
#
# This script provides batch operations for UI maintenance:
# - Mark widgets as variables by pattern
# - Rename widgets by prefix
# - Standardize anchor presets
#
# REQUIRES: MOWidgetEditorUtils C++ utility
# Output: D:/UEProjects/MO57/Content/Python/audit_output/batch_operations.txt

print("=== BATCH UI OPERATIONS SCRIPT LOADING ===")
import unreal
import os

OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "batch_operations.txt")
os.makedirs(OUTPUT_DIR, exist_ok=True)

_output_lines = []

def log(msg):
    print(f"[BATCH] {msg}")
    _output_lines.append(msg)

def write_output():
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_output_lines))
    print(f"[BATCH] Output written to: {OUTPUT_FILE}")

def get_all_widget_blueprints():
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    filter = unreal.ARFilter(
        class_names=["WidgetBlueprint"],
        package_paths=["/MOFramework"],
        recursive_paths=True
    )
    assets = asset_registry.get_assets(filter)
    return [str(asset.package_name) for asset in assets]

# ============================================================
# BATCH OPERATIONS - Modify these to customize behavior
# ============================================================

# Patterns for marking widgets as variables
VARIABLE_PATTERNS = [
    "Button",      # All buttons should be bindable
    "Text",        # All text blocks should be bindable
    "Image",       # All images should be bindable
    "ProgressBar", # All progress bars
    "ScrollBox",   # All scroll boxes
    "Border",      # Borders used for styling
]

# Prefix renames (old -> new)
PREFIX_RENAMES = {
    # "txt": "Text",     # Uncomment to enable
    # "btn": "Button",
    # "img": "Image",
}

# Widgets that should always be marked as variables (exact names)
REQUIRED_VARIABLES = [
    "TitleText",
    "MessageText",
    "DescriptionText",
    "ConfirmButton",
    "CancelButton",
    "CloseButton",
    "BackButton",
    "ProgressBar",
    "TimeRemainingText",
    "ActionNameText",
    "ContentScrollBox",
    "IconImage",
    "BackgroundBorder",
    "EntryButton",
    "CraftButton",
    "BuildButton",
]

# ============================================================
# OPERATION FUNCTIONS
# ============================================================

def mark_variables_by_pattern(wbp, bp_name):
    """Mark widgets as variables based on name patterns."""
    total_modified = 0
    for pattern in VARIABLE_PATTERNS:
        count = unreal.MOWidgetEditorUtils.batch_set_variables_by_pattern(wbp, pattern, True)
        if count > 0:
            log(f"    {bp_name}: Marked {count} widgets matching '{pattern}'")
            total_modified += count
    return total_modified

def mark_required_variables(wbp, bp_name):
    """Mark specific required widgets as variables."""
    # Convert to FName array
    name_array = unreal.Array(unreal.Name)
    for name in REQUIRED_VARIABLES:
        name_array.append(unreal.Name(name))

    count = unreal.MOWidgetEditorUtils.batch_set_widgets_as_variables(wbp, name_array, True)
    if count > 0:
        log(f"    {bp_name}: Marked {count} required variable widgets")
    return count

def rename_by_prefix(wbp, bp_name):
    """Rename widgets by prefix substitution."""
    total_renamed = 0
    for old_prefix, new_prefix in PREFIX_RENAMES.items():
        count = unreal.MOWidgetEditorUtils.batch_rename_by_prefix(wbp, old_prefix, new_prefix)
        if count > 0:
            log(f"    {bp_name}: Renamed {count} widgets from '{old_prefix}*' to '{new_prefix}*'")
            total_renamed += count
    return total_renamed

def run():
    log("=" * 80)
    log("Batch UI Operations")
    log("=" * 80)

    if not hasattr(unreal, 'MOWidgetEditorUtils'):
        log("\nERROR: MOWidgetEditorUtils not found!")
        log("Please compile the MOFramework module first.")
        return

    wbp_paths = get_all_widget_blueprints()
    log(f"\nProcessing {len(wbp_paths)} Widget Blueprints...")

    total_variables = 0
    total_renames = 0
    blueprints_modified = 0

    for path in wbp_paths:
        wbp = unreal.EditorAssetLibrary.load_asset(path)
        if not wbp:
            continue

        bp_name = path.split('/')[-1]
        bp_modified = False

        # Mark variables by pattern
        count = mark_variables_by_pattern(wbp, bp_name)
        if count > 0:
            total_variables += count
            bp_modified = True

        # Mark required variables
        count = mark_required_variables(wbp, bp_name)
        if count > 0:
            total_variables += count
            bp_modified = True

        # Rename by prefix (if any renames configured)
        if PREFIX_RENAMES:
            count = rename_by_prefix(wbp, bp_name)
            if count > 0:
                total_renames += count
                bp_modified = True

        # Save if modified
        if bp_modified:
            unreal.EditorAssetLibrary.save_asset(path)
            blueprints_modified += 1

    log(f"\n" + "=" * 80)
    log("SUMMARY")
    log("=" * 80)
    log(f"  Blueprints modified: {blueprints_modified}")
    log(f"  Widgets marked as variables: {total_variables}")
    log(f"  Widgets renamed: {total_renames}")

    if total_variables > 0 or total_renames > 0:
        log("\n  NOTE: Open modified blueprints and Compile to verify changes")

    log(f"\n" + "=" * 80)
    log("BATCH OPERATIONS COMPLETE")
    log("=" * 80)

print("[BATCH] Starting...")
try:
    run()
    write_output()
except Exception as e:
    print(f"[BATCH ERROR] {e}")
    _output_lines.append(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    try:
        write_output()
    except:
        pass
print("[BATCH] Finished.")
