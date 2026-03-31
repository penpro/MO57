# Explore Widget Blueprint Python API
#
# Run: py "D:/UEProjects/MO57/Content/Python/explore_wbp_api.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def explore_object(obj, name="Object", depth=0):
    """List all attributes and methods on an object."""
    indent = "  " * depth
    log(f"{indent}{name}: {type(obj)}")

    # Get all attributes
    attrs = [a for a in dir(obj) if not a.startswith('__')]

    # Categorize
    methods = []
    properties = []

    for attr in attrs:
        try:
            val = getattr(obj, attr)
            if callable(val):
                methods.append(attr)
            else:
                properties.append((attr, type(val).__name__, str(val)[:50]))
        except:
            properties.append((attr, "?", "access error"))

    if properties:
        log(f"{indent}Properties:")
        for prop, typ, val in properties[:15]:  # Limit output
            log(f"{indent}  {prop} ({typ}): {val}")
        if len(properties) > 15:
            log(f"{indent}  ... and {len(properties) - 15} more")

    if methods:
        log(f"{indent}Methods: {methods[:20]}")
        if len(methods) > 20:
            log(f"{indent}  ... and {len(methods) - 20} more")

def test_load_class():
    """Test loading widget classes."""
    log("=" * 60)
    log("Testing unreal.load_class for UMG widgets")
    log("=" * 60)

    paths = [
        "/Script/UMG.ProgressBar",
        "/Script/UMG.TextBlock",
        "/Script/UMG.Image",
        "/Script/UMG.Border",
        "/Script/UMG.CanvasPanel",
        "/Script/UMG.VerticalBox",
        "/Script/UMG.ScrollBox",
        "/Script/SlateCore.SProgressBar",
    ]

    for path in paths:
        try:
            cls = unreal.load_class(None, path)
            log(f"  {path}: {cls}")
        except Exception as e:
            log(f"  {path}: ERROR - {e}")

def explore_widget_blueprint():
    """Deep explore a widget blueprint."""
    log("=" * 60)
    log("Exploring WBP_MOProgressWidget")
    log("=" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/CommonUI/WBP_MOProgressWidget")
    if not wbp:
        log("Failed to load WBP")
        return

    explore_object(wbp, "WidgetBlueprint")

    # Try generated_class
    log("")
    log("Checking generated_class:")
    try:
        gen_class = wbp.generated_class()
        if gen_class:
            explore_object(gen_class, "GeneratedClass", 1)
    except Exception as e:
        log(f"  Error: {e}")

    # Try to find widget-related subsystems or utilities
    log("")
    log("Checking for editor utilities:")

    utilities_to_check = [
        "WidgetBlueprintLibrary",
        "UMGEditorProjectSettings",
        "WidgetBlueprintEditorUtils",
        "EditorWidgetLibrary",
    ]

    for util in utilities_to_check:
        if hasattr(unreal, util):
            log(f"  Found: unreal.{util}")
        else:
            log(f"  Not found: unreal.{util}")

    # Check what's in unreal module related to widgets
    log("")
    log("Widget-related items in unreal module:")
    widget_items = [item for item in dir(unreal) if 'widget' in item.lower() or 'umg' in item.lower()]
    for item in widget_items[:20]:
        log(f"  {item}")

def check_subsystems():
    """Check for editor subsystems that might help."""
    log("=" * 60)
    log("Checking Editor Subsystems")
    log("=" * 60)

    # Try to get editor subsystem
    try:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if editor_subsystem:
            log(f"UnrealEditorSubsystem: {editor_subsystem}")
    except Exception as e:
        log(f"UnrealEditorSubsystem: {e}")

    # Check for asset tools
    try:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        log(f"AssetTools: {asset_tools}")
    except Exception as e:
        log(f"AssetTools: {e}")

def run():
    test_load_class()
    log("")
    explore_widget_blueprint()
    log("")
    check_subsystems()
    log("")
    log("Exploration complete.")

if __name__ == "__main__":
    run()
