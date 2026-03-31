# Fix All UI Issues - Comprehensive automated fix for Widget Blueprints
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/fix_all_ui_issues.py"
#
# This script runs all fixes in sequence:
# 1. Mark widgets as variables (for BindWidget)
# 2. Apply smart renames (generic names -> meaningful names)
# 3. Validate and report remaining issues
#
# REQUIRES: MOWidgetEditorUtils C++ utility

print("=== FIX ALL UI ISSUES LOADING ===")
import unreal
from collections import defaultdict
import os
import re

OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "fix_all_report.txt")
os.makedirs(OUTPUT_DIR, exist_ok=True)

_output_lines = []

def log(msg):
    print(f"[FIX-ALL] {msg}")
    _output_lines.append(msg)

def write_output():
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_output_lines))
    print(f"[FIX-ALL] Output written to: {OUTPUT_FILE}")

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
# PHASE 1: Mark Widgets as Variables
# ============================================================

# Patterns for widgets that should be marked as variables
VARIABLE_PATTERNS = [
    "Button",
    "Text",
    "Image",
    "ProgressBar",
    "ScrollBox",
    "Border",
    "Icon",
]

# Specific widget names that must be variables
REQUIRED_VARIABLES = [
    "TitleText", "MessageText", "DescriptionText", "ActionNameText",
    "ConfirmButton", "CancelButton", "CloseButton", "BackButton",
    "ProgressBar", "TimeRemainingText", "ContentScrollBox",
    "IconImage", "BackgroundBorder", "EntryButton", "CraftButton",
    "BuildButton", "SelectButton", "RecipeNameText", "RecipeIcon",
    "ItemNameText", "ItemIcon", "CountText", "StatusText",
    "NameText", "ValueText", "LabelText",
]

def fix_variables(wbp, bp_name):
    """Mark important widgets as variables."""
    fixed = 0

    # Mark by pattern
    for pattern in VARIABLE_PATTERNS:
        count = unreal.MOWidgetEditorUtils.batch_set_variables_by_pattern(wbp, pattern, True)
        fixed += count

    # Mark required variables
    name_array = unreal.Array(unreal.Name)
    for name in REQUIRED_VARIABLES:
        name_array.append(unreal.Name(name))

    count = unreal.MOWidgetEditorUtils.batch_set_widgets_as_variables(wbp, name_array, True)
    # Don't double count - pattern matching may have already caught these

    return fixed

# ============================================================
# PHASE 2: Smart Rename Generic Widgets
# ============================================================

GENERIC_PATTERN = re.compile(r'^(Border|CanvasPanel|SizeBox|TextBlock|Image|HorizontalBox|VerticalBox|Overlay|ScrollBox)_?\d*$')

# Context-based rename rules
RENAME_RULES = {
    # (widget_type, parent_contains) -> new_name
    ('CanvasPanel', ''): 'RootCanvas',
    ('Border', 'Background'): 'BackgroundBorder',
    ('Border', 'Content'): 'ContentBorder',
    ('Border', 'Entry'): 'EntryBorder',
    ('Border', 'Header'): 'HeaderBorder',
    ('VerticalBox', 'Content'): 'ContentBox',
    ('VerticalBox', 'List'): 'ListBox',
    ('HorizontalBox', 'Header'): 'HeaderBox',
    ('HorizontalBox', 'Button'): 'ButtonBox',
    ('ScrollBox', ''): 'ContentScrollBox',
    ('TextBlock', 'Title'): 'TitleText',
    ('TextBlock', 'Name'): 'NameText',
    ('TextBlock', 'Desc'): 'DescriptionText',
    ('TextBlock', 'Value'): 'ValueText',
    ('TextBlock', 'Label'): 'LabelText',
    ('TextBlock', 'Count'): 'CountText',
    ('TextBlock', 'Status'): 'StatusText',
    ('Image', 'Icon'): 'IconImage',
    ('Image', 'Background'): 'BackgroundImage',
    ('SizeBox', 'Icon'): 'IconSizeBox',
}

def get_rename_suggestion(widget_name, widget_type, parent_name):
    """Get a suggested rename for a generic widget."""
    if not re.match(GENERIC_PATTERN, widget_name):
        return None

    base_type = widget_type.replace('Common', '').replace('MO', '')

    # Check rules
    for (rule_type, rule_parent), new_name in RENAME_RULES.items():
        if base_type == rule_type:
            if rule_parent == '' or rule_parent.lower() in parent_name.lower():
                return new_name

    # Default: prefix with parent name
    if parent_name and not re.match(GENERIC_PATTERN, parent_name):
        clean_parent = re.sub(r'_\d+$', '', parent_name)
        return f"{clean_parent}{base_type}"

    return None

def fix_renames(wbp, bp_name):
    """Rename generic widgets to meaningful names."""
    layouts = unreal.MOWidgetEditorUtils.get_all_widget_layout_info(wbp)

    # Build existing names set
    existing_names = set()
    widgets = []

    for layout in layouts:
        name = str(layout.name)
        existing_names.add(name)
        widgets.append({
            'name': name,
            'type': str(layout.widget_type),
            'parent': str(layout.parent_name)
        })

    # Find and apply renames
    renamed = 0

    for widget in widgets:
        suggested = get_rename_suggestion(widget['name'], widget['type'], widget['parent'])

        if suggested and suggested != widget['name']:
            # Make unique
            final_name = suggested
            counter = 1
            while final_name in existing_names:
                final_name = f"{suggested}_{counter}"
                counter += 1

            # Apply rename
            success = unreal.MOWidgetEditorUtils.rename_widget(wbp, widget['name'], final_name)
            if success:
                existing_names.discard(widget['name'])
                existing_names.add(final_name)
                renamed += 1

    return renamed

# ============================================================
# MAIN
# ============================================================

def run():
    log("=" * 80)
    log("FIX ALL UI ISSUES")
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

        # Phase 1: Fix variables
        var_count = fix_variables(wbp, bp_name)
        if var_count > 0:
            total_variables += var_count
            bp_modified = True

        # Phase 2: Fix renames
        rename_count = fix_renames(wbp, bp_name)
        if rename_count > 0:
            total_renames += rename_count
            bp_modified = True
            log(f"  {bp_name}: {rename_count} renames")

        # Save if modified
        if bp_modified:
            unreal.EditorAssetLibrary.save_asset(path)
            blueprints_modified += 1

    # Final validation
    log(f"\n" + "=" * 80)
    log("VALIDATION")
    log("=" * 80)

    remaining_issues = 0
    for path in wbp_paths:
        wbp = unreal.EditorAssetLibrary.load_asset(path)
        if not wbp:
            continue

        bp_name = path.split('/')[-1]
        issues = unreal.MOWidgetEditorUtils.validate_widget_blueprint(wbp)

        errors_warnings = [i for i in issues if str(i.severity) in ['Error', 'Warning']]
        if errors_warnings:
            remaining_issues += len(errors_warnings)
            log(f"\n  {bp_name}: {len(errors_warnings)} remaining issues")
            for issue in errors_warnings[:3]:
                log(f"    - {issue.message}")

    log(f"\n" + "=" * 80)
    log("SUMMARY")
    log("=" * 80)
    log(f"\n  Blueprints modified: {blueprints_modified}")
    log(f"  Widgets marked as variables: {total_variables}")
    log(f"  Widgets renamed: {total_renames}")
    log(f"  Remaining validation issues: {remaining_issues}")

    if blueprints_modified > 0:
        log("\n  IMPORTANT: Open modified blueprints and click Compile!")

    log(f"\n" + "=" * 80)
    log("FIX ALL COMPLETE")
    log("=" * 80)

print("[FIX-ALL] Starting...")
try:
    run()
    write_output()
except Exception as e:
    print(f"[FIX-ALL ERROR] {e}")
    _output_lines.append(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    try:
        write_output()
    except:
        pass
print("[FIX-ALL] Finished.")
