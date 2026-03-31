# Validate UI Widgets - Find issues in Widget Blueprints
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/validate_ui_widgets.py"
#
# REQUIRES: MOWidgetEditorUtils C++ utility
# Output: D:/UEProjects/MO57/Content/Python/audit_output/validation_report.txt

print("=== UI VALIDATION SCRIPT LOADING ===")
import unreal
from collections import defaultdict
import os

OUTPUT_DIR = "D:/UEProjects/MO57/Content/Python/audit_output"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "validation_report.txt")
os.makedirs(OUTPUT_DIR, exist_ok=True)

_output_lines = []

def log(msg):
    print(f"[VALIDATE] {msg}")
    _output_lines.append(msg)

def write_output():
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_output_lines))
    print(f"[VALIDATE] Output written to: {OUTPUT_FILE}")

def get_all_widget_blueprints():
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    filter = unreal.ARFilter(
        class_names=["WidgetBlueprint"],
        package_paths=["/MOFramework"],
        recursive_paths=True
    )
    assets = asset_registry.get_assets(filter)
    return [str(asset.package_name) for asset in assets]

def run():
    log("=" * 80)
    log("UI Widget Validation Report")
    log("=" * 80)

    if not hasattr(unreal, 'MOWidgetEditorUtils'):
        log("\nERROR: MOWidgetEditorUtils not found!")
        log("Please compile the MOFramework module first.")
        return

    wbp_paths = get_all_widget_blueprints()
    log(f"\nValidating {len(wbp_paths)} Widget Blueprints...")

    # Aggregate issues by severity
    all_errors = []
    all_warnings = []
    all_info = []

    for path in wbp_paths:
        wbp = unreal.EditorAssetLibrary.load_asset(path)
        if not wbp:
            continue

        bp_name = path.split('/')[-1]
        issues = unreal.MOWidgetEditorUtils.validate_widget_blueprint(wbp)

        for issue in issues:
            severity = str(issue.severity)
            widget_name = str(issue.widget_name) if issue.widget_name else ""
            message = str(issue.message)
            suggestion = str(issue.suggestion) if issue.suggestion else ""

            entry = {
                'blueprint': bp_name,
                'widget': widget_name,
                'message': message,
                'suggestion': suggestion
            }

            if severity == "Error":
                all_errors.append(entry)
            elif severity == "Warning":
                all_warnings.append(entry)
            else:
                all_info.append(entry)

    # Report
    log(f"\n" + "=" * 80)
    log("SUMMARY")
    log("=" * 80)
    log(f"  Errors:   {len(all_errors)}")
    log(f"  Warnings: {len(all_warnings)}")
    log(f"  Info:     {len(all_info)}")

    if all_errors:
        log(f"\n" + "=" * 80)
        log("ERRORS")
        log("=" * 80)
        for e in all_errors:
            log(f"\n  [{e['blueprint']}] {e['widget']}")
            log(f"    {e['message']}")
            if e['suggestion']:
                log(f"    Fix: {e['suggestion']}")

    if all_warnings:
        log(f"\n" + "=" * 80)
        log("WARNINGS")
        log("=" * 80)
        for w in all_warnings:
            log(f"\n  [{w['blueprint']}] {w['widget']}")
            log(f"    {w['message']}")
            if w['suggestion']:
                log(f"    Fix: {w['suggestion']}")

    if all_info:
        log(f"\n" + "=" * 80)
        log("INFO (Style Suggestions)")
        log("=" * 80)

        # Group by message type for cleaner output
        by_message = defaultdict(list)
        for i in all_info:
            by_message[i['message']].append(f"{i['blueprint']}:{i['widget']}")

        for message, locations in sorted(by_message.items(), key=lambda x: -len(x[1])):
            log(f"\n  {message}")
            log(f"    Occurrences: {len(locations)}")
            for loc in locations[:5]:
                log(f"      - {loc}")
            if len(locations) > 5:
                log(f"      ... and {len(locations) - 5} more")

    # Actionable fixes
    log(f"\n" + "=" * 80)
    log("QUICK FIXES")
    log("=" * 80)

    # Find all widgets that need to be marked as variables
    needs_variable = [w for w in all_warnings if "not marked as variable" in w['message']]
    if needs_variable:
        log("\n  Widgets needing IsVariable=true:")
        by_bp = defaultdict(list)
        for w in needs_variable:
            by_bp[w['blueprint']].append(w['widget'])

        for bp, widgets in by_bp.items():
            log(f"\n    {bp}:")
            for w in widgets:
                log(f"      - {w}")

        log("\n  Python fix command:")
        log("    # Run in UE Editor to fix all at once:")
        log("    py \"D:/UEProjects/MO57/Content/Python/fix_widget_variables_final.py\"")

    log(f"\n" + "=" * 80)
    log("VALIDATION COMPLETE")
    log("=" * 80)

print("[VALIDATE] Starting...")
try:
    run()
    write_output()
except Exception as e:
    print(f"[VALIDATE ERROR] {e}")
    _output_lines.append(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    try:
        write_output()
    except:
        pass
print("[VALIDATE] Finished.")
