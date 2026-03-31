# Audit UI Widgets - Find common elements across all Widget Blueprints
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/audit_ui_widgets.py"
#
# This script analyzes all Widget Blueprints to identify:
# - Common widget names used across multiple blueprints
# - Common widget types and their frequency
# - Potential candidates for shared base classes
# - Widgets that should be marked as variables
#
# Output: D:/UEProjects/MO57/Content/Python/audit_output/widget_audit.txt

print("=== UI AUDIT SCRIPT LOADING ===")
import unreal
from collections import defaultdict
import os

# Output file path
OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "widget_audit.txt")

# Ensure output directory exists
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Global output buffer
_output_lines = []

def log(msg):
    """Log to both console and output buffer."""
    print(f"[AUDIT] {msg}")
    _output_lines.append(msg)

def write_output():
    """Write all output to file."""
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_output_lines))
    print(f"[AUDIT] Output written to: {OUTPUT_FILE}")

def get_all_widget_blueprints():
    """Find all Widget Blueprint assets in the project."""
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()

    # Search in MOFramework plugin only (skip broken legacy /Game assets)
    wbp_assets = []

    # Get all assets of type WidgetBlueprint
    filter = unreal.ARFilter(
        class_names=["WidgetBlueprint"],
        package_paths=["/MOFramework"],
        recursive_paths=True
    )

    assets = asset_registry.get_assets(filter)

    for asset in assets:
        # Convert Name to string
        wbp_assets.append(str(asset.package_name))

    return wbp_assets

def get_widgets_in_blueprint(wbp_path):
    """Get all widgets in a Widget Blueprint using the WidgetTree."""
    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp or not isinstance(wbp, unreal.WidgetBlueprint):
        return None

    widgets = []

    # Try to get all widgets from the WidgetTree
    # We'll use a list of common widget names and types to probe
    common_names = [
        # Text elements
        "TitleText", "MessageText", "DescriptionText", "LabelText",
        "ActionNameText", "TimeRemainingText", "InstructionText",
        "NameText", "ValueText", "HeaderText", "SubtitleText",
        "ItemNameText", "CountText", "StatusText", "InfoText",

        # Buttons
        "ConfirmButton", "CancelButton", "CloseButton", "BackButton",
        "ActionButton", "StartButton", "StopButton", "AcceptButton",
        "EntryButton", "SelectButton", "CraftButton", "BuildButton",
        "DropButton", "UseButton", "EquipButton", "InspectButton",

        # Progress/Bars
        "ProgressBar", "HealthBar", "StaminaBar", "HungerBar",

        # Containers
        "ContentBox", "ContentPanel", "ContentScrollBox", "ItemsBox",
        "ButtonBox", "HeaderBox", "FooterBox", "MainPanel",
        "CanvasPanel", "RootCanvas", "Overlay", "Container",
        "VerticalBox", "HorizontalBox", "ScrollBox", "GridPanel",

        # Images/Icons
        "IconImage", "BackgroundImage", "ItemIcon", "ThumbnailImage",
        "Portrait", "Avatar",

        # Borders/Backgrounds
        "BackgroundBorder", "ContentBorder", "SelectionBorder",
        "HighlightBorder", "Background",

        # Specific UI elements
        "Slot", "Entry", "ListItem", "Card",
    ]

    # Also try numbered variations
    for i in range(10):
        common_names.extend([f"Button{i}", f"Text{i}", f"Image{i}", f"Slot{i}"])

    for name in common_names:
        try:
            widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(name))
            if widget:
                widget_type = type(widget).__name__
                is_variable = False
                if hasattr(unreal, 'MOWidgetEditorUtils'):
                    is_variable = unreal.MOWidgetEditorUtils.get_widget_is_variable(widget)
                widgets.append({
                    'name': name,
                    'type': widget_type,
                    'is_variable': is_variable
                })
        except:
            pass

    return widgets

def analyze_widget_patterns(all_blueprints_data):
    """Analyze patterns across all blueprints."""

    # Track widget name frequency
    name_frequency = defaultdict(list)  # name -> list of blueprints

    # Track widget type frequency
    type_frequency = defaultdict(int)

    # Track name+type combinations
    name_type_combos = defaultdict(list)  # (name, type) -> list of blueprints

    # Track non-variable widgets
    non_variable_widgets = []

    for bp_path, widgets in all_blueprints_data.items():
        if not widgets:
            continue

        bp_name = bp_path.split('/')[-1]

        for widget in widgets:
            name = widget['name']
            wtype = widget['type']
            is_var = widget['is_variable']

            name_frequency[name].append(bp_name)
            type_frequency[wtype] += 1
            name_type_combos[(name, wtype)].append(bp_name)

            if not is_var:
                non_variable_widgets.append((bp_name, name, wtype))

    return {
        'name_frequency': dict(name_frequency),
        'type_frequency': dict(type_frequency),
        'name_type_combos': dict(name_type_combos),
        'non_variable_widgets': non_variable_widgets
    }

def generate_report(all_blueprints_data, analysis):
    """Generate a human-readable report."""

    log("")
    log("=" * 70)
    log("UI WIDGET AUDIT REPORT")
    log("=" * 70)

    # Summary
    total_blueprints = len(all_blueprints_data)
    total_widgets = sum(len(w) for w in all_blueprints_data.values() if w)

    log(f"\nSUMMARY")
    log(f"  Total Widget Blueprints scanned: {total_blueprints}")
    log(f"  Total widgets found: {total_widgets}")

    # Common widget names (appear in 2+ blueprints)
    log(f"\n" + "=" * 70)
    log("COMMON WIDGET NAMES (appear in 2+ blueprints)")
    log("=" * 70)
    log("These are candidates for base class BindWidget properties")
    log("")

    common_names = {k: v for k, v in analysis['name_frequency'].items() if len(v) >= 2}
    sorted_common = sorted(common_names.items(), key=lambda x: -len(x[1]))

    for name, blueprints in sorted_common:
        # Get the type from name_type_combos
        types_used = set()
        for (n, t), bps in analysis['name_type_combos'].items():
            if n == name:
                types_used.add(t)

        type_str = ", ".join(types_used)
        log(f"  {name} ({type_str})")
        log(f"    Used in {len(blueprints)} blueprints: {', '.join(blueprints)}")

    # Widget type frequency
    log(f"\n" + "=" * 70)
    log("WIDGET TYPE FREQUENCY")
    log("=" * 70)

    sorted_types = sorted(analysis['type_frequency'].items(), key=lambda x: -x[1])
    for wtype, count in sorted_types:
        log(f"  {wtype}: {count}")

    # Widgets not marked as variables
    log(f"\n" + "=" * 70)
    log("WIDGETS NOT MARKED AS VARIABLES")
    log("=" * 70)
    log("These may need to be marked as variables for BindWidget")
    log("")

    if analysis['non_variable_widgets']:
        for bp_name, widget_name, widget_type in analysis['non_variable_widgets']:
            log(f"  {bp_name}: {widget_name} ({widget_type})")
    else:
        log("  All found widgets are marked as variables!")

    # Per-blueprint breakdown
    log(f"\n" + "=" * 70)
    log("PER-BLUEPRINT BREAKDOWN")
    log("=" * 70)

    for bp_path, widgets in sorted(all_blueprints_data.items()):
        bp_name = bp_path.split('/')[-1]
        if widgets:
            log(f"\n  {bp_name} ({len(widgets)} widgets)")
            for w in sorted(widgets, key=lambda x: x['name']):
                var_marker = "[VAR]" if w['is_variable'] else "[---]"
                log(f"    {var_marker} {w['name']} ({w['type']})")
        else:
            log(f"\n  {bp_name} (no widgets found or failed to load)")

    # Recommendations
    log(f"\n" + "=" * 70)
    log("RECOMMENDATIONS FOR COMMONUI MIGRATION")
    log("=" * 70)

    log("\n1. BASE CLASSES TO CREATE:")

    # Group common names by apparent purpose
    progress_widgets = [n for n in common_names if 'Progress' in n or 'Time' in n or 'Action' in n]
    button_widgets = [n for n in common_names if 'Button' in n]
    text_widgets = [n for n in common_names if 'Text' in n and n not in progress_widgets]
    container_widgets = [n for n in common_names if 'Box' in n or 'Panel' in n or 'Scroll' in n]

    if progress_widgets:
        log(f"\n   Progress Widget Base (already have UMOProgressWidgetBase)")
        log(f"     Common bindings: {', '.join(progress_widgets)}")

    if button_widgets:
        log(f"\n   Consider: Button Panel Base")
        log(f"     Common bindings: {', '.join(button_widgets)}")

    if text_widgets:
        log(f"\n   Consider: Info Panel Base")
        log(f"     Common bindings: {', '.join(text_widgets)}")

    if container_widgets:
        log(f"\n   Consider: Scrollable List Base (already have UMOScrollListBase)")
        log(f"     Common bindings: {', '.join(container_widgets)}")

    log("\n2. STANDARDIZE NAMING:")
    log("   Ensure all blueprints use consistent widget names for BindWidget")

    log("\n3. MARK AS VARIABLES:")
    log("   Run fix_widget_variables_final.py to mark all widgets as variables")

def run():
    log("=" * 70)
    log("UI Widget Audit - Scanning all Widget Blueprints")
    log("=" * 70)

    # Find all widget blueprints
    log("\nFinding Widget Blueprints...")
    wbp_paths = get_all_widget_blueprints()
    log(f"Found {len(wbp_paths)} Widget Blueprints")

    # Analyze each blueprint
    log("\nAnalyzing widgets in each blueprint...")
    all_data = {}

    for path in wbp_paths:
        log(f"  Scanning: {path}")
        widgets = get_widgets_in_blueprint(path)
        all_data[path] = widgets

    # Analyze patterns
    log("\nAnalyzing patterns...")
    analysis = analyze_widget_patterns(all_data)

    # Generate report
    generate_report(all_data, analysis)

    log("\n" + "=" * 70)
    log("AUDIT COMPLETE")
    log("=" * 70)

print("[AUDIT] Starting...")
try:
    run()
    write_output()
except Exception as e:
    print(f"[AUDIT ERROR] {e}")
    _output_lines.append(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    try:
        write_output()
    except:
        pass
print("[AUDIT] Finished.")
