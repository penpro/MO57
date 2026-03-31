# Explore Widget Tree API - Find how to set IsVariable
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/explore_widget_tree.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def explore_widget_blueprint(wbp_path: str):
    """Deep exploration of WidgetBlueprint structure."""
    log("")
    log("=" * 70)
    log(f"Exploring: {wbp_path}")
    log("=" * 70)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log(f"Failed to load")
        return

    log(f"Type: {type(wbp)}")

    # List ALL properties on the WidgetBlueprint
    log("")
    log("All WidgetBlueprint attributes containing 'widget' or 'tree' or 'variable':")
    for attr in dir(wbp):
        if not attr.startswith('_'):
            lower = attr.lower()
            if 'widget' in lower or 'tree' in lower or 'variable' in lower or 'slot' in lower:
                log(f"  {attr}")

    # Try to get editor properties
    log("")
    log("Trying get_editor_property for common names:")
    prop_names = [
        'widget_tree', 'WidgetTree',
        'bindings', 'Bindings',
        'animations', 'Animations',
    ]
    for prop in prop_names:
        try:
            val = wbp.get_editor_property(prop)
            log(f"  {prop}: {val} ({type(val).__name__})")

            # If we got the widget tree, explore it
            if val and 'tree' in prop.lower():
                explore_widget_tree(val)
        except Exception as e:
            pass  # Property doesn't exist

    # Try calling methods
    log("")
    log("Trying method calls:")
    method_names = ['widget_tree', 'get_widget_tree', 'get_all_widgets']
    for method in method_names:
        if hasattr(wbp, method):
            try:
                result = getattr(wbp, method)()
                log(f"  {method}(): {result} ({type(result).__name__})")
                if result and 'tree' in method.lower():
                    explore_widget_tree(result)
            except Exception as e:
                log(f"  {method}(): ERROR - {e}")

    # Find a widget and explore its properties
    log("")
    log("Exploring a found widget's properties:")
    widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name("ProgressBar"))
    if widget:
        log(f"Found widget: {widget.get_name()} ({type(widget).__name__})")

        # List all attributes
        log("  All attributes containing 'var' or 'bind' or 'slot':")
        for attr in dir(widget):
            if not attr.startswith('_'):
                lower = attr.lower()
                if 'var' in lower or 'bind' in lower or 'slot' in lower:
                    log(f"    {attr}")

        # Try common property names
        log("  Trying get_editor_property:")
        for prop in ['Slot', 'slot', 'bIsVariable', 'b_is_variable', 'IsVariable']:
            try:
                val = widget.get_editor_property(prop)
                log(f"    {prop}: {val}")
            except:
                pass

        # Check if there's a slot
        if hasattr(widget, 'slot'):
            slot = widget.slot
            if slot:
                log(f"  Widget slot: {slot} ({type(slot).__name__})")
                for attr in dir(slot):
                    if not attr.startswith('_'):
                        log(f"    slot.{attr}")

def explore_widget_tree(widget_tree):
    """Explore a WidgetTree object."""
    log("")
    log(f"  WidgetTree exploration:")
    log(f"    Type: {type(widget_tree).__name__}")

    # List methods
    log("    Methods containing 'widget' or 'variable':")
    for attr in dir(widget_tree):
        if not attr.startswith('_'):
            lower = attr.lower()
            if 'widget' in lower or 'variable' in lower or 'root' in lower:
                log(f"      {attr}")

    # Try to get root
    if hasattr(widget_tree, 'root_widget'):
        root = widget_tree.root_widget
        log(f"    root_widget: {root}")

    # Try all_widgets
    if hasattr(widget_tree, 'all_widgets'):
        try:
            widgets = list(widget_tree.all_widgets)
            log(f"    all_widgets count: {len(widgets)}")
        except:
            pass

def explore_subsystem():
    """Check for UMG-related subsystems."""
    log("")
    log("=" * 70)
    log("Exploring Editor Subsystems")
    log("=" * 70)

    # Check what's in unreal module related to widgets
    log("unreal module - widget/umg related:")
    for name in dir(unreal):
        lower = name.lower()
        if 'widget' in lower or 'umg' in lower or 'blueprint' in lower:
            if not name.startswith('_'):
                log(f"  {name}")

def try_kismet_approach(wbp_path: str):
    """Try using Kismet/Blueprint library approach."""
    log("")
    log("=" * 70)
    log("Trying Kismet/Blueprint library approach")
    log("=" * 70)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        return

    # Try to find blueprint variable manipulation functions
    if hasattr(unreal, 'BlueprintEditorLibrary'):
        bel = unreal.BlueprintEditorLibrary
        log("BlueprintEditorLibrary methods:")
        for attr in dir(bel):
            if not attr.startswith('_'):
                lower = attr.lower()
                if 'variable' in lower or 'widget' in lower:
                    log(f"  {attr}")

    if hasattr(unreal, 'WidgetBlueprintLibrary'):
        wbl = unreal.WidgetBlueprintLibrary
        log("WidgetBlueprintLibrary methods:")
        for attr in dir(wbl):
            if not attr.startswith('_'):
                log(f"  {attr}")

def run():
    log("=" * 70)
    log("Widget Tree API Explorer")
    log("=" * 70)

    explore_widget_blueprint("/MOFramework/UI/WBP_InspectionProgress")
    explore_subsystem()
    try_kismet_approach("/MOFramework/UI/WBP_InspectionProgress")

    log("")
    log("=" * 70)
    log("Exploration Complete")
    log("=" * 70)

if __name__ == "__main__":
    run()
