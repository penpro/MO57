# Inspect Widget Hierarchy - Find root and parent widgets
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/inspect_widget_hierarchy.py"

print("=== STARTING ===")
import unreal

def log(msg):
    print(f"[MO57] {msg}")

def inspect_blueprint(wbp_path):
    """Inspect a widget blueprint's hierarchy."""
    log(f"\nInspecting: {wbp_path}")
    log("-" * 50)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log(f"  ERROR: Failed to load")
        return

    # Try to get the root widget
    if hasattr(wbp, 'get_editor_property'):
        # Try various property names that might give us the widget tree
        props_to_try = ['widget_tree', 'WidgetTree', 'root_widget', 'RootWidget']
        for prop in props_to_try:
            try:
                val = wbp.get_editor_property(prop)
                if val:
                    log(f"  {prop} = {val}")
            except:
                pass

    # List all widgets we can find by trying common names
    common_names = [
        "RootWidget", "Root", "CanvasPanel", "MainCanvas",
        "RootCanvas", "ContentPanel", "MainPanel", "Background",
        "Container", "RootContainer", "ProgressBar", "ActionNameText",
        "TimeRemainingText", "TitleText", "MessageText"
    ]

    log(f"\n  Searching for widgets:")
    found_widgets = []
    for name in common_names:
        widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(name))
        if widget:
            widget_type = type(widget).__name__
            found_widgets.append((name, widget_type))
            log(f"    FOUND: {name} ({widget_type})")

    if not found_widgets:
        log(f"    No widgets found with common names")

    # Try to find any panel-type widget that could be a parent
    panel_types = ["CanvasPanel", "VerticalBox", "HorizontalBox", "Overlay", "Border", "SizeBox"]
    log(f"\n  Looking for container widgets that could be parents:")
    for name, wtype in found_widgets:
        if any(pt in wtype for pt in panel_types):
            log(f"    POTENTIAL PARENT: {name} ({wtype})")

def run():
    log("=" * 60)
    log("Widget Hierarchy Inspector")
    log("=" * 60)

    blueprints = [
        "/MOFramework/UI/CommonUI/WBP_MOProgressWidget",
        "/MOFramework/UI/WBP_InspectionProgress",
        "/MOFramework/UI/WBP_HarvestProgress",
    ]

    for bp in blueprints:
        try:
            inspect_blueprint(bp)
        except Exception as e:
            log(f"Error inspecting {bp}: {e}")
            import traceback
            traceback.print_exc()

    log("\n" + "=" * 60)
    log("Done")
    log("=" * 60)

run()
