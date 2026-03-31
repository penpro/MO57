# Audit UI Layouts - Find common layout patterns across Widget Blueprints
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/audit_ui_layouts.py"
#
# This script analyzes Widget Blueprint layouts to identify:
# - Common dimensions and sizing patterns
# - Anchor/alignment patterns
# - Slot configurations
# - Candidates for unified base layouts
#
# REQUIRES: MOWidgetEditorUtils C++ utility (compile MOFramework first)
# Output: D:/UEProjects/MO57/Content/Python/audit_output/layout_audit.txt

print("=== UI LAYOUT AUDIT SCRIPT LOADING ===")
import unreal
from collections import defaultdict
import os

# Output file path
OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "layout_audit.txt")

# Ensure output directory exists
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Global output buffer
_output_lines = []

def log(msg):
    """Log to both console and output buffer."""
    print(f"[LAYOUT] {msg}")
    _output_lines.append(msg)

def write_output():
    """Write all output to file."""
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_output_lines))
    print(f"[LAYOUT] Output written to: {OUTPUT_FILE}")

def get_all_widget_blueprints():
    """Find all Widget Blueprint assets in /MOFramework."""
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()

    filter = unreal.ARFilter(
        class_names=["WidgetBlueprint"],
        package_paths=["/MOFramework"],
        recursive_paths=True
    )

    assets = asset_registry.get_assets(filter)
    return [str(asset.package_name) for asset in assets]

def get_anchor_preset_name(min_x, min_y, max_x, max_y):
    """Convert anchor values to human-readable preset name."""
    # Common presets (rounded to 1 decimal)
    presets = {
        (0.0, 0.0, 0.0, 0.0): "TopLeft",
        (0.5, 0.0, 0.5, 0.0): "TopCenter",
        (1.0, 0.0, 1.0, 0.0): "TopRight",
        (0.0, 0.5, 0.0, 0.5): "CenterLeft",
        (0.5, 0.5, 0.5, 0.5): "Center",
        (1.0, 0.5, 1.0, 0.5): "CenterRight",
        (0.0, 1.0, 0.0, 1.0): "BottomLeft",
        (0.5, 1.0, 0.5, 1.0): "BottomCenter",
        (1.0, 1.0, 1.0, 1.0): "BottomRight",
        (0.0, 0.0, 1.0, 0.0): "TopStretch",
        (0.0, 1.0, 1.0, 1.0): "BottomStretch",
        (0.0, 0.0, 0.0, 1.0): "LeftStretch",
        (1.0, 0.0, 1.0, 1.0): "RightStretch",
        (0.0, 0.0, 1.0, 1.0): "FullScreen",
    }

    key = (round(min_x, 1), round(min_y, 1), round(max_x, 1), round(max_y, 1))
    return presets.get(key, f"Custom({min_x:.2f},{min_y:.2f},{max_x:.2f},{max_y:.2f})")

def analyze_blueprint_layout(wbp_path):
    """Analyze a single widget blueprint's layout using C++ utility."""
    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp or not isinstance(wbp, unreal.WidgetBlueprint):
        return None

    # Use C++ utility to get layout info
    root_name = unreal.MOWidgetEditorUtils.get_root_widget_name(wbp)
    root_type = unreal.MOWidgetEditorUtils.get_root_widget_type(wbp)
    layout_info_array = unreal.MOWidgetEditorUtils.get_all_widget_layout_info(wbp)

    # Convert to Python-friendly format
    widgets = []
    for info in layout_info_array:
        # Compute anchor preset
        anchor_preset = get_anchor_preset_name(
            info.anchor_min_x, info.anchor_min_y,
            info.anchor_max_x, info.anchor_max_y
        )

        # Compute size if anchors are point anchors (min == max)
        size_x = None
        size_y = None
        if abs(info.anchor_min_x - info.anchor_max_x) < 0.01:
            size_x = info.offset_right  # For point anchors, right is width
        if abs(info.anchor_min_y - info.anchor_max_y) < 0.01:
            size_y = info.offset_bottom  # For point anchors, bottom is height

        widgets.append({
            'name': str(info.name),
            'type': str(info.widget_type),
            'slot_type': str(info.slot_type),
            'parent': str(info.parent_name),
            'depth': info.depth,
            'is_variable': info.is_variable,
            'anchor_preset': anchor_preset,
            'anchor_min': (info.anchor_min_x, info.anchor_min_y),
            'anchor_max': (info.anchor_max_x, info.anchor_max_y),
            'offset': (info.offset_left, info.offset_top, info.offset_right, info.offset_bottom),
            'alignment': (info.alignment_x, info.alignment_y),
            'z_order': info.z_order,
            'h_align': str(info.horizontal_alignment) if info.horizontal_alignment else None,
            'v_align': str(info.vertical_alignment) if info.vertical_alignment else None,
            'padding': (info.padding_left, info.padding_top, info.padding_right, info.padding_bottom),
            'size_x': size_x,
            'size_y': size_y,
            'width_override': info.width_override if info.has_width_override else None,
            'height_override': info.height_override if info.has_height_override else None,
        })

    return {
        'path': wbp_path,
        'name': wbp_path.split('/')[-1],
        'root_name': root_name,
        'root_type': root_type,
        'widgets': widgets,
        'widget_count': len(widgets)
    }

def categorize_blueprints(all_layouts):
    """Categorize blueprints by apparent purpose based on naming."""
    categories = {
        'menus': [],
        'panels': [],
        'entries': [],
        'progress': [],
        'context': [],
        'hud': [],
        'other': []
    }

    for layout in all_layouts:
        if not layout:
            continue
        name = layout['name'].lower()

        if 'menu' in name:
            categories['menus'].append(layout)
        elif 'panel' in name:
            categories['panels'].append(layout)
        elif 'entry' in name or 'slot' in name:
            categories['entries'].append(layout)
        elif 'progress' in name:
            categories['progress'].append(layout)
        elif 'context' in name:
            categories['context'].append(layout)
        elif 'hud' in name or 'status' in name or 'counter' in name or 'indicator' in name:
            categories['hud'].append(layout)
        else:
            categories['other'].append(layout)

    return categories

def find_dimension_patterns(layouts):
    """Find common dimension patterns across layouts."""
    size_patterns = defaultdict(list)

    for layout in layouts:
        if not layout:
            continue
        for widget in layout.get('widgets', []):
            size_x = widget.get('size_x') or widget.get('width_override')
            size_y = widget.get('size_y') or widget.get('height_override')
            if size_x and size_y and size_x > 0 and size_y > 0:
                # Round to nearest 10 for grouping
                size_key = (
                    round(size_x / 10) * 10,
                    round(size_y / 10) * 10
                )
                size_patterns[size_key].append({
                    'blueprint': layout['name'],
                    'widget': widget['name'],
                    'type': widget['type'],
                    'exact_size': (size_x, size_y)
                })

    return dict(size_patterns)

def find_anchor_patterns(layouts):
    """Find common anchor/positioning patterns."""
    anchor_patterns = defaultdict(list)

    for layout in layouts:
        if not layout:
            continue
        for widget in layout.get('widgets', []):
            preset = widget.get('anchor_preset')
            if preset:
                anchor_patterns[preset].append({
                    'blueprint': layout['name'],
                    'widget': widget['name'],
                    'type': widget['type']
                })

    return dict(anchor_patterns)

def find_structural_patterns(layouts):
    """Find common widget tree structures."""
    # Track parent->children patterns
    structure_patterns = defaultdict(list)

    for layout in layouts:
        if not layout:
            continue

        # Build parent->children map
        children_by_parent = defaultdict(list)
        for widget in layout.get('widgets', []):
            parent = widget.get('parent', '')
            children_by_parent[parent].append(widget)

        # Get root's direct children types
        root_children = children_by_parent.get('', [])
        if not root_children and layout.get('root_name'):
            root_children = children_by_parent.get(layout['root_name'], [])

        if root_children:
            child_types = tuple(sorted(c['type'] for c in root_children))
            structure_patterns[child_types].append(layout['name'])

    return dict(structure_patterns)

def generate_layout_report(all_layouts, categories, size_patterns, anchor_patterns):
    """Generate comprehensive layout report."""

    log("")
    log("=" * 80)
    log("UI LAYOUT AUDIT REPORT")
    log("=" * 80)

    # Summary
    total_bps = len([l for l in all_layouts if l])
    total_widgets = sum(l['widget_count'] for l in all_layouts if l)

    log(f"\nSUMMARY")
    log(f"  Widget Blueprints analyzed: {total_bps}")
    log(f"  Total widgets found: {total_widgets}")

    # Category breakdown
    log(f"\n" + "=" * 80)
    log("BLUEPRINT CATEGORIES")
    log("=" * 80)

    for cat_name, cat_layouts in categories.items():
        if cat_layouts:
            log(f"\n  {cat_name.upper()} ({len(cat_layouts)} blueprints)")
            for layout in cat_layouts:
                root_type = layout.get('root_type', 'Unknown')
                log(f"    - {layout['name']} (root: {root_type}, {layout['widget_count']} widgets)")

    # Detailed layout for menus
    log(f"\n" + "=" * 80)
    log("DETAILED MENU LAYOUTS")
    log("=" * 80)
    log("Showing widget hierarchy with positioning info")

    for layout in categories.get('menus', []):
        log(f"\n  {layout['name']} (root: {layout.get('root_type', '?')})")
        log(f"  " + "-" * 60)
        for widget in layout.get('widgets', []):
            indent = "    " + ("  " * widget.get('depth', 0))

            # Build info string
            info_parts = []

            # Size info
            if widget.get('size_x') and widget.get('size_y'):
                info_parts.append(f"{widget['size_x']:.0f}x{widget['size_y']:.0f}")
            elif widget.get('width_override') or widget.get('height_override'):
                w = widget.get('width_override') or '?'
                h = widget.get('height_override') or '?'
                info_parts.append(f"SizeBox:{w}x{h}")

            # Anchor info
            if widget.get('anchor_preset') and widget['anchor_preset'] != 'Custom(0.00,0.00,0.00,0.00)':
                info_parts.append(f"@{widget['anchor_preset']}")

            # Alignment info for box slots
            if widget.get('h_align') or widget.get('v_align'):
                align = f"{widget.get('h_align', '?')}/{widget.get('v_align', '?')}"
                info_parts.append(f"align:{align}")

            # Variable marker
            var_marker = "[VAR]" if widget.get('is_variable') else "[---]"

            info_str = f" ({', '.join(info_parts)})" if info_parts else ""
            log(f"{indent}{var_marker} {widget['name']} ({widget['type']}){info_str}")

    # Detailed layout for panels
    log(f"\n" + "=" * 80)
    log("DETAILED PANEL LAYOUTS")
    log("=" * 80)

    for layout in categories.get('panels', []):
        log(f"\n  {layout['name']} (root: {layout.get('root_type', '?')})")
        log(f"  " + "-" * 60)
        for widget in layout.get('widgets', []):
            indent = "    " + ("  " * widget.get('depth', 0))
            info_parts = []
            if widget.get('size_x') and widget.get('size_y'):
                info_parts.append(f"{widget['size_x']:.0f}x{widget['size_y']:.0f}")
            if widget.get('anchor_preset') and 'Custom' not in widget['anchor_preset']:
                info_parts.append(f"@{widget['anchor_preset']}")
            info_str = f" ({', '.join(info_parts)})" if info_parts else ""
            var_marker = "[VAR]" if widget.get('is_variable') else "[---]"
            log(f"{indent}{var_marker} {widget['name']} ({widget['type']}){info_str}")

    # Detailed layout for entries
    log(f"\n" + "=" * 80)
    log("DETAILED ENTRY LAYOUTS")
    log("=" * 80)

    for layout in categories.get('entries', []):
        log(f"\n  {layout['name']} (root: {layout.get('root_type', '?')})")
        log(f"  " + "-" * 60)
        for widget in layout.get('widgets', []):
            indent = "    " + ("  " * widget.get('depth', 0))
            info_parts = []
            if widget.get('size_x') and widget.get('size_y'):
                info_parts.append(f"{widget['size_x']:.0f}x{widget['size_y']:.0f}")
            if widget.get('anchor_preset') and 'Custom' not in widget['anchor_preset']:
                info_parts.append(f"@{widget['anchor_preset']}")
            info_str = f" ({', '.join(info_parts)})" if info_parts else ""
            var_marker = "[VAR]" if widget.get('is_variable') else "[---]"
            log(f"{indent}{var_marker} {widget['name']} ({widget['type']}){info_str}")

    # Common dimension patterns
    log(f"\n" + "=" * 80)
    log("COMMON DIMENSION PATTERNS")
    log("=" * 80)
    log("Widgets with similar sizes (grouped by ~10px)")

    sorted_sizes = sorted(size_patterns.items(), key=lambda x: -len(x[1]))
    shown = 0
    for (w, h), widgets in sorted_sizes:
        if len(widgets) >= 2 and shown < 25:
            log(f"\n  ~{w}x{h} ({len(widgets)} widgets)")
            for w_info in widgets[:5]:
                log(f"    - {w_info['blueprint']}: {w_info['widget']} ({w_info['type']}) exact: {w_info['exact_size']}")
            if len(widgets) > 5:
                log(f"    ... and {len(widgets) - 5} more")
            shown += 1

    # Common anchor patterns
    log(f"\n" + "=" * 80)
    log("COMMON ANCHOR PATTERNS")
    log("=" * 80)

    sorted_anchors = sorted(anchor_patterns.items(), key=lambda x: -len(x[1]))
    for anchor, widgets in sorted_anchors:
        if len(widgets) >= 3 and 'Custom' not in anchor:
            log(f"\n  {anchor} ({len(widgets)} widgets)")
            by_type = defaultdict(list)
            for w in widgets:
                by_type[w['type']].append(w)
            for wtype, type_widgets in sorted(by_type.items(), key=lambda x: -len(x[1]))[:5]:
                log(f"    {wtype}: {len(type_widgets)}")
                for tw in type_widgets[:3]:
                    log(f"      - {tw['blueprint']}: {tw['widget']}")

    # Structural patterns
    log(f"\n" + "=" * 80)
    log("STRUCTURAL PATTERNS (Root Children)")
    log("=" * 80)

    structure_patterns = find_structural_patterns(all_layouts)
    sorted_structures = sorted(structure_patterns.items(), key=lambda x: -len(x[1]))
    for child_types, blueprints in sorted_structures:
        if len(blueprints) >= 2:
            log(f"\n  Root children: {', '.join(child_types[:5])}{'...' if len(child_types) > 5 else ''}")
            log(f"    Used by: {', '.join(blueprints[:10])}{'...' if len(blueprints) > 10 else ''}")

    # Recommendations
    log(f"\n" + "=" * 80)
    log("CONSOLIDATION RECOMMENDATIONS")
    log("=" * 80)

    log("\n1. MENU BASE CLASSES:")

    menu_roots = defaultdict(list)
    for layout in categories.get('menus', []):
        if layout.get('root_type'):
            menu_roots[layout['root_type']].append(layout['name'])

    for root_type, menus in menu_roots.items():
        log(f"\n   {root_type} root ({len(menus)} menus):")
        for m in menus:
            log(f"     - {m}")

    log("\n2. PANEL BASE CLASSES:")

    panel_roots = defaultdict(list)
    for layout in categories.get('panels', []):
        if layout.get('root_type'):
            panel_roots[layout['root_type']].append(layout['name'])

    for root_type, panels in panel_roots.items():
        log(f"\n   {root_type} root ({len(panels)} panels):")
        for p in panels:
            log(f"     - {p}")

    log("\n3. ENTRY BASE CLASSES:")

    entry_roots = defaultdict(list)
    for layout in categories.get('entries', []):
        if layout.get('root_type'):
            entry_roots[layout['root_type']].append(layout['name'])

    for root_type, entries in entry_roots.items():
        log(f"\n   {root_type} root ({len(entries)} entries):")
        for e in entries:
            log(f"     - {e}")

    log("\n4. SUGGESTED UNIFIED LAYOUTS:")
    log("   Create these base Widget Blueprints with common structure:")
    log("")
    log("   WBP_MenuBase - Full screen menu template:")
    log("     - CanvasPanel or Overlay root")
    log("     - CloseButton at top-right (TopRight anchor)")
    log("     - TitleText at top-center")
    log("     - ContentArea (scrollable center, FullScreen stretch)")
    log("     - Candidates: Crafting, Building, Skills, Journal, Possession menus")
    log("")
    log("   WBP_TwoColumnMenu - List + Detail layout:")
    log("     - HorizontalBox with two children")
    log("     - Left: ScrollBox for list (40% width)")
    log("     - Right: Detail panel (60% width)")
    log("     - Candidates: CraftingMenu, BuildingMenu")
    log("")
    log("   WBP_EntryBase - Standard list entry:")
    log("     - SizeBox with fixed height (~40-60px)")
    log("     - Border for background/selection")
    log("     - Button for click handling")
    log("     - Candidates: RecipeEntry, QueueEntry, SkillEntry")
    log("")
    log("   WBP_QueueEntryBase - Progress-tracking entry:")
    log("     - Inherits EntryBase structure")
    log("     - ProgressBar")
    log("     - TimeRemainingText")
    log("     - CancelButton")
    log("     - Candidates: CraftQueue, BuildQueue, JobQueue entries")

def run():
    log("=" * 80)
    log("UI Layout Audit - Analyzing Widget Blueprint Structures")
    log("=" * 80)

    # Check for C++ utility
    if not hasattr(unreal, 'MOWidgetEditorUtils'):
        log("")
        log("ERROR: MOWidgetEditorUtils not found!")
        log("Please compile the MOFramework module first.")
        log("")
        log("Steps:")
        log("  1. Close Unreal Editor")
        log("  2. Build the project (MO57Editor target)")
        log("  3. Reopen Unreal Editor")
        log("  4. Run this script again")
        return

    log("MOWidgetEditorUtils found - proceeding with analysis...")

    # Find all widget blueprints
    log("\nFinding Widget Blueprints...")
    wbp_paths = get_all_widget_blueprints()
    log(f"Found {len(wbp_paths)} Widget Blueprints")

    # Analyze each blueprint
    log("\nAnalyzing layouts...")
    all_layouts = []

    for path in wbp_paths:
        log(f"  Analyzing: {path.split('/')[-1]}")
        layout = analyze_blueprint_layout(path)
        all_layouts.append(layout)

    # Categorize
    log("\nCategorizing blueprints...")
    categories = categorize_blueprints(all_layouts)

    # Find patterns
    log("\nFinding dimension patterns...")
    size_patterns = find_dimension_patterns(all_layouts)

    log("\nFinding anchor patterns...")
    anchor_patterns = find_anchor_patterns(all_layouts)

    # Generate report
    generate_layout_report(all_layouts, categories, size_patterns, anchor_patterns)

    log("\n" + "=" * 80)
    log("LAYOUT AUDIT COMPLETE")
    log("=" * 80)

print("[LAYOUT] Starting...")
try:
    run()
    write_output()
except Exception as e:
    print(f"[LAYOUT ERROR] {e}")
    _output_lines.append(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    try:
        write_output()
    except:
        pass
print("[LAYOUT] Finished.")
