# Apply Widget Renames - Batch rename widgets based on smart suggestions
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/apply_widget_renames.py"
#
# This script applies the renames suggested by smart_rename_widgets.py
# Set DRY_RUN = True to preview changes without applying them
#
# REQUIRES: MOWidgetEditorUtils C++ utility

print("=== APPLY WIDGET RENAMES LOADING ===")
import unreal
from collections import defaultdict
import os
import re

OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "rename_applied.txt")
os.makedirs(OUTPUT_DIR, exist_ok=True)

# ============================================================
# CONFIGURATION
# ============================================================

# Set to False to actually apply renames
DRY_RUN = True

# Only process these blueprints (empty = all)
ONLY_BLUEPRINTS = []

# Skip these blueprints
SKIP_BLUEPRINTS = []

# ============================================================

_output_lines = []

def log(msg):
    print(f"[RENAME] {msg}")
    _output_lines.append(msg)

def write_output():
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_output_lines))
    print(f"[RENAME] Output written to: {OUTPUT_FILE}")

def get_all_widget_blueprints():
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    filter = unreal.ARFilter(
        class_names=["WidgetBlueprint"],
        package_paths=["/MOFramework"],
        recursive_paths=True
    )
    assets = asset_registry.get_assets(filter)
    return [str(asset.package_name) for asset in assets]

GENERIC_PATTERN = re.compile(r'^(Border|CanvasPanel|SizeBox|TextBlock|Image|Button|HorizontalBox|VerticalBox|Overlay|ScrollBox|Spacer)_?\d*$')

# Standard rename mappings based on common patterns
# Format: (old_pattern, new_name, condition_func)
RENAME_RULES = [
    # Root canvas panels
    (r'^CanvasPanel_?\d*$', 'RootCanvas', lambda w, p, bp: p == '' or w['depth'] == 0),

    # Background borders at root level
    (r'^Border_?\d*$', 'BackgroundBorder', lambda w, p, bp: w['depth'] <= 1),

    # Size boxes containing icons
    (r'^SizeBox_?\d*$', 'IconSizeBox', lambda w, p, bp: 'Icon' in p or 'Image' in p),

    # Content containers
    (r'^VerticalBox_?\d*$', 'ContentBox', lambda w, p, bp: 'Content' in p or 'Panel' in p),
    (r'^HorizontalBox_?\d*$', 'HeaderBox', lambda w, p, bp: 'Header' in p or 'Title' in p),
    (r'^ScrollBox_?\d*$', 'ContentScrollBox', lambda w, p, bp: True),

    # Text blocks - context based
    (r'^TextBlock_?\d*$', 'TitleText', lambda w, p, bp: 'Title' in p),
    (r'^TextBlock_?\d*$', 'NameText', lambda w, p, bp: 'Name' in p),
    (r'^TextBlock_?\d*$', 'DescriptionText', lambda w, p, bp: 'Description' in p or 'Desc' in p),
    (r'^TextBlock_?\d*$', 'ValueText', lambda w, p, bp: 'Value' in p),
    (r'^TextBlock_?\d*$', 'LabelText', lambda w, p, bp: 'Label' in p),
    (r'^TextBlock_?\d*$', 'CountText', lambda w, p, bp: 'Count' in p or 'Quantity' in p),
    (r'^TextBlock_?\d*$', 'StatusText', lambda w, p, bp: 'Status' in p),

    # Images
    (r'^Image_?\d*$', 'IconImage', lambda w, p, bp: 'Icon' in p),
    (r'^Image_?\d*$', 'BackgroundImage', lambda w, p, bp: 'Background' in p or 'Bg' in p),
]

def get_suggested_name(widget_name, widget_type, parent_name, bp_name, widget_info):
    """Get suggested rename based on rules."""
    for pattern, new_name, condition in RENAME_RULES:
        if re.match(pattern, widget_name):
            if condition(widget_info, parent_name, bp_name):
                return new_name
    return None

def apply_renames_to_blueprint(wbp_path, dry_run=True):
    """Analyze and optionally apply renames to a blueprint."""
    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        return 0

    bp_name = wbp_path.split('/')[-1]

    # Check filters
    if ONLY_BLUEPRINTS and bp_name not in ONLY_BLUEPRINTS:
        return 0
    if bp_name in SKIP_BLUEPRINTS:
        return 0

    layouts = unreal.MOWidgetEditorUtils.get_all_widget_layout_info(wbp)

    # Build widget info
    widgets = {}
    used_names = set()

    for layout in layouts:
        name = str(layout.name)
        used_names.add(name)
        widgets[name] = {
            'name': name,
            'type': str(layout.widget_type),
            'parent': str(layout.parent_name),
            'depth': layout.depth
        }

    # Find renames to apply
    renames = []

    for name, widget in widgets.items():
        if not re.match(GENERIC_PATTERN, name):
            continue

        suggested = get_suggested_name(
            name,
            widget['type'],
            widget['parent'],
            bp_name,
            widget
        )

        if suggested and suggested != name:
            # Make unique if needed
            final_name = suggested
            counter = 1
            while final_name in used_names:
                final_name = f"{suggested}_{counter}"
                counter += 1

            renames.append((name, final_name))
            used_names.add(final_name)

    if not renames:
        return 0

    log(f"\n  {bp_name}: {len(renames)} renames")

    # Apply or preview
    applied = 0
    for old_name, new_name in renames:
        if dry_run:
            log(f"    [DRY RUN] {old_name} -> {new_name}")
        else:
            success = unreal.MOWidgetEditorUtils.rename_widget(wbp, old_name, new_name)
            if success:
                log(f"    [APPLIED] {old_name} -> {new_name}")
                applied += 1
            else:
                log(f"    [FAILED] {old_name} -> {new_name}")

    # Save if changes were made
    if not dry_run and applied > 0:
        unreal.EditorAssetLibrary.save_asset(wbp_path)
        log(f"    Saved {bp_name}")

    return len(renames) if dry_run else applied

def run():
    log("=" * 80)
    log("APPLY WIDGET RENAMES")
    log("=" * 80)

    if DRY_RUN:
        log("\n*** DRY RUN MODE - No changes will be applied ***")
        log("*** Set DRY_RUN = False in script to apply changes ***")

    if not hasattr(unreal, 'MOWidgetEditorUtils'):
        log("\nERROR: MOWidgetEditorUtils not found!")
        return

    wbp_paths = get_all_widget_blueprints()
    log(f"\nProcessing {len(wbp_paths)} Widget Blueprints...")

    total_renames = 0

    for path in wbp_paths:
        count = apply_renames_to_blueprint(path, dry_run=DRY_RUN)
        total_renames += count

    log(f"\n" + "=" * 80)
    log("SUMMARY")
    log("=" * 80)

    if DRY_RUN:
        log(f"\n  Renames suggested: {total_renames}")
        log("\n  To apply these renames:")
        log("  1. Open apply_widget_renames.py")
        log("  2. Set DRY_RUN = False")
        log("  3. Run this script again")
    else:
        log(f"\n  Renames applied: {total_renames}")
        log("\n  Next steps:")
        log("  1. Open modified blueprints in UE")
        log("  2. Compile each blueprint")
        log("  3. Update any C++ BindWidget references")

print("[RENAME] Starting...")
try:
    run()
    write_output()
except Exception as e:
    print(f"[RENAME ERROR] {e}")
    _output_lines.append(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    try:
        write_output()
    except:
        pass
print("[RENAME] Finished.")
