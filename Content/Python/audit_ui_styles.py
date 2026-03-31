# Audit UI Styles - Extract and analyze styling patterns
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/audit_ui_styles.py"
#
# This script extracts colors, fonts, and sizing from all widgets to identify:
# - Common color palettes
# - Font size patterns
# - Candidates for style consolidation
#
# REQUIRES: MOWidgetEditorUtils C++ utility
# Output: D:/UEProjects/MO57/Content/Python/audit_output/style_audit.txt

print("=== UI STYLE AUDIT SCRIPT LOADING ===")
import unreal
from collections import defaultdict
import os

OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "style_audit.txt")
os.makedirs(OUTPUT_DIR, exist_ok=True)

_output_lines = []

def log(msg):
    print(f"[STYLE] {msg}")
    _output_lines.append(msg)

def write_output():
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_output_lines))
    print(f"[STYLE] Output written to: {OUTPUT_FILE}")

def get_all_widget_blueprints():
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    filter = unreal.ARFilter(
        class_names=["WidgetBlueprint"],
        package_paths=["/MOFramework"],
        recursive_paths=True
    )
    assets = asset_registry.get_assets(filter)
    return [str(asset.package_name) for asset in assets]

def color_to_hex(color):
    """Convert LinearColor to hex string."""
    r = int(min(255, max(0, color.r * 255)))
    g = int(min(255, max(0, color.g * 255)))
    b = int(min(255, max(0, color.b * 255)))
    a = int(min(255, max(0, color.a * 255)))
    if a == 255:
        return f"#{r:02X}{g:02X}{b:02X}"
    else:
        return f"#{r:02X}{g:02X}{b:02X}{a:02X}"

def color_name_hint(color):
    """Get a human-readable hint for common colors."""
    r, g, b, a = color.r, color.g, color.b, color.a

    if a < 0.1:
        return "Transparent"
    if r > 0.9 and g > 0.9 and b > 0.9:
        return "White"
    if r < 0.1 and g < 0.1 and b < 0.1:
        return "Black"
    if r > 0.8 and g < 0.3 and b < 0.3:
        return "Red"
    if r < 0.3 and g > 0.8 and b < 0.3:
        return "Green"
    if r < 0.3 and g < 0.3 and b > 0.8:
        return "Blue"
    if r > 0.8 and g > 0.8 and b < 0.3:
        return "Yellow"
    if r > 0.5 and g > 0.5 and b > 0.5:
        return "Gray"

    return ""

def run():
    log("=" * 80)
    log("UI Style Audit Report")
    log("=" * 80)

    if not hasattr(unreal, 'MOWidgetEditorUtils'):
        log("\nERROR: MOWidgetEditorUtils not found!")
        log("Please compile the MOFramework module first.")
        return

    wbp_paths = get_all_widget_blueprints()
    log(f"\nExtracting styles from {len(wbp_paths)} Widget Blueprints...")

    # Collect all styles
    all_text_styles = []
    all_border_styles = []
    all_image_styles = []

    for path in wbp_paths:
        wbp = unreal.EditorAssetLibrary.load_asset(path)
        if not wbp:
            continue

        bp_name = path.split('/')[-1]
        styles = unreal.MOWidgetEditorUtils.extract_widget_styles(wbp)

        for style in styles:
            widget_name = str(style.widget_name)
            widget_type = str(style.widget_type)

            if style.has_text_style:
                all_text_styles.append({
                    'blueprint': bp_name,
                    'widget': widget_name,
                    'font_family': str(style.font_family),
                    'font_size': style.font_size,
                    'color': style.text_color,
                    'color_hex': color_to_hex(style.text_color)
                })

            if style.has_border_style:
                all_border_styles.append({
                    'blueprint': bp_name,
                    'widget': widget_name,
                    'background_color': style.background_color,
                    'background_hex': color_to_hex(style.background_color)
                })

            if style.has_image_style:
                all_image_styles.append({
                    'blueprint': bp_name,
                    'widget': widget_name,
                    'tint': style.image_tint,
                    'tint_hex': color_to_hex(style.image_tint)
                })

    # Analyze text styles
    log(f"\n" + "=" * 80)
    log("TEXT STYLES")
    log("=" * 80)
    log(f"Found {len(all_text_styles)} text widgets with styles")

    # Group by font size
    by_font_size = defaultdict(list)
    for s in all_text_styles:
        size_key = int(s['font_size'])
        by_font_size[size_key].append(s)

    log("\n  FONT SIZES:")
    for size, widgets in sorted(by_font_size.items(), key=lambda x: -len(x[1])):
        log(f"\n    {size}pt ({len(widgets)} widgets)")
        for w in widgets[:5]:
            log(f"      - {w['blueprint']}:{w['widget']}")
        if len(widgets) > 5:
            log(f"      ... and {len(widgets) - 5} more")

    # Group by text color
    by_text_color = defaultdict(list)
    for s in all_text_styles:
        by_text_color[s['color_hex']].append(s)

    log("\n  TEXT COLORS:")
    for color_hex, widgets in sorted(by_text_color.items(), key=lambda x: -len(x[1])):
        if len(widgets) >= 1:
            hint = color_name_hint(widgets[0]['color'])
            hint_str = f" ({hint})" if hint else ""
            log(f"\n    {color_hex}{hint_str} ({len(widgets)} widgets)")
            for w in widgets[:3]:
                log(f"      - {w['blueprint']}:{w['widget']}")
            if len(widgets) > 3:
                log(f"      ... and {len(widgets) - 3} more")

    # Analyze border/background styles
    log(f"\n" + "=" * 80)
    log("BORDER/BACKGROUND STYLES")
    log("=" * 80)
    log(f"Found {len(all_border_styles)} border widgets with styles")

    by_bg_color = defaultdict(list)
    for s in all_border_styles:
        by_bg_color[s['background_hex']].append(s)

    log("\n  BACKGROUND COLORS:")
    for color_hex, widgets in sorted(by_bg_color.items(), key=lambda x: -len(x[1])):
        if len(widgets) >= 1:
            hint = color_name_hint(widgets[0]['background_color'])
            hint_str = f" ({hint})" if hint else ""
            log(f"\n    {color_hex}{hint_str} ({len(widgets)} widgets)")
            for w in widgets[:3]:
                log(f"      - {w['blueprint']}:{w['widget']}")
            if len(widgets) > 3:
                log(f"      ... and {len(widgets) - 3} more")

    # Analyze image tints
    log(f"\n" + "=" * 80)
    log("IMAGE TINTS")
    log("=" * 80)
    log(f"Found {len(all_image_styles)} image widgets with tints")

    by_tint = defaultdict(list)
    for s in all_image_styles:
        by_tint[s['tint_hex']].append(s)

    for color_hex, widgets in sorted(by_tint.items(), key=lambda x: -len(x[1])):
        if len(widgets) >= 1:
            hint = color_name_hint(widgets[0]['tint'])
            hint_str = f" ({hint})" if hint else ""
            log(f"\n    {color_hex}{hint_str} ({len(widgets)} widgets)")
            for w in widgets:
                log(f"      - {w['blueprint']}:{w['widget']}")

    # Recommendations
    log(f"\n" + "=" * 80)
    log("STYLE RECOMMENDATIONS")
    log("=" * 80)

    log("\n  1. CREATE STYLE CONSTANTS:")
    log("     Define these as named colors/sizes in a central location:")

    # Find most common colors
    common_text_colors = sorted(by_text_color.items(), key=lambda x: -len(x[1]))[:5]
    log("\n     Text Colors:")
    for hex_color, widgets in common_text_colors:
        if widgets:
            hint = color_name_hint(widgets[0]['color'])
            log(f"       {hex_color} -> MO_TextColor_{hint or 'Primary'} ({len(widgets)} uses)")

    common_bg_colors = sorted(by_bg_color.items(), key=lambda x: -len(x[1]))[:5]
    log("\n     Background Colors:")
    for hex_color, widgets in common_bg_colors:
        if widgets:
            hint = color_name_hint(widgets[0]['background_color'])
            log(f"       {hex_color} -> MO_BgColor_{hint or 'Default'} ({len(widgets)} uses)")

    common_sizes = sorted(by_font_size.items(), key=lambda x: -len(x[1]))[:5]
    log("\n     Font Sizes:")
    for size, widgets in common_sizes:
        purpose = "Title" if size >= 24 else "Header" if size >= 18 else "Body" if size >= 12 else "Small"
        log(f"       {size}pt -> MO_FontSize_{purpose} ({len(widgets)} uses)")

    log("\n  2. CONSOLIDATE INTO SLATE STYLES:")
    log("     Create UMOWidgetStyles class with named presets")
    log("     Apply via SetStyle() in C++ base classes")

    log(f"\n" + "=" * 80)
    log("STYLE AUDIT COMPLETE")
    log("=" * 80)

print("[STYLE] Starting...")
try:
    run()
    write_output()
except Exception as e:
    print(f"[STYLE ERROR] {e}")
    _output_lines.append(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    try:
        write_output()
    except:
        pass
print("[STYLE] Finished.")
