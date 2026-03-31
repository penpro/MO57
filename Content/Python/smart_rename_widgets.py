# Smart Widget Renamer - Analyzes context to suggest better widget names
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/smart_rename_widgets.py"
#
# This script:
# 1. Finds widgets with generic names (Border_0, TextBlock_1, etc.)
# 2. Analyzes parent/sibling context to suggest meaningful names
# 3. Generates rename commands or applies them directly
#
# REQUIRES: MOWidgetEditorUtils C++ utility
# Output: D:/UEProjects/MO57/Content/Python/audit_output/rename_suggestions.txt

print("=== SMART WIDGET RENAMER LOADING ===")
import unreal
from collections import defaultdict
import os
import re

OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "rename_suggestions.txt")
os.makedirs(OUTPUT_DIR, exist_ok=True)

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

# Pattern for generic auto-generated names
GENERIC_PATTERN = re.compile(r'^(Border|CanvasPanel|SizeBox|TextBlock|Image|Button|HorizontalBox|VerticalBox|Overlay|ScrollBox|Spacer)_?\d*$')

# Context-based naming suggestions
CONTEXT_HINTS = {
    # Parent contains these words -> suggest these prefixes for children
    'Title': {'TextBlock': 'TitleText', 'Border': 'TitleBorder'},
    'Header': {'TextBlock': 'HeaderText', 'Border': 'HeaderBorder', 'HorizontalBox': 'HeaderBox'},
    'Footer': {'TextBlock': 'FooterText', 'Border': 'FooterBorder', 'HorizontalBox': 'FooterBox'},
    'Content': {'Border': 'ContentBorder', 'VerticalBox': 'ContentBox', 'ScrollBox': 'ContentScrollBox'},
    'Button': {'TextBlock': 'ButtonText', 'Image': 'ButtonIcon', 'Border': 'ButtonBorder'},
    'Entry': {'Border': 'EntryBorder', 'TextBlock': 'EntryText', 'Image': 'EntryIcon'},
    'Item': {'TextBlock': 'ItemText', 'Image': 'ItemIcon', 'Border': 'ItemBorder'},
    'Recipe': {'TextBlock': 'RecipeText', 'Image': 'RecipeIcon'},
    'Slot': {'Border': 'SlotBorder', 'Image': 'SlotIcon'},
    'Progress': {'TextBlock': 'ProgressText', 'Border': 'ProgressBorder'},
    'Status': {'TextBlock': 'StatusText', 'Border': 'StatusBorder'},
    'Info': {'TextBlock': 'InfoText', 'Border': 'InfoBorder', 'VerticalBox': 'InfoBox'},
    'Detail': {'TextBlock': 'DetailText', 'Border': 'DetailBorder', 'VerticalBox': 'DetailBox'},
    'List': {'ScrollBox': 'ListScrollBox', 'VerticalBox': 'ListBox'},
    'Queue': {'ScrollBox': 'QueueScrollBox', 'VerticalBox': 'QueueBox'},
    'Panel': {'Border': 'PanelBorder', 'CanvasPanel': 'PanelCanvas'},
    'Menu': {'Border': 'MenuBorder', 'VerticalBox': 'MenuBox'},
    'Background': {'Border': 'BackgroundBorder', 'Image': 'BackgroundImage'},
    'Icon': {'Image': 'IconImage', 'SizeBox': 'IconSizeBox'},
    'Name': {'TextBlock': 'NameText'},
    'Description': {'TextBlock': 'DescriptionText'},
    'Value': {'TextBlock': 'ValueText'},
    'Label': {'TextBlock': 'LabelText'},
    'Count': {'TextBlock': 'CountText'},
    'Time': {'TextBlock': 'TimeText'},
}

# Blueprint name patterns for context
BP_CONTEXT = {
    'Recipe': ['RecipeNameText', 'RecipeIcon', 'RecipeDescriptionText'],
    'Craft': ['CraftButton', 'CraftProgressBar'],
    'Build': ['BuildButton', 'BuildProgressBar'],
    'Inventory': ['ItemIcon', 'ItemNameText', 'ItemCountText'],
    'Queue': ['QueueEntryBorder', 'ProgressBar', 'CancelButton'],
    'Progress': ['ProgressBar', 'ActionNameText', 'TimeRemainingText'],
    'Confirmation': ['TitleText', 'MessageText', 'ConfirmButton', 'CancelButton'],
    'Menu': ['CloseButton', 'TitleText', 'ContentScrollBox'],
    'Panel': ['BackButton', 'TitleText', 'ContentBox'],
    'Entry': ['EntryButton', 'BackgroundBorder', 'SelectionBorder'],
    'Slot': ['SlotBorder', 'ItemIcon', 'CountText'],
    'Status': ['StatusText', 'ValueText', 'IconImage'],
}

def is_generic_name(name):
    """Check if widget has a generic auto-generated name."""
    return GENERIC_PATTERN.match(name) is not None

def get_widget_base_type(widget_type):
    """Extract base type from full class name."""
    # Remove common suffixes
    base = widget_type.replace('Common', '').replace('MO', '')
    return base

def suggest_name_from_context(widget_name, widget_type, parent_name, siblings, bp_name):
    """Suggest a better name based on context."""
    base_type = get_widget_base_type(widget_type)

    # Check parent name for context hints
    for hint_key, type_map in CONTEXT_HINTS.items():
        if hint_key.lower() in parent_name.lower():
            if base_type in type_map:
                return type_map[base_type]

    # Check blueprint name for context
    for bp_hint, common_names in BP_CONTEXT.items():
        if bp_hint.lower() in bp_name.lower():
            # Find a common name that matches our type
            for common_name in common_names:
                if base_type.lower() in common_name.lower():
                    return common_name

    # Check sibling names for patterns
    sibling_types = [s['type'] for s in siblings if s['name'] != widget_name]
    sibling_names = [s['name'] for s in siblings if s['name'] != widget_name and not is_generic_name(s['name'])]

    # If siblings have good names, try to follow the pattern
    for sib_name in sibling_names:
        # Extract suffix pattern (e.g., "Text" from "TitleText")
        for suffix in ['Text', 'Image', 'Icon', 'Border', 'Box', 'Button', 'Bar']:
            if sib_name.endswith(suffix) and base_type == suffix.replace('Bar', 'ProgressBar'):
                prefix = sib_name[:-len(suffix)]
                return f"{prefix}{base_type}"

    # Default: Use parent name as prefix
    if parent_name and not is_generic_name(parent_name):
        # Clean parent name
        clean_parent = parent_name.replace('_0', '').replace('_1', '')
        return f"{clean_parent}{base_type}"

    # Fallback: Use position-based naming
    return None

def analyze_blueprint(wbp_path):
    """Analyze a blueprint and generate rename suggestions."""
    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        return []

    bp_name = wbp_path.split('/')[-1]
    layouts = unreal.MOWidgetEditorUtils.get_all_widget_layout_info(wbp)

    # Build hierarchy
    widgets_by_name = {}
    children_by_parent = defaultdict(list)

    for layout in layouts:
        name = str(layout.name)
        parent = str(layout.parent_name)
        widget_type = str(layout.widget_type)

        widgets_by_name[name] = {
            'name': name,
            'type': widget_type,
            'parent': parent,
            'depth': layout.depth
        }
        children_by_parent[parent].append(widgets_by_name[name])

    # Find generic names and suggest replacements
    suggestions = []

    for name, widget in widgets_by_name.items():
        if is_generic_name(name):
            parent = widget['parent']
            siblings = children_by_parent[parent]

            suggested = suggest_name_from_context(
                name,
                widget['type'],
                parent,
                siblings,
                bp_name
            )

            if suggested and suggested != name:
                # Check if suggested name already exists
                if suggested not in widgets_by_name:
                    suggestions.append({
                        'blueprint': bp_name,
                        'old_name': name,
                        'new_name': suggested,
                        'type': widget['type'],
                        'parent': parent,
                        'reason': f"Parent: {parent}" if parent else "Root context"
                    })

    return suggestions

def run():
    log("=" * 80)
    log("SMART WIDGET RENAME SUGGESTIONS")
    log("=" * 80)

    if not hasattr(unreal, 'MOWidgetEditorUtils'):
        log("\nERROR: MOWidgetEditorUtils not found!")
        return

    wbp_paths = get_all_widget_blueprints()
    log(f"\nAnalyzing {len(wbp_paths)} Widget Blueprints...")

    all_suggestions = []

    for path in wbp_paths:
        suggestions = analyze_blueprint(path)
        all_suggestions.extend(suggestions)

    # Group by blueprint
    by_blueprint = defaultdict(list)
    for s in all_suggestions:
        by_blueprint[s['blueprint']].append(s)

    log(f"\n" + "=" * 80)
    log("RENAME SUGGESTIONS")
    log("=" * 80)
    log(f"\nFound {len(all_suggestions)} widgets that should be renamed")

    for bp_name, suggestions in sorted(by_blueprint.items()):
        log(f"\n  {bp_name} ({len(suggestions)} renames)")
        log("  " + "-" * 50)
        for s in suggestions:
            log(f"    {s['old_name']} -> {s['new_name']}")
            log(f"      Type: {s['type']}, {s['reason']}")

    # Generate Python commands to apply renames
    log(f"\n" + "=" * 80)
    log("PYTHON COMMANDS TO APPLY RENAMES")
    log("=" * 80)
    log("\n# Copy and paste these commands to apply renames:")
    log("# (Or run apply_widget_renames.py after reviewing)")
    log("")

    for bp_name, suggestions in sorted(by_blueprint.items()):
        bp_path = None
        for path in wbp_paths:
            if path.endswith(bp_name):
                bp_path = path
                break

        if bp_path and suggestions:
            log(f"\n# {bp_name}")
            log(f"wbp = unreal.EditorAssetLibrary.load_asset('{bp_path}')")
            for s in suggestions:
                log(f"unreal.MOWidgetEditorUtils.rename_widget(wbp, '{s['old_name']}', '{s['new_name']}')")
            log(f"unreal.EditorAssetLibrary.save_asset('{bp_path}')")

    # Summary stats
    log(f"\n" + "=" * 80)
    log("SUMMARY")
    log("=" * 80)
    log(f"\n  Total renames suggested: {len(all_suggestions)}")
    log(f"  Blueprints affected: {len(by_blueprint)}")

    # Count by type
    by_type = defaultdict(int)
    for s in all_suggestions:
        by_type[s['type']] += 1

    log("\n  By widget type:")
    for wtype, count in sorted(by_type.items(), key=lambda x: -x[1]):
        log(f"    {wtype}: {count}")

    log(f"\n" + "=" * 80)
    log("NEXT STEPS")
    log("=" * 80)
    log("\n  1. Review the suggestions above")
    log("  2. Run: py \"D:/UEProjects/MO57/Content/Python/apply_widget_renames.py\"")
    log("  3. Or manually copy/paste specific rename commands")
    log("  4. Compile blueprints to verify changes")

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
